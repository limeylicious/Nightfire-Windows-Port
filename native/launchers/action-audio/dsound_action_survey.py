"""Action native-audio survey, step 2 (static, read-only): which DSOUND entry points the Action
game code (.text), XMV movie code and other libraries actually call; their stdcall argument
counts; Driving counterparts (from work-dsound-match.json) and whether Driving's native glue
covers them; DSOUND vtables in data; game/XMV access to DSOUND globals; the Action runtime's
existing audio hooks; and a check of the addresses in nightfire-driving-lean/NATIVE-AUDIO-PLAN.md.

Run after dsound_action_map.py:  python native-driving/action-audio/dsound_action_survey.py
Writes work-dsound-survey.json next to this script.
"""
import re, json, sys, struct
from pathlib import Path
from collections import defaultdict
HERE = Path(__file__).resolve().parent; ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import dsound_action_map as M

DS_LO, DS_HI = M.A_DS
RUNTIME = ROOT / 'nightfire-port/runtime'
GEN = M.AGEN


def strip(b): return re.sub(r'/\*.*?\*/', '', b, flags=re.S)


def main():
    ax, dx = M.Xbe(M.AXBE), M.Xbe(M.DXBE)
    info, bodies = M.gen_funcs(GEN)
    dinfo, _ = M.gen_funcs(M.DGEN)
    alab = {int(e['address'], 16): e['research_name'] for e in json.loads(M.ALAB.read_text())['entries']}
    dn = M.dsym()
    w = json.load(open(HERE / 'work-dsound-match.json'))
    d2a = {int(k, 16): int(v[0], 16) for k, v in w['match'].items()}
    a2d = {a: d for d, a in d2a.items()}
    corr = {int(k, 16): int(v, 16) for k, v in w['corr'].items()}
    glue = {va: (h, n) for va, h, n in M.glue_entries()}
    starts = sorted(info)
    import bisect
    def owner(va):
        i = bisect.bisect_right(starts, va) - 1
        return starts[i] if i >= 0 and va < starts[i] + info[starts[i]][0] else None
    sect = ax.sect
    inds = lambda v: DS_LO <= v < DS_HI

    # ---- call graph (generated C) ----
    abi = re.compile(r'RECOMP_ABI_CALL\(0x([0-9A-F]+)u, sub_')
    direct = re.compile(r'\bsub_([0-9A-F]{8})\(\)')
    calls_abi = defaultdict(set); calls_tail = defaultdict(set); icalls = defaultdict(list)
    for va, b in bodies.items():
        for line in b.splitlines():
            for m in abi.finditer(line): calls_abi[int(m.group(1), 16)].add(va)
            if 'RECOMP_ABI_CALL' not in line:
                for m in direct.finditer(line): calls_tail[int(m.group(1), 16)].add(va)
            if 'RECOMP_ICALL' in line or 'RECOMP_ITAIL' in line: icalls[va].append(line.strip())

    def rets(va, seen=()):
        b = bodies.get(va, '')
        r = sorted({int(m.group(1), 0) if m.group(1) else 0 for m in re.finditer(r'/\* ret(?: (0x[0-9A-Fa-f]+|\d+))? \*/', b)})
        if r: return r
        t = re.search(r'sub_([0-9A-F]{8})\(\); return; /\* tail jmp', b)
        if t and int(t.group(1), 16) not in seen: return rets(int(t.group(1), 16), seen + (va,))
        return []

    # ---- data references (vtables / function pointers) to DSOUND code ----
    dataref = defaultdict(list)
    for nm, va0, vs, ra, rs in ax.secs:
        if nm in ('.rdata', '.data', 'XMV', 'DSOUND', '.text'):
            for o in range(0, rs - 3, 4):
                v = struct.unpack_from('<I', ax.d, ra + o)[0]
                if inds(v) and v in info and not (nm in ('DSOUND', '.text', 'XMV') and owner(va0 + o)):
                    dataref[v].append(va0 + o)
    # group consecutive data slots into tables
    slots = sorted((p, v) for v, ps in dataref.items() for p in ps)
    tables = []; cur = []
    for p, v in slots:
        if cur and p != cur[-1][0] + 4: tables.append(cur); cur = []
        cur.append((p, v))
    if cur: tables.append(cur)

    # ---- entry points: DSOUND functions called/tail-jumped from outside DSOUND ----
    ext = {}
    for tgt in set(calls_abi) | set(calls_tail):
        if not inds(tgt): continue
        cs_ = sorted(c for c in calls_abi.get(tgt, ()) if not inds(c))
        ts_ = sorted(c for c in calls_tail.get(tgt, ()) if not inds(c))
        if cs_ or ts_: ext[tgt] = (cs_, ts_)
    # entries reached only through data (vtables)
    for v, ps in dataref.items():
        if v not in ext and any(not inds(p) for p in ps): ext[v] = ([], [])

    rows = []
    for tgt in sorted(ext):
        cs_, ts_ = ext[tgt]
        bysec = defaultdict(list)
        for c in cs_: bysec[sect(c)].append(c)
        tsec = defaultdict(list)
        for c in ts_: tsec[sect(c)].append(c)
        d = a2d.get(tgt)
        r = rets(tgt)
        rows.append(dict(
            va=f'0x{tgt:06X}', research_label=alab.get(tgt, ''), driving_va=(f'0x{d:06X}' if d else None),
            driving_symbol=(dn.get(d, '') if d else ''), driving_glue=(glue[d][0] if d in glue else None),
            ret_bytes=r, nargs=([x // 4 for x in r] if r else None), length=info.get(tgt, (None,))[0],
            callers={s: [f'0x{c:06X}' for c in v] for s, v in bysec.items()},
            tail_callers={s: [f'0x{c:06X}' for c in v] for s, v in tsec.items()},
            data_refs=[f'0x{p:06X}' for p in dataref.get(tgt, [])],
            dsound_internal_callers=len([c for c in calls_abi.get(tgt, ()) if inds(c)]) + len([c for c in calls_tail.get(tgt, ()) if inds(c)]),
        ))

    # ---- XMV indirect calls (vtable slots used on objects) ----
    xmv_lo, xmv_hi = 0x12FD60, 0x12FD60 + 0x27494
    xmv_ic = []
    for va, ls in icalls.items():
        if xmv_lo <= va < xmv_hi:
            for l in ls: xmv_ic.append((f'0x{va:06X}', l[:200]))

    # ---- code outside DSOUND touching DSOUND data/BSS (capstone over the function extents) ----
    code_lo = 0x11C801            # first byte after the last DSOUND code we know of in the main block
    dsdata = defaultdict(set)
    for va in starts:
        if inds(va): continue
        n = info[va][0]
        code = ax.read(va, n)
        if code is None: continue
        for i in M.md.disasm(code, va):
            for kind, off, size in (('disp', i.disp_offset, i.disp_size), ('imm', i.imm_offset, i.imm_size)):
                if size == 4 and off:
                    v = struct.unpack_from('<I', i.bytes, off)[0]
                    if inds(v) and v not in info and not owner(v):
                        dsdata[v].add((va, i.mnemonic + ' ' + i.op_str))
    glob_rows = []
    for v in sorted(dsdata):
        drv = [d for d, a in corr.items() if a == v]
        glob_rows.append(dict(va=f'0x{v:06X}', in_raw=(v < DS_LO + 0x1D504), users=sorted({f'0x{c:06X} [{sect(c)}]' for c, _ in dsdata[v]})[:12],
                              sample=sorted({s for _, s in dsdata[v]})[:3], driving=[f'0x{d:06X}' for d in drv]))
    # every DSOUND global the library itself uses, with the Driving address it corresponds to
    lib_glob = {}
    for d, a in corr.items():
        if inds(a) and a not in info and not owner(a) and M.D_DS[0] <= d < M.D_DS[1]:
            lib_glob[f'0x{a:06X}'] = f'0x{d:06X}'

    # ---- runtime hooks: every DSOUND/XMV-range VA named in the Action audio runtime ----
    hooks = defaultdict(set)
    for f in [RUNTIME / 'nightfire_game_audio.h', RUNTIME / 'nightfire_video_audio.c', RUNTIME / 'nightfire_game_audio_trace.h',
              RUNTIME / 'nightfire_audio_spatial.h', GEN / 'nightfire_thread_check.c', GEN / 'nightfire_dsp_startup.c', GEN / 'nightfire_ac97.c',
              ROOT / 'nightfire-port/src/recomp_manual.c', ROOT / 'nightfire-port/src/main.c']:
        if not f.exists(): continue
        for ln, line in enumerate(f.read_text(errors='replace').splitlines(), 1):
            for m in re.finditer(r'\b0x0{0,2}(1[1-3][0-9A-Fa-f]{4}|0?e[0-9a-f]{4})u?\b', line):
                v = int(m.group(1), 16)
                if (DS_LO <= v < 0x157200) or (0xE0000 <= v < 0xE2000): hooks[v].add(f'{f.name}:{ln}')
    hook_rows = [dict(va=f'0x{v:06X}', section=sect(v), label=alab.get(v, ''), where=sorted(hooks[v])[:6]) for v in sorted(hooks)]

    out = dict(entry_points=rows,
               tables=[dict(at=f'0x{t[0][0]:06X}', section=sect(t[0][0]), slots=[[f'0x{p:06X}', f'0x{v:06X}', alab.get(v, '')] for p, v in t]) for t in tables],
               xmv_indirect_calls=xmv_ic, outside_access_to_dsound_data=glob_rows, dsound_library_globals=lib_glob,
               runtime_hook_vas=hook_rows)
    json.dump(out, open(HERE / 'work-dsound-survey.json', 'w'), indent=1)
    print('entry points called from outside DSOUND:', len(rows))
    for r in rows:
        print(f"{r['va']} {r['research_label']:42s} nargs={r['nargs']} D={r['driving_va']} {r['driving_symbol']:36s} glue={r['driving_glue']} "
              f"callers={ {k: len(v) for k, v in r['callers'].items()} } tail={ {k: len(v) for k, v in r['tail_callers'].items()} } data={r['data_refs']}")
    print('tables:'); [print(' ', t[0][0] and f'0x{t[0][0]:06X}', sect(t[0][0]), [(f'0x{v:06X}', alab.get(v, '')) for p, v in t]) for t in tables]
    print('XMV indirect calls:', len(xmv_ic))
    print('outside access to DSOUND data:'); [print(' ', g) for g in glob_rows]
    print('runtime hook VAs:'); [print(' ', h) for h in hook_rows]


if __name__ == '__main__':
    main()
