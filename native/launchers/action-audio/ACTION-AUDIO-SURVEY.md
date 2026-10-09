# Native audio, Action engine: DirectSound survey (2026-10-08)

This was a static check only. Nothing was built or run, and nothing outside
`native-driving/action-audio/` was changed. The full data is in
`action-dsound-map.json`.

The scripts, which are run in this order, are:

1. `dsound_action_map.py`
2. `dsound_action_survey.py`
3. `dsound_action_export.py`

Their intermediate output is in the `work-*.json` files.

## Inputs (verified)

| File | SHA-256 |
|---|---|
| `nightfire-analysis/game_files/default.xbe` (same as `nightfire-port/game_files/default.xbe`) | `b464b787…2641aa1`, the expected PAL hash |
| `nightfire-analysis/game_files/Driving.xbe` (same as the `nightfire-port` copy) | `0f4c50e4…0bfcef` |

Other inputs:

- Generated C:
  - Action: `nightfire-port/src/recomp/gen`
  - Driving: `nightfire-driving-native/src/recomp/gen`
- Driving symbols: `tools/ghidra-driving-analysis/xb-symbol-cli.txt`
- Action research labels: `nightfire-port/analysis/nightfire-research-labels.json`. These were treated only as leads to check.
- Disassembler: capstone 5.0.6 from `nightfire-port/analysis/python-deps`.

The DSOUND section in each game:

- **Action:** 0x112600–0x12FD4C. That breaks down as:

  | Range | Contents |
  |---|---|
  | to 0x11C801 | code |
  | 0x11C801–0x12F000 | constant tables (full-HRTF filter tables) |
  | 0x12F000–0x12FD4C | globals (BSS from 0x12FB04) |

  - The generator also emitted some routines inside the table area (0x120009, 0x12000E, 0x120064). These are table bytes decoded as if they were code, not real routines.
- **Driving:** 0x17AC40–0x183AA4.

## Method

This is the same approach as `native-driving/action-phase0/d3d_action_map.py`, with one stronger check added.

1. **Masked comparison.** Each DSOUND routine is compared as machine code. Every 32-bit image address and every external call target is ignored.
2. **Deep check (new).** A match is accepted only if the whole call tree is also equal, with addresses ignored.
   - This check is needed. The DSOUND C wrappers are byte-identical apart from the method they call.
   - The old "identical code group, assigned by order" rule mapped Driving `IDirectSoundBuffer_SetFilter` (0x17B5E0) to Action `IDirectSoundBuffer_SetHeadroom` (0x1133E6), which is wrong.

Results:

- **322 of 329** Driving DSOUND routines match, all with equal call trees. The 7 that do not match are:
  - the 6 routines Action does not link (the 3 SetFilter levels, the 2 SetI3DL2Listener levels and DirectSoundCreateBuffer);
  - 1 table at 0x182960 that the generator decoded as if it were code.
- **Address pairs:** 1,351 Driving→Action address pairs, with 0 conflicts.
- **Name check:** 57 matched routines carry both a Driving symbol and an Action research label. All 57 agree.
- **Argument counts:** taken from each routine's own `ret N`. For tail-jump wrappers they come from the routine it jumps to.

## 1. The 19 replaced Driving wrappers, mapped to default.xbe

**Result: 16 exact matches, plus 3 that Action does not link.**

- The 16 exact matches have equal call trees and the same length.
- The argument counts equal the Driving glue's `n` for all 16.

| Driving VA | Routine (Driving symbol) | Glue handler | Args | Action VA | Called in Action from |
|---|---|---|---|---|---|
| 17C259 | DirectSoundCreate | lean_ds_DirectSoundCreate | 3 | **1148B8** | xboxInitSound 0E1AE0; XMV 130624 |
| 17B59D | IDirectSound_DownloadEffectsImage | lean_ds_DownloadEffectsImage | 5 | **11338B** | xboxInitSound 0E1AE0 |
| 17C09F | IDirectSound_CreateSoundBuffer | lean_ds_CreateSoundBuffer | 4 | **1146FE** | 0E0F00, xboxCreateSoundBuffers 0E10C0 |
| 17AD34 | IDirectSound_Release | lean_ds_ReleaseDS | 1 | **112733** | XMV 130624 |
| 17AD4A | IDirectSoundBuffer_Release | lean_ds_ReleaseBuffer | 1 | **112749** | DSOUND only (destructor 113DC9); the game never calls it |
| 17B634 | IDirectSoundBuffer_Play | lean_ds_Play | 4 | **11343A** | SFXUpdate 0E19C0 |
| 17B658 | IDirectSoundBuffer_Stop | lean_ds_Stop | 1 | **11345E** | SFXUpdate 0E19C0 |
| 17BE3B | IDirectSoundBuffer_SetBufferData | lean_ds_SetBufferData | 3 | **1143E8** | 0E1BB0, 0E1D80 |
| 17B670 | IDirectSoundBuffer_SetLoopRegion | lean_ds_SetLoopRegion | 3 | **113476** | 0E1660, 0E1BB0 |
| 17B6CC | IDirectSoundBuffer_SetCurrentPosition | lean_ds_SetCurrentPosition | 2 | **1134D2** | 0E1BB0 |
| 17B6AC | IDirectSoundBuffer_GetCurrentPosition | lean_ds_GetCurrentPosition | 3 | **1134B2** | psiStreamGetPlayPos 0E1860 |
| 17B5FC | IDirectSoundBuffer_SetMixBins | lean_ds_SetMixBinsA | 2 | **113402** | xboxCreateSoundBuffers 0E10C0 |
| 17B618 | IDirectSoundBuffer_SetMixBinVolumes | lean_ds_SetMixBinsB | 2 | **11341E** | dsndSetPan 0E16C0 |
| 17B5C4 | IDirectSoundBuffer_SetVolume | lean_ds_SetVolume | 2 | **1133CA** | dsndBufferSetVolume 0E1450 |
| 17B996 | IDirectSoundBuffer_SetFrequency | lean_ds_SetFrequency | 2 | **113C2B** | 0E15B0, 0E1BB0 |
| 17B690 | IDirectSoundBuffer_GetStatus | lean_ds_OutCall2 | 2 | **113496** | SFXUpdate 0E19C0 |
| 17BE1B | IDirectSound_SetI3DL2Listener | lean_ds_SetI3DL2Listener | 3 | — | Not linked: no CDirectSound_SetI3DL2Listener in Action |
| 17C2A0 | DirectSoundCreateBuffer | lean_ds_DirectSoundCreateBuffer | 2 | — | Not linked. Its code is identical to DirectSoundCreateStream 1148FF, but the call target differs (CreateSoundStream), so it was rejected. |
| 17B5E0 | IDirectSoundBuffer_SetFilter | lean_ds_GetStatus | 2 | — | Not linked. Same shape as 7 Action wrappers; the call tree differs at CDirectSoundVoice_SetFilter 17B05C. |

Two of the Driving glue handler names are misleading:

- `lean_ds_GetStatus` is really **SetFilter**.
- `lean_ds_OutCall2` is really **GetStatus**.

Use the symbols, not the handler names, when building the Action table.

## 2. DSOUND entry points that Action actually calls

There are **33 entry points** called directly from outside DSOUND: 29 from game code and 4 only from the XMV movie code. XMV also uses **4 stream vtable slots**, through indirect calls.

How they are covered:

- **15 already covered:** Driving's native code already handles 15 of the 33.
- **22 new handlers needed:** 18 entry points plus the 4 vtable slots.

Other findings:

- No game code tail-jumps into DSOUND.
  - The only tail jump from `.text` is the linker's adjust-`this` thunk at 0x0EE225. It is reached only through a DSOUND vtable at 0x162694.
- No game, XMV or XPP code reads or writes a DSOUND global.

All callers are the game's sound module (0x0E0F00–0x0E1D80, plus 0x0E8CF0) and XMV routines 0x130488 and 0x130624.

| Action VA | Entry point | Args | Called from | Driving native? |
|---|---|---|---|---|
| 1148B8 | DirectSoundCreate | 3 | xboxInitSound; XMV 130624 | yes |
| 11275F | DirectSoundUseFullHRTF | 0 | xboxInitSound | **no** (no-op is enough) |
| 11338B | IDirectSound_DownloadEffectsImage | 5 | xboxInitSound | yes. Image 0x194840, 0x6168 bytes; location {reverb 0, crosstalk 1}. The output descriptor 0x2AE59C is never read. |
| 112733 | IDirectSound_Release | 1 | XMV 130624 | yes |
| 1133B2 | IDirectSound_SynchPlayback | 1 | XMV 130624 | **no** |
| 113C13 | IDirectSound_CommitDeferredSettings | 1 | SFXUpdate | **no** |
| 114334 | IDirectSound_SetOrientation (listener) | 8 | 0E1010 | **no** |
| 11437E | IDirectSound_SetPosition (listener) | 5 | 0E0FB0, 0E1D80 | **no** |
| 1143B3 | IDirectSound_SetVelocity (listener) | 5 | 0E0FF0, 0E1D80 | **no** |
| 1134FD | DirectSoundDoWork | 0 | SFXUpdate, 0E8CF0; XMV 130624 | **no** |
| 1146FE | IDirectSound_CreateSoundBuffer | 4 | 0E0F00, 0E10C0 | yes |
| 1143E8 | IDirectSoundBuffer_SetBufferData | 3 | 0E1BB0, 0E1D80 | yes |
| 1133CA | …SetVolume | 2 | 0E1450 | yes |
| 1133E6 | …SetHeadroom | 2 | 0E10C0. Sets 600 for 2D and 0 for 3D, which equals the defaults Driving's native code assumes. | **no** (trivial) |
| 113402 | …SetMixBins | 2 | 0E10C0 | yes |
| 11341E | …SetMixBinVolumes | 2 | dsndSetPan | yes |
| 11343A | …Play | 4 | SFXUpdate. Flags 0 or 1 observed. | yes |
| 11345E | …Stop | 1 | SFXUpdate | yes |
| 113476 | …SetLoopRegion | 3 | 0E1660, 0E1BB0 | yes |
| 113496 | …GetStatus | 2 | SFXUpdate | yes |
| 1134B2 | …GetCurrentPosition | 3 | 0E1860 | yes |
| 1134D2 | …SetCurrentPosition | 2 | 0E1BB0 | yes |
| 113C2B | …SetFrequency | 2 | 0E15B0, 0E1BB0 | yes |
| 113C47 | …SetMaxDistance | 3 | 0E1490 | **no** |
| 113C6B | …SetMinDistance | 3 | 0E1490 | **no** |
| 113C8F | …SetPosition | 5 | 0E1530, SFXUpdate | **no** |
| 113CC4 | …SetVelocity | 5 | 0E1570 | **no** |
| 113CF9 | …SetRolloffCurve | 4 | 0E10C0. Curve at 0x19A9A8, 5 points. | **no** |
| 113D1D | …SetI3DL2Source | 3 | 0E15E0 | **no** |
| 1148FF | DirectSoundCreateStream | 2 | XMV 130488 | **no** |
| 1134EE | IDirectSoundStream_SetVolume | 2 | **game** dsndStreamSetVolume 0E1990 | **no** |
| 1134F3 | IDirectSoundStream_SetMixBins | 2 | **game** BackgroundMovieSetupMix 0E1910 | **no** |
| 1134F8 | IDirectSoundStream_Pause | 2 | XMV 130624 | **no** |

The three stream wrappers 1134EE, 1134F3 and 1134F8 are 5-byte tail jumps to 1132E7, 113339 and 113135.

**Stream vtable 0x16264C (`.rdata`).**

- Slot order: the XDK XMediaObject order, confirmed by what each slot calls.
- Live check: past runs logged exactly this vtable ([STREAM-VTABLE] lines in nightfire-port session/analysis logs).
- Stream format: stereo Xbox ADPCM, 44,100 Hz, at most 2 packets, completion callback 0x130437.

| Slot | Method | VA | Calls | XMV call sites | Args |
|---|---|---|---|---|---|
| 0 | AddRef | 112ED7 | — | 1305FA, 13061F | 1 |
| 1 | Release | 112F1E | DSound_CRefCount_Release | 1304E9, 1303B6 | 1 |
| 2 | GetInfo | 112F6C | — | never | 2 |
| 3 | GetStatus | 11306D | CMcpxStream_GetStatus | never | 2 |
| 4 | Process | 1130BE | CMcpxStream process | 130803 (packet, output NULL) | 3 |
| 5 | **Discontinuity** | 112FD3 | CMcpxStream_Discontinuity 11958D | 13082E, 130C1E | 1 |
| 6 | **Flush** | 113020 | CMcpxStream_Flush 11967F | **never** | 1 |

XMV keeps its streams in an array at decoder+0x12C, with the count at +0x44.

**Check of `NATIVE-AUDIO-PLAN.md`.**

Every Action address it lists is right, with these points:

1. **"SetHeadroom 0x112A92"** in the ds_setheadroom row is the inner CDirectSoundVoice method. The entry point the game calls is 0x1133E6.
2. **Missing from the list** are three entry points and the vtable slots XMV uses:
   - IDirectSound_Release 112733, called from XMV.
   - DirectSoundUseFullHRTF 11275F.
   - Stream vtable slots AddRef 112ED7, Release 112F1E and Process 1130BE (the plan already names slot 5, Discontinuity 112FD3).
   - The plan names only the vtable itself. Its slots are reached by XMV through indirect calls, so they also need hooking.
3. **Movie audio on PAL is one stereo stream** in every logged run: all 1,164 logged stream creations are 2-channel. The plan's "5.1 = three stereo streams" was not seen. XMV does loop over several streams, so keep support for more than one.
4. **Count:** about 37 game-facing hooks are needed (33 entry points plus 4 slots). Plus 3 unused slots if hooked for safety, that makes 40. This is close to the plan's estimate of "≈35".

## 3. How Action's partial native audio works today

### How it hooks in

1. **Every call checkpoint.** In Action's `src/recomp/gen/recomp_types.h:764-783`, `RECOMP_ABI_CALL` calls `nightfire_thread_check(va, 0)` before every call and `nightfire_thread_check(va, 1)` after it.
   - This covers direct calls, and indirect calls too: `RECOMP_ICALL`, `RECOMP_ICALL_SAFE` and `RECOMP_ITAIL` all end in `RECOMP_ABI_CALL`.
   - Tail jumps (`sub_X(); return;`) bypass it.
   - These checkpoints can **only watch**. They cannot skip a call.
   - From the checkpoint function (`nightfire_thread_check.c:632`), these paths run:
     - `nightfire_audio_boundary(va, after)` at line 689, only when the CMake definition `NIGHTFIRE_NATIVE_VIDEO_AUDIO_HOOKS` is set (it is on in both builds).
     - `video_progress` at line 691. After 0x1148FF returns, it calls `nightfire_audio_attach` (lines 352-358 and 407-413).
     - The `audio_boundary` diagnostics at line 749. These log AUDIO-SETUP for DSOUND-internal 0x11B8FA, 0x11B692, 0x1149B5, 0x11A28E, 0x11A393, 0x11B78A and 0x11BBF6. They stop the run if device start-up at 0x11BBF6 fails. They are always on.
2. **A hand-added line in generated code.** `sub_0011343A` (Play), at `recomp_0007.c:41042`, calls `nightfire_game_audio_call()` first.
3. **The indirect-call lookup.** `recomp_lookup_manual` (`src/recomp_manual.c:61-64`) asks `nightfire_audio_lookup()` first. This affects indirect calls only.

### Game sounds (`runtime/nightfire_game_audio.h`)

This layer is switched on by these settings in the current launchers:

- `NIGHTFIRE_NATIVE_GAME_AUDIO=1`
- `…_STREAM=1`
- `…_SPATIAL=1`

It uses its own XAudio2 engine: one source voice per buffer, 16-bit.

The original DSOUND routine always runs first, except where marked "REPLACED".

| VA | What happens |
|---|---|
| 11343A Play | **REPLACED**, but only for registered, decoded buffers. The prologue sets eax to 0 and pops 20 bytes, so the guest APU voice never starts. Deferred or unsupported buffers run the original Play on the emulated APU. The after-checkpoint then calls `game_play` again; a guard makes that do nothing. |
| 114647 (inner CDirectSound_CreateSoundBuffer, below 1146FE) | Watched after the call; registers the buffer format. |
| 1143E8 SetBufferData | Copied after the call. The audio is decoded once into a private host copy; ADPCM that is not fully written yet is deferred. Because it is a snapshot, live rewrites are missed. That is why the music ring needs the 0x0E0DD0 hook. |
| 11345E Stop, 113476 SetLoopRegion, 1134D2 SetCurrentPosition, 1133CA SetVolume, 113C2B SetFrequency, 112749 Release | Copied after the call. |
| 113496 GetStatus, 1134B2 GetCurrentPosition | The original reads the APU state, then the host values **overwrite** the game's output. |
| 1133E6, 113C47, 113C6B, 113C8F, 113CF9, 11437E, 113C13 | Copied only when SPATIAL is on: headroom, distance attenuation and the listener position. |
| .text 0x0E0DD0 (the game's music producer copy) | With STREAM on, marks blocks of the music ring as ready, using the game table 0x2AE8C0. |
| *Not handled:* SetMixBins, SetMixBinVolumes (pan), buffer and listener velocity, SetOrientation, SetI3DL2Source, DownloadEffectsImage, DoWork, UseFullHRTF | These run on the original code only. As a result there is no panning, reverb or Doppler. |

### Movie audio (`runtime/nightfire_video_audio.c`)

This layer uses waveOut, with one device per stream. It is on unless `NIGHTFIRE_NATIVE_VIDEO_AUDIO=0`; the launchers set it to 1.

| VA | What happens |
|---|---|
| 1148FF CreateStream | The original runs. Afterwards `nightfire_audio_attach` registers the stream, but only if its callback is 0x130437, its vtable slots are 1/4/5 = 112F1E/1130BE/112FD3, and it is on the owner thread. |
| slot 4 Process 1130BE | **REPLACED** through the indirect-call lookup: decode, waveOutWrite, then set status to PENDING. |
| slot 5 112FD3 | **REPLACED, but treated as Flush.** It resets waveOut and completes every packet with 0x80004004. **This is the known bug:** slot 5 is Discontinuity, which should let queued audio play out. XMV calls it at the end of the data (13082E, 130C1E), so the end of movie sound can be cut off. Real Flush (slot 6) is never called. |
| slot 1 Release 112F1E | **REPLACED.** Counts down references, closes waveOut on the last release, then calls the original. |
| slot 0 AddRef | Watched: counts references. |
| 1134F8 / 113135 Pause | Watched before the call: waveOut pause or restart. Mode 2 waits for SynchPlayback. The original also runs. |
| 1133B2 SynchPlayback | Watched before the call: restarts any waiting streams. |
| 1134FD DoWork, XMV 130624 | Watched before the call: polls for finished packets, writes their size and status, and runs the XMV callback 0x130437 on the calling thread. Registers are saved, and 0x113525 is used as a fake return address. |
| *Not handled:* stream SetVolume 1134EE, SetMixBins 1134F3 | The game's movie volume and mix settings do not reach waveOut. |

### What still goes to the emulated APU

All the original DSOUND code still runs:

1. **Start-up and effects image.**
   - `DirectSoundCreate` starts up the emulated APU:
     - `main.c` always sets `RECOMP_AC97_READY=1`.
     - It calls `mcpx_apu_init_standalone`.
     - The APU registers at 0xFE800000–0xFE880000 are trapped and routed through the VEH handler.
   - The DSP start-up handshake (0x1156F8, `nightfire_dsp_startup_command`) is simulated when `NIGHTFIRE_DIAGNOSTIC_DSP=1`.
   - The AC97 reset (0x11BD1E, 0x11BF43, 0x11C003, 0x11C3B8; `nightfire_ac97_write8`) is handled by a shim.
   - `DownloadEffectsImage` loads the image into the emulated DSP. The DSP programs are stubbed, so there is no reverb or crosstalk.
2. **Every buffer and 3D call** still programs the APU voice registers.
3. **Every Play the native layer does not handle** is mixed by the emulated APU and output through the toolkit's own XAudio2 (`apu_xaudio2.c`).
4. **The XMV stream** gets a guest CMcpxStream, and its SetVolume, SetMixBins and Pause calls reach the APU. Its packets never do.
5. **DoWork** runs `CDirectSound_DoWork`.

Today, three outputs therefore play side by side: the APU's XAudio2, the game layer's XAudio2, and waveOut for movies.

### Switching them off cleanly for a full native DirectSound (one switch, off by default)

1. **Add the hook.** Add the Driving-style override to **both** Action `RECOMP_ABI_CALL` variants in `recomp_types.h`. Driving's `apply_native_audio_hook.py` will not apply here, because its OLD string does not match.
   - Put the override before `NIGHTFIRE_CALL_CHECK(va,0)` and `break`, so the watching code never sees calls that the native code handles.
   - Use an exact VA switch (Driving style: a range check, then `default: return 0`).
   - It also catches the vtable slots: indirect calls reach `RECOMP_ABI_CALL(_va,…)` with a run-time VA, after `recomp_lookup_manual`.
2. **When the switch is on, force the old layers off in code.** Do not rely on environment variables: the launchers force `NIGHTFIRE_NATIVE_VIDEO_AUDIO=1`.
   - Make `game_enabled()` return false. This switches off the Play prologue, every copied call and the 0x0E0DD0 ring hook.
   - Make `nightfire_audio_attach()` return early. `nightfire_audio_lookup()` and `nightfire_audio_boundary()` then find no streams and do nothing.
   - The prologue in `sub_0011343A` then never runs at all, because the call-site hook comes first.
3. **Do not start the chip** (Driving `LEAN_AUDIO_NO_CHIP` style):
   - Skip `mcpx_apu_init_standalone` and the `RECOMP_AC97_READY` trap.
   - With `DirectSoundCreate` handled natively, none of these DSOUND internals run: the DSP handshake, the AC97 shims, the 0x12FC48 table and the AUDIO-SETUP stop.
   - Call `xa2_init()` from the native start-up, as Driving's `nat_init` does.
   - The only APU access outside DSOUND is XPP 0x15BDE0, which reads XGSCNT 0xFE80200C. It is apparently a USB audio-device clock sync, so it should only run when such a device is attached. With the trap off it reads plain memory.

## 4. Globals

**The game never touches DSOUND globals.** The DSOUND globals block has the same layout in both games, mostly offset by 0x53D58.

| Action | Driving | Meaning |
|---|---|---|
| 0x12FB98 | 0x1838F0 | CDirectSound singleton pointer. Only DoWork (0x1134FD) reads it from outside the library's own code. Under native `DirectSoundCreate` it stays 0, so DoWork must be native. |
| 0x12F50C | 0x183264 | Library "unusable" flag. Every C++ method returns 0x80004005 when it is set. |
| 0x12F518 | 0x183270 | DSOUND critical section. Imports 0x15D220 and 0x15D21C (Driving 0x189C40 and 0x189C3C). |
| 0x12FD08 | (0x183A60 by layout) | DSP command block. Start-up command at +0x810. |
| 0x12FC48 | 0x1839A0 | Contiguous-memory page table (16 × 16 bytes). |
| 0x12F4BC | 0x183214 | AC97 channel table. |

Game-side globals:

| Action | Driving | Meaning |
|---|---|---|
| 0x2AE598 | 0x244C84 | The game's IDirectSound pointer, written by `DirectSoundCreate`. |
| 0x2AE5A8 | — | 64 slots × 3 buffers, 192 in all. Each slot has:<br>• +0: a 2D mono buffer<br>• +0x100: a 2D stereo buffer<br>• +0x200: a 3D mono buffer<br>All are Xbox ADPCM at 0xAC00 = 44,032 Hz.<br>The two 2D buffers use mixbins {0,1,2=−10000,3,4,5}.<br>The 3D buffer uses mixbins {6,8,7,9,2,10}, where 10 is the I3DL2 send. |
| 0x2AE8C0 | — | 64-byte table for each slot. The music ring's source is at +40 and its byte count at +44. |
| 0x2AF8C4 | — | 101-entry volume table, in mB. |

## 5. Key risks

1. **Look-alike wrappers.** Seven Action buffer wrappers, and Driving's SetFilter, are byte-identical apart from the method they call. Only the call-tree check tells them apart. The handler names in Driving's glue are also wrong for two entries (see section 1).
2. **Movie streams are reached through a vtable.**
   - The native stream object must start with a guest vtable pointer. Using 0x16264C is simplest, because its slots are then hooked VAs.
   - The completion callback 0x130437 is guest code. It must run on the game thread, from DoWork or the XMV poll, never on the mixer thread. The guest registers (`g_eax`, `g_esp`, …) are per-thread.
   - Discontinuity must let queued audio play out (the current bug).
3. **New work beyond Driving:** 22 handlers.
   - 3D, plus a deferred commit of the listener and buffers. The plan's `ds_3d` covers this.
   - I3DL2 send on bin 10. Reverb is phase 2.
   - Streams, SynchPlayback, Pause modes 0–3, and DoWork.
   - Simple no-op handlers are enough for UseFullHRTF and SetHeadroom (600/0).
4. **Music is rewritten in place.** The game writes music into a looping ADPCM buffer while it plays (0x0E0DD0 and 0x0E1BB0). Driving's mixer reads guest memory as it mixes, so it follows these live rewrites. The existing snapshot layer does not.
   - GetCurrentPosition (0x0E1860) and GetStatus (SFXUpdate, which uses it to manage voices) must follow the XDK cursor rules.
5. **Turning off the chip.** Remove the APU trap as well. Otherwise any stray APU access faults, because `main.c`'s handler only routes it when `g_apu_state` is set.
6. **Formats and counts are within Driving's limits.** There are 192 buffers plus 1–3 streams, against 256 voices. Sounds are 44,032 Hz buffers and 44,100 Hz streams, up to 6 mixbins each.
