# Release folders

## `alpha/`: first alpha test (2026-10-10)

The launchers and set-up of the owner's first alpha test, copied byte for byte
from the owner's `releases\Nightfire-PC-Alpha` folder (group `release-alpha` in
the root `SOURCE-MANIFEST.json`).

**It contains no game and no engines.** The two engines are made from the
game's own code, so they never go in this repository. This alpha's
`Setup.cmd` (`engine/setup.py`) copies engines that were built locally from
[`../native/`](../native/README.md), links the player's own extracted PAL game
files and checks them by hash. Its `README.txt` is written for the person
running that set-up copy.

| File | What it does |
| --- | --- |
| `Setup.cmd` | One-time set-up: copies the locally built engines, links the game files, checks the PAL hashes |
| `Play Nightfire.cmd` | The whole game, Action and Driving linked in one window, with the PC settings |
| `Play Nightfire (Fullscreen).cmd` | The same, borderless fullscreen |
| `Split-screen Multiplayer.cmd` | Starts at the menus, set up for up to four players on one PC |
| `Debug/` | Full logging, the F9 picture recorder, Smooth Motion off, two windows, skipping the opening drive, split-screen variants and the lockstep record/replay test |
| `engine/launcher/nightfire.py` | The launcher behind all of them (`--mode play` or `--mode debug`) |
| `engine/driving/` | Driving's run scripts and its frozen start settings |
| `engine/action/start-action.cmd` | Action's start settings |

Play mode keeps the crash and freeze reports but leaves out the sound
recording, frame counter and picture dumps; debug mode keeps everything.

A public alpha needs one more step first: making the code generation and its
repairs repeatable, so that Setup can build both engines from each player's
own disc. The alpha's Setup would then use Microsoft's free C++ Build Tools, and
a later beta would bundle a portable compiler.
