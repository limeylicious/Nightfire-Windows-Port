"""Action phase 0 survey of default.xbe (PAL) D3D use, adapted from native-driving/phase0/*.py:
entry points called from outside the library, which of them reach the hardware, the Action
equivalent of the Driving replacement set, push-buffer put writers (library and game code),
game code touching D3D library globals, and XMV movie-player calls into D3D.
Static only (generated C in nightfire-port/src/recomp/gen). Read-only; writes work-survey.json."""
import re, json, sys, struct
from pathlib import Path
from collections import defaultdict
HERE = Path(__file__).resolve().parent; ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import d3d_action_map as M

CODE_LO, CODE_HI = 0x100480, 0x10EB70      # D3D section raw part (code + library constants)
LIB_LO, LIB_HI = 0x100480, 0x112600        # D3D + D3DX + XGRPH
BSS_LO, BSS_HI = 0x10EB70, 0x111FB8        # D3D library globals (pDevice 0x1117C0, device 0x10EB80)
PDEV, SDEV = 0x1117C0, 0x10EB80
XMV = (0x12FD60, 0x12FD60 + 0x27494)


def strip(b): return re.sub(r'/\*.*?\*/', '', b, flags=re.S)


def main():
    w = json.load(open(HERE / 'work-full-match.json'))
    d2a = {int(k, 16): int(v[0], 16) for k, v in w['match'].items()}
    a2d = {a: d for d, a in d2a.items()}
    rset = {int(r['va'], 16): r['name'] for r in json.load(open(M.REPLACE))}
    dnames = {int(m.group(2), 16): m.group(1).replace('D3D8__', '') for m in (re.match(r'(\w+)\s*=\s*0x([0-9a-fA-F]+)', l) for l in M.DSYM.read_text(errors='replace').splitlines()) if m}
    alab = {int(e['address'], 16): e['research_name'] for e in json.loads(M.ALAB.read_text())['entries']}
    dhw = {int(e['va'], 16): e['hardware'] for e in json.load(open(ROOT / 'native-driving/phase0/d3d-hw-reach.json'))}
    def name(a):
        d = a2d.get(a)
        return (rset.get(d) or dnames.get(d, '')) if d else '' or alab.get(a, '')
    def nm(a):
        n = name(a)
        return n if n else alab.get(a, '')
    ax = M.Xbe(M.AXBE)
    info, bodies = M.gen_funcs(M.AGEN)
    sect = ax.sect
    # call graph
    callre = re.compile(r'RECOMP_ABI_CALL\(0x([0-9A-F]+)u')
    directre = re.compile(r'\bsub_([0-9A-F]{8})\(\)')
    callers = defaultdict(set); tail = defaultdict(set); calls = defaultdict(set)
    sb = {}
    for va, b in bodies.items():
        s = sb[va] = strip(b)
        for m in callre.finditer(b): callers[int(m.group(1), 16)].add(va)
        for line in b.splitlines():
            if 'RECOMP_ABI_CALL' in line: continue
            for m in directre.finditer(line): tail[int(m.group(1), 16)].add(va)
        for m in re.finditer(r'\bsub_([0-9A-F]{8})\b', s): calls[va].add(int(m.group(1), 16))
    # data references to library function starts
    dataref = defaultdict(list)
    for nmx, va0, vs, ra, rs in ax.secs:
        if nmx in ('.rdata', '.data', 'XMV', 'DSOUND'):
            for o in range(0, rs - 3, 4):
                v = struct.unpack_from('<I', ax.d, ra + o)[0]
                if CODE_LO <= v < CODE_HI and v in info and not (CODE_LO <= va0 + o < CODE_HI): dataref[v].append(va0 + o)
    lib = lambda v: LIB_LO <= v < LIB_HI
    def rets(b):
        return sorted({int(m.group(1), 0) if m.group(1) else 0 for m in re.finditer(r'/\* ret(?: (0x[0-9A-Fa-f]+|\d+))? \*/', b)})
    # ---- hardware reasons (same criteria as Driving d3d_replace_set.py / d3d_hw_reach.py) ----
    MAKESPACE = (d2a[0x16CDA0], d2a[0x16CC20])
    GPU = [d2a[x] for x in (0x16CDA0, 0x16CC20, 0x16C940, 0x166B00, 0x16CDF0, 0x16CE10, 0x16CA30, 0x16CAE0, 0x16EC10) if x in d2a]
    why = {}
    for va, s in sb.items():
        if not (CODE_LO <= va < CODE_HI): continue
        r = []
        if any(re.search(r'RECOMP_ABI_CALL\(0x%08Xu' % m, bodies[va]) for m in MAKESPACE): r.append('makespace')
        if re.search(r'MEM32\(e[a-d]x(?: \+ (?:0x)?[0-9A-F]+)?\) = 0x[0-9A-F]{4,6};', s) and f'MEM32(0x{PDEV:X})' in s: r.append('header')
        if re.search(r'0xF[DE][0-9A-F]{6}u?\b', s): r.append('mmio')
        if va in GPU: r.append('gpu-sync')
        if re.search(r'\b(?:in|out)[bwl]?\b|_port_(?:in|out)', s): r.append('port')
        if r: why[va] = r
    reach = {}
    def hw(va, stack=()):
        if va in reach: return reach[va]
        if va in stack: return None
        if va in why: reach[va] = (va,); return reach[va]
        for c in sorted(calls.get(va, ())):
            if not lib(c): continue
            p = hw(c, stack + (va,))
            if p: reach[va] = (va,) + p; return p and reach[va]
        reach[va] = None; return None
    # ---- entry table ----
    rows = []
    for va in sorted(info):
        if not (CODE_LO <= va < CODE_HI): continue
        ext = sorted(c for c in callers[va] if not lib(c)); extt = sorted(c for c in tail[va] if not lib(c))
        if not ext and not extt and not dataref[va]: continue
        r = rets(bodies[va]); d = a2d.get(va); p = hw(va)
        rows.append(dict(va=f'0x{va:06X}', name=nm(va), research_label=alab.get(va, ''),
                         driving_va=(f'0x{d:06X}' if d else None),
                         in_driving_replace_set=(d in rset) if d else False,
                         driving_hw=(dhw.get(d) if d else None),
                         hardware=bool(p), hw_path=[f'0x{x:06X}' for x in p] if p else [],
                         ret_pops=r, n_callers=len(ext), caller_sections=sorted({sect(c) for c in ext}),
                         unhooked_callers=[f'0x{c:06X}' for c in extt], data_refs=[f'0x{a:06X}' for a in dataref[va]][:6],
                         xmv_callers=[f'0x{c:06X}' for c in ext if XMV[0] <= c < XMV[1]]))
    # ---- Action replacement set by the Driving criteria ----
    rsA = sorted(why)
    mappedR = {d2a[d] for d in rset if d in d2a}
    # ---- put writers (library and whole image) ----
    def put_writer(s):
        dev = set(); hit = False
        for line in s.splitlines():
            m1 = re.match(r'\s*(e[a-z]{2}) = (MEM32\(0x%X\)|0x%Xu?);' % (PDEV, SDEV), line)
            if m1: dev.add(m1.group(1)); continue
            m2 = re.match(r'\s*(e[a-z]{2}) = ', line)
            if m2 and m2.group(1) in dev: dev.discard(m2.group(1))
            if re.match(r'\s*MEM32\(0x%Xu?\) = ' % SDEV, line): hit = True
            m3 = re.match(r'\s*MEM32\((e[a-z]{2})\) = ', line)
            if m3 and m3.group(1) in dev: hit = True
        return hit
    puts = sorted(va for va, s in sb.items() if put_writer(s))
    # ---- game code reading/writing library globals ----
    acc = re.compile(r'MEM(8|16|32|F|D)\((0x[0-9A-F]+)\)(\s*=(?!=))?')
    gw = defaultdict(set); gr = defaultdict(set); pdev_users = set()
    for va, s in sb.items():
        if lib(va): continue
        for a in acc.finditer(s):
            addr = int(a.group(2), 16)
            if BSS_LO <= addr < BSS_HI:
                (gw if a.group(3) else gr)[addr].add(va)
                if addr == PDEV: pdev_users.add(va)
    # game functions that dereference the device put pointer (inlined push-buffer writes)
    inl = []
    for va in sorted(pdev_users):
        s = sb[va]
        if re.search(r'MEM32\((e[a-z]{2})\) = ', s) and put_writer(s): inl.append(va)
    # ---- XMV ----
    xmv = defaultdict(set)
    for va, b in bodies.items():
        if XMV[0] <= va < XMV[1]:
            for c in calls[va]:
                if lib(c): xmv[c].add(va)
    # residual: hardware-reaching library functions still reachable from outside the library
    # when every routine of the mapped Driving set is native (a native routine is a cut point).
    roots = [int(r['va'], 16) for r in rows]
    seen = set(); stack = [r for r in roots if r not in mappedR]; via = {r: r for r in stack}
    while stack:
        f = stack.pop()
        if f in seen: continue
        seen.add(f)
        for c in calls.get(f, ()):
            if lib(c) and c not in mappedR and c not in seen: via.setdefault(c, via[f]); stack.append(c)
    residual = sorted(v for v in seen if v in why)
    callers_of = defaultdict(set)
    for f, cs_ in calls.items():
        for c in cs_: callers_of[c].add(f)
    out = dict(
        residual_hw=[dict(va=f'0x{v:06X}', name=nm(v), why=why[v], driving_va=(f'0x{a2d[v]:06X}' if v in a2d else None),
                          reached_from_entry=f'0x{via[v]:06X}', lib_callers=sorted(f'0x{x:06X}' for x in callers_of[v])) for v in residual],
        unreached_hw=[dict(va=f'0x{v:06X}', name=nm(v), why=why[v], callers=sorted(f'0x{x:06X}' for x in callers_of[v])) for v in rsA if v not in mappedR and v not in seen],
        entry_points=rows,
        action_replace_set=[dict(va=f'0x{v:06X}', name=nm(v), why=why[v], driving_va=(f'0x{a2d[v]:06X}' if v in a2d else None),
                                 covered_by_driving_set=v in mappedR) for v in rsA],
        put_writers=[dict(va=f'0x{v:06X}', name=nm(v), section=sect(v), covered=v in mappedR) for v in puts],
        game_inline_put_writers=[f'0x{v:06X}' for v in inl],
        game_global_access=[dict(addr=f'0x{a:06X}', writers=sorted(f'0x{x:06X}' for x in gw[a]), readers=sorted(f'0x{x:06X}' for x in gr[a])) for a in sorted(set(gw) | set(gr))],
        xmv_calls={f'0x{c:06X}': dict(name=nm(c), driving_va=(f'0x{a2d[c]:06X}' if c in a2d else None), in_driving_replace_set=(a2d.get(c) in rset), hardware=bool(hw(c)), callers=sorted(f'0x{x:06X}' for x in v)) for c, v in sorted(xmv.items())},
    )
    json.dump(out, open(HERE / 'work-survey.json', 'w'), indent=1)
    print('entry points', len(rows), '| hw', sum(r['hardware'] for r in rows), '| no Driving counterpart', sum(1 for r in rows if not r['driving_va']))
    print('Action replace set', len(rsA), '| not covered by mapped Driving set', sum(1 for v in rsA if v not in mappedR), '| mapped Driving set size', len(mappedR))
    print('residual hw', [(r['va'], r['name'], r['why'], r['reached_from_entry']) for r in out['residual_hw']])
    print('unreached hw not covered', out['unreached_hw'])
    print('put writers', len(puts), [f'0x{v:06X}' for v in puts if v not in mappedR])
    print('game inline put writers', [f'0x{v:06X}' for v in inl])
    print('game global access', len(out['game_global_access']), 'addresses')
    print('xmv targets', {k: (v['name'], len(v['callers'])) for k, v in out['xmv_calls'].items()})


if __name__ == '__main__':
    main()
