# Project status through checkpoint 510

Updated October 4, 2026. This is a reviewed summary of local evidence, not a game
release. Game assets, private logs, frame captures, translated game output and
profiling dumps are not included in this repository.

## Current references

| Area | Recorded status |
| --- | --- |
| Action single-player | Preserved working reference; The Exchange has been completed in user testing. Full-game correctness is not established. |
| Action local multiplayer | Frozen checkpoint241. Original-menu Skyrail rounds with bots reached normal debriefing in guided playtests. Online networking has not been implemented. |
| Normal Driving | Frozen checkpoint321 remains preserved. |
| Cumulative Driving baseline |491 is the last verified cumulative non-OpenGL game build. Paris and a vehicle scene render, but playable performance remains unfinished. |
| Experimental residency508 | Default OFF and unpromoted. It keeps render targets resident across drains under the documented experimental access contract. |
| Latest completed investigation |510: tutorial-input attribution, a stall locator and a NO-GO promotion decision. No renderer repair or FPS improvement is claimed for510. |

## What the newer source contains

The separate experimental source directory includes accumulated bridge work for
font/geometry submission, target publication, memory-region checks, D3D11 copies,
readback and conversion, plus the latest residency guards and diagnostics.
Checkpoint numbers in file names describe provenance, not independently enabled
features or proof that every contained experiment is successful. Older comments
may describe the state when a file was first introduced; this status document
records the latest bounded evidence.

Previous copy/submission approaches did not consistently improve measured FPS.
Residency experiments have shown promising historical timings, but their
acceptance work remains separate from performance claims. The current510
diagnostic runs are instrumented and naturally time-dependent; they are not
fresh FPS benchmarks. No OpenGL integration is being promoted.

## Checkpoint510: confirmed text failure

Six planned360-second Paris runs completed: three clean-profile A runs and three
B508 runs, using the same executable, with the text recorder enabled and stall
sampling disabled. There were7,426,631 records,202 exact captured-image joins,
and no recorded ring loss. The images were inspected locally.

Some dark firing prompts follow the game's normal changing input alpha. A
different failure was confirmed: fully opaque gold tutorial lettering can vanish
when a command drain splits its font stream. A shadow may remain without its
foreground. The actual linked generic fallback lacks the immediate glyph methods
used by that stream. An independent test using the unchanged production source
reproduced the loss on the captured whole/split command stream.

The failure occurs in both A and B508. It therefore does not prove a new508
residency regression. It does satisfy the agreed bright-input/dark-output failure
rule: **NO-GO for promoting508 until the shared defect is repaired and rechecked**.

One recording limitation was found: generic hooks were placed in an unlinked
inspection executor. Missing events are not proof of no execution. The separate
production-source test establishes this specific defect, not complete live-path
coverage. Historical pole occlusion/order remains unresolved.

## Checkpoint510: stall findings

Twenty planned360-second stationary runs completed:10 per profile, split equally
between vehicle and Paris, with the stall locator enabled and text recording
disabled. Sampling began after a3-second gap without a present.

| Observation | Actual result |
| --- | ---: |
| Sampled gaps of20 seconds or longer |0 |
| Total thread snapshots |1,872 |
| Distinct shorter sampled intervals |16, all in one clean-A Paris run |
| Largest sampled gap |6.9279079 seconds |
| Runs without snapshots |19 |
| Recorded context/resume/copy errors |0 |
| Truncated stack traces |1,680 |

The requested20-second freeze was **not reproduced within the fixed budget**.
Most sampled main-thread locations were native waits with unresolved AMD driver
callers. The responsible renderer operation and cause remain unproved. No508-
specific stall was established; no stall was fixed. A zero-snapshot run does not
prove every possible stall is absent. Long30-frame timing blocks are not single
continuous freezes. Sampling and output can alter timing.

## Validation and preservation

The isolated optimized build and focused Windows recorder/locator, parser,
material-observer and linked-fallback checks passed their stated checks. Every
game run ended at its planned diagnostic watchdog; that is not mission completion.
Selected original frames from every stall run were inspected. Vehicle car/road
and Paris scenery were present; extra intermittent dark text was recorded in
both profiles without assigning input-level causes from those images alone.

Final local preservation verification covered12,782 unique files, including253
protected entries. Normal runtime, frozen builds, original PAL inputs and the
pinned toolkit were preserved. This GitHub update changes only the export tree.

## Next target

Repair the shared partial-drain font loss while preserving exactly-once command
consumption, partial vertex/quad state, rendering state, failures and original
guest completion/publication boundaries. Validate the actual linked path, then
compare split/unsplit GPU results and inspect natural-timing A/B captures before
reconsidering508 promotion. A fresh CPU profile can follow a promoted baseline.

No speculative frame-time improvement, high-refresh physics change, networking,
full-game completion or playable Driving certification is implied by this export.
