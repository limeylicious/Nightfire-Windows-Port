"""Idempotent: LEAN_TIMER_EXACT60 hook in the XAPI multimedia-timer setup (0x10EED3).

The game asks for a 1000/60 = 16 ms period (whole milliseconds), so on the Xbox its step
timer runs at 62.5 Hz. With LEAN_TIMER_EXACT60=1 (off by default; enhanced option) the
100 ns period computed at 0x10EF9A (edx:eax = ms * -10000) becomes exactly -1/60 s.
"""
import pathlib, re, sys
GEN = pathlib.Path(__file__).resolve().parent.parent / 'src/recomp/gen'
MARK = 'lean_timer_exact60(); /* LEAN_TIMER_EXACT60 */'
for p in sorted(GEN.glob('recomp_*.c')):
    s = p.read_bytes().decode('latin-1')
    if 'void sub_0010EED3(void)' not in s:
        continue
    if MARK in s:
        print('already applied:', p.name); sys.exit(0)
    nl = '\r\n' if '\r\n' in s else '\n'
    old = 'loc_0010EF9A: ;' + nl + '    MEM32(esi + 0x3C) = eax;'
    assert s.count(old) == 1, s.count(old)
    new = 'loc_0010EF9A: ;' + nl + '    { extern void lean_timer_exact60(void); ' + MARK + ' }' + nl + '    MEM32(esi + 0x3C) = eax;'
    p.write_bytes(s.replace(old, new).encode('latin-1'))
    print('applied:', p.name); sys.exit(0)
sys.exit('sub_0010EED3 not found')
