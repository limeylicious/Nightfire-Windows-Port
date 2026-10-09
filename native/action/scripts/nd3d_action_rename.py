"""Native D3D for the Action engine (copied from nightfire-driving-native): rename the generated definitions of the replaced Xbox D3D
routines so runtime/native/nd3d_glue.c can define them instead.

For every VA in runtime/native/nd3d-action-routines.json, the definition line
    void sub_XXXXXXXX(void)
in src/recomp/gen/*.c (recomp_*.c and nightfire_render_states.c) becomes
    void orig_sub_XXXXXXXX(void)   /* NATIVE_D3D: renamed */
Calls, tail jumps and the dispatch table keep the name sub_XXXXXXXX, so every
route reaches the native definition; with NIGHTFIRE_NATIVE_D3D unset the native
definition calls orig_sub_XXXXXXXX. Idempotent. --undo restores the names.
--check reports state without writing."""
import json, re, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / 'src/recomp/gen'
vas = [int(r['va'], 16) for r in json.loads((ROOT / 'runtime/native/nd3d-action-routines.json').read_text())]
mode = sys.argv[1] if len(sys.argv) > 1 else '--apply'
done = {va: None for va in vas}
for f in sorted(GEN.glob('*.c')):
    t = f.read_text(errors='surrogateescape'); orig = t
    for va in vas:
        name = f'sub_{va:08X}'
        new_def = f'void orig_{name}(void)   /* NATIVE_D3D: renamed */'
        if mode == '--undo':
            if new_def in t: t = t.replace(new_def, f'void {name}(void)'); done[va] = f.name
        else:
            pat = re.compile(r'^void %s\(void\)\s*$' % name, re.M)
            if new_def in t: done[va] = f.name
            elif pat.search(t):
                done[va] = f.name
                if mode == '--apply': t = pat.sub(new_def, t, count=1)
    if t != orig and mode != '--check':
        f.write_text(t, errors='surrogateescape')
missing = [f'0x{va:06X}' for va, v in done.items() if v is None]
print(f'{mode}: {len(vas) - len(missing)} of {len(vas)} definitions handled', ('; MISSING ' + ' '.join(missing)) if missing else '')
sys.exit(1 if missing else 0)
