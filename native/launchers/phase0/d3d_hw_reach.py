"""Which D3D entry points reach hardware (push buffer, GPU registers, GPU waits)?
Static: transitive call graph over the generated C. Read-only."""
import re, json
from pathlib import Path
from collections import defaultdict
ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / 'nightfire-driving-lean/src/recomp/gen'
fre = re.compile(r'^void sub_([0-9A-F]{8})\(void\)\s*$', re.M)
bodies = {}
for f in sorted(GEN.glob('*.c')):
    t = f.read_text(errors='replace'); ms = list(fre.finditer(t))
    for i, m in enumerate(ms):
        b = t[m.end(): ms[i+1].start() if i+1 < len(ms) else len(t)]
        e = b.find('\n}\n'); b = b[:e] if e >= 0 else b
        bodies[int(m.group(1), 16)] = re.sub(r'/\*.*?\*/', '', b, flags=re.S)
calls = defaultdict(set)
for va, b in bodies.items():
    for m in re.finditer(r'\bsub_([0-9A-F]{8})\b', b): calls[va].add(int(m.group(1), 16))
LIB = lambda v: 0x165DC0 <= v < 0x17AC40
reason = {}
for va, b in bodies.items():
    if not LIB(va): continue
    r = []
    if re.search(r'0xF[DE][0-9A-F]{6}u?\b', b): r.append('mmio')
    # push-buffer header store: MEM32(reg) = 0x000?xxxx with method-ish low bits, inside D3D
    if re.search(r'MEM32\((?:e[abcd]x|e[sd]i|ebp)(?: \+ (?:0x)?[0-9A-F]+)?\) = 0x[0-9A-F]{4,6}[048C];', b) and 'MEM32(0x175418)' in b: r.append('pushbuf-store')
    if r: reason[va] = r
for va in (0x16CDA0, 0x16CC20, 0x16C940, 0x166B00, 0x16CDF0, 0x16CE10, 0x16CA30, 0x16CAE0, 0x16EC10):
    reason.setdefault(va, []).append('known-gpu')
# transitive
reach = {}
def hw(va, stack=()):
    if va in reach: return reach[va]
    if va in stack: return None
    if va in reason: reach[va] = (va,); return reach[va]
    for c in sorted(calls.get(va, ())):
        if not LIB(c): continue
        p = hw(c, stack + (va,))
        if p: reach[va] = (va,) + p; return reach[va]
    reach[va] = None; return None
ent = json.load(open(Path(__file__).with_name('d3d-entry-table.json')))
out = []
for e in ent:
    va = int(e['va'], 16)
    if va in (0x16FF91, 0x170000, 0x170017): continue
    p = hw(va)
    out.append(dict(va=e['va'], name=e['name'].replace('D3D8__',''), hardware=bool(p),
                    path=[f'0x{x:06X}' for x in p] if p else [], why=reason.get(p[-1], []) if p else []))
Path(__file__).with_name('d3d-hw-reach.json').write_text(json.dumps(out, indent=1))
print('hardware-reaching:', sum(o['hardware'] for o in out), 'pure CPU:', sum(not o['hardware'] for o in out))
for o in out:
    if not o['hardware']: print('  CPU ', o['va'], o['name'])
