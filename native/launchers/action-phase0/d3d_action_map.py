"""Action phase 0: map Driving.xbe D3D routines (XDK 1.0.4831) to their default.xbe (PAL)
counterparts by comparing machine code with image addresses and call targets masked.
Read-only on both XBEs and both generated-C trees. Writes into this folder only.

Run:  set PYTHONPATH=nightfire-port\\analysis\\python-deps  &&  python -I? (capstone needed)
      python native-driving/action-phase0/d3d_action_map.py
"""
import re, json, struct, sys, hashlib, difflib
from pathlib import Path
from collections import defaultdict, Counter
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'nightfire-port/analysis/python-deps'))
import capstone as cs

OUT = Path(__file__).resolve().parent
DXBE = ROOT / 'nightfire-analysis/game_files/Driving.xbe'
AXBE = ROOT / 'nightfire-analysis/game_files/default.xbe'
DGEN = ROOT / 'nightfire-driving-native/src/recomp/gen'
AGEN = ROOT / 'nightfire-port/src/recomp/gen'
DSYM = ROOT / 'tools/ghidra-driving-analysis/xb-symbol-cli.txt'
ALAB = ROOT / 'nightfire-port/analysis/nightfire-research-labels.json'
REPLACE = ROOT / 'nightfire-driving-native/runtime/native/nd3d-replace-set.json'
ADDR_LO, ADDR_HI = 0x11000, 0x2FB660      # same masking rule on both sides
D_D3D = (0x165DC0, 0x165DC0 + 0x12A7C); A_D3D = (0x100480, 0x100480 + 0x11B38)
D_LIB_END = 0x17AC40                       # D3D + XGRAPHC (Driving)
A_LIB = (0x100480, 0x112600)               # D3D + D3DX + XGRPH (Action)


class Xbe:
    def __init__(self, p):
        self.d = d = p.read_bytes(); self.sha = hashlib.sha256(d).hexdigest()
        base = struct.unpack_from('<I', d, 0x104)[0]
        n = struct.unpack_from('<I', d, 0x11C)[0]; sh = struct.unpack_from('<I', d, 0x120)[0] - base
        self.secs = []
        for i in range(n):
            fl, va, vs, ra, rs, na = struct.unpack_from('<IIIIII', d, sh + i * 56)
            nm = d[na - base:na - base + 16].split(b'\0')[0].decode('latin1')
            self.secs.append((nm, va, vs, ra, rs))
    def sect(self, va):
        for nm, a, vs, ra, rs in self.secs:
            if a <= va < a + vs: return nm
        return '?'
    def read(self, va, n):
        for nm, a, vs, ra, rs in self.secs:
            if a <= va < a + vs:
                o = va - a; avail = max(0, min(n, rs - o))
                return self.d[ra + o: ra + o + avail] + b'\0' * (n - avail)
        return None
    def u32(self, va): return struct.unpack('<I', self.read(va, 4))[0]


def gen_funcs(gen):
    """{va: (len, insns, file)} from the 'Original:' headers, plus a body index for call analysis."""
    hdr = re.compile(r'^ \* sub_([0-9A-F]{8})\n \* Original: 0x([0-9A-F]+) - 0x([0-9A-F]+) \((\d+) bytes, (\d+) insns\)', re.M)
    info = {}; bodies = {}
    fre = re.compile(r'^void (?:orig_)?sub_([0-9A-F]{8})\(void\)', re.M)
    for f in sorted(gen.glob('*.c')):          # all generated files (Action keeps some in nightfire_*.c)
        if f.name in ('recomp_dispatch.c', 'recomp_stubs_unresolved.c'): continue
        t = f.read_text(errors='replace')
        for m in hdr.finditer(t): info[int(m.group(1), 16)] = (int(m.group(4)), int(m.group(5)), f.name)
        ms = list(fre.finditer(t))
        for i, m in enumerate(ms):
            b = t[m.end(): ms[i + 1].start() if i + 1 < len(ms) else len(t)]
            e = b.find('\n}\n'); bodies[int(m.group(1), 16)] = b[:e] if e >= 0 else b
    return info, bodies


md = cs.Cs(cs.CS_ARCH_X86, cs.CS_MODE_32); md.detail = True
BR = {'call', 'jmp'} | {'j' + c for c in 'o no b ae e ne be a s ns p np l ge le g'.split()} | {'jae', 'jnae', 'jc', 'jnc', 'jecxz', 'loop', 'loope', 'loopne'}


def tokens(x, start, length):
    """List of (normalised-bytes, branchtoken, [(kind, value)]) per instruction."""
    code = x.read(start, length)
    out = []
    if code is None: return out
    for i in md.disasm(code, start):
        b = bytearray(i.bytes); fields = []; br = None
        if i.mnemonic in BR and len(i.operands) == 1 and i.operands[0].type == cs.x86.X86_OP_IMM:
            tgt = i.operands[0].imm & 0xFFFFFFFF
            off = i.imm_offset if i.imm_size else len(b) - (4 if len(b) >= 5 else 1)
            for k in range(off, len(b)): b[k] = 0
            br = ('INT', tgt - start) if start <= tgt < start + length else ('EXT',)
            fields.append(('call' if i.mnemonic == 'call' else 'jmp', tgt))
        else:
            if i.disp_size == 4:
                v = struct.unpack_from('<I', b, i.disp_offset)[0]
                if ADDR_LO <= v < ADDR_HI:
                    b[i.disp_offset:i.disp_offset + 4] = b'\0\0\0\0'; fields.append(('disp', v))
            if i.imm_size == 4:
                v = struct.unpack_from('<I', b, i.imm_offset)[0]
                if ADDR_LO <= v < ADDR_HI:
                    b[i.imm_offset:i.imm_offset + 4] = b'\0\0\0\0'; fields.append(('imm', v))
        out.append((bytes(b), br, fields, i.address - start, i.mnemonic))
        if i.address + i.size >= start + length: break
    return out


def main():
    dx, ax = Xbe(DXBE), Xbe(AXBE)
    dinfo, dbod = gen_funcs(DGEN); ainfo, abod = gen_funcs(AGEN)
    dnames = {int(m.group(2), 16): m.group(1).replace('D3D8__', '') for m in (re.match(r'(\w+)\s*=\s*0x([0-9a-fA-F]+)', l) for l in DSYM.read_text(errors='replace').splitlines()) if m}
    alab = {int(e['address'], 16): e['research_name'] for e in json.loads(ALAB.read_text())['entries']}
    rset = json.loads(REPLACE.read_text())
    rnames = {int(r['va'], 16): r['name'] for r in rset}

    dfun = sorted(v for v in dinfo if D_D3D[0] <= v < D_LIB_END)
    afun = sorted(v for v in ainfo if A_LIB[0] <= v < A_LIB[1])
    dtok = {v: tokens(dx, v, dinfo[v][0]) for v in dfun}
    atok = {v: tokens(ax, v, ainfo[v][0]) for v in afun}
    key = lambda tl: tuple((t[0], t[1]) for t in tl)
    aidx = defaultdict(list)
    for v in afun: aidx[key(atok[v])].append(v)

    match = {}; amb = {}
    for v in dfun:
        c = aidx.get(key(dtok[v]), [])
        if len(c) == 1: match[v] = (c[0], 'exact')
        elif len(c) > 1: amb[v] = c
    # boundary-tolerant: Driving code equals a prefix of an Action function (or vice versa)
    for v in dfun:
        if v in match or v in amb: continue
        kd = key(dtok[v]); c = []
        for a in afun:
            ka = key(atok[a])
            if len(kd) >= 4 and (ka[:len(kd)] == kd or kd[:len(ka)] == ka) and len(ka) >= 4:
                c.append(a)
        if len(c) == 1: match[v] = (c[0], 'exact-prefix')
        elif c: amb[v] = c
    # functions the Action generator never emitted: search the raw D3D section bytes with the
    # masked fields as wildcards, then confirm by re-tokenising at the hit.
    araw_lo, araw_hi = A_D3D[0], 0x10EB70           # D3D raw data ends at 0x10EB70 (BSS after)
    araw = ax.read(araw_lo, araw_hi - araw_lo)
    rawhits = {}
    for v in dfun:
        if v in match or v in amb: continue
        pat = b''
        for t in dtok[v]:
            nb = t[0]; ob = dx.read(v + t[3], len(nb))
            pat += b''.join(re.escape(bytes([x])) if x == y else b'.' for x, y in zip(ob, nb))
        hits = [m.start() + araw_lo for m in re.finditer(pat, araw, re.S)]
        good = [h for h in hits if key(tokens(ax, h, dinfo[v][0])) == key(dtok[v])]
        if len(good) == 1:
            match[v] = (good[0], 'exact-raw'); atok[good[0]] = tokens(ax, good[0], dinfo[v][0])
            ainfo.setdefault(good[0], (dinfo[v][0], len(dtok[v]), '(not in Action gen)'))
            rawhits[v] = good[0]
        elif len(good) > 1:
            amb[v] = good
            for h in good:
                atok.setdefault(h, tokens(ax, h, dinfo[v][0])); ainfo.setdefault(h, (dinfo[v][0], len(dtok[v]), '(not in Action gen)'))
    # resolve ambiguous candidates by call-target agreement and address order
    def callpairs(dv, av):
        return [(f1[1], f2[1]) for t1, t2 in zip(dtok[dv], atok[av]) for f1, f2 in zip(t1[2], t2[2]) if f1[0] in ('call', 'jmp') and t1[1] == ('EXT',)]
    rejected = {}
    # identical code in several routines (e.g. two render-state setters that differ only in the
    # state slot address): the linker keeps library order, so assign groups in address order.
    groups = defaultdict(list)
    for v, c in amb.items(): groups[tuple(sorted(c))].append(v)
    for c, vs in groups.items():
        if len(vs) == len(c):
            for v, a in zip(sorted(vs), c): match[v] = (a, 'exact (identical code group, assigned by order)')
    for _pass in range(2):
     for _ in range(3):
        for v, cands in list(amb.items()):
            if v in match: continue
            cands = [a for a in cands if rejected.get(v, (None,))[0] != a]
            used = {m[0] for m in match.values()}
            free = [a for a in cands if a not in used]
            lo = max((match[k][0] for k in match if k < v), default=0)
            hi = min((match[k][0] for k in match if k > v), default=1 << 32)
            ordered = [a for a in free if lo < a < hi]
            def score(a):
                return sum(1 for d1, a1 in callpairs(v, a) if d1 in match and match[d1][0] == a1)
            pick = ordered if len(ordered) == 1 else (sorted(ordered or free, key=score, reverse=True) if (ordered or free) else [])
            if len(ordered) == 1 or (len(pick) > 1 and score(pick[0]) > score(pick[1])) or len(pick) == 1:
                match[v] = (pick[0], 'exact (disambiguated by order/calls)')
     for v, (a, conf) in list(match.items()):
        bad = [(d1, a1) for d1, a1 in callpairs(v, a) if d1 in match and match[d1][0] != a1]
        if bad: rejected[v] = (a, bad); del match[v]
    # near matches for the rest
    near = {}
    used = {m[0] for m in match.values()}
    for v in dfun:
        if v in match: continue
        sd = [t[4] + ('' if t[1] is None else str(t[1])) + t[0].hex() for t in dtok[v]]
        best = []
        for a in afun:
            if a in used: continue
            la = len(atok[a])
            sa = [t[4] + ('' if t[1] is None else str(t[1])) + t[0].hex() for t in atok[a]]
            sm = difflib.SequenceMatcher(None, sd, sa, autojunk=False)
            if sm.real_quick_ratio() < 0.3 or sm.quick_ratio() < 0.3: continue
            best.append((sm.ratio(), a))
        best.sort(reverse=True)
        if best: near[v] = best[:2]
    # validate: call targets of matched pairs must map consistently
    incons = defaultdict(list)
    for v, (a, conf) in match.items():
        for d1, a1 in callpairs(v, a):
            if d1 in match and match[d1][0] != a1: incons[v].append((d1, a1))

    # address correspondence from aligned instructions of exact pairs
    votes = defaultdict(Counter)
    for v, (a, conf) in match.items():
        if v in incons: continue
        for t1, t2 in zip(dtok[v], atok[a]):
            for f1, f2 in zip(t1[2], t2[2]):
                if f1[0] == f2[0]: votes[f1[1]][f2[1]] += 1
    corr = {d: c.most_common(1)[0][0] for d, c in votes.items()}
    conflicts = {d: dict(c) for d, c in votes.items() if len(c) > 1}

    json.dump(dict(match={f'0x{k:06X}': [f'0x{a:06X}', c] for k, (a, c) in match.items()},
                   near={f'0x{k:06X}': [[round(r, 3), f'0x{a:06X}'] for r, a in b] for k, b in near.items()},
                   ambiguous_unresolved={f'0x{k:06X}': [f'0x{a:06X}' for a in c] for k, c in amb.items() if k not in match},
                   rejected_inconsistent={f'0x{k:06X}': f'0x{a:06X}' for k, (a, b) in rejected.items()},
                   inconsistent_calls={f'0x{k:06X}': [[f'0x{x:06X}', f'0x{y:06X}'] for x, y in l] for k, l in incons.items()},
                   corr={f'0x{k:06X}': f'0x{v:06X}' for k, v in sorted(corr.items())},
                   corr_conflicts={f'0x{k:06X}': {f'0x{a:06X}': n for a, n in c.items()} for k, c in conflicts.items()},
                   meta=dict(driving_sha=dx.sha, action_sha=ax.sha, d_funcs=len(dfun), a_funcs=len(afun))),
              open(OUT / 'work-full-match.json', 'w'), indent=1)

    # ---- the 91-entry map ----
    amap = {}
    for r in rset:
        v = int(r['va'], 16); dl = dinfo.get(v, (None,))[0]
        if v in match:
            a, conf = match[v]
            conf = 'exact-masked' if conf.startswith('exact') and ainfo[a][0] == dl else ('exact-masked (length differs: ' + conf + ')' if conf.startswith('exact') else conf)
            if v in incons: conf += ' [call-target inconsistency]'
            near_r = None
        elif v in near and near[v][0][0] >= 0.75:
            a = near[v][0][1]; conf = f'near ({near[v][0][0]:.3f})'; near_r = near[v][0][0]
        else:
            a = None; conf = 'not found' + (f' (best partial 0x{near[v][0][1]:06X} ratio {near[v][0][0]:.2f})' if v in near else '')
        amap[f'0x{v:06X}'] = dict(action_va=(f'0x{a:06X}' if a else None), name=rnames[v] or dnames.get(v, ''),
                                  research_label=(alab.get(a, '') if a else ''), confidence=conf,
                                  length_driving=dl, length_action=(ainfo[a][0] if a else None),
                                  action_gen_sub_exists=(a is not None and ainfo[a][2] != '(not in Action gen)'))
    json.dump(amap, open(OUT / 'work-replace-map.json', 'w'), indent=1)
    cnt = Counter(m['confidence'].split(' ')[0] for m in amap.values())
    print('Driving funcs', len(dfun), 'Action lib funcs', len(afun), 'matched', len(match), 'near', len(near), 'amb-unresolved', sum(1 for k in amb if k not in match))
    print('replace-set:', dict(cnt)); print('inconsistent:', len(incons), 'corr entries', len(corr), 'conflicts', len(conflicts))


if __name__ == '__main__':
    main()
