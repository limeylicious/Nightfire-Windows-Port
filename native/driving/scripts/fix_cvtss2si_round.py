"""Idempotent: rounding of float->int conversions the lifter emitted as truncation or as
always-nearest (Driving lean). Backs up each changed file to recovery/ first.

1) cvtss2si (106 sites): lifted as a C cast `(int32_t)x`, which truncates; the instruction
   rounds by MXCSR (nearest-even; Driving never changes MXCSR). Now
   `lean_cvtss2si(x, site)`: LEAN_FIX_CVTSS2SI=1 -> _mm_cvtss_si32 (hardware behaviour),
   otherwise the old truncation. LEAN_CVTSS2SI_LOG=1 counts per site where they differ.
2) fist/fistp (59 sites): lifted as llrint() (always nearest); the instruction rounds by the
   x87 control word, and the D3D library sets truncation (or ax,0xC00 + fldcw) around its
   fistp runs (0x15EDB0, 0x16085F..0x162355). Now `lean_fist_rc(x, site)`: LEAN_FIX_FIST=1
   -> rounding by g_fp_control_word RC, otherwise llrint. LEAN_FIST_LOG=1 counts differences.
Site id = enclosing function address * 16 + occurrence index in that function (<16).
"""
import pathlib, re, shutil, sys, time
LEAN = pathlib.Path(__file__).resolve().parent.parent
GEN = LEAN / 'src/recomp/gen'
MARK = '/* LEAN_FIX_ROUND */'
DECL = ('int32_t lean_cvtss2si(float x, uint32_t site); long long lean_fist_rc(double x, uint32_t site); '
        + MARK)
CVT = re.compile(r'^(\s+)(\w+) = \(int32_t\)(.+?); /\* cvtss2si \*/$')
FIST = re.compile(r'llrint\(fp_top\(\)\)(.*/\* fistp? \*/)$')
FUNC = re.compile(r'^void sub_([0-9A-F]{8})\(void\)')

def main():
    stamp = time.strftime('%Y%m%d-%H%M%S')
    bak = LEAN / 'recovery' / ('round-fix-' + stamp)
    table, total = [], [0, 0]
    for p in sorted(GEN.glob('recomp_*.c')):
        raw = p.read_bytes().decode('latin-1')
        if MARK in raw:
            print('already applied:', p.name); continue
        if '/* cvtss2si */' not in raw and '/* fistp */' not in raw and '/* fist */' not in raw:
            continue
        nl = '\r\n' if '\r\n' in raw else '\n'
        lines = raw.split(nl)
        func, occ, n = 0, {}, 0
        for i, l in enumerate(lines):
            m = FUNC.match(l)
            if m: func = int(m.group(1), 16); occ = {}; continue
            m = CVT.match(l)
            if m:
                k = occ.get('c', 0); occ['c'] = k + 1; assert k < 16, (p.name, hex(func))
                site = func * 16 + k
                lines[i] = '%s%s = lean_cvtss2si(%s, 0x%Xu); /* cvtss2si */' % (m.group(1), m.group(2), m.group(3), site)
                table.append(('cvtss2si', site, p.name)); total[0] += 1; n += 1; continue
            if ('/* fistp */' in l or '/* fist */' in l) and 'llrint(fp_top())' in l:
                k = occ.get('f', 0); occ['f'] = k + 1; assert k < 16, (p.name, hex(func))
                site = func * 16 + k
                lines[i] = l.replace('llrint(fp_top())', 'lean_fist_rc(fp_top(), 0x%Xu)' % site, 1)
                table.append(('fist', site, p.name)); total[1] += 1; n += 1
        if n:
            bak.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, bak / p.name)
            j = next(k for k, l in enumerate(lines) if l.startswith('#include'))
            lines.insert(j + 1, DECL)
            p.write_bytes(nl.join(lines).encode('latin-1'))
            print('patched', p.name, n)
    if table:
        out = LEAN / 'analysis' / 'round-fix-sites.txt'
        out.parent.mkdir(exist_ok=True)
        out.write_text(''.join('%s site 0x%X func 0x%06X #%d %s\n' % (k, s, s >> 4, s & 15, f) for k, s, f in table))
        print('cvtss2si sites %d, fist/fistp sites %d; table %s; backup %s' % (total[0], total[1], out, bak))

if __name__ == '__main__':
    main()
