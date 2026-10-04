# Lean renderer – Task 1 report (2026-10-04)

## Source composition (how the perf candidates are built)
- perf508/build508.py: normal `nightfire-driving/src` + `runtime` copy, then
  `analysis/perf445/candidate-source/CMakeLists.txt` (last CMake change in the 491 chain),
  then the 289 files in `analysis/perf508/effective491-source.json` (the cumulative 491
  layer chain: vehicle443/repair, perf445..469 overlays, recovery491), then that checkpoint's
  own default-OFF overlay. 509/510/511 copy the previous build-source and add one overlay each.
- 491 layer list: `analysis/recovery491/build-result.json` ("layers").
- Clean comparison profile: `analysis/perf501/config501.py` – scene env from
  `analysis/runs/20260927-174445-vehicle439-cpu-profile` (vehicle) and
  `20260927-174835-vehicle439-paris-regression` (Paris), with 39 experimental/observer flags = 0,
  plus REGION_CACHE445=1, CPU448=1, COLOR_SEED276=1, RESOLVE459=1, ASYNC_TAIL295=0.

## Lean copy
- `nightfire-driving-lean/` = effective491 only (no 508–511 diagnostic overlays, no PC508 define).
  378 files; `lean-source-manifest.json` lists origin and SHA-256 for each one. All 289 effective491
  hashes and the perf445 CMake hash were verified before copying.
- Configured with `-DDRIVING_FAST_BOOTSTRAP_BUILD=OFF -DDRIVING_REGION_CACHE445=ON` into
  `build/`. 50 of 50 generated units are MaxSpeed (/O2). Toolkit at 051a128, clean.
- Build: success, 375 s (2 workers), 85 warning lines, 0 errors.
- Runner: `run-lean-scene.py vehicle|paris` (outputs go to `runs/`).

## Runs (120 s watchdog, no inputs; exit 3 = watchdog)
| scene | presents reached | late ms/present (last 30-frame windows) | 491/perf501-off same frames |
|---|---|---|---|
| vehicle | 390 | 425, 410 (frames 330-390, in level) | ~400-470 (frames 390-570) |
| Paris | 180 | 386-426 (frames 60-180) | 403-515 (frames 30-180) |

BATCH236-TIME at the end of the run:
- Vehicle: draws 31,797 in 3,492 flushes; queue_cpu 6.4 s, flush_backend 15.0 s, publish 1.4 s.
- Paris: draws 55,774 in 6,731 flushes; fallback 1,440; queue_cpu 14.7 s, flush_backend 27.8 s,
  publish 2.9 s.

Captures: the vehicle frame 240 capture is the snowy-road intro with the car, which looks
correct. No capture after frame 240 was made because the 120 s run ended at frame 390.
The Paris frame 120 capture shows the street, cars, truck and the scope/HUD, and looks
correct. Same as the 491 reference, about 2.4 FPS.

## Renderer entry points linked
Driving (lean = 491):
- `runtime/driving_gpu143.c` → xbox_kernel (GPU/MMIO/pushbuffer front end).
- `runtime/driving_pb_exec231.c` → xbox_kernel; a 19-line wrapper that `#include`s
  `runtime/generic263/nv2a_pb_exec263.inc` (1,765 lines, the pinned toolkit executor plus
  local policies). The toolkit's nv2a_pb_exec.c and nv2a_pb_scan.c are filtered out.
- `runtime/gpu144/nightfire_hardware.c` (2,586 lines) → exe, /O2 /fp:strict, D3D11 backend.
- `runtime/gpu144/nv2a_pb_exec.c` is not linked. It is only in the EXCLUDE_FROM_ALL target
  `driving_hardware_foundation144`.

Action (nightfire-port):
- `runtime/renderer/CMakeLists.txt` builds the OBJECT library `nightfire_gpu_runtime` from
  `runtime/nv2a_pb_exec.c` (3,602 lines), `runtime/nv2a_pb_scan.c` and
  `runtime/nightfire_hardware.c` (1,674 lines), linked into xbox_kernel. It is optimized
  even in Debug builds.
