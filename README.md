# Nightfire Windows Port

An AI-developed, standalone Windows static-recompilation project for the original PAL Xbox edition of *007 Nightfire*. It uses [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) as the toolkit; the running game is a native Windows executable, not an emulator session.

**This repository is a reviewed source snapshot, not a downloadable game or a complete build checkout.** The original Xbox executables, game data, media, generated translations, extracted shader/program tables, local logs, captures and release packages are deliberately excluded. Anyone building locally must supply their own compatible PAL copy and regenerate the excluded output. This repository does not grant rights to the game.

## A Note from the author
Yes, this is entirely done with AI. I simply wanted to see how feasible it was to decompile a game of my childhood. I wanted to transform the original game into something we could still play as we remember it as well as one that's a bit more modern, with nice QOL extras and things like online multiplayer. I personally don't want to claim any credit for anything here bar being a testing lab-rat for certain issues or moments that needed a moving hand. I personally give consent to reuse this for **anything** you desire. I didn't make the code, I have no right to tell anyone how to use it. (so long as it complies with whatever license terms or permissions the resources/tools used mention)

## Current progress (October 9, 2026)

Both of the game's engines now run as native Windows code, without emulating the Xbox graphics or sound chips. The newest source is in [native/](native/README.md).

- **Action engine** (missions and multiplayer): draws through the project's own Direct3D 11 renderer and plays sound through its own mixer. The owner has played it from the Paris opening through The Exchange and into local multiplayer.
- **Driving engine** (vehicle levels, such as the Paris chase): uses the same native renderer and audio. Its game logic steps on the game's own timer, so it plays at the original speed.
- **One game, one window:** Action hands over to Driving for the Paris chase and back again in a single window, as it does on the console.
- **Crash capture:** every native launcher writes a crash report, minidump or freeze report to a per-session folder.

### Optional PC settings

These are switched on only by the "overlay" launchers. The plain launchers stay as close to the original as possible. Settings are saved per Windows user.

- An F10 settings menu in both engines.
- Internal resolution: the original 640x480, 2x, 3x, 4x, or matched to the window.
- Widescreen 16:9 through the game's own built-in widescreen mode (takes effect after a restart).
- Picture shape (stretch or keep 4:3), brightness, and smooth or sharp scaling.
- Display mode: windowed, fullscreen or borderless fullscreen, switchable while playing.
- A sharp picture on high-DPI screens (per-monitor DPI awareness).
- In the Action engine, a "PC Graphics" page inside the game's own Options > Audio/Video menu, and mouse control of the menus.

### Known gaps

- Sound plays without the console's reverb and other audio effects.
- Some Paris effects still differ from the console, such as gunfire flashes and glare.
- On battery power the test laptop runs the game more slowly.
- Mouse control does not yet reach every Action submenu.
- The Driving engine has no in-game settings page yet; it uses the F10 menu.

The goal remains a faithful version of the PAL game on Windows. Online multiplayer is not implemented, and nothing in this repository starts network play.

## End goal

A simple package for players: unpack it, copy the game folder from your own PAL disc into the folder it names, and play. The whole game would be there: both engines running natively, Action and Driving in one window, multiplayer, and the optional PC settings.

The package itself would contain no game content. The recompiled game code is a translation of the game, so a set-up step would generate and build it on the player's own PC from their copy, and create Driving's save storage fresh.

## What is in this repository

- [native/](native/README.md): the native Action and Driving sources, launchers and the in-game settings page (new in this update).
- [experiments/driving-lean](experiments/driving-lean/README.md): the first GPU-resident Driving renderer, as of October 4.
- [experiments/driving-510](experiments/driving-510/README.md) and [docs/STATUS-510.md](docs/STATUS-510.md): the older bridge renderer's last investigation.
- `nightfire-port/` and `nightfire-driving/`: the original source-export layout of the earlier, partly emulated builds.

Exported bytes and their source-relative provenance are listed in `SOURCE-MANIFEST.json`; the export gate checks both the working tree and staged Git blobs. See [SOURCE-EXPORT.md](SOURCE-EXPORT.md) for the exact scope and why this snapshot cannot be built by itself.

## Development and attribution

The Nightfire-specific implementation, repair work and documentation in this project have been produced by **AI agents, including OpenAI Codex and Anthropic Claude**, under the owner's direction. Claude has also provided independent technical review and planning. The owner supplied their game files privately, set priorities, and performed hands-on gameplay testing and bug reporting. Upstream projects and the original game have their own authors and rights holders; their work is credited in [CREDITS.md](CREDITS.md).

The runtime is based on a pinned `xboxrecomp` checkout (`051a128df5ec27ef14f1ceaaead11c5457321eef`) and project-specific adapters. This repository does not include the toolkit checkout.

## Repository description

> Native Windows static recompilation of PAL Xbox 007 Nightfire, built on xboxrecomp. Source only: bring your own copy of the game. No game files or generated code included.
