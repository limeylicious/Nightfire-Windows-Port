"""Idempotent: LEAN_GADGET_GRANT hook in the gadget select routine (0xBAE50).

Test only (off by default): after the game selects gadget type t (not the type-2 weapon
path), lean_gadget_grant() gives that slot LEAN_GADGET_GRANT charges if its count is 0.
"""
import pathlib, sys
GEN = pathlib.Path(__file__).resolve().parent.parent / 'src/recomp/gen'
MARK = 'lean_gadget_grant(); /* LEAN_GADGET_GRANT */'
for p in sorted(GEN.glob('recomp_*.c')):
    s = p.read_bytes().decode('latin-1')
    if 'void sub_000BAE50(void)' not in s:
        continue
    if MARK in s:
        print('already applied:', p.name); sys.exit(0)
    nl = '\r\n' if '\r\n' in s else '\n'
    old = 'loc_000BAED9: ;' + nl + '    MEM32(ecx + 8) = eax;' + nl
    assert s.count(old) == 1, s.count(old)
    new = old + '    { extern void lean_gadget_grant(void); ' + MARK + ' }' + nl
    p.write_bytes(s.replace(old, new).encode('latin-1'))
    print('applied:', p.name); sys.exit(0)
sys.exit('sub_000BAE50 not found')
