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
- `runtime/lean/apu/`: lean copies of the toolkit's MCPX APU files (derived
  from xemu, LGPL-2.1-or-later; notices kept in each file). They deliver the
  voice-processor output to XAudio2 and ramp voice gains to avoid clicks. The
  DSP is still stubbed, so reverb and other effects are absent.
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

## Not included

This is documentary source, not a standalone build. Runtime files the lean
build uses unchanged (the rest of `nightfire-driving/runtime`, the 491 layer
chain), the generated game translations (`src/recomp`, including a hand
patch to one generated unit), the `generic263` executor, the toolkit and all
game files are excluded, as in the rest of this repository. The workspace's
`lean-source-manifest.json` is excluded because it lists local paths.
