# Scope of this source snapshot

This tree is produced from the **explicit, hash-pinned allowlist** in `tools/source_allowlist.json`, applied by `tools/export_source.py`. It contains selected project runtime, build, entry-point and diagnostic-parser files. It is not a copy of the development workspace and must never be populated by a blanket `git add` from that workspace. New files cannot enter through a discovery glob.

The export excludes `game_files`, every `.xbe`, all game media and extracted graphics, executables/DLLs, builds, releases, caches, logs, screenshots, videos, raw captures, memory dumps, analysis archives, recovery ZIPs, `src/recomp/gen`, `recomp_manual.c` (apart from the two reviewed native copies below), and larger PAL-derived shader/program tables. This includes `driving_contract*`, `driving_native247.h`, `driving_admission283.h`, `driving_native_effects350.h`, `driving_native_remaining352.h`, `driving_sprite_contract396.h`, `driving_font_plan348.h`, and conservatively `driving_fog455.h`. The `generic263` copied executor and the `xboxrecomp` checkout remain excluded; acquire upstream dependencies separately and retain their component licences.

The project cannot be built directly from this snapshot because generated translated game code and a few PAL-derived tables are intentionally absent. The owner's local development workspace remains the buildable source of truth. The repository is public. Review every newly included file, test fixture and upstream licence before adding it; publishing source is not a licence to redistribute the game or third-party components.

The original PAL executable and assets are **never** copied by the export script. It refuses changed/unreviewed inputs, links and path escapes, rejects executable/media signatures, and scans text for private paths and credential patterns. The verifier requires an exact inventory and checks both source hashes and the proposed Git index. These automated checks supplement source review; they are not a universal secret or copyright detector.

Some previously exported bridge sources contain short shader/state admission fingerprints or hardware constants. Those are not media assets, and this snapshot does not claim to contain zero original program bytes. Large extracted programs and generated game translations remain outside the export. Existing source notices and component rights remain effective.

`.gitattributes` preserves the audited source bytes, including line endings, so `SOURCE-MANIFEST.json` hashes match the Git blobs rather than a platform-normalized copy.

Current local references: frozen Action241 and normal Driving321; the last verified cumulative non-OpenGL game baseline is491. Checkpoint510 is a separate diagnostic layer on508, whose residency profile has **not** been promoted. The normal source roots are refreshed independently of the experimental snapshot; no local player binary is changed by this export.

`experiments/driving-510/` records reviewed source from the actual510 build plus selected parser scripts. Its CMake file is intentionally absent because the private build configuration contains local absolute paths and depends on excluded generated files. This directory is documentary source, not a standalone build or enabled feature. Source-relative origins, exact hashes and export groups are in the manifest.

`experiments/driving-lean/` (group `driving-lean`) records the new and changed files of the lean GPU-resident Driving renderer, copied byte for byte from the owner's `nightfire-driving-lean` workspace on 2026-10-04. Like the 510 snapshot it is documentary: its CMake file references unchanged runtime files, generated game translations and the pinned toolkit, none of which are copied here. The workspace's own `lean-source-manifest.json` is excluded because it lists local absolute paths. No source bytes are redacted or rewritten during copying; missing inputs are explicitly excluded.

`native/` (groups `native-action`, `native-driving`, `native-launch`) records the hand-written source of the native Action and Driving builds and their launchers, copied byte for byte on 2026-10-09 from the owner's `nightfire-port-native`, `nightfire-driving-native` and `native-driving` workspaces. The same update refreshed `experiments/driving-lean/` and the crash-capture files under `nightfire-port/`. Left out on purpose: lifted-code test fixtures, address maps and disassembly dumps, game text and menu dumps, the menu extractor and its helper library, launch data captured from the game, internal hand-off notes, files containing local paths, and every file adapted from xemu. The xemu-derived lean APU copies, `nightfire-port/runtime/nightfire_adpcm.h` and the LGPL licence text that accompanied them were removed from this repository on 2026-10-09; the builds that still use such code take it from the pinned toolkit. Some files whose comments name the owner were held back pending the owner's decision.

On 2026-10-10 `native/` was refreshed from the same workspaces: key binds, frame rate options, Smooth Motion fixes, the grappling-hook and new-profile fixes, four controllers and split-screen, the Multiplayer Local / Online pages, the lockstep record-and-replay test and Driving's own sound output. Files whose comments name the owner stay out, including `lean_present.inc`, and so do the xemu-derived files the Action build still compiles.

On 2026-10-11, on the owner's decision, the build files held back for naming the owner were added: `lean_present.inc` (both engines), Action's `nightfire_pacer93.h` and Driving's `lean_gpu.c`. Each named the owner in one comment. Their allowlist rows carry a `reword` entry that replaces that one line, matched by line number and SHA-256, with the same sentence saying "the owner"; every other byte is copied unchanged.

`native/regen/`, `native/action/regen/` and `native/driving/regen/` (group `regen`, added 2026-10-11) are maintained in this repository rather than copied from a workspace: the scripts that rebuild each engine's translated code from the player's own disc, and the recipes they apply. A recipe holds addresses, our names, our own code, single-instruction fixes and SHA-256 guards; it holds no translated code, no diffs of it and none of the game's strings. Their source path is their repository path, and they pass the same text, size and hash checks. Since 2026-10-11 the Driving recipe needs no private parts: functions the port used to carry as hand-checked transcriptions are lifted by the toolkit on the player's PC, and the start-up file reads the game's file names from the player's XBE. On the owner's decision, `native/action/src/recomp_manual.c` and `native/driving/src/recomp_manual.c` are exported by exact path (they now hold only an address-to-function table and diagnostics); the name stays denied everywhere else. `native/setup/` (same group) holds the public Setup's build script and its map of shared files.

`release/alpha/` (group `release-alpha`) records the launchers and set-up scripts of the owner's first alpha folder, copied byte for byte from `releases/Nightfire-PC-Alpha`. On 2026-10-11 its `Setup.cmd`, `engine/setup.py`, `README.txt` and game-files note were updated here first, for the public Setup that builds both engines; the owner's alpha folder takes the same files on its next update. Only text files are listed; the engines that its Setup copies, the game files it links and every output folder (sessions, runs, saves, lockstep) are excluded by the export policy.

Run from this repository using Python3.9 or newer:

```text
python -B tools/test_export_policy.py
python -B tools/export_source.py
python -B tools/verify_export.py
# After reviewing and staging the exact export:
python -B tools/verify_export.py --staged
```

The exporter needs the owner's sibling development workspace. The verifier and policy tests work on this source snapshot alone. Status documents are manually reviewed summaries, not copied private logs or analysis archives.
