"""Phase 0: table of D3D-library entry points called from outside the D3D section.
Read-only: parses generated C, XbSymbol names and the XBE. Writes d3d-entry-table.{json,md}."""
import re, json, struct
from pathlib import Path
from collections import defaultdict
ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / 'nightfire-driving-lean/src/recomp/gen'
SYM = ROOT / 'tools/ghidra-driving-analysis/xb-symbol-cli.txt'
XBE = ROOT / 'nightfire-analysis/game_files/Driving.xbe'
OUT = Path(__file__).resolve().parent
D3D_LO, D3D_HI = 0x165DC0, 0x175418   # code ends before library data at D3D__pDevice
SECTIONS = [('.text',0x11000,0x15D370),('D3DX',0x15D380,0x165DA8),('D3D',0x165DC0,0x17883C),
            ('XGRPH',0x178840,0x17AC21),('DSOUND',0x17AC40,0x183AA4),('XPP',0x183AC0,0x189BCC)]
def sect(va):
    for n,a,b in SECTIONS:
        if a<=va<b: return n
    return '?'
names = {}
for line in SYM.read_text(errors='replace').splitlines():
    m = re.match(r'(\w+)\s*=\s*0x([0-9a-fA-F]+)', line)
    if m: names[int(m.group(2),16)] = m.group(1)
funcs = {}
fre = re.compile(r'^void sub_([0-9A-F]{8})\(void\)\s*$', re.M)
for f in sorted(GEN.glob('*.c')):
    t = f.read_text(errors='replace')
    ms = list(fre.finditer(t))
    for i,m in enumerate(ms):
        end = t.find('\n}\n', m.end())
        funcs[int(m.group(1),16)] = (f.name, t[m.end():end if end>0 else len(t)])
callre = re.compile(r'RECOMP_ABI_CALL\(0x([0-9A-F]+)u')
directre = re.compile(r'\bsub_([0-9A-F]{8})\(\)')
callers = defaultdict(set); tail = defaultdict(set)
for va,(fn,body) in funcs.items():
    for m in callre.finditer(body): callers[int(m.group(1),16)].add(va)
    # calls not routed through RECOMP_ABI_CALL (tail calls etc.) bypass the hook
    for line in body.splitlines():
        if 'RECOMP_ABI_CALL' in line: continue
        for m in directre.finditer(line): tail[int(m.group(1),16)].add(va)
def regs_in(body):
    res = {}
    for r in ('eax','ecx','edx'):
        for line in body.splitlines():
            code = line.split('/*')[0]
            if 'RECOMP_ABI_CALL' in code or 'RECOMP_ICALL' in code:
                res[r] = 'write'; break      # calls clobber eax/ecx/edx
            if re.fullmatch(r'\s*PUSH32\(esp, %s\);\s*' % r, code): continue   # save or stack-slot reserve
            if not re.search(r'\b%s\b' % r, code): continue
            m = re.match(r'\s*%s\s*=(?!=)(.*)' % r, code)
            res[r] = 'write' if (m and not re.search(r'\b%s\b' % r, m.group(1))) else 'read'
            break
    return [r for r,v in res.items() if v == 'read']
def rets(body):
    s = set()
    for m in re.finditer(r'/\* ret(?: (0x[0-9A-Fa-f]+|\d+))? \*/', body):
        s.add(int(m.group(1),0) if m.group(1) else 0)
    return sorted(s)
# indirect references: dwords in .rdata/.data that point at D3D function starts
d = XBE.read_bytes(); base = struct.unpack_from('<I',d,0x104)[0]
nsec = struct.unpack_from('<I',d,0x11C)[0]; sh = struct.unpack_from('<I',d,0x120)[0]-base
dataref = defaultdict(list)
for i in range(nsec):
    fl,va,vs,ra,rs,na = struct.unpack_from('<IIIIII',d,sh+i*56)
    nm = d[na-base:na-base+16].split(b'\0')[0].decode('latin1')
    if nm in ('.rdata','.data'):
        for o in range(0, rs-3, 4):
            v = struct.unpack_from('<I',d,ra+o)[0]
            if D3D_LO <= v < D3D_HI and v in funcs: dataref[v].append(va+o)
rows = []
for va in sorted(funcs):
    if not (D3D_LO <= va < D3D_HI): continue
    ext = sorted(c for c in callers[va] if not (D3D_LO <= c < 0x17883C))
    extt = sorted(c for c in tail[va] if not (D3D_LO <= c < 0x17883C))
    if not ext and not extt and not dataref[va]: continue
    body = funcs[va][1]; r = rets(body); regs = regs_in(body)
    pop = r[0] if len(r)==1 else None
    conv = ('thiscall/fastcall?' if 'ecx' in regs or 'edx' in regs else 'stdcall' if pop else 'cdecl/void')
    rows.append(dict(va=f'0x{va:06X}', name=names.get(va,''), ret_pops=r, stack_args=(pop//4 if pop is not None else None),
                     reg_inputs=regs, convention_guess=conv, file=funcs[va][0],
                     callers=[f'0x{c:06X}({sect(c)})' for c in ext],
                     unhooked_callers=[f'0x{c:06X}({sect(c)})' for c in extt],
                     data_refs=[f'0x{a:06X}' for a in dataref[va]]))
(OUT/'d3d-entry-table.json').write_text(json.dumps(rows, indent=1))
with (OUT/'d3d-entry-table.md').open('w') as f:
    f.write('| VA | name | pops | regs in | guess | callers | not via hook | data refs |\n|---|---|---|---|---|---|---|---|\n')
    for x in rows:
        f.write(f"| {x['va']} | {x['name'].replace('D3D8__','')} | {x['ret_pops']} | {','.join(x['reg_inputs'])} | {x['convention_guess']} | {len(x['callers'])} | {len(x['unhooked_callers'])} | {len(x['data_refs'])} |\n")
print(len(rows), 'entry points;', sum(1 for x in rows if x['unhooked_callers']), 'with unhooked callers;',
      sum(1 for x in rows if x['data_refs']), 'with data refs;', sum(1 for x in rows if not x['name']), 'unnamed')
