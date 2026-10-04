# Nightfire Windows Port

An AI-developed, standalone Windows static-recompilation project for the original PAL Xbox edition of *007 Nightfire*. It uses [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) as the toolkit; the running game is a native Windows executable, not an emulator session.

**This private repository is a reviewed source snapshot, not a downloadable game or a complete build checkout.** The original Xbox executables, game data, media, generated translations, extracted shader/program tables, local logs, captures and release packages are deliberately excluded. Anyone building locally must supply their own compatible PAL copy and regenerate the excluded output. This repository does not grant rights to the game.

The project has two native targets:

- **Action engine:** the original single-player and local multiplayer paths have reached playable milestones. The working local build remains in the owner's separate development workspace; this repository does not package it.
- **Driving engine:** Paris and a vehicle scene render, but performance remains below a playable target. The latest completed investigation is **checkpoint 510**. It identified a shared tutorial-text loss; the proposed 508 residency profile remains unpromoted. See [current status and measured results](docs/STATUS-510.md).

The immediate goal is a faithful, working version of the original PAL game on Windows. Optional PC features and online multiplayer belong to a later enhanced version. Nothing in this repository starts network play.

## Development and attribution

The Nightfire-specific implementation, repair work and documentation in this project have been produced by **AI agents, including OpenAI Codex and Anthropic Claude**, under the owner's direction. Claude has also provided independent technical review and planning. The owner supplied their game files privately, set priorities, and performed hands-on gameplay testing and bug reporting. Upstream projects and the original game have their own authors and rights holders; their work is credited in [CREDITS.md](CREDITS.md).

The runtime is based on a pinned `xboxrecomp` checkout (`051a128df5ec27ef14f1ceaaead11c5457321eef`) and project-specific adapters. This repository does not include the toolkit checkout. See [SOURCE-EXPORT.md](SOURCE-EXPORT.md) for the exact scope and why this snapshot cannot be built by itself.

The existing `nightfire-port/` and `nightfire-driving/` roots retain the normal source-export layout. Newer Driving bridge and diagnostic sources are kept in [experiments/driving-510](experiments/driving-510/README.md), separately from those references. Experimental source is not a promoted player build. Exported bytes and their source-relative provenance are listed in `SOURCE-MANIFEST.json`; the export gate checks both the working tree and staged Git blobs.

## Repository description

> AI-developed standalone Windows static recompilation of PAL Xbox 007 Nightfire. Private source snapshot; no game files or generated translations.
