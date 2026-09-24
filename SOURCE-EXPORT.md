# Scope of this private source snapshot

This tree is produced from a **strict allowlist** in `tools/export_source.py`. It contains selected hand-authored project runtime, build and entry-point files. It is not a copy of the working directory and should never be populated by a blanket `git add` from that directory.

The export excludes `game_files`, every `.xbe`, all game media and extracted graphics, executables/DLLs, builds, releases, caches, logs, screenshots, videos, raw captures, memory dumps, analysis archives, recovery ZIPs, `src/recomp/gen`, `recomp_manual.c`, and larger PAL-derived shader/program tables. It also excludes the `xboxrecomp` checkout; acquire the upstream toolkit separately and retain its component licences.

The project cannot be built directly from this snapshot because generated translated game code and a few PAL-derived tables are intentionally absent. The owner's local development workspace remains the buildable source of truth. Before any wider source release, review every included file, test fixture and upstream licence again; privacy alone is not a redistribution licence.

The original PAL executable and assets are **never** copied by the export script. The script refuses files outside its allowlist, refuses symlinks, rejects executable/media signatures and scans the staged text for obvious private paths and credentials. Review the staged Git diff and file inventory before every push.

`.gitattributes` preserves the audited source bytes, including line endings, so `SOURCE-MANIFEST.json` hashes match the Git blobs rather than a platform-normalized copy.

Current local baselines at export: Action checkpoint 241, Driving normal build 321, and Driving measurement checkpoint 340. Default-off renderer experiments stay off.
