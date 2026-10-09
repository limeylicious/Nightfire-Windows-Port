# Action menu data: archive, blob format, and a plan for a "PC Graphics" page (2026-10-09)

Read-only study plus offline tools. Game files are only read; everything extracted goes to `extracted\`.
No game code changed, nothing built into the games, no game run, no Ghidra. NightfireResearch material was not used
(none on the laptop); everything below is from our generated C and the PAL data.

Tools (`tools\`, all standalone):
| tool | what it does |
|---|---|
| `extract_filesys.py list / get <name> / all` | reads `eurocom\filesys.d00-d10`, writes extracted files |
| `build_edl_helper.cmd` → `edl_helper.dll` | compiles the game's own translated EDL decoder (`edl_gen.c`, regenerated from `nightfire-port-native` by `extract_edl_funcs.py`, with `edl_shim.h`) |
| `menu_blob.py <blob> [--all] [--page id]` | parses a menu blob (validated: walks every record of both blobs to the end marker) |
| `txt_table.py <xxtxt.dat> [ids] / --dump` | resolves string IDs exactly as `Txt_BindLabel` does |
Dumps: `dump-av-options-40000031.txt`, `dump-pause-4000004B.txt`, `dump-debug-cheats-40000007.txt`.

## 1. Archive (filesys) and where the menu blob comes from
- The 11 volumes form **one continuous stream**. `filesys.d00` starts with a directory:
  `"KXF\x12"`, +0x0C volume size, **+0x18 entry count (385)**, +0x1C volume count (11); entries from +0x20, 0x26 bytes each,
  **sorted by key = CRC-32 (zlib) of the lower-case file name** (`FS_MatchFilenameToHeader` 0x0E2960 normalises the
  name, hashes with the table at 0x1D52E8 and binary-searches). Entry: +0x0C stream offset, +0x14 stored size,
  +0x18 file size, +0x1C flags (4 stored, 2 compressed).
- Compressed entry: `"KXC\x04"`, u32 block count, then **EDL blocks**: `"EDL"` + byte (bit 7 big-endian, low 7 bits
  method 0 stored / 1 Huffman+LZ / 2 bitwise), u32 block size (with this 12-byte header), u32 unpacked size, data.
  Decoder: `maybeEDL_DecompressBlock` 0x0D4BC0 → `Inflate_directcopy` 0x0D3A30 / `Inflate_huffman` 0x0D3A80 /
  `Inflate_bitwise` 0x0D4350. The tool runs that exact code; every size checked matches the directory and
  `uktxt.dat` reads as the real UK menu text.
- Level/front-end **load files** are `%08x.bin` (`LoaderLoad` 0x0BE910): front end **`07000048.bin`**, missions
  `07000001.bin` … `0700004b.bin`, `07000500.bin`, `07000999.bin`. Load file = 0x20 header (u32 table size, u32 entry
  count) + table (u32 size, u8 type, zero-terminated name ending in 8 hex digits) + data from 0x20+table size.
- **Menu blob = entry of type 8**, handed by `LoaderProcess` 0x0BE7B0 to **`MenuManager_Load` 0x092B50**, which only
  stores the pointer in 0x25FB08 (`MenuManager_Create` 0x093960 parses it later):
  - front end: `07000048.bin` entry `08000002`, 133,536 bytes (all front-end pages incl. Audio/Video options);
  - each mission: entry `08000001` (e.g. 27,424 bytes in `07000012.bin`): pause page, debug pages.
- **Best place for native C:** take over `MenuManager_Load` (0x092B50, 2 instructions, called only from
  `LoaderProcess`/`ResetMap_Load`): when the blob holds page 0x40000031, build an extended copy in guest heap memory
  and store that pointer instead. Files on disk are never touched; nothing extra to dump.

## 2. Menu blob format (from `MenuManager_Create` 0x093960, validated on both blobs)
u32 header (-6), then records: **i32 tag** + fields (B u8, H u16, I u32, all little-endian, no alignment):
| tag | name | fields | meaning |
|---|---|---|---|
| -16 | block | I id, I flags, I hash, B, **I length** | start of a page; `length` bytes of records follow; skipped as a whole when the page does not apply |
| -7 | define | I id, B type, B flags, H x, H y, H w, H h, H style, I, H, I, I | a page (type 0xD) or a control on it; created/found via `Manager_SendMessage(0x42/0x43)` |
| -10 | msg | B message, I p1, I p2 | `__Menu_SendMessage(current control, message, p1, p2)` |
| -8 | text | 32 bytes | inline text/glyph codes (UTF-16 narrowed to 31 chars), message 0x23 |
| -9 | script | I id, I | starts an action script on the control (`sub_0969A0`) |
| -11 | keyframe | H×5, I | `Script_AddKeyFrame` 0x096A10 |
| -12 | scriptmsg | I target, B msg, I p1, I p2 | `Script_AddMessage` 0x096B10, e.g. **target -3, msg 0x44, p1 page id = "go to page"** |
| -13 | mgrmsg | B msg, I p1, I p2 | `Manager_SendMessage` |
| -2 | skip | I | |
| -3 | setup | H | `sub_071C60` |
| -4 | template | H, I, H×4 | `Component_AddComponent` 0x071CC0 (sprite/graphic templates) |
| -5 | instance | H×8, I×4 | `Component_InitInstance` 0x071D20 |
| -14 | end | – | end of the blob |
Control types seen (`define` B type): 0xD page, 5 label/row, 1 button/list row, 2 checkbox, 6 graphic, 9 left/right
value picker, 0xA slider, 0xC8 special. Control messages used by the data: 0x18 label = string ID (p2),
0x1B/0x1C colours, 0x2B flags (4 selectable row, 6 static), 0x1D picker setup, 0x21 attach script, 0x24 image,
0x2D style. Page messages (code): 0x4B enter (p = previous page), 0x4D control changed (control at arg+0x18).

### Worked example: Audio/Video options (page 0x40000031, block at blob +0x1CD23, 0xB60 bytes; full dump in `dump-av-options-40000031.txt`)
```
1CD23 block    40000031 80000002 7350048 0 B60
1CD38 define   40000031 type D (page) 0,0 640x480
1CD96 define   10000135 type 5 row  55,86  301x16   msg 18 → 0x194 'Music Volume'
1CE10 define   10000134 type 5 row  56,114          msg 18 → 0x193 'Effects Volume'
1CE8A define   10000136 type 5 row  56,142          msg 18 → 0x2B5 'Subtitles'
1CF04 define   10000137 type 5 row  56,256          msg 18 → 0x2B6 'Screen Adjust'
1CF7E define   10000138 type 5 row  56,312          msg 18 → 0x198 'Credits'
1CFEB   script 20000002 / scriptmsg -3 0x44 40000030      ← "Credits" goes to page 0x40000030 by data alone
1D034 define   10000135 type A slider 349,86 ... (and 10000134)
1D0AC define   10000136 type 9 picker 350,142
1D21A define   1000019A type 5 row  56,170          msg 18 → 0x2B8 'Multiplayer Split Screen' (+ type 9 picker)
1D30E define   100001A3 type 1 button               msg 18 → 0x37E 'Restore Defaults'
1D465 define   10000222 type 1 button               msg 18 → 0x550 'Die Another Day Trailer' → scriptmsg 0x44 4000004E
1D51B define   10000223 type 5 row  56,227          msg 18 → 0x5B8 'Widescreen'   (+ type 9 picker at 350,227)
1D595 define   10000198 type 5 row  57,198          msg 18 → 0x195 'Speaker'      (+ type 9 picker)
```
**The front end already contains a "Widescreen" row** (control 0x10000223, label + two-item picker) and a "Speaker"
row. `P_CNAVOPTIONS` (0x08D7A0) sends `__Menu_Send(page, 0x10000198 / 0x10000223, 0x2B, 1, 0)` to exactly these two on
entry, i.e. hides/disables them on Xbox (flags value 1 versus 4/6 used by the data; inferred), and never reads them.

### The Codename wheel
The wheel `C_SBCNOPTIONS` builds its items **in code** (`Menu_UpdateWheel` 0x07F680 with controls
0x100000FF/0x100001A5/0x100001EC/0x10000109 and the page list in `C_SBCNOPTIONS` 0x08C7F0), so a sixth wheel item
needs code changes in two places plus wheel graphics. Not data.

### In-game menus (mission blob `08000001`)
- Pause page 0x4000004B (`dump-pause-4000004B.txt`): **tabs MISSION / OBJECTIVES / CONTROLS / SCORE** (one list
  control 0x10000028 whose rows/tabs are driven by `C_GCPAUSE` 0x080590 in code); CONTROLS tab = Y-Axis Inverted.
- **CHCH debug checkboxes (incl. widescreen 0x10000012) are on page 0x40000007** (`dump-debug-cheats-40000007.txt`;
  type 2 checkboxes, 18 rows) which links on to the tweak pages 0x40000044 / 0x4000004C / 0x4000000C. Nothing in the
  data or code opens 0x40000007, so it is unreachable in the retail game.

## 3. How to reach a new page
- **A button on the Audio/Video page (recommended, data only):** clone the "Credits" row 0x10000138 (row + script +
  `scriptmsg -3 0x44 <newpage>`), give it a new ID and label, place it in the free slot (e.g. y 284 between Screen Adjust
  256 and Credits 312, or below Restore Defaults). The game's own script system performs the page change; no handler
  change is needed. Back on the new page returns to 0x40000031 the same way the Credits page does.
- A sixth Codename wheel item: needs code in `C_SBCNOPTIONS` / `Menu_UpdateWheel` and wheel artwork: more work, more risk.

## 4. Text for new labels
`Txt_BindLabel` 0x06D460: `index = base[id >> 24] + (id & 0xFFFFFF)`, 7 groups (bases 0, 0x3E8, 0x714, 0x76F, 0x7A6,
0x811, 0x887), strings are plain Windows-1252 bytes (™ 0x99, ’ 0x92, `~A`-style button codes) in `uktxt.dat` etc.
(`GetLanguage` 0x06D130 picks the file). An unused group (e.g. **0x7F**) currently ends in "Invalid Text Label" or reads
past the 7-entry base table. **Native C can answer it**: rename the generated `sub_0006D460` (1,195 direct callers, all
through `RECOMP_ABI_CALL`) and put a native version in front that returns guest-heap strings for `0x7Fxxxxxx` (per
language via `GetLanguage`) and calls the original for everything else. No change to UKTxt.Dat and friends.

## 5. Injection plan ("PC Graphics" page in the Action front end)
1. **Blob extension in `MenuManager_Load`** (native wrapper for 0x092B50). If the blob contains `block 40000031`:
   - copy the blob into guest heap (`xbox_HeapAlloc`) with room for ~4 KB more;
   - **insert a "PC Graphics" button** into the 0x40000031 block (clone of the Credits row records, new control ID,
     label `0x7F000001`, `scriptmsg -3 0x44 0x40000060`) and add its byte count to that block's length field;
   - **append a new page block 0x40000060** before the `end` record, cloned from the 0x40000031 block: the page define,
     the title label (0x7F000002 "PC Graphics"), the help line (reuse 0x217), and three or four row + picker pairs
     cloned from the Subtitles row/picker (IDs 0x100002F0.. , labels 0x7F000010..: Picture shape, Resolution,
     Brightness, Scaling), plus a "Back"-style return like the Credits page.
   All records are cloned from the user's own blob at run time (positions/IDs/labels patched), so nothing from the
   game is shipped.
2. **Native text**: native `Txt_BindLabel` for group 0x7F (above).
3. **Page logic**: wrap `Handler_HandleMessage` 0x08E320 (or add the IDs to its dispatch through a native pre-check):
   for page 0x40000060 / controls 0x100002F0.. call native C:
   - 0x4B enter: for each picker `__Menu_Send(page, id, 0x17)` clear, `0x10` add item text (guest strings), `0x1E` select
     current value from the overlay store (`lean_overlay.inc`: nfo_shape incl. Widescreen 16:9, nfo_res, nfo_bright,
     nfo_filter; `nfo_load()`);
   - 0x4D changed: read the picker (`__Menu_SendMessage(ctrl, 0x40)` as `C_CHCHWS` does), write the store, `nfo_save()`;
     Widescreen keeps "(restart to apply)" in its item text, or switches live by copying `C_CHCHWS`'s action
     (write 0x1DF9E8/0x1F6610, `Camera_CalcViewAngles(1..8, 60°)`).
   Guest calls from native C use the existing `nd3d_call`-style helpers of the native Action tree.
4. Optional quick win (a row, not a page): stop hiding the original "Widescreen" row 0x10000223 on the Audio/Video page
   and back it with the same store.

**Size estimate:** ~4 KB added to the 133 KB blob at run time; native C ≈ 400–600 lines (blob splice ~200,
`Txt_BindLabel` wrapper ~60, page handler ~200, glue/renames ~50); one generated routine renamed (`sub_0006D460`) and
two wrapped (`sub_00092B50`, `sub_0008E320`), in `nightfire-port-native` only; no change to game files.
Risks to check when built: exact meaning of the `define` trailing fields (style/navigation order), the 0x2B flag
values, and that IDs 0x40000060 / 0x100002F0.. stay unused (true for the PAL front-end and mission blobs checked).

## Open points
- Navigation order between rows (which `define` field orders Up/Down) is inferred from positions only.
- Message 0x2B value 1 assumed "hidden/disabled".
- Only one mission blob (`07000012.bin`) was checked for the in-game pages; the front end is complete.
