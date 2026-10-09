"""Phase 0: D3D library globals (0x175418-0x17883C) that game code reads or writes directly."""
import re, json
from pathlib import Path
from collections import defaultdict
ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / 'nightfire-driving-lean/src/recomp/gen'
SYM = ROOT / 'tools/ghidra-driving-analysis/xb-symbol-cli.txt'
names = {int(m.group(2),16): m.group(1) for m in (re.match(r'(\w+)\s*=\s*0x([0-9a-fA-F]+)', l) for l in SYM.read_text(errors='replace').splitlines()) if m}
fre = re.compile(r'^void sub_([0-9A-F]{8})\(void\)\s*$', re.M)
acc = re.compile(r'MEM(8|16|32|F|D)\((0x17[5-8][0-9A-F]{3})\)(\s*=(?!=))?')
w = defaultdict(set); r = defaultdict(set)
for f in sorted(GEN.glob('recomp_00*.c')):
    t = f.read_text(errors='replace'); ms = list(fre.finditer(t))
    for i, m in enumerate(ms):
        va = int(m.group(1), 16)
        if 0x165DC0 <= va < 0x17AC40: continue      # D3D and XGRAPHC library code
        body = t[m.end(): ms[i+1].start() if i+1 < len(ms) else len(t)]
        for a in acc.finditer(body):
            addr = int(a.group(2), 16)
            if not (0x175418 <= addr < 0x17883C): continue
            (w if a.group(3) else r)[addr].add(va)
def nm(a):
    best = max((k for k in names if k <= a and a - k < 0x400 and k >= 0x175418), default=None)
    return f"{names[best].replace('D3D8__','')}+0x{a-best:X}" if best is not None else ''
rows = [dict(addr=f'0x{a:06X}', near=nm(a), writers=sorted(f'0x{x:06X}' for x in w[a]), readers=sorted(f'0x{x:06X}' for x in r[a]))
        for a in sorted(set(w) | set(r))]
Path(__file__).with_name('d3d-game-state-access.json').write_text(json.dumps(rows, indent=1))
print(len(rows), 'globals;', len(set().union(*w.values())) if w else 0, 'writer functions;', len(set().union(*r.values())) if r else 0, 'reader functions')
for x in rows: print(x['addr'], x['near'], 'W', len(x['writers']), 'R', len(x['readers']))
