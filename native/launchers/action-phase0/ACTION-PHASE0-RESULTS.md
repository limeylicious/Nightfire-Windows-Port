# Native graphics, Action engine: phase 0 results (2026-10-08)

This was a static check only. No build was made, no game was run, and nothing in
`nightfire-port` or `nightfire-driving-native` was changed. Scripts and outputs are in
`native-driving/action-phase0/`.

## Inputs (verified)

| File | SHA-256 |
|---|---|
| `nightfire-analysis/game_files/default.xbe` (same as `nightfire-port/game_files/default.xbe`) | `b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1`, the expected PAL hash |
| `nightfire-analysis/game_files/Driving.xbe` (same as `nightfire-port/game_files/Driving.xbe`) | `0f4c50e4f84b1edeedab4667933b36c9c228da463cc89180e4c70cf3a88bfcef` |

- Generated C: `nightfire-port/src/recomp/gen/*.c` (Action) and `nightfire-driving-native/src/recomp/gen` (Driving).
- Disassembler: capstone 5.0.6 from `nightfire-port/analysis/python-deps`, which was already in the project. Nothing was installed.
- Action D3D section: 0x100480. Code and constants run to 0x10EB70; the library globals (uninitialised data) follow, to 0x111FB8.

## Method

1. Each Driving D3D/XGRAPHC routine was disassembled. Its extent comes from the generated-C `Original:` header.
2. The comparison ignores what moves between the two games: every 32-bit displacement or immediate that falls inside the image, and every call or jump target. Branches inside a routine stay as offsets into that routine.
3. Each Driving routine was then compared with every Action library routine.
   - Routines the Action generator never emitted were found by searching the raw D3D bytes, with wildcards in place of the ignored fields.
4. When several Action routines had identical code, the match was decided by:
   - library order (the linker keeps object order);
   - whether the routines it calls had already been matched consistently.
   - Matches whose call targets contradicted other matches were rejected.
5. Global addresses come from lining up the operands of exactly matched routine pairs. There are 1,721 Driving→Action address pairs with **0 conflicts**.

Scripts:
- `d3d_action_map.py`: matching.
- `d3d_action_globals.py`: every address used in the `nd3d*` sources.
- `d3d_action_survey.py`: entry points, hardware reach, put writers, game access to globals, XMV.
- `d3d_action_export.py`: writes the JSON.

They are run in that order. Intermediate output is in `work-*.json`.

## 1. The 91 replaced Driving routines in default.xbe

| Result | Count |
|---|---|
| Exact match with addresses ignored, same length | **80** |
| Near match | **0** |
| Not in default.xbe | **11** |

- Of the 80 matches, 46 also carry a research label, and every one of those labels agrees with the Driving name (0 disagreements). The other 34 have no label.
- All 80 have a generated `sub_` in nightfire-port:
  - 65 are in `recomp_00*.c`.
  - 15 render-state setters (EdgeAntiAlias, ShadowFunc, FrontFace, LineWidth, LogicOp, FillMode, BackFillMode, TwoSidedLighting, StencilEnable, StencilFail, OcclusionCullEnable, StencilCullEnable, MultiSampleAntiAlias, MultiSampleMask, SampleAlpha) are in `nightfire_render_states.c`. They are reached only through the render-state table near 0x10D970, via `nightfire_render_state_dispatch.c`.
- OcclusionCullEnable and StencilCullEnable have identical code apart from the state slot. They were assigned by library order, and their operands confirm the assignment (slot 0x1758AC→0x111C54 first).

**The 11 not linked into default.xbe.** Action never uses these, so none are needed. Each was searched for with a masked 5- and 8-instruction prefix, and only generic hits came back.

| Driving VA | Routine |
|---|---|
| 0x166330 | CopyRects blit helper |
| 0x1663E0 | CopyRects |
| 0x1669E0 | SetPalette |
| 0x165FD0 | GetVisibilityTestResult |
| 0x166B40 | BeginVisibilityTest |
| 0x166BE0 | EndVisibilityTest |
| 0x16B160 | SetPixelShaderConstant |
| 0x16B4C0 | D3DVertexBuffer_Lock2 |
| 0x16CE90 | vertex-buffer wait |
| 0x16B9D0 | SetVertexDataColor |
| 0x16BAA0 | **RunPushBuffer** |

Because RunPushBuffer is not linked, Action cannot run precompiled push buffers at all.

### Exact matches (Driving → Action)

**Basic device calls**

| Routine | Driving → Action |
|---|---|
| SetViewport | 1666D0→103D50 |
| SetTexture | 166830→103EB0 |
| IsBusy | 166B00→1040F0 |
| SetScissors | 166D10→1041F0 |
| SetScreenSpaceOffset | 167030→104390 |
| BlockUntilVerticalBlank | 166030→103AA0 |
| SetTileNoWait | 166C40→104130 |

**Render-state setters**

| Routine | Driving → Action |
|---|---|
| SetRenderState_Simple | 1673E0→100580 |
| EdgeAntiAlias | 1676E0→1008E0 |
| ShadowFunc | 167720→100920 |
| FogColor | 167760→100960 |
| CullMode | 1677B0→1009B0 |
| FrontFace | 167820→100A20 |
| NormalizeNormals | 167860→100A60 |
| TextureFactor | 1678A0→100AA0 |
| LineWidth | 167900→100B00 |
| Dxt1NoiseEnable | 167970→100B70 |
| LogicOp | 167A70→100C70 |
| FillMode | 167AD0→100CD0 |
| BackFillMode | 167B20→100D20 |
| TwoSidedLighting | 167B80→100D80 |
| VertexBlend | 167BF0→100DF0 |
| ZEnable | 1687F0→101A90 |
| StencilEnable | 168880→101B20 |
| StencilFail | 168910→101BB0 |
| YuvEnable | 168980→101C20 |
| OcclusionCullEnable | 1689B0→101C50 |
| StencilCullEnable | 168A20→101CC0 |
| MultiSampleAntiAlias | 168B70→101E10 |
| MultiSampleMask | 168BF0→101E90 |
| SampleAlpha | 168C40→101EE0 |

**Texture-stage setters**

| Routine | Driving → Action |
|---|---|
| TexCoordIndex | 167C40→100E40 |
| BumpEnv | 167D50→100F50 |
| BorderColor | 167DC0→100FC0 |
| ColorKeyColor | 167E00→101000 |

**Render targets and clear**

| Routine | Driving → Action |
|---|---|
| CommonSetDebugRegisters | 1681D0→1013D0 |
| CommonSetRenderTarget | 1682A0→1014A0 |
| Clear | 168C90→1043E0 |

**Shaders**

| Routine | Driving → Action |
|---|---|
| unnamed | 169790→102F10 |
| unnamed | 1698B0→103030 |
| unnamed | 169C80→103400 |
| unnamed | 169E60→1035E0 |
| SetVertexShaderConstant1 | 16A790→102570 |
| SetVertexShaderConstant4 | 16A7F0→1025D0 |
| SetVertexShaderConstantNotInlineFast | 16A8A0→102680 |
| LoadVertexShader | 16AA50→102830 |
| SelectVertexShader | 16AAB0→102890 |
| SetShaderConstantMode | 16AB30→102910 |
| unnamed | 16AC30→102A10 |
| SetVertexShader | 16AD90→102B50 |
| SetPixelShader | 16AF60→10A030 |

**Drawing**

| Routine | Driving → Action |
|---|---|
| DrawVertices | 16B620→1049C0 |
| DrawIndexedVertices | 16B6C0→104A60 |
| SetVertexData2f | 16B930→104CD0 |
| SetVertexData4f | 16B970→104D10 |
| Begin | 16BA20→104D70 |
| End | 16BA60→104DB0 |

**Kick-off, fences and waits**

| Routine | Driving → Action |
|---|---|
| CDevice_KickOff | 16C940→1064B0 |
| SetFence | 16CA30→1065A0 |
| BlockOnTime | 16CAE0→106650 |
| MakeRequestedSpace | 16CC20→106790 |
| MakeSpace | 16CDA0→106910 |
| unnamed | 16CDB0→106920 |
| KickOffAndWaitForIdle | 16CDF0→106960 |
| BlockOnResource | 16CE10→106980 |

**Device set-up and present**

| Routine | Driving → Action |
|---|---|
| unnamed | 16CED0→105190 |
| unnamed | 16CFC0→105280 |
| InitializeFrameBuffers | 16D2A0→105560 |
| CDevice_Init | 16D800→105AC0 |
| CMiniport_IsFlipPending | 16EC10→10AFC0 |

**Lazy state flush**

| Routine | Driving → Action |
|---|---|
| LazySetPointParams | 170320→108950 |
| unnamed | 170490→108AC0 |
| unnamed | 1705F0→108C20 |
| unnamed | 1707E0→108E10 |
| unnamed | 1709C0→108FF0 |
| unnamed | 170E80→1094B0 |
| unnamed | 171160→109790 |
| unnamed | 1714A0→109AD0 |
| SetStateVB | 171720→109D50 |
| unnamed | 173A00→10D2B0 |

The full list is in `action-address-map.json` → `replace_set`.

- **Whole library:** 244 of the 310 Driving D3D and XGRAPHC routines have an exact counterpart.
- **One rejected match:** 0x167320, the Driving GetLevelDesc tail-jump stub, matched code whose jump target contradicted the other matches. It is not in the replace set.

## 2. Globals map (Driving → default.xbe)

Every value comes from aligned operands of matched code. The 4 PS-pack tables are the exception: they were checked byte by byte.

| Driving | Action | What |
|---|---|---|
| 0x175418 | **0x1117C0** | D3D__pDevice |
| 0x175424 | **0x1117CC** | dirty-flag word |
| 0x175428 | **0x1117D0** | deferred texture-stage state |
| 0x175628 | **0x1119D0** | g_RenderState |
| 0x175798 | **0x111B40** | g_DeferredRenderState |
| 0x178500 | **0x111C68** | g_Stream[] |
| 0x1758D0 | **0x10EB80** | static CDevice object, the target of pDevice |
| 0x176528 | 0x10F7D8 | device+0xC58 vertex-shader constant shadow |
| 0x176408 | 0x10F6B8 | per-stage table |
| 0x1785C0 / 0x1785C4 | 0x111D28 / 0x111D2C | state table cleared at init |
| 0x1786D8 | 0x111E40 | FVF vertex-shader object |
| 0x18E888 | 0x161EC8 | render-state method table (.rdata, bytes identical) |
| 0x189C04 | 0x15D1DC | import MmAllocateContiguousMemoryEx |
| 0x189DE8 / 0x189DEC / 0x189EB0 / 0x18B5DC | 0x15D334 / 0x15D2E8 / 0x15D320 / 0x15D394 | 1.0f / 0.0f / 0.5f / 8.0f (values checked) |
| 0x189E00, 0x189ED4, 0x189FCC, 0x18A4A8, 0x192C04 | 0x15D34C, 0x15D348, 0x15D370, 0x15D3F0, 0x161A24 | 2.0, 4.0, 1/255, 2^32, 256.0 |
| 0x1A1E60-0x1A1E7C | 0x1625E0-0x1625FC | depth-scale constants (65535, 16777215, …) |
| 0x173FE0 | 0x10DD98 | 0.53125f, in the D3D section |
| 0x1D3558 / 0x1D3568 | 0x1D5BCC / 0x1D5BDC | texture-stage slot remap bytes (.data) |
| 0x1D3580 / 0x1D3590 / 0x1D35A0 | 0x1D5C00 / 0x1D5C10 / 0x1D5C20 | PS-pack {1}, {255}, nibble table. Not needed: SetPixelShaderConstant is not linked. |

**The global layout is not one constant shift.** It moves in three blocks:

| Driving range | Shift | Contents |
|---|---|---|
| 0x175418-0x1758BC | -0x63C58 | pDevice, dirty word, texture state, render states |
| 0x1758D0-0x176528+ | -0x66D50 | device object and its shadow arrays |
| 0x1785xx-0x1786D8 | -0x66898 | g_Stream and the FVF object |

Each address therefore has to be mapped individually, never by a base offset.

- 348 absolute values appear in the `nd3d*` sources. Each one's Action equivalent and how it was found are in `action-address-map.json` → `all_native_source_addresses`:
  - 0x18000, 0x20000 and similar are constants, not addresses.
  - Code labels inside matched routines are mapped by their offset.
- 6 slots inside g_RenderState were found by shift between direct anchors on both sides: 0x17547C, 0x175500, 0x175650, 0x175670, 0x1756D4 and 0x1756D8.
- Unmapped, and none of them needed for Action:
  - 0x0F4340 (game wrapper for RunPushBuffer) and 0x0F43A0 (game wrapper for SetPixelShaderConstant);
  - 0x242CC0;
  - the helpers used only inside the 11 absent routines: 0x1661B0, 0x166B70 and 0x16E020.

## 3. Action D3D entry points: 43, against 89 in Driving

- **Called from game code (`.text`):** 38 routines.
- **Called from XMV:** 4 routines (next section).
- **Reached only through a data table:** LineWidth 0x100B00.
  - It is the one table-only routine counted as an entry point, because a stray dword at 0x124FFC, inside DSOUND, happens to point to it.
  - The other 14 table setters have no external caller.
- **Called by a tail jump that bypasses `RECOMP_ABI_CALL`:** 0x103A90 KickOffAndWaitForIdle, from 0x0E5FC0. Driving had the same with End, so hooking at the routine definitions (the rename to `orig_sub_X`) is still required.

**Hardware routines left uncovered once the 80 mapped routines are native:**

| Action VA | Routine | Why | Callers |
|---|---|---|---|
| **0x1019F0** | D3DDevice_SetDepthClipPlanes | MakeSpace (push-buffer write) | game 0x0E3B7C |
| **0x104860** | D3DDevice_DrawVerticesUP | MakeSpace and inline command headers | game, 2 call sites (0x0E4C41, …) |
| **0x109EF0** | CDevice_SetStateUP | MakeSpace (called by DrawVerticesUP) | 0x104860 |
| **0x103CA0** | unnamed, a raster-status read | reads chip register dev+0x4FC → +0x600808 (CRTC raster) through a pointer, so the MMIO pattern scan missed it | **XMV** 0x1309D1 |
| 0x10B820 (Driving 0x16F470) | chip MMIO during Direct3D_CreateDevice (0x1004A0→0x105FF0→0x1086EF) | mmio | Same status as in Driving, where it stays recompiled |
| 0x107DEA (Driving 0x16F7BA) | MMIO under CMiniport::InitHardware 0x1082DD | mmio | Driving's native CDevice_Init calls InitHardware the same way |

- The only routines that write the push buffer and have no Driving counterpart are **SetDepthClipPlanes, DrawVerticesUP and SetStateUP**.
  - 56 Action library routines write the put pointer. All the others fall inside the mapped native set.
- Two CPU-only entry points have no Driving counterpart: **GetDisplayMode 0x103B80** (XMV) and **GetRenderTarget2 0x103D10** (game). Neither needs a native version, but both read device or miniport fields that native set-up must keep filled.

## 4. Game code and the D3D globals

- No game or XMV routine writes the push buffer inline. The check looked for a load of pDevice followed by a store through the put pointer.
- As in Driving, game code writes the library state directly: 59 addresses, written by 13 routines and read by 9.
  - The routines include `d3dSetTexture` 0x0E3C10, `d3dSetDeferredTextureState` 0x0E5230, `maybeResetRenderState` 0x0E6550 and `d3dSetup` 0x0E6860.
  - The addresses are the dirty word 0x1117CC, texture-stage state 0x1117D0-0x11199C, g_RenderState slots 0x111AB4-0x111AF8, and deferred render state 0x111B40-0x111BAC.
- The Driving state rule therefore carries over unchanged: keep the guest arrays and read them, with the dirty flags, at every draw.

## 5. XMV movie player

XMV's only D3D calls are:

| Routine | Calls |
|---|---|
| GetDisplayMode 0x103B80 | 5 calls, from `maybeCreateVideoDecoder` |
| raster-status read 0x103CA0 | 1 call |
| D3DSurface_GetDesc 0x10BB00 | 1 call |
| D3DSurface_LockRect 0x10BB20 | 1 call. It reaches the GPU wait through BlockOnResource, which is mapped. |

- XMV writes no push buffer and calls no draw routine.
- It apparently decodes into a locked surface, and game code draws that surface. This is an inference and was not traced in a run.
- The raster-status read (0x103CA0) needs a native or stub version once the hardware stand-ins are off. Movie pacing may depend on it.

## 6. How the Action runtime consumes graphics today

- `runtime/nv2a_pb_exec.c` (3,602 lines) walks the push buffer the recompiled library writes. It decodes NV2A methods: surfaces, clears and a partial or heuristic vertex path. It runs with `RECOMP_PB_EXEC`.
- `runtime/nightfire_hardware.c` (1,679 lines) rasterises those decoded triangles optionally through D3D11 and syncs render targets back to guest RAM.
- `runtime/renderer/CMakeLists.txt` only builds these, together with `nv2a_pb_scan.c`, as the object library `nightfire_gpu_runtime`, plus test programs.
- This is the push-buffer decoding layer that native graphics would retire, as in Driving.

## Biggest risks

1. **Hard-coded Driving addresses.**
   - The native sources contain 348 Driving addresses, as `#define`s, literals, `sub_XXXXXXXX` symbols and return-address labels.
   - Sharing one library means turning them into a per-game table. The global layout moves in three blocks, so one base offset will not work.
   - `nd3d_unported()` also hashes `va - 0x160000`; the Action library starts at 0x100480.
2. **Routines that only Action needs:**
   - DrawVerticesUP with SetStateUP: user-pointer vertex data written straight into the push buffer;
   - SetDepthClipPlanes;
   - the raster-status register read 0x103CA0 used by XMV.
3. **The 15 table-dispatched render-state setters** live outside the main generated files (`nightfire_render_states.c` and the dispatch table). The definition rename has to cover those files and that table too.
4. **Run coverage.** None of this has been exercised in Action yet; Action has far more content than Driving. Matches are exact at the machine-code level, but globals are only proven for the code paths that were compared.
5. **Device creation and InitHardware MMIO** (0x10B820, 0x107DEA): Action works the same way as Driving here, so the hardware-off switches (no register-poll thread) need the same care as Driving phase 4.
