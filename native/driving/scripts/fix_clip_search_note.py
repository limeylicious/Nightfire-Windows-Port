"""Idempotent: LEAN_CLIPSEARCH_LOG hook at the return of the clip-track search (sub_00076C60).

Diagnostic only: logs (once per caller/table/key) each lookup whose result is not the first
track, with the requested key bytes (+0xC action, +0xD variant) and the found track's key.
"""
import pathlib, sys
GEN = pathlib.Path(__file__).resolve().parent.parent / 'src/recomp/gen'
MARK = '/* LEAN_CLIPSEARCH_LOG */'
for p in sorted(GEN.glob('recomp_*.c')):
    s = p.read_bytes().decode('latin-1')
    if 'void sub_00076C60(void)' not in s:
        continue
    if MARK in s:
        print('already applied:', p.name); sys.exit(0)
    i = s.index('void sub_00076C60(void)'); j = s.index('\n}', i)
    body = s[i:j]
    nl = '\r\n' if '\r\n' in body else '\n'
    old = 'loc_00076CBD: ;' + nl + '    POP32(esp, edi);' + nl + '    eax = ebp;' + nl
    assert body.count(old) == 1, body.count(old)
    new = old + '    { extern void lean_clipsearch_note(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t); lean_clipsearch_note(ebp, MEM32(esp + 8), MEM32(esp + 0xC), MEM32(esp + 0x10), MEM32(esp + 4)); } ' + MARK + nl
    p.write_bytes((s[:i] + body.replace(old, new) + s[j:]).encode('latin-1'))
    print('applied:', p.name); sys.exit(0)
sys.exit('sub_00076C60 not found')
