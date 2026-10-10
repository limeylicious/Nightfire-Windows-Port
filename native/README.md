# Native builds (source snapshot, 2026-10-10)

This directory holds the hand-written source of the **native** Action and
Driving builds: the versions that run the recompiled game without emulating
the Xbox graphics or sound chips. Files are copied byte for byte from the
owner's workspaces and listed with hashes in the root `SOURCE-MANIFEST.json`
(groups `native-action`, `native-driving` and `native-launch`).

| Directory | Workspace | Contents |
| --- | --- | --- |
| `action/` | `nightfire-port-native` | Action engine (missions and multiplayer) |
| `driving/` | `nightfire-driving-native` | Driving engine (vehicle levels) |
| `launchers/` | `native-driving` | Launchers, the one-window host, survey scripts and notes |

The launchers and set-up of the first alpha test are in
[`../release/alpha/`](../release/README.md).

## Where things are

- **Graphics:** `driving/runtime/native/` replaces the game's Xbox Direct3D
  library routines with Direct3D 11 (`nd3d_g1_state.c` to `nd3d_g5_present.c`;
  `nd3d-replace-set.json` lists the routines by address). Action uses a copy in
  `action/runtime/native/` plus `action/runtime/native_action/` for routines
  only Action calls. Both share the GPU-resident renderer in `runtime/lean/`.
  The Action build regenerates its copy from the Driving tree.
- **Sound:** the game's DirectSound calls are answered by the project's own
  mixer instead of an emulated sound chip. Driving's mixer
  (`driving/runtime/lean/lean_dsound.c`) plays through the project's own
  XAudio2 output, `nf_audio_out.c`. Action uses `native_action/nds_*.c`.
- **Optional PC settings** (only with `NF_OVERLAY=1`, set by the overlay
  launchers): `runtime/lean/lean_overlay.inc` holds the F10 menu, internal
  resolution, picture shape, widescreen, brightness, scaling, display mode and
  the frame rate rows. `runtime/lean/lean_binds.inc` and `nf_binds.h` hold the
  key binds (both engines). `action/runtime/native_action/pcg_menu.c` adds the
  "PC Options" pages (PC Graphics, Key Binds), the Multiplayer Local / Online
  pages and mouse control to Action's own menus. Settings are saved in
  `%LOCALAPPDATA%\NightfirePC\`.
- **Smooth Motion:** `runtime/lean/lean_interp.inc` draws in-between pictures
  between game steps for high-refresh screens. The game logic keeps its
  original rate. It is experimental (some fast-moving parts can still be drawn
  out of place).
- **Controllers and split-screen:** `action/runtime/nightfire_input.c` (four
  controller ports, virtual pads for one shared keyboard) and
  `nightfire_direct_mouse122.h` (mouse look per player).
  `action/show-nightfire-native-4pads.cmd` starts the split-screen setup.
- **Lockstep test** (groundwork for online play):
  `action/runtime/native_action/nf_lockstep.c` with
  `show-nightfire-native-lockstep-record.cmd` and `-replay.cmd` records a
  session's inputs, replays them and compares the game memory frame by frame
  (`action/scripts/nf_state_diff.py`). Online play itself is not implemented.
- **Fixes applied after code generation:** `driving/scripts/nf_frndint_fix.py`
  makes translated `frndint` honour the x87 rounding mode (the Paris
  grappling-hook fix). `action/scripts/nf_rcr_fix.py` replaces the diagnostic
  stops the translator left for rotate-through-carry (`rcr`) in the C
  library's 64-bit divide helpers with the real instruction (the new-profile
  crash fix). Each `build-native.cmd` runs its script.
- **One window:** `launchers/one_window_host.py` and
  `launchers/play-nightfire-native.cmd` run Action, hand over to Driving for
  the Paris chase and back again in a single window.
- **Crash capture:** `runtime/lean/lean_session.c` and `lean_hang.c` (Driving)
  and `runtime/nightfire_session242.c` (Action) write a crash report,
  minidump or freeze report per session.
- **Notes:** `launchers/` also keeps the surveys and test write-ups behind
  each step (phase 0 results, the Action audio survey, the in-game menu map,
  widescreen and DPI checks).

## Not included

Like the rest of this repository, this is documentary source, not a
standalone build. The generated game translations (`src/recomp`), any code
lifted from the game, tables and launch data taken from the game, the
toolkit checkout, game files, extracted menu data, logs, crash sessions,
backups and build outputs are all excluded. A few files whose comments name
the owner are held back, among them `runtime/lean/lean_present.inc`.

The Driving build no longer compiles any sound code adapted from xemu
(LGPL-2.1-or-later). The Action build still does: the toolkit's xemu-derived
APU core and DirectSound library, plus `runtime/native_action/nds_out.c` and
the `nightfire_adpcm.h` header, which the project treats as derived. None of
them are published here; the toolkit's parts come from the pinned toolkit
checkout. Replacing them in Action is planned.
