# Release folders

## `alpha/`: first alpha test (2026-10-10)

The launchers and set-up of the owner's first alpha test, copied byte for byte
from the owner's `releases\Nightfire-PC-Alpha` folder (group `release-alpha` in
the root `SOURCE-MANIFEST.json`).

**It contains no game and no engines.** The two engines are made from the
game's own code, so they never go in this repository. In a copy of this
repository, `Setup.cmd` (`engine/setup.py`) first builds both engines on the
player's PC from their own extracted PAL game files
([`../native/setup/`](../native/setup/README.md)), then copies them into
`engine\`, links the game files and checks them by hash. In the owner's
workshop it copies the engines the workshop last built instead. Its
`README.txt` is written for the person running Setup.

| File | What it does |
| --- | --- |
| `Setup.cmd` | Set-up: builds both engines from the game files (or copies the workshop's), links the game files, checks the PAL hashes |
| `Play Nightfire.cmd` | The whole game, Action and Driving linked in one window, with the PC settings |
| `Play Nightfire (Fullscreen).cmd` | The same, borderless fullscreen |
| `Split-screen Multiplayer.cmd` | Starts at the menus, set up for up to four players on one PC |
| `Debug/` | Full logging, the F9 picture recorder, Smooth Motion off, two windows, skipping the opening drive, split-screen variants and the lockstep record/replay test |
| `engine/launcher/nightfire.py` | The launcher behind all of them (`--mode play` or `--mode debug`) |
| `engine/driving/` | Driving's run scripts and its frozen start settings |
| `engine/action/start-action.cmd` | Action's start settings |

Play mode keeps the crash and freeze reports but leaves out the sound
recording, frame counter and picture dumps; debug mode keeps everything.

Since 2026-10-11 both engines build from this repository alone: the Action
engine's own sound output replaced the last file that could not be published.
