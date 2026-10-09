"""Fix the `xor reg,reg; repe cmpsd; jcc` mis-lift in generated code (idempotent).

The lifter emits the jcc after a `repe cmpsd` as a test of the register the
preceding `xor reg,reg` zeroed (e.g. `if ((ebx != 0))`), so the string compare's
result is lost: "different" is never reported (jne never taken / je always taken).
The compare loop leaves its result in `_flags` (1 = last dwords equal). A zero-count
compare leaves ZF from the xor (= equal), so `_flags` is seeded to 1 before the loop.
Found 2026-10-06 in five places (95 rep cmps sites scanned; the cmpsb ones are fine):

  group 1 audio : 0x13BC20  stream slot header compare (format change -> slot state 2)
  group 2 d3d   : 0x15E149  D3DX 256-dword compare;  0x165DE0  D3D 192-dword compare
  group 3 dsound: 0x17C4A9  two 4-dword compares (bypassed when LEAN_AUDIO_NATIVE=1)

Each fixed jcc becomes `(lean_fix_cmpsd(group) ? <test on _flags> : <old test>)`.
LEAN_FIX_CMPSD=audio | d3d | all (comma list allowed) switches groups on; off by default.
Helpers live in runtime/lean/lean_flags.c; lean_cmpsd_note() counts 0x13BC20
"header changed" results. Usage: python scripts/fix_cmpsd_flags.py [gen dir]
"""
import pathlib, sys

GEN = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parent.parent / 'src/recomp/gen')
MARK = '/* LEAN_FIX_CMPSD */'
DECL = 'int lean_fix_cmpsd(int group); int lean_cmpsd_note(int changed); ' + MARK
LOOP = '{ int32_t _st = RECOMP_DF_STEP(4);'
SITES = [   # function, old jcc line, kind, group
    ('sub_0013BC20', 'if ((ebx != 0)) goto loc_0013BD29;', 'jne', 1),
    ('sub_0015E149', 'if ((edx != 0)) goto loc_0015E277;', 'jne', 2),
    ('sub_00165DE0', 'if ((ebx == 0)) goto loc_00165E5C;', 'je', 2),
    ('sub_0017C4A9', 'if ((edx != 0)) goto loc_0017C4CF;', 'jne', 3),
    ('sub_0017C4A9', 'if ((eax != 0)) goto loc_0017C4E9;', 'jne', 3),
]

done = 0
for path in sorted(GEN.glob('recomp_*.c')):
    raw = path.read_bytes(); crlf = b'\r\n' in raw
    text = raw.decode('latin-1').replace('\r\n', '\n'); changed = False
    for func, old, kind, group in SITES:
        start = text.find('\nvoid %s(void)\n' % func)
        if start < 0:
            continue
        end = text.find('\nvoid sub_', start + 10)
        end = len(text) if end < 0 else end
        body = text[start:end]
        i = body.find(old)
        if i < 0:
            if MARK in body:
                continue
            sys.exit('%s: expected site not found in %s' % (func, path.name))
        if '/* repe cmpsd */' not in body[max(0, i - 200):i]:
            sys.exit('%s: site is not right after a repe cmpsd' % func)
        loop = body.rfind(LOOP, 0, i)
        if loop < 0 or i - loop > 400:
            sys.exit('%s: compare loop not found' % func)
        seed = 'if (lean_fix_cmpsd(%d)) _flags = 1; %s\n    ' % (group, MARK)
        body = body[:loop] + seed + body[loop:]
        i = body.find(old)
        cond_old = old[len('if ('):old.index(') goto')]
        cond_new = '(_flags == 0)' if kind == 'jne' else '(_flags != 0)'
        if group == 1:
            cond_new = 'lean_cmpsd_note%s' % cond_new
        goto = old[old.index(' goto'):]
        new = 'if (lean_fix_cmpsd(%d) ? %s : %s)%s %s' % (group, cond_new, cond_old, goto, MARK)
        body = body[:i] + new + body[i + len(old):]
        text = text[:start] + body + text[end:]
        changed = True
        done += 1
    if changed:
        if DECL not in text:
            first = text.find('\n')
            text = text[:first + 1] + DECL + '\n' + text[first + 1:]
        if crlf:
            text = text.replace('\n', '\r\n')
        path.write_bytes(text.encode('latin-1'))
print('fixed %d site(s)' % done if done else 'already applied (or nothing to do)')
