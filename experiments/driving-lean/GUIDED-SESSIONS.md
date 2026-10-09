# Guided play sessions (2026-10-05)

Double-click `play-lean-paris-guided.cmd` or `play-lean-vehicle-guided.cmd`. These run the same as the `-smooth` scripts: in-between frames, vsync, gamma 0.8 and the FPS counter. Each session's capture is saved to a new folder.

- **Mark key: F9.** Press it in the game window when something looks or sounds wrong. You hear a short Windows chime and the title shows `MARK n SAVED` for 2 seconds. The game picture and a state snapshot are saved.
- When the game closes, the console prints the session summary.
- The normal `play-lean-*.cmd` scripts are unchanged.

## Session folder: `sessions/<YYYYMMDD-HHMMSS>-<scene>/`

| File | Contents |
|---|---|
| `summary.txt` | Duration and exit reason (closed normally, crash, freeze, time limit, or killed/unexpected exit). Also: FPS min/avg overall and during gameplay, freeze-monitor stalls, number of marks, audio totals. |
| `game.log` | Full game log. |
| `settings.json` | Every LEAN_/DRIVING_/RECOMP_ setting in effect. |
| `build.txt` | Exe path, build time, SHA-256, size. |
| `system.txt` | GPU and driver, resolution and refresh rate, CPU and current clock, RAM, OS, AC/battery, whether Ghidra/java is running. |
| `stats.csv` | Once a second: presented FPS, game FPS, audio peak/rms/non-silent, XAudio2 queued/dropped/glitches. |
| `events.txt` | Start, marks, freezes, crash, with times. |
| `marks.txt`, `mark-NN.bmp` | Per F9 press: time, game frame, FPS, audio levels and APU state, plus the game picture. |
| `crash.txt`, `crash.dmp` | Unhandled exception: code, address and function, guest registers, the call chain (`sub_XXXXXXXX` = guest function), the last 200 log lines, and a minidump of about 0.4 MB. |
| `abort.txt`, `abort.dmp` | The same for abort()/assert. |
| `freeze.txt`, `freeze-N.dmp` | The freeze monitor's per-thread call chains and lock/IRQL/APU/DPC state, plus a minidump (at most 2). |

Only the newest 5 sessions keep their `.dmp` files.

## Pieces

- **In the exe** (`runtime/lean/lean_session.c`, `LEAN_SESSION_CAPTURE=1` and `LEAN_SESSION_DIR`): the stats sampler, unhandled-exception and SIGABRT handlers, freeze minidumps, and F9 marks.
- **Wrapper** (`run-lean-guided.py`): system and build info, collecting the logs, exit reason and summary, and dump retention.
- **Freeze monitor** (`LEAN_HANG_DUMP=1`): fires after 4 s without a game frame, once the first 200 frames have been shown. Minimising or losing focus doesn't stop game frames, so it doesn't fire then.
- **Test-only switches:** `LEAN_TEST_CRASH=<s>`, `LEAN_TEST_FREEZE=<s>`, `LEAN_TEST_ABORT=<s>`.

## Verified

| Test | Session | Exit reason | Files |
|---|---|---|---|
| Window closed + 2 injected F9 presses | `20261005-144658-vehicle` | closed normally | 2 marks with pictures |
| Forced crash at 40 s | `20261005-144817-vehicle` | crash: access violation at `lean_session_test_tick` | crash.txt (call chain down to `sub_0005A1B0`) + crash.dmp (367 KB) |
| Forced freeze at 40 s | `20261005-144901-vehicle` | freeze (then stopped by the time limit) | freeze.txt shows the stuck game thread + freeze-1.dmp |
| Forced abort at 40 s | `20261005-145041-vehicle` | crash: abort() | abort.txt + abort.dmp |

**Cost:** none measurable. Car level over 120 s, 5 s averages:

| | Presented FPS | Game FPS |
|---|---|---|
| Guided | 89.1–89.9 | 48.3–50.1 |
| Plain smooth | 89.2–89.9 | 48.9–50.1 |
