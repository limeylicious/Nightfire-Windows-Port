# Driving lean renderer (source snapshot, 2026-10-04)

This directory holds the new and changed source of the **lean GPU-resident
renderer** for the Driving engine (`Driving.xbe`). It is the Driving build the
owner currently plays. Files are copied byte for byte from the
`nightfire-driving-lean` workspace and listed with hashes in the root
`SOURCE-MANIFEST.json` (group `driving-lean`).

## What it changes

The older Driving bridge copied rendered frames from the GPU back into
emulated Xbox memory about 20 times per frame and ran at about 2.4 FPS. The
lean renderer keeps colour and depth surfaces on the GPU and runs the game's
vertex and pixel programs as generated HLSL.

- `runtime/lean/lean_gpu.c`: pushbuffer front end that replaces
  `driving_gpu143.c` and the copied generic executor in the lean build.
- `runtime/lean/lean_d3d.c`, `lean_d3d.h`, `lean_sampler.c`: Direct3D 11
  backend (resident surfaces, texture formats, register combiners, clears,
  blits).
- `runtime/lean/lean_present.inc`, `runtime/driving_present201.c`,
  `runtime/driving_flip204.c`: Direct3D swap-chain present and 50 Hz flip
  pacing (`LEAN_SWAPCHAIN=0` falls back to the GDI window).
- `runtime/lean/apu/` (removed on 2026-10-09): lean copies of the toolkit's
  MCPX APU files, derived from xemu under LGPL-2.1-or-later. They are no longer
  published here; `CMakeLists.txt` still names them. The native builds in
  [`native/`](../../native/README.md) play sound through the project's own
  mixer instead.
- `runtime/kernel_bridge.c`, `runtime/driving_dsp149.c`, `src/main.c`: APU
  interrupt delivery, audio-processor command acknowledgement, drift-free
  50 Hz vblank and lean start-up, all under `DRIVING_LEAN_RENDERER`.
- `runtime/lean/lean_flags.c`: fallback for a flags variable the lifter reads.
- `CMakeLists.txt`: the lean workspace's build file. Configure with
  `-DDRIVING_LEAN_RENDERER=ON` to produce `nightfire_driving_lean.exe`.
- `play-lean-vehicle.cmd`, `play-lean-paris.cmd`, `run-lean-scene.py`:
  launchers; `scripts/`: image, audio and window helpers used in testing.
- `LEAN-TASK1-REPORT.md`, `LEAN-TASK2-REPORT.md`: design notes and
  measurements.

## Measured (owner's laptop, PAL game, default settings)

- Vehicle and Paris scenes hold 50 FPS; about 10-22 ms of work per frame.
- The vehicle countdown matches real time (4:58 at 50.2 s, 4:13 at 95.2 s).
- Game audio plays with no buffer underruns.

## Known gaps

- Lighting differs from the console: car headlight glow and pools of light
  under street lamps are missing.
- Rifle shots from the helicopter do not damage the cars.
- In the vehicle tutorial the red car can follow the wrong route and stall.

## Tutorial AI route fix (described, not shipped)

The vehicle-tutorial route fix is a hand edit to generated translation unit
`src/recomp/gen/recomp_0007.c`, so it is not in this repository. Where the
lifter emitted a conditional jump reading its never-assigned `_flags`
fallback, the edit evaluates the preceding `test`/`cmp` instead:

- `je` at guest address 0xCB003: taken when the 8-bit `test` result is zero.
- `jne` at 0xCC01C (to 0xCC0C5): it has two predecessors. The fall-through
  path from the 8-bit `test` at 0xCC01A uses not-zero; the `cmp` path that
  jumps there is evaluated in place as not-equal, then continues at 0xCC022.

Before the fix, route-locked road segments were never skipped. Whoever
regenerates the translation needs the lifter to emit real flag tests for
these sites (or re-apply the edit). It is awaiting the owner's in-game test.

## Not included

This is documentary source, not a standalone build. Runtime files the lean
build uses unchanged (the rest of `nightfire-driving/runtime`, the 491 layer
chain), the generated game translations (`src/recomp`, including a hand
patch to one generated unit), the `generic263` executor, the toolkit and all
game files are excluded, as in the rest of this repository. The workspace's
`lean-source-manifest.json` is excluded because it lists local paths.
