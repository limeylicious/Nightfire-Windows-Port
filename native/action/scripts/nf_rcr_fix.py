"""Profile crash fix (native-driving/ingame-menu/BUILD.md, 2026-10-09): the four 64-bit
division helpers of the game's C library were translated with `rcr reg, 1` (rotate right
through carry) left as a diagnostic stop, so any 64-bit divide or remainder whose divisor
is 2^32 or more closed the game (first seen when creating a new profile):

    0x0EEFC0 _alldiv   0x0EF2A0 _aullrem   0x0EF550 _aulldiv   0x0F3520 _aulldvrm

Each stop line becomes the instruction itself (the carry comes from the `shr` just before):

    reg = (reg >> 1) | (CF << 31); CF = old bit 0        /* RCR_FIX */

Only these four functions are touched. Idempotent. --undo restores the stop lines,
--check only reports."""
import re, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
GEN = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / 'src/recomp/gen'
VAS = [0x000EEFC0, 0x000EF2A0, 0x000EF550, 0x000F3520]
mode = sys.argv[1] if len(sys.argv) > 1 else '--apply'
STOP = re.compile(r'^(\s*)nightfire_diagnostic_stop\(__func__, "rcr (e[abcd]x), 1"\);(\r?)$', re.M)
FIXED = re.compile(r'^(\s*)\{ uint32_t _o = (e[abcd]x); \2 = \(\2 >> 1\) \| \(\(uint32_t\)_cf << 31\); _cf = \(int\)\(_o & 1u\); \}   /\* RCR_FIX: rcr \2, 1 \*/(\r?)$', re.M)
def fix(m):
    sp, r, cr = m.group(1), m.group(2), m.group(3)
    return f'{sp}{{ uint32_t _o = {r}; {r} = ({r} >> 1) | ((uint32_t)_cf << 31); _cf = (int)(_o & 1u); }}   /* RCR_FIX: rcr {r}, 1 */{cr}'
def unfix(m):
    return f'{m.group(1)}nightfire_diagnostic_stop(__func__, "rcr {m.group(2)}, 1");{m.group(3)}'
found = {va: [0, 0] for va in VAS}   # [stops left, fixed]
for f in sorted(GEN.glob('recomp_*.c')):
    with open(f, encoding='latin-1', newline='') as h: t = h.read()
    out, changed = [], False
    # split at function definitions so only the four helpers are touched
    parts = re.split(r'(?m)^(?=void (?:orig_)?sub_[0-9A-F]{8}\(void\))', t)
    for p in parts:
        m = re.match(r'void (?:orig_)?sub_([0-9A-F]{8})\(void\)', p)
        va = int(m.group(1), 16) if m else None
        if va in found:
            if mode == '--apply':
                q = STOP.sub(fix, p)
                if q != p: changed = True; p = q
            elif mode == '--undo':
                q = FIXED.sub(unfix, p)
                if q != p: changed = True; p = q
            found[va][0] += len(STOP.findall(p)); found[va][1] += len(FIXED.findall(p))
        out.append(p)
    if changed and mode != '--check':
        with open(f, 'w', encoding='latin-1', newline='') as h: h.write(''.join(out))
bad = []
for va, (left, fixed) in found.items():
    want_fixed = mode != '--undo'
    ok = (fixed == 2 and left == 0) if want_fixed else (left == 2 and fixed == 0)
    if mode == '--check': ok = left + fixed == 2
    if not ok: bad.append(f'0x{va:06X} (stops {left}, fixed {fixed})')
print(f'{mode}: {len(VAS) - len(bad)} of {len(VAS)} helpers', 'fixed' if mode == '--apply' else 'handled', ('; PROBLEM ' + ', '.join(bad)) if bad else '')
sys.exit(1 if bad else 0)
