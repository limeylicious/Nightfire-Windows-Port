# Rebuilding the translated code from your own disc

The port's translated game code (`src/recomp/gen` in each engine's build tree) is never
stored in this repository. It is a translation of the game, so it is made on the
player's PC from their own disc files. This folder holds what that takes.

```
python native\regen\regen.py --game "Game Files" --toolkit tools\xboxrecomp --out regen-20261011
```

- `--game` is the folder with the extracted disc. `default.xbe` (Action) and `Driving.xbe`
  are found by name and checked against the PAL SHA-256 hashes.
- `--toolkit` is [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) (MIT) at commit
  `051a128`. It needs Python 3 and Capstone.
- `--out` must be a new folder. Nothing outside it is written.
- `--compare-action GEN` / `--compare-driving GEN` also compare the result with an existing
  gen folder, read-only.
- `--local-driving DIR` adds recipe parts that are not published (see below).

For each engine `regen.py` makes a build tree under `--out`, runs the toolkit, then applies
the engine's recipe and checks every file against the recipe's manifest.

## What a recipe holds (`native/<engine>/regen/`)

| File | Contents |
|---|---|
| `inputs/function-list.json`, `inputs/classification.json` | Action only: the function list and classification the first translation used, as start address, size, section and category (names are `sub_<start>`). The current disassembler finds a different list, so the original is kept as addresses. No labels made from the game's strings. |
| `recipe.json` (Action), `steps.json` (Driving) | The steps after the toolkit, in order (below). |
| `own/` | Source files in the gen folder that are entirely our code. |
| `own-src/` | Files for the build's `src/` folder. `recomp_manual.c` (an address-to-function table and diagnostics) is still on the export deny list, so it is not in this repository yet. |
| `manifest.json` | SHA-256 of every file of the tested build, with line endings normalised. |

`build_action.py` applies the Action recipe in these stages:

1. **harden**: `scripts/harden_generated.py` turns unsupported instructions into visible stops.
2. **splice**: functions whose end address was corrected are translated again over the
   corrected range (switch statements whose cases the first pass missed).
3. **hand**: the separate translated files (callbacks the first pass did not find) are
   made from toolkit lifts by start and end address, plus our own files from `own/`.
4. **prehunks** and **build**: our call-check edits to `recomp_types.h`, then the scripts
   `build-native.cmd` runs on every build (renames and hooks).
5. **hunks**: our remaining edits (hooks, instruction fixes, notes and renames). Each is keyed
   by function address and line, and carries a SHA-256 of the lines it replaces, so it
   refuses to apply to anything but the expected toolkit output.

`repair_driving.py` applies the Driving recipe (`steps.json`, 25 steps) to the toolkit's
fresh output:

1. **seed** and **lift**: copy the fresh output into a work folder and translate the
   functions the first pass missed, by start and end address (`lifts-hand.json`,
   `lifts-aux.json`).
2. **script**: our fix scripts from `native/driving/scripts/`, each pinned by SHA-256
   (lost flags, rounding, `cmpsd`, timer, hooks, renames).
3. **flags_counters**: number the remaining flag fallbacks in file order.
4. **ops**: our edits keyed by function address, guarded by hashes (`ops/`).
5. **hand** and **copy**: assemble the separate callback files from lifts plus their edits
   (`hand-files.json`, `ops-hand/`), then copy files that are entirely our code.

Six of Driving's 53 files also need parts that are not published: functions the port carries
as hand-checked transcriptions, and a start-up file that names the game's files. They stay
on the owner's PC and are added with `--local-driving`. Without them the other 47 files are
identical; replacing those parts with toolkit lifts is planned.

## Rules for what goes here

Addresses, end addresses, names we chose, pattern rules, our own code and hashes are fine.
Translated code, diffs of it, and the game's strings are not. A hunk's new lines are our
edits. Where one rewrites a single translated instruction (as `nf_rcr_fix.py` does), only
that line is included.
