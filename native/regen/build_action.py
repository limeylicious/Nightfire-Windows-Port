"""Rebuild the Action engine's repaired generated code from fresh toolkit output.

    python build_action.py TREE --recipe-dir native/action/regen --toolkit XBOXRECOMP [--stop-after STAGE]

TREE is an Action build tree (scripts/, runtime/native/, game_files/default.xbe,
analysis/functions.json) whose src/recomp/gen already holds the toolkit's output for the
stored function list. The stages, in order:

  harden    scripts/harden_generated.py (visible stops, incoming EBP)
  splice    re-translate functions whose end was corrected (full switch CFGs) in place
  hand      write the separate translated files, each from toolkit lifts or copied blocks,
            and copy our own source files (recipe own/) into the folder
  prehunks  our edits the build scripts expect to find (recomp_types.h call checks)
  build     the scripts build-native.cmd runs on every build (renames and hooks)
  hunks     our own edits: hooks, fixes and notes, keyed by function address and guarded
            by SHA-256 hashes of the lines they replace

The recipe holds only addresses, our names, our code and hashes. Every translated line
comes from the toolkit running on the player's own default.xbe.
"""
import argparse, hashlib, json, os, re, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BANNER = re.compile(r'^/\*\*\n \* \S+\n \* Original: 0x([0-9A-Fa-f]+) - 0x[0-9A-Fa-f]+', re.M)


def sha(lines):
    return hashlib.sha256('\n'.join(lines).encode()).hexdigest()[:20]


def read(p):
    return Path(p).read_text(encoding='utf-8', errors='surrogateescape').replace('\r\n', '\n')


def write(p, text):
    Path(p).write_text(text, encoding='utf-8', errors='surrogateescape', newline='\n')


def blocks(text):
    """[(addr or None, start, end)] covering text: a preamble then one block per banner."""
    marks = list(BANNER.finditer(text))
    out = [(None, 0, marks[0].start() if marks else len(text))]
    for i, m in enumerate(marks):
        out.append((int(m[1], 16), m.start(), marks[i + 1].start() if i + 1 < len(marks) else len(text)))
    return out


# ---------------------------------------------------------------- toolkit transforms
def t_sync(code):
    """Frameless functions that keep a live EBP: publish it before calls and after POP."""
    code = re.sub(r'(    PUSH32\(esp, 0x[0-9A-F]+u\); RECOMP_ABI_CALL)', r'    g_ebp = ebp; g_seh_ebp = ebp;\n\1', code)
    return code.replace('POP32(esp, ebp);', 'POP32(esp, ebp);\n    g_ebp = ebp; g_seh_ebp = ebp;')


def t_leave(code):
    """LEAVE restores the caller's frame in both local and shared guest state."""
    return code.replace('POP32(esp, ebp); /* leave */', 'POP32(esp, ebp); /* leave */\n    g_ebp = ebp;\n    g_seh_ebp = ebp;')


TRANSFORMS = {'sync': t_sync, 'leave': t_leave}


def lift(ctx, u):
    from relift import relift
    s = int(u['start'], 16)
    up = int(u['upper'], 16) if u.get('upper') else None
    code, end, n, ntab = relift(ctx['xbe'], ctx['functions'], s, up, strip_interior=u.get('mode', 'keep') == 'strip')
    if u.get('expect_end') and int(u['expect_end'], 16) != end:
        raise SystemExit(f'lift 0x{s:08X}: end 0x{end:08X}, recipe expects {u["expect_end"]}')
    code = code.replace('uint32_t ebp;', 'uint32_t ebp = g_ebp;')
    for t in u.get('transforms', []):
        code = TRANSFORMS[t](code)
    return code, end, n


def lift_memmove(ctx, u):
    """CRT copy routine: byte-backed CFG with its alignment tables seeded explicitly."""
    import struct
    from tools.recomp.translator import FunctionTranslator
    from relift import load_db
    b = ctx['xbe']
    start, end = int(u['start'], 16), int(u['end'], 16)
    database = {a: dict(f) for a, f in load_db(ctx['functions']).items()}
    database[start] = dict(database[start], end=end, size=end - start)
    t = FunctionTranslator(b, database)
    instructions, tables, _ = t._recover_cfg(start, end, set(), set())
    for table in u['seed_tables']:
        table = int(table, 16)
        tables[table] = list(struct.unpack_from('<3I', b, table - 0x10000 + 4))
    seeds = {start} | {x for tab in tables.values() for x in tab}
    instructions = t.disasm.disassemble_cfg(b[start - 0x10000:end - 0x10000], start, end, seeds)
    t._recovered_cfg[start] = dict(end=end, instructions=instructions, jump_tables=tables)
    return t.translate_function(start, database[start]).replace('uint32_t ebp;', 'uint32_t ebp = g_ebp;')


# ---------------------------------------------------------------- stages
def stage_harden(ctx):
    gen = ctx['gen']
    if not (gen / 'recomp_types.h').exists():
        tmpl = ctx['toolkit'] / 'templates/runtime/recomp_types.h'
        write(gen / 'recomp_types.h', read(tmpl))
    if 'nightfire_diagnostic_stop(' in read(gen / 'recomp_0000.c'):
        return
    run(ctx, 'harden_generated.py')


def stage_splice(ctx):
    gen = ctx['gen']
    files = {p.name: read(p) for p in sorted(gen.glob('recomp_0*.c'))}
    for u in ctx['recipe'].get('splice', []):
        s = int(u['start'], 16)
        sig = f'void sub_{s:08X}(void)\n'
        name = next(n for n, t in files.items() if sig in t)
        t = files[name]
        if u.get('kind') == 'memmove':
            code = lift_memmove(ctx, u)
            a = t.index(sig); z = t.index('\n/**', a)
            files[name] = t[:a] + code[code.index(sig):] + '\n' + t[z:]
            continue
        code, end, n = lift(ctx, u)
        sig = sig + '{'
        a = t.index(sig); z = t.index('\n}\n', a) + 3
        t = t[:a] + code[code.index(sig):] + t[z:]
        files[name] = re.sub(rf'0x{s:08X} - 0x[0-9A-F]+ \(\d+ bytes, \d+ insns\)',
                             f'0x{s:08X} - 0x{end:08X} ({end - s} bytes, {n} insns); {u["note"]}', t)
    for n, t in files.items():
        write(gen / n, t)


def gen_block(ctx, addr):
    """A block of the hardened toolkit output, from its banner to the end of the function."""
    for p in sorted(ctx['gen'].glob('recomp_0*.c')):
        t = read(p)
        for a, s, e in blocks(t):
            if a == addr:
                body = t[s:e]
                return body[:body.index('\n}\n') + 3]
    raise SystemExit(f'no generated block 0x{addr:08X}')


def stage_hand(ctx):
    for name in ctx['recipe'].get('own', []):
        write(ctx['gen'] / name, read(ctx['recipe_dir'] / 'own' / name))
    for name in ctx['recipe'].get('own_src', []):   # src/recomp_manual.c, src/nightfire_diagnostics.h
        src = ctx['recipe_dir'] / 'own-src' / name
        if src.exists():
            write(ctx['tree'] / 'src' / name, read(src))
        else:
            print(f'  own-src/{name} is not in this copy of the recipe; the build needs it in src/', flush=True)
    for name, parts in ctx['recipe'].get('hand', {}).items():
        out = []
        for part in parts:
            if 'lift' in part:
                out.append(lift(ctx, dict(part, start=part['lift']))[0])
            elif 'copy' in part:
                out.append(ctx['copies'][int(part['copy'], 16)])
            elif 'text' in part:
                out.append(part['text'])
        write(ctx['gen'] / name, '\n'.join(out))


def run(ctx, script, *args):
    r = subprocess.run([sys.executable, str(ctx['tree'] / 'scripts' / script), *args], cwd=ctx['tree'],
                       capture_output=True, text=True)
    ctx['log'].append(f'{script} {" ".join(args)}: exit {r.returncode} {r.stdout.strip()[-300:]} {r.stderr.strip()[-300:]}')
    if r.returncode:
        raise SystemExit(f'{script} failed: {r.stdout}{r.stderr}')


def stage_build(ctx):
    # Same order as build-native.cmd.
    for script, args in ctx['recipe']['build_scripts']:
        run(ctx, script, *args)


def apply_hunks(lines, hunks, where):
    for h in sorted(hunks, key=lambda h: -h['at']):
        at, n = h['at'], h['old']
        old = lines[at:at + n]
        if sha(old) != h['sha']:
            raise SystemExit(f'{where}: hunk at line {at} does not match (sha {sha(old)} != {h["sha"]})')
        lines[at:at + n] = h['new']
    return lines


def stage_prehunks(ctx):
    stage_hunks(ctx, 'pre_hunks')


def stage_hunks(ctx, key='hunks'):
    gen = ctx['gen']
    for name, spec in ctx['recipe'].get(key, {}).items():
        p = gen / name
        t = read(p) if p.exists() else ''
        if 'file' in spec:
            t = '\n'.join(apply_hunks(t.split('\n'), spec['file'], name))
        if 'functions' in spec:
            pieces = []
            for a, s, e in blocks(t):
                body = t[s:e]
                key = 'pre' if a is None else f'{a:08X}'
                if key in spec['functions']:
                    body = '\n'.join(apply_hunks(body.split('\n'), spec['functions'][key], f'{name}:{key}'))
                pieces.append(body)
            t = ''.join(pieces)
        write(p, t)


STAGES = ['harden', 'splice', 'hand', 'prehunks', 'build', 'hunks']


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('tree')
    ap.add_argument('--recipe-dir', required=True)
    ap.add_argument('--toolkit', required=True)
    ap.add_argument('--stop-after', choices=STAGES)
    a = ap.parse_args()
    tree = Path(a.tree).resolve()
    sys.path[:0] = [str(Path(a.toolkit).resolve()), str(HERE)]
    from tools.recomp import config
    xbe = tree / 'game_files/default.xbe'
    config.configure_from_xbe(str(xbe))
    ctx = dict(tree=tree, gen=tree / 'src/recomp/gen', toolkit=Path(a.toolkit).resolve(),
               recipe=json.loads((Path(a.recipe_dir) / 'recipe.json').read_text()), recipe_dir=Path(a.recipe_dir).resolve(), xbe=xbe.read_bytes(),
               functions=tree / 'analysis/functions.json', log=[], copies={})
    for st in STAGES:
        if st == 'hand':
            # Blocks copied from the toolkit output are taken before anything renames them.
            for name, parts in ctx['recipe'].get('hand', {}).items():
                for part in parts:
                    if 'copy' in part:
                        ctx['copies'][int(part['copy'], 16)] = gen_block(ctx, int(part['copy'], 16))
        globals()['stage_' + st](ctx)
        print(f'{st}: done', flush=True)
        if st == a.stop_after:
            break
    for line in ctx['log']:
        print('  ' + line)


if __name__ == '__main__':
    main()
