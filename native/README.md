# Native builds (source snapshot, 2026-10-09)

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

## Where things are

- **Graphics:** `driving/runtime/native/` replaces the game's Xbox Direct3D
  library routines with Direct3D 11 (`nd3d_g1_state.c` to `nd3d_g5_present.c`;
  `nd3d-replace-set.json` lists the routines by address). Action uses a copy in
  `action/runtime/native/` plus `action/runtime/native_action/` for routines
  only Action calls. Both share the GPU-resident renderer in `runtime/lean/`.
- **Sound:** the game's DirectSound calls are answered by the project's own
  mixer (`driving/runtime/lean/lean_dsound.c`; Action's `native_action/nds_*.c`)
  instead of an emulated sound chip.
- **Optional PC settings** (only with `NF_OVERLAY=1`, set by the overlay
  launchers): `runtime/lean/lean_overlay.inc` holds the F10 menu, internal
  resolution, picture shape, widescreen, brightness, scaling and display mode;
  `action/runtime/native_action/pcg_menu.c` adds the "PC Graphics" page and mouse control to
  Action's own menus. Settings are saved in
  `%LOCALAPPDATA%\NightfirePC\overlay-settings.ini`.
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
backups and build outputs are all excluded.

The native builds still compile a few sound files the toolkit adapted from
xemu (LGPL-2.1-or-later): the MCPX voice processor, DSP and core in Driving,
and the Xbox ADPCM decoder header in Action. They are not published here;
take them from the pinned toolkit checkout. The project's own replacements
(`lean_audio_dsp.c`, `nds_dsp.c`) exist but are not yet the default.
