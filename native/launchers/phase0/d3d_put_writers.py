"""Every D3D-range function that advances the push-buffer put pointer (dev+0):
it loads the device (MEM32(0x175418) or the static 0x1758D0) into a register and
later stores through that register with no offset, or stores MEM32(0x1758D0)."""
import re, json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / 'nightfire-driving-lean/src/recomp/gen'
names = {int(m.group(2),16): m.group(1).replace('D3D8__','') for m in (re.match(r'(\w+)\s*=\s*0x([0-9a-fA-F]+)', l) for l in (ROOT/'tools/ghidra-driving-analysis/xb-symbol-cli.txt').read_text(errors='replace').splitlines()) if m}
out = []
for f in sorted(GEN.glob('recomp_00*.c')):
    t = f.read_text(errors='replace')
    ms = list(re.finditer(r'^void sub_([0-9A-F]{8})\(void\)\s*$', t, re.M))
    for i, m in enumerate(ms):
        va = int(m.group(1), 16)
        if not (0x165DC0 <= va < 0x17AC40): continue
        b = t[m.end(): ms[i+1].start() if i+1 < len(ms) else len(t)]
        e = b.find('\n}\n'); b = b[:e] if e >= 0 else b
        b = re.sub(r'/\*.*?\*/', '', b, flags=re.S)
        dev = set(); hit = False
        for line in b.splitlines():
            m1 = re.match(r'\s*(e[a-z]{2}) = (MEM32\(0x175418\)|0x1758D0u?);', line)
            if m1: dev.add(m1.group(1)); continue
            m2 = re.match(r'\s*(e[a-z]{2}) = ', line)
            if m2 and m2.group(1) in dev: dev.discard(m2.group(1))
            if re.match(r'\s*MEM32\(0x1758D0u?\) = ', line): hit = True
            m3 = re.match(r'\s*MEM32\((e[a-z]{2})\) = ', line)
            if m3 and m3.group(1) in dev: hit = True
        if hit: out.append(va)
rs = {int(r['va'],16) for r in json.load(open(ROOT/'nightfire-driving-native/runtime/native/nd3d-replace-set.json'))}
print(len(out), 'put writers;', 'not in replace set:')
for va in out:
    if va not in rs: print(f'  0x{va:06X} {names.get(va,"")}')
print('in replace set but no put write found:', [f'0x{v:06X}' for v in sorted(rs) if v not in out])
