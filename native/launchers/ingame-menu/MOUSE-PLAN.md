# Mouse support for Action's menus: plan (read-only study, 2026-10-09; not built)

## What the game already has
- **A focus "cursor" with hit-testing.** Each player's menu state (0x25F1D0, 0x1D8 bytes per player) has a cursor
  object at +0x94 (x +0x24, y +0x26, w +0x2C, h +0x2E). `Page_Update` 0x095A80 moves it every frame to the centre of the
  focused control (`page+0xAC`; current page at `menu+0x1BC`). `Menu_CursorOverMe` 0x0723C0 hit-tests that point against
  every control of the page (list at `page+0x94`, linked by `+0`, rectangle `+0x70 x, +0x72 y, +0x74 w, +0x76 h`
  relative to the page's own `+0x70/+0x72`; it skips pages (type `+0x7B` = 0xD) and controls with `+0x7A` set) and
  the control under it draws highlighted (`Button_Update` 0x070FE0, `Combo_Update` 0x0719B0, `Label_Update` 0x08FFF0,
  checkboxes). So the menu already "points"; it is just always driven by the pad.
- **A focus request slot.** `Page_Update` checks `page+0xB0 + player*4` (4 players) and, when it names a control on the
  current page, makes that control the focus (sets `page+0xAC`, the cursor position, `menu+0x1CC`). This is the
  game's own way to move focus to a given control, so the mouse can use it instead of faking Up/Down presses.
- **Control rectangles are in 640x480 menu units** (the menu blob's `define` records; e.g. the Audio/Video rows
  56,86 301x16), the same units as the game's picture.
- **Pad input from the PC** goes through `fb_present.c` (window messages) → `nf_pc_controls` → the XPP pad packet
  (`nightfire_input.c`, `[INPUT] ... digital=.. A=.. B=..` lines). Left click currently only captures the mouse for
  mouse-look (`mouse_acquire`), Esc releases it.

## Plan (all with NF_OVERLAY=1, Action only, in nightfire-port-native)
1. **Menu mode** (`pcg_menu.c`): `nf_menu_active()` = the player-1 menu state has a current page
   (`G32(0x25F1D0 + 0x1BC) != 0`) and the manager is running (front end or pause). While it is true the mouse is not
   captured for mouse-look and the Windows arrow cursor is shown over the game window; outside menus nothing changes.
2. **Mouse to menu units** (`lean_overlay.inc`, ~20 lines): `nf_overlay_client_to_game(x, y, &gx, &gy)` using the same
   picture rectangle the overlay draws into (`nfo_present`: stretch / 4:3 / 16:9 bars) and the real client size, so it is
   right in windowed, fullscreen, with Windows scaling (DPI-aware client pixels) and at any internal resolution.
   Outside the picture (in the bars) = no control.
3. **Window messages** (`fb_present.c`, ~60 lines): in menu mode record `WM_MOUSEMOVE` position, left/right button
   presses and `WM_MOUSEWHEEL` into a small locked struct instead of capturing; Esc keeps its current meaning.
4. **Focus follows the mouse** (`pcg_menu.c`, native wrapper of `Page_Update` 0x095A80 added to `pcg_menu_rename.py`,
   ~120 lines): before the original runs, if the mouse moved: map it to menu units, walk the current page's control list
   with the same rectangle test as `Menu_CursorOverMe` (skip type 0xD and hidden `+0x7A`, prefer selectable types:
   button 1, row 5 with a picker, picker 9, slider 0xA, checkbox 2), and write the hit control to the focus request slot
   `page+0xB0` (player 0). The original `Page_Update` then moves focus and the highlight exactly as for the pad.
5. **Clicks and wheel become pad presses** (`fb_present.c` / `nightfire_input.c`, ~60 lines), one sample long, only in
   menu mode and only when the pointer is over a control: left click = A, right click = B, wheel = Up/Down, or
   Left/Right when the focused control is a picker or slider (type 9 / 0xA). A click on a picker's left or right half
   could also mean Left/Right (optional).
6. **Not touched:** gameplay mouse-look and capture, Driving (its EA menus are a different system), the F10 overlay
   (it already takes the mouse while open).

**Size:** about 250–350 lines in `pcg_menu.c`, `fb_present.c`, `nightfire_input.c` and `lean_overlay.inc`, plus one
more renamed routine (`Page_Update`). Test: front end (Codename wheel, Audio/Video, PC Graphics), pause menu tabs,
windowed / fullscreen / 16:9.

## Open points to check while building
- Which `define`/runtime field marks a control selectable (the data's `msg 0x2B` 4 vs 6); until found, the type filter
  above plus "is in the page's navigation list" is used.
- Wheel-style controls (the Codename wheel `C_SBCNOPTIONS`, scroll lists) may need Left/Right instead of focus moves.
- The per-player slot layout (`page+0xB0 + 4*player`) is read from `Page_Update`; confirm player 0 is index 0.
- The pause menu's tab strip is one list control (0x10000028) with rows drawn by code: hit-testing its rows needs the
  list's row rectangles (`List_*` 0x090C00..) rather than separate controls.
