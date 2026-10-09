# Native builds (source snapshot, 2026-10-09)

This directory holds the hand-written source of the **native** Action and
Driving builds: the versions that run the recompiled game without emulating
the Xbox graphics or sound chips. Files are copied byte for byte from the
owner's workspaces and listed with hashes in the root `SOURCE-MANIFEST.json`
(groups `native-action`, `native-driving` and `native-launch`).

| Directory | Workspace | Contents |
| --- | --- | --- |
| `action/` | `nightfire-port-native` | Action engine (missions and multiplayer) |
| `driving/` | `nightfire-driving-native` | Driving engine (vehicle levels) |
| `launchers/` | `native-driving` | Launchers, the one-window host and helper scripts |

## Not included

Like the rest of this repository, this is documentary source, not a
standalone build. The generated game translations (`src/recomp`), any code
lifted from the game, tables taken from the game, the toolkit checkout, game
files, extracted menu data, logs, crash sessions, backups and build outputs
are all excluded.
