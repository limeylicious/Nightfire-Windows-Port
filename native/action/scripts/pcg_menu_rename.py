"""PC Graphics page (native-driving/ingame-menu/BUILD.md): rename the generated
definitions of the five menu routines that runtime/native_action/pcg_menu.c
replaces, so the native versions take every call, tail jump and dispatch entry:

    0x06D460 Txt_BindLabel          (native text for string group 0x7F)
    0x08E320 Handler_HandleMessage  (native handler for page 0x40000060)
    0x092B50 MenuManager_Load       (front-end menu blob with the PC Graphics page)
    0x0959E0 Page_SetHelpText       (description line for the PC Graphics button and rows)
    0x095A80 Page_Update            (mouse in the menus)

    void sub_XXXXXXXX(void)  ->  void orig_sub_XXXXXXXX(void)   /* PC_GRAPHICS: renamed */

Without NF_OVERLAY=1 the native versions just call orig_sub_XXXXXXXX.
Idempotent. --undo restores the names, --check only reports."""
import re, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / 'src/recomp/gen'
VAS = [0x0006D460, 0x0008E320, 0x00092B50, 0x000959E0, 0x00095A80]
mode = sys.argv[1] if len(sys.argv) > 1 else '--apply'
done = {va: None for va in VAS}
for f in sorted(GEN.glob('*.c')):
    t = f.read_text(errors='surrogateescape'); orig = t
    for va in VAS:
        name = f'sub_{va:08X}'
        new_def = f'void orig_{name}(void)   /* PC_GRAPHICS: renamed */'
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
print(f'{mode}: {len(VAS) - len(missing)} of {len(VAS)} definitions handled', ('; MISSING ' + ' '.join(missing)) if missing else '')
sys.exit(1 if missing else 0)
