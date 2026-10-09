"""Idempotent: LEAN_FIX_CLIPSEARCH hook for the clip-track binary search (sub_00076C60).

The `setb` at 0x76CA2 is reached from two byte compares (0x76C93 and 0x76C9F); the lifter
emitted the never-assigned `_flags` fallback, so the search always took the low half and
returned the first track. A clip's later actions (e.g. the Q-Smoke fire clip, action 0xA)
were never found. With LEAN_FIX_CLIPSEARCH=1 the setb uses the last compare (_fa < _fb).
"""
import pathlib, sys
GEN = pathlib.Path(__file__).resolve().parent.parent / 'src/recomp/gen'
OLD = '    SET_LO8(eax, _flags /* setb */);'
NEW = '    { extern int lean_fix_clipsearch(void); SET_LO8(eax, lean_fix_clipsearch() ? (_fa < _fb) : _flags /* setb */); } /* LEAN_FIX_CLIPSEARCH */'
for p in sorted(GEN.glob('recomp_*.c')):
    s = p.read_bytes().decode('latin-1')
    if 'void sub_00076C60(void)' not in s:
        continue
    if 'LEAN_FIX_CLIPSEARCH' in s:
        print('already applied:', p.name); sys.exit(0)
    i = s.index('void sub_00076C60(void)'); j = s.index('\n}', i)
    body = s[i:j]
    assert body.count(OLD) == 1, body.count(OLD)
    p.write_bytes((s[:i] + body.replace(OLD, NEW) + s[j:]).encode('latin-1'))
    print('applied:', p.name); sys.exit(0)
sys.exit('sub_00076C60 not found')
