"""Paris crane hook probe (LEAN_HOOK_PROBE=1; notes: hires\\hook-20261009\\AIM-PATH.md, probe\\README.md).

Adds one-line calls marked /* LEAN_HOOK_PROBE */ to the generated code and installs
lean_hook_probe.c (kept next to this script) as src/recomp/gen/lean_hook_probe.c, which the
CMake glob of src/recomp/gen/*.c compiles. Without LEAN_HOOK_PROBE=1 the calls return at once.

  recomp_0004.c  sub_00071D80 loc_00071DAF   bullet record slot -> bullet type (sniper = 0x1A), shot line
  recomp_0007.c  sub_000D0A30 loc_000D0B45   after the C64B0 grid walk: walked cells, hook cell visited?
                 sub_000D0A30 loc_000D0BA2   each region the walk reaches, before the D0A30 filter
                 sub_000D0A30 loc_000D0BFA   CF520 result
                 sub_000D0C90 entry          once per step: hook region +0x10..0x13 changed?
                 sub_000CF300 loc_000CF42C   one-shot clear of +0x12 bit 0
  recomp_0003.c  sub_00048EC0 (2), sub_000495D0 (3), sub_0004CBC0 (3): message bodies clearing +0x12 bit 0

Only whole lines are inserted, so --undo restores the files byte for byte.
Usage: python hook_probe_patch.py [--check | --apply | --undo] [--root <nightfire-driving-native>]
Default root: the parent of this script's folder (place it in nightfire-driving-native\\scripts).
Exit code 0 when the requested state is reached; --check: 0 applied, 3 not applied, 1 partial or anchors not found."""
import hashlib, re, shutil, sys
from pathlib import Path

MARK = '/* LEAN_HOOK_PROBE */'
HERE = Path(__file__).resolve().parent
HELPER = 'lean_hook_probe.c'
HELPER_TAG = b'/* LEAN_HOOK_PROBE=1 (diagnostic, Paris crane hook'

def call(proto, args):
    return '    { extern void %s; %s(%s); } %s' % (proto, proto.split('(')[0], args, MARK)

W5 = 'lean_hook_probe_write(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)'
# (file, function, anchor regex (whole line, no EOL), 'after'|'before', expected matches, line(match, occurrence))
SITES = [
    ('recomp_0004.c', 'sub_00071D80', r'loc_00071DAF: ;', 'after', 1,
     lambda m, k: call('lean_hook_probe_record(uint32_t, uint32_t, uint32_t)', 'MEM32(0x1E84E4), esi, MEM32(ebp + 8)')),
    ('recomp_0007.c', 'sub_000D0A30', r'loc_000D0B45: ;', 'after', 1,
     lambda m, k: call('lean_hook_probe_walk(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)',
                       'MEM32(ebp + 8), edi, esi, MEM32(esp + 0x2C), MEM32(esp + 0x30)')),
    ('recomp_0007.c', 'sub_000D0A30', r'loc_000D0BA2: ;', 'after', 1,
     lambda m, k: call('lean_hook_probe_cand(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)',
                       'MEM32(ebp + 8), MEM32(ebx + 4) + (ZX16(MEM16(eax)) << 6), MEM32(0x23E270), '
                       'MEM32(esp + 0x1C) | ((uint32_t)MEM8(esp + 0x1B) << 8), MEM8(ebx + 8)')),
    ('recomp_0007.c', 'sub_000D0A30', r'loc_000D0BFA: ;', 'after', 1,
     lambda m, k: call('lean_hook_probe_cf520(uint32_t, uint32_t, uint32_t)', 'MEM32(ebp + 8), esi, LO8(eax)')),
    ('recomp_0007.c', 'sub_000D0C90', r'loc_000D0C90: ;', 'after', 1,
     lambda m, k: call('lean_hook_probe_step(void)', '')),
    ('recomp_0007.c', 'sub_000CF300', r'    MEM16\(edi \+ 0x12\) = LO16\(eax\);', 'before', 1,
     lambda m, k: call(W5, '0x000CF300u, %d, edi, LO16(eax), 0' % k)),
]
for fn, n in (('sub_00048EC0', 2), ('sub_000495D0', 3), ('sub_0004CBC0', 3)):
    SITES.append(('recomp_0003.c', fn, r'    MEM16\(eax \+ 0x12\) = LO16\((e[a-d]x)\);', 'before', n,
                  (lambda f: lambda m, k: call(W5, '0x%su, %d, eax, LO16(%s), esi' % (f[4:], k, m.group(1))))(fn)))

FUNC_RE = re.compile(r'^void (?:orig_)?sub_[0-9A-F]{8}\(void\)')

def body_range(lines, func):
    start = next((i for i, l in enumerate(lines) if l.rstrip('\r\n') == 'void %s(void)' % func), None)
    if start is None: return None
    end = next((i for i in range(start + 1, len(lines)) if FUNC_RE.match(lines[i])), len(lines))
    return start, end

def eol_of(line):
    return '\r\n' if line.endswith('\r\n') else '\n'

def plan(gen):
    """Return {file: (old_text, new_text)} for --apply, plus problems and the count already present."""
    out, problems, present, total = {}, [], 0, 0
    for fname in sorted({s[0] for s in SITES}):
        p = gen / fname
        if not p.exists(): problems.append('%s missing' % fname); continue
        old = p.read_bytes().decode('latin-1')
        lines = old.splitlines(keepends=True)
        inserts = []   # (index, text)
        for (f, func, rx, where, want, mk) in SITES:
            if f != fname: continue
            rng = body_range(lines, func)
            if rng is None: problems.append('%s: %s not found' % (fname, func)); continue
            pat = re.compile('^' + rx + '$')
            hits = [(i, m) for i in range(*rng) for m in [pat.match(lines[i].rstrip('\r\n'))] if m]
            if len(hits) != want:
                problems.append('%s: %s anchor %r matched %d times, expected %d' % (fname, func, rx, len(hits), want)); continue
            for k, (i, m) in enumerate(hits, 1):
                total += 1
                text = mk(m, k)
                j = i + 1 if where == 'after' else i - 1
                if 0 <= j < len(lines) and lines[j].rstrip('\r\n') == text: present += 1; continue
                inserts.append((i + 1 if where == 'after' else i, text + eol_of(lines[i])))
        for i, t in sorted(inserts, reverse=True): lines.insert(i, t)
        out[fname] = (old, ''.join(lines))
    return out, problems, present, total

def main(argv):
    mode = '--check'; root = HERE.parent
    a = list(argv)
    while a:
        x = a.pop(0)
        if x in ('--check', '--apply', '--undo'): mode = x
        elif x == '--root' and a: root = Path(a.pop(0)).resolve()
        else: print('usage: hook_probe_patch.py [--check|--apply|--undo] [--root DIR]'); return 2
    gen = root / 'src' / 'recomp' / 'gen'
    if not gen.is_dir(): print('%s: no %s' % (mode, gen)); return 1
    src_helper, dst_helper = HERE / HELPER, gen / HELPER
    if mode == '--undo':
        removed = 0
        for fname in sorted({s[0] for s in SITES}):
            p = gen / fname
            if not p.exists(): continue
            old = p.read_bytes().decode('latin-1')
            lines = old.splitlines(keepends=True)
            keep = [l for l in lines if not l.rstrip('\r\n').endswith(MARK)]
            removed += len(lines) - len(keep)
            if len(keep) != len(lines): p.write_bytes(''.join(keep).encode('latin-1'))
        helper = 'absent'
        if dst_helper.exists():
            if dst_helper.read_bytes().startswith(HELPER_TAG): dst_helper.unlink(); helper = 'removed'
            else: helper = 'KEPT (not the probe file)'
        print('--undo: %d probe lines removed; gen/%s %s' % (removed, HELPER, helper)); return 0
    out, problems, present, total = plan(gen)
    helper_ok = dst_helper.exists() and src_helper.exists() and dst_helper.read_bytes() == src_helper.read_bytes()
    if problems:
        print('%s: NOT applied, anchors did not match: %s' % (mode, '; '.join(problems))); return 1
    if mode == '--check':
        state = 'applied' if present == total and helper_ok else ('not applied' if present == 0 and not dst_helper.exists() else 'PARTIAL')
        print('--check: %d of %d probe lines present, helper %s -> %s' % (present, total, 'installed' if helper_ok else 'missing/different', state))
        return {'applied': 0, 'not applied': 3}.get(state, 1)
    if not src_helper.exists(): print('--apply: %s missing next to this script' % HELPER); return 1
    if dst_helper.exists() and not dst_helper.read_bytes().startswith(HELPER_TAG):
        print('--apply: gen/%s exists and is not the probe file; not touching it' % HELPER); return 1
    changed = [f for f, (o, n) in out.items() if o != n]
    if changed:   # one-time copies of the untouched files (undo is byte-exact anyway)
        bk = root / 'recovery' / 'hookprobe'
        bk.mkdir(parents=True, exist_ok=True)
        for f in changed:
            if not (bk / f).exists() and MARK not in out[f][0]: (bk / f).write_bytes(out[f][0].encode('latin-1'))
    for f in changed: (gen / f).write_bytes(out[f][1].encode('latin-1'))
    if not helper_ok: shutil.copyfile(src_helper, dst_helper)
    print('--apply: %d of %d probe lines were already present, %d inserted (%s); helper %s; sha256 %s' % (
        present, total, total - present, ', '.join(changed) or 'no file changed',
        'already installed' if helper_ok else 'installed', hashlib.sha256(dst_helper.read_bytes()).hexdigest()[:16]))
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
