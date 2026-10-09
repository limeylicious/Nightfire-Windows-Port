# In-game menus: map for a "PC graphics" settings entry (read-only study, 2026-10-09)

Sources: generated C in `nightfire-port-native/src/recomp/gen` (Action) and `nightfire-driving-native/src/recomp/gen`
(Driving), the PAL XBEs, and the NightfireResearch labels (`nightfire-port/analysis/nightfire-research-labels.json`,
Action only; treated as leads and checked against the code below). No code changed, nothing built or run, Ghidra not used.
Helpers: `gen.py` (print a generated function / callers / references), `handler_map.py` (ID → handler table,
output in `action-handler-ids.txt`), `driving-pause-strings.txt`.

---------------------------------------------------------------------------------------------------------
## Action (default.xbe)

### 1. The widescreen handler 0x081DD0 = `C_CHCHWS_Handler` (debug/cheat checkbox, not a player option)
- Reached only through the central message router **`Handler_HandleMessage` 0x08E320** (no direct references):
  it switches on the 32-bit ID at `object+0x18`; control ID **0x10000012** → 0x081DD0.
- It belongs to the **"CHCH" control family**: MUSIC, FLY, DRONES, BLIND, WEAP, HEALTH, HUD, DEBUG, COORDS, **WS**,
  ALLOWFREEZE, DUMMY, DRAWALL, ZEROG, LOCKUP, BRIGHT, UNLOCK, CONTROLS (IDs 0x10000012–0x1000021D). These are the
  developers' cheat/debug toggles, not the player's Options.
- Behaviour: message 0x51 → `__Menu_SendMessage(ctrl, 0x2E, MEM32(0x1DF9E8), 0)` (show the current value);
  message 0x4B → read the box (0x40) into 0x1DF9E8 and 0x1F6610, then `Camera_CalcViewAngles(player 1..8, 60°)`.
  So **the game can switch widescreen live** from this box (cameras re-applied at once; the D3D present flag is
  only set at device creation).
- Which page holds the CHCH controls is defined in the menu data (no page code names them); a retail route to it was
  not found in code (`P_TWEAKS` 0x40000044 / `P_TWEAKS2` 0x40000046 are the likely debug pages). **No hidden
  widescreen item exists in the player's Options pages**: their handlers (below) never touch 0x1DF9E8/0x1F6610.

### 2. How menus work
| piece | address (research name) | notes |
|---|---|---|
| message router | 0x08E320 `Handler_HandleMessage` | ID at `obj+0x18` → handler; pages 0x4000xxxx (`P_*`), controls 0x1000xxxx (`C_*`); full table in `action-handler-ids.txt` (≈130 IDs) |
| send to a control | 0x072820 `__Menu_Send(page, ctrlID, msg, p1, p2)`, 0x0728F0 `__Menu_SendEx(page, ctrlID, row, msg, p)`, 0x072630 `__Menu_SendMessage(ctrl, msg, p1, p2)` | |
| change page | 0x092C40 `Manager_SendMessage(0x44, pageID, …)` | e.g. `Manager_SendMessage(0x44, 0x40000031)` opens Audio/Video options |
| page layouts | **data**: `MenuManager_Create` 0x093960 reads a binary blob (pointer at 0x25FB08, set by `MenuManager_Load` 0x092B50 from `LoaderProcess` 0x0BE7B0 / `ResetMap_Load` 0x0BFB60) with `BIN_GetDWord/Word/Byte` 0x06B170/150/140, switch over 15 record types → `Component_AddComponent` 0x071CC0, `Component_InitInstance` 0x071D20 | pages and rows live in the packed `eurocom\filesys.d00-d10` data, not in code |
| control types (`+0x7B`) | Button 0x070D40, Checkbox 0x071280, Combo 0x0716C0 (value picker), Label 0x08FDD0, List 0x090C00, Memo 0x094B80, Page 0x095790, Radio 0x0960E0, Scroll 0x0975B0, Spin 0x098550, Text 0x099550, Window 0x099BE0 (each with `_SendMessage` / `_Update`) | |
| control messages (inferred) | 0x10 add item (text ptr), 0x17 clear items, 0x1E set selected index, 0x27 set range, 0x2B enable/disable, 0x2E set value, 0x40 get value, 0x69 (slider step) | from P_CNAVOPTIONS / C_CHCHWS / P_PAUSE |
| page messages (inferred) | 0x4B enter (p = previous page), 0x4C… leave/apply, 0x4D control changed (dispatch on the control ID) | P_CNAVOPTIONS switch 0x4B–0x50 |
| text | `Txt_BindLabel` 0x06D460(stringID) → char*: group = `id>>24` (base from table at 0x1FEC78), index = `id & 0xFFFFFF`, out of range → "Invalid Text Label". Files `UKTxt.Dat FRTxt.Dat GRTxt.Dat SPTxt.Dat ITTxt.Dat DUTxt.Dat USATxt.Dat JAPTxt.Dat` (names at 0x17C1F4[lang]), loaded by `Txt_LoadLanguage` 0x06D250 from the packed data; language `GetLanguage` 0x06D130 / `Txt_SetLanguage` 0x06D450 | the PAL text files are inside filesys (compressed or packed: no plain menu text found on disk) |
| text drawing | `Text_Init` 0x099550, `Text_SetupChar` 0x098B50, `TextBox_Update` 0x098DE0, `Label_Update` 0x08FFF0 | |

### 3. Front-end Options path
Main (`P_MAIN` 0x40000002) → … → Codename menu **`P_CNMENU` 0x4000001D**, whose wheel control **`C_SBCNOPTIONS`
0x08C7F0** (built by `Menu_UpdateWheel` 0x07F680 from controls 0x100000FF/0x100001A5/0x100001EC/0x10000109)
opens, by wheel index (message 0x40 = selected item):

| index | page | handler |
|---|---|---|
| 0 | 0x40000020 | `P_CNNAME` (name) |
| 1 | 0x40000022 | `P_CNCONTROLS` |
| 2 | 0x4000003E | (no handler; data-only page) |
| 3 | 0x4000002D | `P_CNOPTIONS` (game options) |
| 4 | 0x4000002E | `P_CNMPOPTIONS` (multiplayer options) |
| 5 | **0x40000031** | **`P_CNAVOPTIONS` 0x08D7A0 (audio/video options)** |
| 6 | (codename save/default, `Menu_UpdateDefaultCodename` 0x07F100) | |

`P_CNAVOPTIONS` on enter (0x4B, from 0x4000001D):
```
__Menu_Send(page, 0x10000135, 0x27, 0, 20)   music volume slider: range 0..20
__Menu_Send(page, 0x10000135, 0x2E, SFXMusicGetVolume()/5, 0)
__Menu_Send(page, 0x10000134, ...)           effects volume, same
__Menu_Send(page, 0x10000136, 0x17, 0, 0)    sound-mode picker: clear
__Menu_Send(page, 0x10000136, 0x10, Txt_BindLabel(0x1B7,..))   add "…"
__Menu_Send(page, 0x10000136, 0x10, Txt_BindLabel(0x1B6,..))   add "…"
__Menu_Send(page, 0x10000136, 0x1E, MEM32(0x1F6614), 0)        select current
__Menu_Send(page, 0x1000019A, …0x2BA / 0x2B9…, 0x1E, MEM32(0x1F660C))  split-screen layout picker
__Menu_Send(page, 0x10000198 / 0x10000223, 0x2B, 1, 0)          buttons enabled
```
It is a small page of exactly the kind needed: value pickers filled from code, with the stored values in plain globals.

### 4. In-game pause menu
`P_PAUSE` 0x080030 (page 0x4000004B) holds one list control **0x10000028** (`C_GCPAUSE` 0x080590), rows defined in
data and filled in code via `__Menu_SendEx(page, 0x10000028, row, msg, p)`: e.g. row 9 gets 8 items (text IDs
0x28, 0x37, 0x46, 0x55, 0x64, 0x73, 0x100016D, 0x100017C, i.e. the controller layouts) and its selection from the
per-player settings block (`0x1FE6DE + player*0x158`). `C_GCPAUSE` reacts per row (5-way switches at 0x0818EC /
0x081918). So the pause menu is also a list of option rows the code fills; adding a row needs the data definition.

---------------------------------------------------------------------------------------------------------
## Driving (Driving.xbe): different engine (EA "EAGL")
- Pause menu object built by **`sub_000DAB50`**: it looks up named elements in a loaded UI layout
  (`sub_000D3850(layout, "pauseN")`, `sub_000D5810(parent, name, 3, …)`) and stores them in fields
  (`+0x100`, `+0x110…+0x1DC`). Layout data: `hudNTSC.gal` / `HUDQ` inside the `.viv` archives; fonts EAGL `.xfn`
  (`data\loading\loading.xfn`, `debug.xfn`).
- Pages and element names: mission (`continueMis` `restartMis` `quitMis`, `restartConfirm` `quitConfirm` `yes` `no`),
  objectives (`objCheck` `objBullet`), score, **controls** (`controlsHeader` `controlsDetails` `controlsYAxis`
  `controlsInverted` `invert`, arrows `controlsArrowL/R`, gels `controlsGelL/M/R`), page switching `changepage`,
  hints (`hintTitle` `hintText` `hintQuit`). The only settings row is the Y-axis invert toggle on the controls page.
- Text: keys looked up in the mission's text (the `.viv` files carry readable text, e.g. Spanish lines in misc.viv).
- No widescreen or video row exists. Widescreen comes from the display object (0x1EBFF4 +0x4C) set at start
  (see `../widescreen-check/RESULTS.md`).

---------------------------------------------------------------------------------------------------------
## Best host and smallest workable approach

### Action: host = Audio/Video options (`P_CNAVOPTIONS`, page 0x40000031), reached from the Codename menu
Option A (smallest, no data editing): **take over the page's handler.**
1. Route page 0x40000031 (and its controls) to native C: wrap `sub_0008D7A0` (it is called only from
   `Handler_HandleMessage` 0x08E320 case 0x40000031, so a dispatch override or a renamed generated routine, as done
   for the native D3D library, keeps everything else untouched).
2. On enter (0x4B), fill one extra existing picker row with PC values. Simplest: **reuse the split-screen layout picker
   0x1000019A** (two items, only meaningful in multiplayer) **or** add PC rows to the existing pickers' item lists only
   when a PC flag is set. Values come from / go to the overlay's settings store (`lean_overlay.inc`
   nfo_shape/nfo_res/nfo_bright/nfo_filter, `nfo_save()`), text from native C strings (pass a char* with message
   0x10, exactly as the game passes `Txt_BindLabel` results), so no Txt.Dat change is needed.
3. On change (0x4D for that control ID) write the value to the store; "restart to apply" for Widescreen as in F10.
Option B (cleaner, needs the menu blob format): add a new page "PC graphics" to the Codename wheel as a cloned
copy of the 0x40000031 page record with new IDs (e.g. 0x400000F0 / 0x100002xx), handled entirely in native C.
Requires decoding `MenuManager_Create`'s 15 record types and injecting records at load (the blob pointer at
0x25FB08 can be swapped for a patched copy before `MenuManager_Create`), plus a wheel entry.
Live widescreen in Action is possible by reusing `C_CHCHWS`'s logic: write 0x1DF9E8/0x1F6610 and call
`Camera_CalcViewAngles(n, 60°)` for players 1..8 (the present flag would still need a restart).

### Driving: keep the F10 overlay (recommended)
Adding a row means adding named elements to the EAGL layout (`hudNTSC.gal`), a separate data format. The one existing
toggle row (controls page, Y-axis invert) is a plausible template for a later clone, but it is not worth it for the
first version: Driving's settings follow the Action choice anyway (one shared ini, read at start).

### Recommended order
1. Action Option A on the Audio/Video page (one or two PC rows: Picture shape incl. Widescreen 16:9, Resolution),
   backed by the overlay store; keep F10 for everything and for Driving.
2. Decide on Option B after decoding the menu blob records (needs the filesys reader; NightfireResearch tools may
   already have one: check upstream before writing one).

## Open points
- Which page shows the CHCH debug controls, and whether retail can reach it (data-only; needs the menu blob).
- Exact semantics of messages 0x4C–0x50 and 0x69 (inferred from use, not traced).
- The text of string IDs 0x1B6/0x1B7/0x2B9/0x2BA (in UKTxt.Dat inside filesys; not extracted here).
