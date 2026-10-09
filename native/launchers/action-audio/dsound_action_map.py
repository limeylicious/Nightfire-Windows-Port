"""Action native-audio survey, step 1: map Driving.xbe DSOUND routines (XDK DSOUND 1.0.4831)
to their default.xbe (PAL) counterparts by masked machine-code comparison.

Adapted from native-driving/action-phase0/d3d_action_map.py (same masking rule: every
32-bit displacement/immediate inside the image and every external call/jump target is
ignored; branches inside a routine stay as offsets). Read-only on both XBEs and both
generated-C trees; writes work-dsound-match.json next to this script.

Run:  python native-driving/action-audio/dsound_action_map.py
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
GLUE = ROOT / 'nightfire-driving-native/runtime/lean/lean_dsound_glue.c'
ADDR_LO, ADDR_HI = 0x11000, 0x310000       # same masking rule on both sides (both images)
D_DS = (0x17AC40, 0x183AA4)                # Driving DSOUND section
A_DS = (0x112600, 0x12FD4C)                # Action DSOUND section
A_DS_RAW_END = 0x112600 + 0x1D504          # raw bytes end (BSS after)
EXPECT_A_SHA = 'b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1'


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


HDR = re.compile(r'^ \* sub_([0-9A-F]{8})\n \* Original: 0x([0-9A-F]+) - 0x([0-9A-F]+) \((\d+) bytes, (\d+) insns\)', re.M)
FRE = re.compile(r'^void (?:orig_)?sub_([0-9A-F]{8})\(void\)', re.M)


def gen_funcs(gen):
    """{va: (len, insns, file)} from the 'Original:' headers, and {va: body text}."""
    info = {}; bodies = {}
    for f in sorted(gen.glob('*.c')):
        if f.name in ('recomp_dispatch.c', 'recomp_stubs_unresolved.c'): continue
        t = f.read_text(errors='replace')
        for m in HDR.finditer(t): info[int(m.group(1), 16)] = (int(m.group(4)), int(m.group(5)), f.name)
        ms = list(FRE.finditer(t))
        for i, m in enumerate(ms):
            b = t[m.end(): ms[i + 1].start() if i + 1 < len(ms) else len(t)]
            e = b.find('\n}\n'); bodies[int(m.group(1), 16)] = b[:e] if e >= 0 else b
    return info, bodies


md = cs.Cs(cs.CS_ARCH_X86, cs.CS_MODE_32); md.detail = True
BR = {'call', 'jmp'} | {'j' + c for c in 'o no b ae e ne be a s ns p np l ge le g'.split()} | {'jae', 'jnae', 'jc', 'jnc', 'jecxz', 'loop', 'loope', 'loopne'}


def tokens(x, start, length):
    """List of (normalised-bytes, branchtoken, [(kind, value)], offset, mnemonic) per instruction."""
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


def glue_entries():
    """The Driving glue switch: [(va, handler, nargs)]."""
    t = GLUE.read_text(errors='replace')
    return [(int(m.group(1), 16), m.group(2), int(m.group(3)))
            for m in re.finditer(r'case 0x([0-9A-Fa-f]{8})u: r = (lean_ds_\w+)\(.*?\); n = (\d+); break;', t)]


def dsym():
    out = {}
    for l in DSYM.read_text(errors='replace').splitlines():
        m = re.match(r'(\w+)\s*=\s*0x([0-9a-fA-F]+)', l)
        if m: out[int(m.group(2), 16)] = m.group(1).replace('DSOUND__', '')
    return out


def main():
    dx, ax = Xbe(DXBE), Xbe(AXBE)
    assert ax.sha == EXPECT_A_SHA, 'default.xbe is not the expected PAL image'
    dinfo, dbod = gen_funcs(DGEN); ainfo, abod = gen_funcs(AGEN)
    dnames = dsym()
    alab = {int(e['address'], 16): e['research_name'] for e in json.loads(ALAB.read_text())['entries']}

    dfun = sorted(v for v in dinfo if D_DS[0] <= v < D_DS[1])
    afun = sorted(v for v in ainfo if A_DS[0] <= v < A_DS[1])
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
            if len(kd) >= 4 and len(ka) >= 4 and (ka[:len(kd)] == kd or kd[:len(ka)] == ka):
                c.append(a)
        if len(c) == 1: match[v] = (c[0], 'exact-prefix')
        elif c: amb[v] = c
    # routines the Action generator never emitted: raw search of the DSOUND code bytes
    araw = ax.read(A_DS[0], A_DS_RAW_END - A_DS[0])
    for v in dfun:
        if v in match or v in amb: continue
        pat = b''
        for t in dtok[v]:
            nb = t[0]; ob = dx.read(v + t[3], len(nb))
            pat += b''.join(re.escape(bytes([x])) if x == y else b'.' for x, y in zip(ob, nb))
        if not pat: continue
        hits = [m.start() + A_DS[0] for m in re.finditer(pat, araw, re.S)]
        good = [h for h in hits if key(tokens(ax, h, dinfo[v][0])) == key(dtok[v])]
        if len(good) == 1:
            match[v] = (good[0], 'exact-raw'); atok[good[0]] = tokens(ax, good[0], dinfo[v][0])
            ainfo.setdefault(good[0], (dinfo[v][0], len(dtok[v]), '(not in Action gen)'))
        elif len(good) > 1:
            amb[v] = good
            for h in good:
                atok.setdefault(h, tokens(ax, h, dinfo[v][0])); ainfo.setdefault(h, (dinfo[v][0], len(dtok[v]), '(not in Action gen)'))

    def callpairs(dv, av):
        return [(f1[1], f2[1]) for t1, t2 in zip(dtok[dv], atok[av]) for f1, f2 in zip(t1[2], t2[2])
                if f1[0] in ('call', 'jmp') and t1[1] == ('EXT',)]
    rejected = {}
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
    # near matches for the rest (instruction-sequence similarity)
    near = {}
    used = {m[0] for m in match.values()}
    sig = lambda tl: [t[4] + ('' if t[1] is None else str(t[1])) + t[0].hex() for t in tl]
    asig = {a: sig(atok[a]) for a in afun}
    for v in dfun:
        if v in match: continue
        sd = sig(dtok[v]); best = []
        for a in afun:
            if a in used: continue
            sm = difflib.SequenceMatcher(None, sd, asig[a], autojunk=False)
            if sm.real_quick_ratio() < 0.3 or sm.quick_ratio() < 0.3: continue
            best.append((sm.ratio(), a))
        best.sort(reverse=True)
        if best: near[v] = best[:3]
    # ---- deep verification: the whole call tree must be equal modulo addresses ----
    # (DSOUND wrappers are byte-identical apart from their callee, so shallow equality plus
    # 'identical code group, assigned by order' is not enough; e.g. IDirectSoundBuffer_SetFilter
    # vs IDirectSoundBuffer_SetHeadroom.)
    dcache, acache = dict(dtok), dict(atok)
    def dt(v): 
        if v not in dcache: dcache[v] = tokens(dx, v, dinfo[v][0]) if v in dinfo else []
        return dcache[v]
    def at(v):
        if v not in acache: acache[v] = tokens(ax, v, ainfo[v][0]) if v in ainfo else []
        return acache[v]
    memo = {}
    def deq(d, a, stack=()):
        if (d, a) in memo: return memo[(d, a)]
        if (d, a) in stack: return (True, None)
        td, ta = dt(d), at(a)
        if not td or not ta:
            r = (bool(td) == bool(ta), None if bool(td) == bool(ta) else (d, a))
            memo[(d, a)] = r; return r
        if key(td) != key(ta): memo[(d, a)] = (False, (d, a)); return memo[(d, a)]
        for t1, t2 in zip(td, ta):
            if t1[1] != ('EXT',): continue
            for f1, f2 in zip(t1[2], t2[2]):
                if f1[0] in ('call', 'jmp'):
                    ok, where = deq(f1[1], f2[1], stack + ((d, a),))
                    if not ok: memo[(d, a)] = (False, where); return memo[(d, a)]
        memo[(d, a)] = (True, None); return memo[(d, a)]
    deep = {}
    for v in list(match):
        a, conf = match[v]
        ok, where = deq(v, a)
        if ok: deep[v] = 'deep-equal'; continue
        cands = [x for x in afun if key(atok[x]) == key(dtok[v]) and deq(v, x)[0]]
        if len(cands) == 1:
            match[v] = (cands[0], 'exact (reassigned: unique deep-equal candidate)'); deep[v] = 'deep-equal'
        else:
            rejected[v] = (a, [('deep', f'0x{where[0]:06X}->0x{where[1]:06X}')]); del match[v]
            deep[v] = f'shallow only, call tree differs at 0x{where[0]:06X}/0x{where[1]:06X}' + (f'; {len(cands)} deep candidates' if cands else '')
    # unmatched routines: any unique deep-equal Action routine?
    for v in dfun:
        if v in match: continue
        cands = [x for x in afun if key(atok[x]) == key(dtok[v]) and deq(v, x)[0] and x not in {m[0] for m in match.values()}]
        if len(cands) == 1: match[v] = (cands[0], 'exact (unique deep-equal)'); deep[v] = 'deep-equal'
    incons = defaultdict(list)
    for v, (a, conf) in match.items():
        for d1, a1 in callpairs(v, a):
            if d1 in match and match[d1][0] != a1: incons[v].append((d1, a1))
    # address correspondence from aligned operands of exact pairs (globals, constants, code)
    votes = defaultdict(Counter)
    for v, (a, conf) in match.items():
        if v in incons: continue
        for t1, t2 in zip(dtok[v], atok[a]):
            for f1, f2 in zip(t1[2], t2[2]):
                if f1[0] == f2[0]: votes[f1[1]][f2[1]] += 1
    corr = {d: c.most_common(1)[0][0] for d, c in votes.items()}
    conflicts = {d: dict(c) for d, c in votes.items() if len(c) > 1}

    json.dump(dict(match={f'0x{k:06X}': [f'0x{a:06X}', c] for k, (a, c) in sorted(match.items())},
                   near={f'0x{k:06X}': [[round(r, 3), f'0x{a:06X}'] for r, a in b] for k, b in sorted(near.items())},
                   ambiguous_unresolved={f'0x{k:06X}': [f'0x{a:06X}' for a in c] for k, c in amb.items() if k not in match},
                   rejected_inconsistent={f'0x{k:06X}': [f'0x{a:06X}', str(b)] for k, (a, b) in rejected.items()},
                   deep={f'0x{k:06X}': v for k, v in sorted(deep.items())},
                   inconsistent_calls={f'0x{k:06X}': [[f'0x{x:06X}', f'0x{y:06X}'] for x, y in l] for k, l in incons.items()},
                   corr={f'0x{k:06X}': f'0x{v:06X}' for k, v in sorted(corr.items())},
                   corr_conflicts={f'0x{k:06X}': {f'0x{a:06X}': n for a, n in c.items()} for k, c in conflicts.items()},
                   lengths={'driving': {f'0x{v:06X}': dinfo[v][0] for v in dfun}, 'action': {f'0x{v:06X}': ainfo[v][0] for v in sorted(ainfo) if A_DS[0] <= v < A_DS[1]}},
                   meta=dict(driving_sha=dx.sha, action_sha=ax.sha, d_funcs=len(dfun), a_funcs=len(afun))),
              open(OUT / 'work-dsound-match.json', 'w'), indent=1)

    # ---- the 19 Driving glue entries ----
    rows = []
    for va, handler, n in glue_entries():
        dl = dinfo.get(va, (None,))[0]
        if va in match:
            a, conf = match[va]
            same_len = ainfo[a][0] == dl
            conf = ('exact-masked' if same_len else 'exact-masked (length differs)') + ('' if conf == 'exact' else f' [{conf}]')
            if va in incons: conf += ' [call-target inconsistency]'
        else:
            a = None
            conf = 'not linked in default.xbe' + (f' ({deep[va]})' if va in deep else '') +                    (f'; nearest 0x{near[va][0][1]:06X} {alab.get(near[va][0][1], "")} ratio {near[va][0][0]:.2f}' if va in near else '')
        rows.append(dict(driving_va=f'0x{va:06X}', driving_symbol=dnames.get(va, ''), glue_handler=handler, nargs_glue=n,
                         action_va=(f'0x{a:06X}' if a else None), research_label=(alab.get(a, '') if a else ''),
                         confidence=conf, deep=deep.get(va, ''), length_driving=dl, length_action=(ainfo[a][0] if a else None),
                         insns=len(dtok[va]),
                         ext_calls=[[f'0x{d1:06X}', f'0x{a1:06X}', dnames.get(d1, ''), alab.get(a1, '')] for d1, a1 in (callpairs(va, a) if a else [])]))
    json.dump(rows, open(OUT / 'work-glue19.json', 'w'), indent=1)
    cnt = Counter(r['confidence'].split(' ')[0] for r in rows)
    print('Driving DSOUND funcs', len(dfun), 'Action DSOUND funcs', len(afun), 'matched', len(match),
          'near', len(near), 'amb-unresolved', sum(1 for k in amb if k not in match))
    print('glue19:', dict(cnt)); print('inconsistent:', len(incons), 'corr entries', len(corr), 'conflicts', len(conflicts))
    for r in rows:
        print(r['driving_va'], r['driving_symbol'], '->', r['action_va'], r['research_label'], r['confidence'], r['length_driving'], r['length_action'])


if __name__ == '__main__':
    main()
