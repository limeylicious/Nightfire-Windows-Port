# Lean renderer – Task 2 report (2026-10-04)

## Result
- **Old 491 bridge:** about 400–425 ms per frame (about 2.4 FPS), and game time ran in slow motion.
- **Lean build:** a steady **20 ms per frame (50 FPS, the PAL refresh rate)** in both the vehicle and Paris scenes, with game time matching real time.
- **Pictures:** visually match the old path. They also restore effects the old path dropped: the scope blur, the tutorial text, and the light glows. See `LEAN-COMPARISON.png`.

## What was built
- `runtime/lean/lean_gpu.c` (~400 lines) replaces `driving_gpu143.c` + the generic executor in the lean build only.
  - It reuses the existing pushbuffer walker, RAMHT/DMA object decoding and blit helpers.
  - It caches DMA objects per drain.
  - It holds the full Kelvin register state.
  - It assembles indexed, draw-array, inline and immediate-mode vertices.
  - It converts all primitive types to lists.
  - Semaphores are written immediately: all guest inputs are copied at draw time, so nothing waits for the GPU.
- `runtime/lean/lean_d3d.c` (~700 lines) is the new D3D11 backend.
  - **Surfaces:**
    - Colour and depth surfaces stay resident on the GPU, keyed by guest address.
    - The AA surface is kept as one 1280x480 target, which matches the game's own storage. The game's own quincunx resolve draw samples it unchanged.
    - Render-to-texture works through a texture-to-surface alias, which is validated against CPU overwrites.
  - **Vertex programs:** translated to HLSL and run on the GPU, including ARL, EXP, LOG, LIT and DST. Nothing runs on the CPU any more.
  - **Pixel stage:** a general register-combiner to HLSL generator (8 stages, final combiner, mux, dot products, blue-to-alpha, output shifts). It also covers all four texture stages (2D, projective, cube and pass-through modes), alpha test, alpha kill and fog.
  - **Textures:**
    - Formats: swizzled/linear ARGB/XRGB/565/1555/4444/L8/A8/AL8, palettised I8, DXT1/3/5 (uploaded as BC directly), YUV and RGBA variants.
    - Cached and content-hashed once per drain.
  - **Clears:** full, rectangle (ClearView) and masked clears.
  - **Blits:** NV09F blits copy GPU to GPU.
- **Readback:** one 640x480 readback per presented frame, which feeds the existing window (`driving_present201`).
- **Build:**
  - Build with `-DDRIVING_LEAN_RENDERER=ON` into `build-lean`; the output is `nightfire_driving_lean.exe`.
  - The 491 build in `build/` is untouched and still builds from the same tree.

## Fixes found on the way
1. **Teal roads/mountains.** 16-bit (R5G6B5) surfaces receive a pre-packed clear value. The game clears a 512x512 565 target to white (0xFFFF) and multiplies roads and terrain by it. Decoding that value as ARGB gave teal; it is now decoded per surface format.
2. **Game clock 0.77x real time.** The 50 Hz vblank was scheduled with GetTickCount64, whose 15.6 ms granules make it about 32 Hz. The lean build now uses a drift-free QPC 50 Hz (`LEAN_VBLANK_MODE`, default 1).
3. **50 ms per frame lost in the flip handshake.** The original display-queue handshake (flip204 + timer thread) cost about 50 ms per frame. Making the timer loop faster broke it: the queue stopped advancing and hit a 5 s timeout. The lean default (`LEAN_FLIP_MODE=1`) presents each requested buffer directly and paces to 50 Hz. Even unpaced, the game itself holds 50 FPS.

## Measurements (lean, default settings)
- **Vehicle:**
  - Per frame: wall 20.0 ms; drain 10–14 ms (draw submit 3.5–4 ms, readback 2.3 ms); game code outside drains 7–10 ms.
  - On-screen countdown: 4:58 at 50.2 s and 4:13 at 95.2 s, i.e. 45 game seconds in 45 real seconds.
- **Paris:**
  - Per frame: wall 20.0 ms; drain 7–8 ms.
  - Loading: about 12 s.
  - Helicopter flight between the two scopes: about 13 s. At about 7 FPS (artificial delay) it was about 15 s, so the flight follows game time, not frame count.

## Known issues / not done
- **Paris freeze about 41 s into the scene.** Drawing and presenting stop; the main thread is polling in audio-library code. The old 491 build shows the same stop: in all nine 360 s Paris runs, presents stop 20–55 s before the watchdog. So this is a pre-existing game/audio blocker, not the renderer.
- **Approximations:**
  - Bump-env stages are approximated without the du/dv offset.
  - Colour-key kill is not emulated.
  - Shadow-compare depth textures are not implemented.
  - Two-sided lighting colours (oB0/oB1) are not used.
  - None of these were observed to matter in the two scenes.
- **Quincunx resolve:** a bilinear 2:1 horizontal filter stands in for the exact kernel.
- **Presentation:** still goes through a 640x480 CPU readback and the GDI window. A D3D swap chain would save about 2 ms.

## How to run
- `play-lean-paris.cmd` / `play-lean-vehicle.cmd` run up to 30 minutes; close the window to stop.
- `python run-lean-scene.py vehicle|paris --build lean|base [--seconds N] [--env NAME=VALUE]`
- Diagnostics (env):
  - `LEAN_CAPTURE_EVERY_MS` saves wall-clock captures.
  - `LEAN_FLIP_MODE` (0 = original queue, 1 = direct 50 Hz, 2 = unpaced).
  - `LEAN_VBLANK_MODE`.
  - `LEAN_WHITE_STAGES`, `LEAN_NO_FOG`, `LEAN_WHITE_DIFFUSE`.
  - `LEAN_LOG_TEXTURES`, `LEAN_FLIP_TRACE`, `LEAN_LOG_SW_AFTER_MS`.

# Task 3 – Paris freeze (2026-10-04)

**Cause.**
- A lean-only sampling profiler (`runtime/lean/lean_sampler.c`, `LEAN_SAMPLE_AT_MS`) showed one guest thread spending 100% of its time in `sub_0017C924`. The call chain was `0017CFE0 < 0017B703 < 0017BAF4 < 0017BE1B < 00140880`.
- `sub_0017C924` is DirectSound's GP-DSP command mailbox. It waits for `[page0+0x810]` to become 0, then posts command 2 (a DSP memory update).
- No DSP program runs, and the existing DSP149 simulation only acknowledges the single startup command 3.
- About 41 s into Paris the game posts its first command 2. The next command then spins forever, holding the audio lock that the game loop needs.
- The old 491 build has the same freeze: all nine of its 360 s Paris runs stop presenting before the watchdog.

**Fix (lean build only).**
- `runtime/driving_dsp149.c` starts a mailbox responder thread once the startup handshake is seen.
- The thread polls the mailbox every 1 ms (high-resolution waitable timer) and acknowledges each posted command, like a DSP finishing its next frame.
- `LEAN_DSP_MAILBOX=0` restores the original wait.
- No DSP code is executed, so DSP-computed effects (for example reverb or the mixbin effects chain) are not produced.

**Results.**
- **Paris, 240 s:**
  - 10,140 frames presented, with no freeze.
  - Three command-2 acknowledgements.
  - The game plays the first scope, the flight, the second scope, a further flight, and a third scope.
  - At about 80 s it shows "Mission Failed", because nobody shoots without input. It then idles on that screen at about 50 FPS.
  - Frame time is 22–23 ms in the heavier later part of the scene.
- **Vehicle, 120 s:** about 21 ms per frame (47–50 FPS); one command-2 acknowledgement; the countdown continues.

**Audio status.** The toolkit APU emulation is active: the voice processor runs and outputs through XAudio2 at 48 kHz, while the DSP is a passthrough stub. Any sound you hear comes from that voice path, without DSP effects. Real DSP effects would need DSP program execution (`apu_dsp.c` contains a DSP56300 core that is currently stubbed) or a native mixer like Action's.
