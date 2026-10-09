"""Action phase 0: every absolute guest address used by the Driving native D3D sources
(nightfire-driving-native/runtime/native/nd3d*.{c,h}) and its default.xbe equivalent.
Uses work-full-match.json (from d3d_action_map.py). Read-only on sources and XBEs."""
import re, json, sys, struct, bisect
from pathlib import Path
from collections import defaultdict
HERE = Path(__file__).resolve().parent; ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import d3d_action_map as M

NAT = ROOT / 'nightfire-driving-native/runtime/native'
NAMES = {0x175418: 'D3D__pDevice', 0x175424: 'dirty-flag word', 0x175428: 'deferred texture-stage state',
         0x175628: 'g_RenderState', 0x175798: 'g_DeferredRenderState', 0x178500: 'g_Stream[]',
         0x1758D0: 'static CDevice object (*D3D__pDevice)', 0x176528: 'device+0xC58 VS constant shadow',
         0x1786D8: 'FVF vertex shader object', 0x1785C0: 'state table cleared at init (0x46 dwords)',
         0x189DE8: '1.0f', 0x189DEC: '0.0f', 0x189EB0: '0.5f', 0x18B5DC: '8.0f', 0x189C04: 'import MmAllocateContiguousMemoryEx',
         0x18A4A8: '2^32 (double)', 0x189FCC: '1/255', 0x189E00: '2.0f', 0x189ED4: '4.0f', 0x1A1E74: '65535.0f',
         0x1A1E70: '16777215.0f', 0x173FE0: '0.53125f', 0x192C04: '256.0', 0x18E888: 'render-state method table',
         0x176408: 'per-stage table (texture coord / stream)', 0x175314: 'D3D data table', 0x160000: '(constant, not address?)'}


# .data tables whose Action position was checked by hand against the bytes (see results doc)
VERIFIED = {0x1D3568: 0x1D5BDC, 0x1D3580: 0x1D5C00, 0x1D3590: 0x1D5C10, 0x1D35A0: 0x1D5C20}
NAMES.update({0x1D3558: 'texture-stage slot remap bytes', 0x1D3568: 'slot remap bytes (programs)', 0x1D3580: '{1,1,1,1} PS pack', 0x1D3590: '{255 x4} PS pack',
              0x1D35A0: 'nibble table r*0x11111111', 0x242CC0: 'PS pack MAXPS operand (BSS)', 0x0F4340: 'game RunPushBuffer wrapper', 0x0F43A0: 'game SetPixelShaderConstant wrapper',
              0x160000: 'nd3d_unported() slot base (glue implementation detail)'})


def main():
    w = json.load(open(HERE / 'work-full-match.json'))
    corr = {int(k, 16): int(v, 16) for k, v in w['corr'].items()}
    match = {int(k, 16): int(v[0], 16) for k, v in w['match'].items()}
    dx, ax = M.Xbe(M.DXBE), M.Xbe(M.AXBE)
    dinfo, _ = M.gen_funcs(M.DGEN)
    dstarts = sorted(dinfo)
    # constants used by the native sources
    uses = defaultdict(set)
    for f in sorted(NAT.glob('nd3d*.[ch]')):
        for ln, line in enumerate(f.read_text(errors='replace').splitlines(), 1):
            for m in re.finditer(r'\b(?:0x|sub_)([0-9A-Fa-f]{5,8})u?\b', line):
                v = int(m.group(1), 16)
                if 0x11000 <= v < 0x260000: uses[v].add(f'{f.name}:{ln}')
    ckeys = sorted(corr)
    out = {}
    for v in sorted(uses):
        sec = dx.sect(v); rec = dict(driving_section=sec, used_in=sorted(uses[v])[:4], name=NAMES.get(v, ''))
        a = None; how = None
        if v in corr: a, how = corr[v], 'direct (operand of matched code)'
        elif v in match: a, how = match[v], 'function start (matched)'
        else:
            i = bisect.bisect_right(dstarts, v) - 1
            fs = dstarts[i] if i >= 0 else None
            if fs is not None and v < fs + dinfo[fs][0] and fs in match and sec in ('D3D', 'XGRPH') and v != fs:
                a, how = match[fs] + (v - fs), f'code label inside matched 0x{fs:06X}+0x{v-fs:X}'
            elif fs is not None and v < fs + dinfo[fs][0] and sec in ('D3D', 'XGRPH'):
                how = f'code label inside UNMATCHED 0x{fs:06X}'
            else:
                j = bisect.bisect_right(ckeys, v)
                lo = ckeys[j - 1] if j > 0 else None; hi = ckeys[j] if j < len(ckeys) else None
                same = lambda k: k is not None and dx.sect(k) == sec
                dlo = corr[lo] - lo if same(lo) else None; dhi = corr[hi] - hi if same(hi) else None
                if dlo is not None and dlo == dhi and hi - lo <= 0x400:
                    a, how = v + dlo, f'inferred: between 0x{lo:06X} and 0x{hi:06X}, same shift'
                elif dlo is not None and v - lo <= 0x40:
                    a, how = v + dlo, f'inferred (weak): 0x{v-lo:X} past 0x{lo:06X}'
                elif dhi is not None and hi - v <= 0x40:
                    a, how = v + dhi, f'inferred (weak): 0x{hi-v:X} before 0x{hi:06X}'
                else: how = 'no anchor'
        if a is not None and a == v and sec not in ('D3D', 'XGRPH', '.rdata'):
            how = 'constant (identical value in both; not an address)'
        if v in VERIFIED: a, how = VERIFIED[v], 'verified by data bytes (layout differs from the shift)'
        # value check for initialised constants
        if a is not None and sec in ('.rdata', '.data') and not how.startswith('constant'):
            dv, av = dx.read(v, 8), ax.read(a, 8)
            rec['value_check'] = 'same 8 bytes' if dv == av else ('same 4 bytes' if dv[:4] == av[:4] else f'DIFFERENT {dv[:4].hex()} vs {av[:4].hex()}')
        if a is None and sec == '.rdata':
            # look for the same constant in Action .rdata
            dv = dx.read(v, 4); s = [x for x in ax.secs if x[0] == '.rdata'][0]
            raw = ax.d[s[3]:s[3] + s[4]]
            rec['action_rdata_candidates'] = [f'0x{s[1]+m.start():06X}' for m in re.finditer(re.escape(dv), raw) if m.start() % 4 == 0][:6]
        rec.update(action=(f'0x{a:06X}' if a is not None else None), action_section=(ax.sect(a) if a is not None else None), how=how)
        out[f'0x{v:06X}'] = rec
    json.dump(out, open(HERE / 'work-globals.json', 'w'), indent=1)
    for k, r in out.items():
        print(k, r['driving_section'], '->', r['action'], r['how'], r.get('value_check', ''), r.get('action_rdata_candidates', ''), r['name'])


if __name__ == '__main__':
    main()
