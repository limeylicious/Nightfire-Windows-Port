"""Replacement set R: D3D functions that directly write the push buffer, touch GPU
registers, or wait for the GPU. Everything else in D3D can stay recompiled once R is native."""
import re, json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / 'nightfire-driving-lean/src/recomp/gen'
names = {int(m.group(2),16): m.group(1).replace('D3D8__','') for m in (re.match(r'(\w+)\s*=\s*0x([0-9a-fA-F]+)', l) for l in (ROOT/'tools/ghidra-driving-analysis/xb-symbol-cli.txt').read_text(errors='replace').splitlines()) if m}
fre = re.compile(r'^/\*\*\n \* sub_([0-9A-F]{8})\n \* Original: 0x[0-9A-F]+ - 0x[0-9A-F]+ \((\d+) bytes, (\d+) insns\)', re.M)
info = {}; bodies = {}
for f in sorted(GEN.glob('recomp_00*.c')):
    t = f.read_text(errors='replace')
    for m in fre.finditer(t): info[int(m.group(1),16)] = int(m.group(3))
    ms = list(re.finditer(r'^void sub_([0-9A-F]{8})\(void\)\s*$', t, re.M))
    for i, m in enumerate(ms):
        b = t[m.end(): ms[i+1].start() if i+1 < len(ms) else len(t)]
        e = b.find('\n}\n'); b = b[:e] if e >= 0 else b
        bodies[int(m.group(1),16)] = re.sub(r'/\*.*?\*/', '', b, flags=re.S)
cov = set()
for run in ('20261007-165417-lean-paris', '20261007-165814-lean-vehicle'):
    L = (ROOT/'nightfire-driving-lean/runs'/run/'driving-startup.log').read_text(errors='replace')
    cov |= {int(x,16) for x in re.findall(r'\[LEAN-COV\] t=\d+ f=-?\d+ first sub_([0-9A-F]{8})', L)}
rows = []
for va, b in sorted(bodies.items()):
    if not (0x165DC0 <= va < 0x175418): continue
    why = []
    if re.search(r'RECOMP_ABI_CALL\(0x0016C(?:DA0|C20)u', b): why.append('makespace')
    if re.search(r'MEM32\(e[a-d]x(?: \+ (?:0x)?[0-9A-F]+)?\) = 0x[0-9A-F]{4,6};', b) and 'MEM32(0x175418)' in b: why.append('header')
    if re.search(r'0xF[DE][0-9A-F]{6}u?\b', b): why.append('mmio')
    if va in (0x16CDA0, 0x16CC20, 0x16C940, 0x166B00, 0x16CDF0, 0x16CE10, 0x16CA30, 0x16CAE0, 0x16EC10, 0x16CE90): why.append('gpu-sync')
    if re.search(r'\b(?:in|out)[bwl]?\b|driving_port_', b): why.append('port')
    if why: rows.append(dict(va=f'0x{va:06X}', name=names.get(va,''), insns=info.get(va), ran=va in cov, why=why))
Path(__file__).with_name('d3d-replace-set.json').write_text(json.dumps(rows, indent=1))
print(len(rows), 'functions,', sum(r['insns'] or 0 for r in rows), 'insns;', sum(r['ran'] for r in rows), 'ran in phase-0 runs (', sum((r['insns'] or 0) for r in rows if r['ran']), 'insns)')
for r in rows: print(('RAN ' if r['ran'] else '    ')+r['va'], r['insns'], r['name'], ','.join(r['why']))
