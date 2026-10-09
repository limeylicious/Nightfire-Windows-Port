"""Idempotent: unordered (NaN) handling for branches after `fnstsw ax; sahf` (LEAN_FIX_SAHF=1).

After sahf, C0->CF, C2->PF, C3->ZF; an unordered x87 compare sets all three. The lifter
tests g_fp_cmp (-1/0/1, 2 = unordered) without the unordered case, so with a NaN operand
jne/jae jump when x86 would not and je/jbe do not jump when x86 would. Driving has 8 such
branches, all in CRT maths (0x132C7D, 0x136BB9, 0x1386CA). Off by default.
"""
import pathlib, sys
GEN = pathlib.Path(__file__).resolve().parent.parent / 'src/recomp/gen'
FIX = {
    '(g_fp_cmp != 0) /* sahf */': '(lean_fix_sahf() ? (g_fp_cmp != 0 && g_fp_cmp != 2) : (g_fp_cmp != 0)) /* sahf */',
    '(g_fp_cmp == 0) /* sahf */': '(lean_fix_sahf() ? (g_fp_cmp == 0 || g_fp_cmp == 2) : (g_fp_cmp == 0)) /* sahf */',
    '(g_fp_cmp >= 0) /* sahf */': '(lean_fix_sahf() ? (g_fp_cmp == 0 || g_fp_cmp == 1) : (g_fp_cmp >= 0)) /* sahf */',
    '(g_fp_cmp <= 0) /* sahf */': '(lean_fix_sahf() ? (g_fp_cmp <= 0 || g_fp_cmp == 2) : (g_fp_cmp <= 0)) /* sahf */',
}
DECL = 'int lean_fix_sahf(void); /* LEAN_FIX_SAHF */'
total = 0
for p in sorted(GEN.glob('recomp_*.c')):
    s = p.read_bytes().decode('latin-1')
    if '/* sahf */' not in s:
        continue
    n = 0
    for a, b in FIX.items():
        c = s.count(a); s = s.replace(a, b); n += c
    if n:
        if DECL not in s:
            nl = '\r\n' if '\r\n' in s else '\n'
            i = s.index('#include')
            j = s.index(nl, i) + len(nl)
            s = s[:j] + DECL + nl + s[j:]
        p.write_bytes(s.encode('latin-1'))
    print(p.name, 'patched', n)
    total += n
print('total newly patched', total)
