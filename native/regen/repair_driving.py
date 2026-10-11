"""Rebuild the repaired Driving gen folder from fresh toolkit output.

  python repair_driving.py FRESH_GEN OUT_GEN [--recipe DIR] [--local DIR] [--scripts DIR]
                           [--toolkit DIR] [--xbe PATH] [--analysis DIR] [--work DIR]
                           [--stop-before STEP] [--only-recomp] [--report FILE]

FRESH_GEN is the toolkit's untouched output (recomp_*.c, recomp_funcs.h, recomp_dispatch.c,
recomp_stubs_unresolved.c, optionally recomp_types.h). OUT_GEN receives the repaired folder
(it must not exist or be empty). Nothing outside OUT_GEN and the work folder is written.

The recipe folder (default: ./recipe next to this script) holds steps.json, the ordered
list of steps. Step kinds:
  seed            copy FRESH_GEN into WORK/src/recomp/gen, normalise line endings, add the
                  toolkit's templates/runtime/recomp_types.h when the fresh folder has none
  script          run one of our repair scripts (pinned by SHA-256) inside WORK, the layout
                  the scripts expect (ROOT = WORK, ROOT/src/recomp/gen, ROOT/analysis, ...)
  flags_counters  number every `if (_flags` site in file order as lean_flags_hit(N),
                  skipping the listed (function address, ordinal) sites
  ops             apply address-keyed line edits (recipe/ops/*.json, plus --local/ops)
  lift            lift whole functions with the toolkit by start/end address ("cfg": decode
                  reachable code only, "seed_tables": jump tables given by address)
  hand            assemble hand files from lifts, then apply their ops
  copy            copy files that are entirely our own code from the recipe (or --local);
                  with "fill": "xbe", @XBE_STRING(0xVA)@ placeholders are replaced by the
                  game's strings read from the player's own XBE (--xbe)
Edits are keyed by function address (the "Original: 0x..." banner) and guarded by SHA-256
of the normalised text they apply to; text already in its final state is left alone, so
every step can be re-run. The recipe holds addresses, hashes, shape rules and our own code
only: lines of generated code that an edit keeps or moves are referenced by position, not
stored.
"""
import argparse, hashlib, json, os, re, shutil, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BANNER = re.compile(r'^/\*\*\n \* (\S+)\n \* Original: 0x([0-9A-Fa-f]+) - 0x([0-9A-Fa-f]+)', re.M)


# ---------------------------------------------------------------- text helpers
def norm(text):
    """CRLF/CR -> LF and no trailing whitespace on any line (same as compare_gen.norm)."""
    return '\n'.join(l.rstrip() for l in text.replace('\r\n', '\n').replace('\r', '\n').split('\n'))


def sha(text):
    return hashlib.sha256(text.encode('utf-8', 'surrogateescape')).hexdigest()


def sha16(text):
    return sha(text)[:16]


def read(p):
    return Path(p).read_bytes().decode('utf-8', 'surrogateescape')


def write(p, text):
    Path(p).parent.mkdir(parents=True, exist_ok=True)
    Path(p).write_bytes(text.encode('utf-8', 'surrogateescape'))


def blocks(text):
    """Split a recomp_NNNN.c text into [('header', text), (addr, text), ...] in file order.
    A function block runs from its banner to the next banner."""
    marks = list(BANNER.finditer(text))
    out = [('header', text[:marks[0].start()] if marks else text)]
    for i, m in enumerate(marks):
        end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
        out.append((int(m[2], 16), text[m.start():end]))
    return out


def segments(src_blocks, absorbed=()):
    """Group blocks into segments: one per block, except that a block whose address is in
    `absorbed` is joined to the segment before it (a function the working tree no longer
    keeps under its own banner)."""
    segs, order = {}, []
    cur = None
    for k, t in src_blocks:
        if k == 'header' or k not in absorbed:
            cur = k
            order.append(k)
            segs[k] = t
        else:
            segs[cur] += t
    return order, segs


# ---------------------------------------------------------------- line edits
RULES = {}


def make_line(src, item, pools=None):
    """One inserted line. str: our own text. int: copy that source line. dict: start from
    source line 's' (or, with 'l', from line 'i'/'s' of the toolkit lift of that address)
    and keep it, pass it through the named shape rule 'rule', or keep its first p and last
    q characters around our text x."""
    if isinstance(item, str):
        return item
    if isinstance(item, int):
        return src[item]
    lines = pools[item['l']] if 'l' in item else src
    if 'i' in item:
        return lines[item['i']]
    line = lines[item['s']]
    if 'rule' in item:
        pat, rep = RULES[item['rule']]
        return re.sub(pat, rep, line, count=1)
    p, q = item.get('p', 0), item.get('q', 0)
    return line[:p] + item.get('x', '') + (line[len(line) - q:] if q else '')


def make_range(item, pools):
    """Lines [a, b) of the toolkit lift item['l'] (b may be negative, counted from the end),
    each passed through the shape rules item['rules'] in order (a rule that does not match
    leaves the line unchanged)."""
    a, b = item['range']
    out = []
    for line in pools[item['l']][a:b]:
        for r in item.get('rules', []):
            pat, rep = RULES[r]
            line = re.sub(pat, rep, line, count=1)
        out.append(line)
    return out


def apply_hunks(text, hunks, pools=None):
    """hunks: [{'at': i, 'del': n, 'ins': [...]}] against the source line list. Applied
    back to front; hunks at the same position keep their list order."""
    src = text.split('\n')
    out = list(src)
    order = sorted(range(len(hunks)), key=lambda k: (hunks[k]['at'], k), reverse=True)
    for k in order:
        h = hunks[k]
        ins = []
        for x in h.get('ins', []):
            ins.extend(make_range(x, pools) if isinstance(x, dict) and 'range' in x else [make_line(src, x, pools)])
        out[h['at']:h['at'] + h.get('del', 0)] = ins
    return '\n'.join(out)


LIFT_POOLS = {}


def pools_for(edit):
    out = {}
    for key in edit.get('lifts', []):
        if key not in LIFT_POOLS:
            return None
        out[key] = LIFT_POOLS[key]
    return out


def apply_edit(text, edit, payloads):
    """Return (new_text, state) where state is applied / already / mismatch / ..."""
    if sha16(text) == edit['dst']:
        return text, 'already'
    if sha16(text) != edit['src']:
        return text, 'mismatch'
    hunks = list(edit.get('hunks', []))
    if edit.get('payload'):
        local = payloads.get(edit['payload'])
        if local is None:
            return text, 'missing-local'
        hunks = sorted(hunks + local, key=lambda h: h.get('seq', 0))
    pools = pools_for(edit)
    if pools is None:
        return text, 'missing-lift'
    new = apply_hunks(text, hunks, pools)
    if sha16(new) != edit['dst']:
        return text, 'result-mismatch'
    return new, 'applied'


def load_ops(paths):
    edits = []
    for p in paths:
        for f in sorted(Path(p).glob('*.json')) if Path(p).is_dir() else [Path(p)]:
            data = json.loads(f.read_text())
            for e in data.get('edits', []):
                e['_from'] = f.name
                edits.append(e)
    return edits


def load_payloads(local):
    out = {}
    if local and (Path(local) / 'payloads').is_dir():
        for f in sorted((Path(local) / 'payloads').glob('*.json')):
            out.update(json.loads(f.read_text()))
    return out


def step_ops(gen, edits, payloads, log):
    """Apply edits. Each edit names a 'file' and a 'block' ('header', 'file' or 0xADDR)."""
    by_file = {}
    for e in edits:
        by_file.setdefault(e['file'], []).append(e)
    counts = {}
    for name, es in sorted(by_file.items()):
        path = gen / name
        text = norm(read(path)) if path.exists() else ''
        if any(e['block'] == 'file' for e in es):
            for e in es:
                text, st = apply_edit(text, e, payloads)
                counts[st] = counts.get(st, 0) + 1
                if st not in ('applied', 'already'):
                    log.append(f'{name} whole file: {st} ({e["_from"]})')
            write(path, text)
            continue
        absorbed = {int(k, 16) for e in es for k in e.get('absorbs', [])}
        order, segs = segments(blocks(text), absorbed)
        for e in es:
            k = 'header' if e['block'] == 'header' else int(e['block'], 16)
            if k not in segs:
                counts['absent'] = counts.get('absent', 0) + 1
                log.append(f'{name} {e["block"]}: block absent ({e["_from"]})')
                continue
            segs[k], st = apply_edit(segs[k], e, payloads)
            counts[st] = counts.get(st, 0) + 1
            if st not in ('applied', 'already'):
                log.append(f'{name} {e["block"]}: {st} ({e["_from"]})')
        write(path, ''.join(segs[k] for k in order))
    return counts


# ---------------------------------------------------------------- flags counters
def step_flags_counters(gen, step, log):
    """Wrap each `if (_flags` condition as `if ((lean_flags_hit(N)), _flags` in file order."""
    files = sorted(gen.glob('recomp_[0-9][0-9][0-9][0-9].c'))
    texts = {f: norm(read(f)) for f in files}
    if any('lean_flags_hit(' in t for t in texts.values()):
        n = sum(len(re.findall(r'lean_flags_hit\(\d+\)', t)) for t in texts.values())
        return {'already': n}
    skip = {}
    for s in step.get('skip', []):
        skip.setdefault(int(s['function'], 16), set()).add(s.get('ordinal', 0))
    n = 0
    for f in files:
        out = []
        for k, t in blocks(texts[f]):
            parts = re.split(r'(if \(_flags\b)', t)
            occ = 0
            for i in range(1, len(parts), 2):
                if k in skip and occ in skip[k]:
                    pass
                else:
                    parts[i] = 'if ((lean_flags_hit(%d)), _flags' % n
                    n += 1
                occ += 1
            out.append(''.join(parts))
        write(f, ''.join(out))
    if step.get('expect') is not None and n != step['expect']:
        log.append(f'flags_counters: numbered {n} sites, expected {step["expect"]}')
    return {'numbered': n}


# ---------------------------------------------------------------- scripts
def step_script(root, step, scripts, a, log):
    src = Path(scripts) / step['script']
    if not src.exists():
        log.append(f'{step["script"]}: not found in {scripts}')
        return {'missing': 1}
    have = hashlib.sha256(src.read_bytes()).hexdigest()
    if step.get('sha256') and have != step['sha256']:
        log.append(f'{step["script"]}: SHA-256 {have[:16]} differs from the pinned {step["sha256"][:16]}')
    dst = root / 'scripts' / step['script']
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, dst)
    for extra in step.get('with', []):            # files the script expects next to itself
        shutil.copyfile(Path(scripts) / extra, root / 'scripts' / extra)
    for rel, what in step.get('outside', {}).items():   # files the script reads beside ROOT
        dst_o = root.parent / rel
        dst_o.parent.mkdir(parents=True, exist_ok=True)
        if what == 'xbe':
            if not a.xbe:
                log.append(f'{step["script"]}: needs --xbe')
                return {'missing': 'xbe'}
            shutil.copyfile(a.xbe, dst_o)
        elif what == 'placeholder':                  # only hashed or copied by the script
            dst_o.write_bytes(b'/* placeholder: not used for the generated code */\n')
    args = [a.replace('{gen}', str(root / 'src/recomp/gen')).replace('{root}', str(root)) for a in step.get('args', [])]
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1', PYTHONIOENCODING='utf-8')
    r = subprocess.run([sys.executable, str(dst), *args], cwd=root, env=env, capture_output=True, text=True)
    ok = r.returncode in step.get('exit', [0])
    tail = (r.stdout + r.stderr).strip().splitlines()[-3:]
    if not ok:
        log.append(f'{step["script"]}: exit {r.returncode}: {" | ".join(tail)}')
    # scripts may write CRLF or LF; keep the folder normalised between steps
    for p in (root / 'src/recomp/gen').iterdir():
        if p.suffix in ('.c', '.h'):
            t = read(p)
            if '\r' in t:
                write(p, norm(t))
    return {'exit': r.returncode, 'out': ' | '.join(tail)}


# ---------------------------------------------------------------- lifting
LIFT_CODE = r'''
import json, sys
from pathlib import Path
from tools.recomp import config
from tools.recomp.translator import BatchTranslator
xbe, functions, labels, identified, abi, spec, out, game = sys.argv[1:9]
config.set_game_name(game)
config.configure_from_xbe(xbe)
spec = json.loads(Path(spec).read_text())
funcs = json.loads(Path(functions).read_text())
by = {int(f['start'], 16): f for f in funcs}
for s in spec:
    st, en = int(s['start'], 16), int(s['end'], 16)
    if st in by:
        by[st].update(end=f'0x{en:08x}', size=en - st)
    else:
        funcs.append({'start': f'0x{st:08X}', 'end': f'0x{en:08X}', 'size': en - st, 'name': f'sub_{st:08X}',
                      'section': '.text', 'confidence': 1.0, 'detection_method': 'manual',
                      'calls_to': [], 'called_by': []})
funcs.sort(key=lambda f: int(f['start'], 16))
out = Path(out); out.mkdir(parents=True, exist_ok=True)
(out / '_functions.json').write_text(json.dumps(funcs))
t = BatchTranslator(xbe_path=xbe, func_json_path=str(out / '_functions.json'), labels_json_path=labels,
                    identified_json_path=identified, abi_json_path=abi, output_dir=str(out / '_o'),
                    trace_functions=set())
bad = 0
import struct
from tools.recomp.config import va_to_file_offset
xbe_bytes = Path(xbe).read_bytes()
for s in spec:
    st = int(s['start'], 16)
    if s.get('cfg'):
        # reachable code only (data inside the range is never decoded); tables whose first
        # entry is not a code pointer are seeded from the entries after it (s['seed_tables'])
        en = int(s['end'], 16); ft = t.translator
        ins, tables, _ = ft._recover_cfg(st, en, set(), set())
        for tab in s.get('seed_tables', []):
            tab = int(tab, 16)
            tables[tab] = list(struct.unpack_from('<3I', xbe_bytes, va_to_file_offset(tab) + 4))
        seeds = {st} | {x for v in tables.values() for x in v}
        ins = ft.disasm.disassemble_cfg(xbe_bytes[va_to_file_offset(st):va_to_file_offset(st) + en - st], st, en, seeds)
        ft._recovered_cfg[st] = dict(end=en, instructions=ins, jump_tables=tables)
    code = t.translate_single(st)
    if code:
        (out / f'{st:08X}.c').write_text(code)
    else:
        bad += 1
        print(f'could not translate 0x{st:08X}', file=sys.stderr)
print(f'lifted {len(spec) - bad}/{len(spec)}')
'''


def step_lift(work, step, a, log):
    spec = json.loads((Path(a.recipe) / step['spec']).read_text())
    out = work / 'lifts' / step.get('group', 'hand')
    if out.is_dir() and all((out / f'{int(s["start"], 16):08X}.c').exists() for s in spec):
        return {'already': len(spec)}
    an = Path(a.analysis)
    need = [an / 'disasm/functions.json', an / 'disasm/labels.json',
            an / 'func_id/identified_functions.json', an / 'abi/abi_functions.json']
    for p in need + [Path(a.xbe), Path(a.toolkit) / 'tools']:
        if not p.exists():
            log.append(f'lift: missing {p}')
            return {'missing': str(p)}
    (work / 'lift_spec.json').write_text(json.dumps(spec))
    (work / 'lift_run.py').write_text(LIFT_CODE)
    out.mkdir(parents=True, exist_ok=True)
    # Keep the caller's PYTHONPATH (it may hold Capstone) after the toolkit.
    path = os.pathsep.join(filter(None, [str(Path(a.toolkit).resolve()), os.environ.get('PYTHONPATH')]))
    env = dict(os.environ, PYTHONPATH=path, PYTHONDONTWRITEBYTECODE='1')
    r = subprocess.run([sys.executable, str(work / 'lift_run.py'), str(a.xbe), *map(str, need),
                        str(work / 'lift_spec.json'), str(out), step.get('game_name', 'Nightfire PAL Driving')],
                       cwd=str(a.toolkit), env=env, capture_output=True, text=True)
    if r.returncode:
        log.append('lift: ' + (r.stdout + r.stderr)[-800:])
    return {'exit': r.returncode, 'out': r.stdout.strip()[-200:]}


def load_pools(work):
    """Lifts of the 'aux' group, referenced by edits as {'l': '0xADDR', ...}."""
    d = work / 'lifts' / 'aux'
    if d.is_dir():
        for f in d.glob('*.c'):
            LIFT_POOLS.setdefault(f'0x{f.stem}', norm(read(f)).split('\n'))


def step_hand(work, gen, step, a, payloads, log):
    """Assemble each hand file from lifts (joined in order) and apply its edit."""
    spec = json.loads((Path(a.recipe) / step['spec']).read_text())
    edits = {e['file']: e for e in load_ops([Path(a.recipe) / step['ops']] +
                                            ([Path(a.local) / step['ops']] if a.local and (Path(a.local) / step['ops']).exists() else []))}
    counts = {}
    for name, parts in spec.items():
        texts = []
        for p in parts:
            f = work / 'lifts' / step.get('group', 'hand') / f'{int(p, 16):08X}.c'
            if not f.exists():
                log.append(f'{name}: lift {p} missing')
                texts = None
                break
            texts.append(norm(read(f)))
        if texts is None:
            counts['missing-lift'] = counts.get('missing-lift', 0) + 1
            continue
        base = '\n'.join(texts)
        e = edits.get(name)
        if e is None:
            text, st = base, 'no-edit'
        else:
            text, st = apply_edit(base, e, payloads)
        counts[st] = counts.get(st, 0) + 1
        if st not in ('applied', 'already', 'no-edit'):
            log.append(f'{name}: {st}')
            continue
        write(gen / name, text)
    return counts


XBE_STR = re.compile(r'@XBE_STRING\(0x([0-9A-Fa-f]{8})(?:,(\d+))?\)@')


def xbe_va_reader(path):
    """VA -> bytes of the XBE image (section headers of the player's own file)."""
    data = Path(path).read_bytes()
    base, nsec, sec_va = (int.from_bytes(data[o:o + 4], 'little') for o in (0x104, 0x11C, 0x120))
    secs = []
    for i in range(nsec):
        h = sec_va - base + i * 56
        va, vsize, raw, rsize = (int.from_bytes(data[h + o:h + o + 4], 'little') for o in (4, 8, 12, 16))
        secs.append((va, min(vsize, rsize), raw))
    def at(va):
        for sva, size, raw in secs:
            if sva <= va < sva + size:
                return data[raw + va - sva:raw + size]
        raise KeyError(f'0x{va:08X} is not in a section of {path}')
    return at


def fill_xbe_strings(text, xbe):
    """Replace @XBE_STRING(0xVA)@ (or @XBE_STRING(0xVA,SKIP)@) with the NUL-terminated
    string at that VA of the player's XBE (from character SKIP on), escaped for a C literal."""
    if not XBE_STR.search(text):
        return text
    at = xbe_va_reader(xbe)
    def one(m):
        raw = at(int(m[1], 16))
        s = raw[:raw.index(0)][int(m[2] or 0):].decode('ascii')
        return s.replace('\\', '\\\\').replace('"', '\\"')
    return XBE_STR.sub(one, text)


def step_copy(gen, step, a, log):
    n = 0
    for name, info in step['files'].items():
        for base in [Path(a.recipe)] + ([Path(a.local)] if a.local else []):
            src = base / info['from']
            if src.exists():
                text = norm(read(src))
                if info.get('fill') == 'xbe':
                    if not a.xbe:
                        log.append(f'copy {name}: needs --xbe to fill its placeholders')
                        break
                    text = fill_xbe_strings(text, a.xbe)
                if sha(text) != info['sha256']:
                    log.append(f'copy {name}: SHA-256 differs from the recipe')
                write(gen / name, text)
                n += 1
                break
        else:
            log.append(f'copy {name}: {info["from"]} not found (local-only file?)')
    return {'copied': n}


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('fresh')
    ap.add_argument('out')
    ap.add_argument('--recipe', default=str(HERE / 'recipe'))
    ap.add_argument('--local', help='folder with local-only recipe parts (never published)')
    ap.add_argument('--scripts', default=str(HERE / 'scripts'), help='folder with our repair scripts')
    ap.add_argument('--toolkit', help='pinned xboxrecomp checkout (templates/, tools/)')
    ap.add_argument('--xbe', help='Driving.xbe (only for lifting)')
    ap.add_argument('--analysis', help='toolkit analysis folder with disasm/, func_id/, abi/ (only for lifting)')
    ap.add_argument('--work', help='work folder (default: OUT_GEN + ".work")')
    ap.add_argument('--stop-before', help='stop before this step id (leaves the work tree)')
    ap.add_argument('--report', help='write a JSON step report here')
    ap.add_argument('--snapshots', help='copy the gen folder here after every step (debugging)')
    a = ap.parse_args()
    for k in ('recipe', 'local', 'scripts', 'toolkit', 'xbe', 'analysis', 'work', 'snapshots', 'fresh', 'out'):
        if getattr(a, k):
            setattr(a, k, str(Path(getattr(a, k)).resolve()))
    recipe = Path(a.recipe)
    steps = json.loads((recipe / 'steps.json').read_text())['steps']
    fresh, out = Path(a.fresh), Path(a.out)
    if out.exists() and any(out.iterdir()):
        sys.exit(f'{out} exists and is not empty')
    work = Path(a.work) if a.work else Path(str(out).rstrip('/\\') + '.work')
    root = work / 'tree'
    gen = root / 'src/recomp/gen'
    payloads = load_payloads(a.local)
    if (recipe / 'rules.json').exists():
        RULES.update(json.loads((recipe / 'rules.json').read_text()))
    report, log = [], []
    for step in steps:
        sid, kind = step['id'], step['kind']
        if a.stop_before and sid == a.stop_before:
            break
        if kind == 'seed':
            if root.exists():
                shutil.rmtree(root)
            gen.mkdir(parents=True)
            (root / 'analysis').mkdir()
            for p in sorted(fresh.iterdir()):
                if p.suffix in ('.c', '.h'):
                    write(gen / p.name, norm(read(p)))
            if not (gen / 'recomp_types.h').exists():
                tpl = Path(a.toolkit or '') / 'templates/runtime/recomp_types.h'
                if not tpl.exists():
                    sys.exit('fresh folder has no recomp_types.h and --toolkit has no templates/runtime/recomp_types.h')
                write(gen / 'recomp_types.h', norm(read(tpl)))
            for rel, info in step.get('inputs', {}).items():
                src = recipe / info['from']
                write(root / rel, read(src))
            res = {'files': len(list(gen.iterdir()))}
        elif kind == 'script':
            res = step_script(root, step, a.scripts, a, log)
        elif kind == 'flags_counters':
            res = step_flags_counters(gen, step, log)
        elif kind == 'ops':
            load_pools(work)
            dirs = [recipe / step['dir']]
            if a.local and (Path(a.local) / step['dir']).exists():
                dirs.append(Path(a.local) / step['dir'])
            res = step_ops(gen, load_ops(dirs), payloads, log)
        elif kind == 'lift':
            res = step_lift(work, step, a, log)
        elif kind == 'hand':
            load_pools(work)
            res = step_hand(work, gen, step, a, payloads, log)
        elif kind == 'copy':
            res = step_copy(gen, step, a, log)
        elif kind == 'remove':
            res = {'removed': 0}
            for name in step['files']:
                if (gen / name).exists():
                    (gen / name).unlink()
                    res['removed'] += 1
        else:
            sys.exit(f'unknown step kind {kind}')
        report.append({'id': sid, 'kind': kind, **res})
        if a.snapshots:
            snap = Path(a.snapshots) / f'{len(report):02d}-{sid}'
            if snap.exists():
                shutil.rmtree(snap)
            shutil.copytree(gen, snap)
        print(f'{sid:20s} {json.dumps(res)[:150]}', flush=True)
    if not a.stop_before:
        out.mkdir(parents=True, exist_ok=True)
        for p in sorted(gen.iterdir()):
            if p.is_file() and p.suffix in ('.c', '.h'):
                write(out / p.name, norm(read(p)))
    if log:
        print('problems:\n  ' + '\n  '.join(log))
    if a.report:
        Path(a.report).write_text(json.dumps({'steps': report, 'problems': log}, indent=1))
    sys.exit(1 if log else 0)


if __name__ == '__main__':
    main()
