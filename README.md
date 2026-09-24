# Nightfire Windows Port

An AI-developed, standalone Windows static-recompilation project for the original PAL Xbox edition of *007 Nightfire*. It uses [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) as the toolkit; the running game is a native Windows executable, not an emulator session.

**This private repository is a reviewed source snapshot, not a downloadable game or a complete build checkout.** The original Xbox executables, game data, media, generated translations, extracted shader/program tables, local logs, captures and release packages are deliberately excluded. Anyone building locally must supply their own compatible PAL copy and regenerate the excluded output. This repository does not grant rights to the game.

The project has two native targets:

- **Action engine:** the original single-player and local multiplayer paths have reached playable milestones. The working local build remains in the owner's separate development workspace; this repository does not package it.
- **Driving engine:** Paris renders and is under graphics/performance investigation. The normal build is kept separate from default-off experiments. Checkpoint 340 measured that one shared-draw approach adds more GPU copy cost than it saves.

The immediate goal is a faithful, working version of the original PAL game on Windows. Optional PC features and online multiplayer belong to a later enhanced version. Nothing in this repository starts network play.

## Development and attribution

The Nightfire-specific implementation, repair work and documentation in this project have been produced by **OpenAI Codex AI agents** under the owner's direction. The owner supplied their game files privately, set priorities, and performed hands-on gameplay testing and bug reporting. Upstream projects and the original game have their own authors and rights holders; their work is credited in [CREDITS.md](CREDITS.md).

The runtime is based on a pinned `xboxrecomp` checkout (`051a128df5ec27ef14f1ceaaead11c5457321eef`) and project-specific adapters. This repository does not include the toolkit checkout. See [SOURCE-EXPORT.md](SOURCE-EXPORT.md) for the exact scope and why this snapshot cannot be built by itself.

## Repository description

> AI-developed standalone Windows static recompilation of PAL Xbox 007 Nightfire. Private source snapshot; no game files or generated translations.
