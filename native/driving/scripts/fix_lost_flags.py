"""Fix 'lost flags' mis-lifts in Nightfire Driving generated code (RIFLE fix).
Idempotent. Usage: python fix_lost_flags.py <gen dir>
"""
import re, sys, pathlib

SITES = [
 ('sub_0007A530', '    eax--;\n    PUSH32(esp, edi);', None,
  '    if ((eax != 0)) goto loc_0007A56D;', '    if ((_lz != 0)) goto loc_0007A56D;'),
 (None, '    eax = MEM32(ecx + 0x60);\n    eax--;\n    eax = 0x190CF0;', 'eax--;',
  '    if ((eax != 0)) goto loc_0007A710;', '    if ((_lz != 0)) goto loc_0007A710;'),
 (None, 'loc_000B8E27: ;\n    eax--;', None,
  '    if ((eax != 0)) goto loc_000B8E54;', '    if ((_lz != 0)) goto loc_000B8E54;'),
 (None, 'loc_00101480: ;\n    ecx = MEM32(esp + 0x18);\n    eax--;', None,
  '    if ((eax == 0)) goto loc_001014EB;', '    if ((_lz == 0)) goto loc_001014EB;'),
 (None, 'loc_001017F0: ;\n    eax--;', None,
  '    if ((eax == 0)) goto loc_0010184D;', '    if ((_lz == 0)) goto loc_0010184D;'),
 (None, 'loc_001019A1: ;\n    esi--;', None,
  '    if ((esi == 0)) goto loc_001019FA;', '    if ((_lz == 0)) goto loc_001019FA;'),
 (None, '    MEM32(0x1D187C) = eax;\n    POP32(esp, esi);', 'POP32',
  '    if ((esi != 0)) goto loc_00108AF4;', '    if ((_lz != 0)) goto loc_00108AF4;'),
 (None, '    ecx = ecx | eax;\n    ecx = MEM32(esp + 0xC);', 'ORLINE',
  '    if ((ecx != 0)) goto loc_00134409;', '    if ((_lz != 0)) goto loc_00134409;'),
 (None, '    MEM32(0x245978) = eax;\n    POP32(esp, esi);', 'POP32',
  '    if ((esi != 0)) goto loc_00141384;', '    if ((_lz != 0)) goto loc_00141384;'),
 (None, '    eax = MEM32(ecx + 0x60);\n    eax--;\n    MEMF(esp + 0x10) = (float)fp_top(); /* fst */', None,
  '    if ((eax != 0)) goto loc_0007A5FA;', '    if ((_lz != 0)) goto loc_0007A5FA;'),
 (None, '    SET_LO8(edx, LO8(edx) & 1);\n    MEM8(esi) = LO8(edx);\n    edx = eax;', 'AND',
  '    if ((LO8(edx) == 0)) goto loc_000B27AA;', '    if ((_lz == 0)) goto loc_000B27AA;'),
]
MARK = ' /* RIFLE lost-flags fix */'

# Variable-count shrd/shld in _aullshr (sub_00133F10) / _allshl (sub_00133F30):
# x86 leaves the destination unchanged when cl == 0; the lifted C shifted by 32.
SHIFT_FIXES = [
 ('    eax = (eax >> LO8(ecx)) | (edx << (32 - LO8(ecx))); /* shrd */',
  '    eax = LO8(ecx) ? ((eax >> LO8(ecx)) | (edx << (32 - LO8(ecx)))) : eax; /* shrd */' + MARK),
 ('    edx = (edx << LO8(ecx)) | (eax >> (32 - LO8(ecx))); /* shld */',
  '    edx = LO8(ecx) ? ((edx << LO8(ecx)) | (eax >> (32 - LO8(ecx)))) : edx; /* shld */' + MARK),
]

def capture(setter, how):
    lines = setter.split('\n')
    if how == 'POP32':
        i = next(k for k, l in enumerate(lines) if 'POP32' in l)
        lines.insert(i, '    _lz = esi;' + MARK)
    elif how == 'ORLINE':
        i = next(k for k, l in enumerate(lines) if 'ecx | eax' in l)
        lines.insert(i + 1, '    _lz = ecx;' + MARK)
    elif how == 'AND':
        i = next(k for k, l in enumerate(lines) if 'LO8(edx) & 1' in l)
        lines.insert(i + 1, '    _lz = LO8(edx);' + MARK)
    else:
        i = next(k for k, l in enumerate(lines) if re.match(r'\s+(eax|esi)--;', l))
        reg = lines[i].strip()[:3]
        lines.insert(i + 1, '    _lz = %s;' % reg + MARK)
    return '\n'.join(lines)

def main(gen):
    done = 0
    for f in sorted(pathlib.Path(gen).glob('recomp_00*.c')):
        raw = f.read_bytes()
        crlf = b'\r\n' in raw
        text = raw.decode('latin-1').replace('\r\n', '\n')
        orig = text
        for _, setter, how, jold, jnew in SITES:
            if jnew + MARK in text:
                done += 1; continue
            p = text.find(setter)
            while p >= 0:
                q = text.find(jold, p)
                if 0 <= q and q - p < 2000: break
                p = text.find(setter, p + 1)
            if p < 0: continue
            fs = text.rfind('\n{\n', 0, p)
            new_setter = capture(setter, how if how in ('POP32', 'ORLINE', 'AND') else None)
            seg = text[p:q].replace(setter, new_setter, 1)
            text = text[:p] + seg + jnew + MARK + text[q + len(jold):]
            if 'uint32_t _lz = 0;' not in text[fs:text.find('\n}\n', fs)]:
                text = text[:fs + 3] + '    uint32_t _lz = 0; (void)_lz;' + MARK + '\n' + text[fs + 3:]
            done += 1
        for old, new in SHIFT_FIXES:
            if new in text:
                done += 1
            elif old in text:
                text = text.replace(old, new, 1); done += 1
        if text != orig:
            f.write_bytes((text.replace('\n', '\r\n') if crlf else text).encode('latin-1'))
            print('patched', f.name)
    total = len(SITES) + len(SHIFT_FIXES)
    print('sites fixed:', done, 'of', total)
    return 0 if done == total else 1

if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
