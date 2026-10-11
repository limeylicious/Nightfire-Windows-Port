# Building both engines on the player's PC

`build_engines.py` makes the two Windows programs, the Action engine and the Driving
engine, from the player's own PAL game files. The alpha's `Setup.cmd`
([`../../release/alpha/`](../../release/README.md)) runs it first when it finds itself in a
copy of this repository, then copies the engines into its `engine\` folder as usual.

```
python native\setup\build_engines.py --game "release\alpha\Game Files" --workshop workshop
```

Everything it makes goes into `--workshop` (the alpha uses `workshop\` at the top of the
repository, which Git ignores):

| Folder | Contents |
| --- | --- |
| `python-deps\` | Capstone 5.0.6, installed there by pip on the first run |
| `xboxrecomp\` | The translator ([xboxrecomp](https://github.com/sp00nznet/xboxrecomp), MIT), cloned at commit `051a128` with Windows line endings |
| `nightfire-port-native\` | The Action build folder |
| `nightfire-driving-native\` | The Driving build folder |
| `logs\` | One log per step |

## Steps

1. **Checks:** Python 3.8 or newer (tested with 3.11 to 3.13), Git, CMake, the Visual Studio C++ build tools (found with
   `vswhere`) and the two XBEs, by their PAL SHA-256.
2. **Translation:** [`../regen/regen.py`](../regen/README.md) translates both XBEs and
   applies the recipes. Every file must match the recipe's manifest of the tested build.
   Skipped when a build folder already holds a matching translation (`--retranslate`
   forces it).
3. **Build folders:** each is filled from `native/action/` or `native/driving/`, the
   shared files listed in `trees.json` and the translation. On later runs only files that
   changed are rewritten, so the compiler rebuilds only what changed; files Setup did not
   put there are removed, except the build's own folders (`build-native`, `logs`, `cache`,
   `sessions` and the like). A folder Setup did not make is never touched.
4. **Test stand-ins:** the CMake files also describe test programs whose sources are not
   published (they hold translated game code or come from analysis folders). Those targets
   are never built, but CMake needs their files to exist, so Setup writes a one-line
   stand-in for each missing one.
5. **Build:** the preparation scripts each `build-native.cmd` runs (renames and hooks; all
   leave a matching translation unchanged), then CMake with Visual Studio, x64,
   RelWithDebInfo. Action's `nd3d_action_translate.py` is not run: it writes
   `runtime/native/` from the Driving sources and an address map that is not published, and
   its output is already in `native/action/runtime/native/`.

## `trees.json`

The owner's build folders use some files that this repository publishes once, under
`nightfire-port/`, `nightfire-driving/` or `experiments/driving-510/`. `trees.json` maps each
of them to its place in the build folder. Every mapping was checked byte for byte against the
owner's build folders on 2026-10-11. When one of those files changes in a build folder, the
new version goes under `native/<engine>/` and its mapping is removed (Setup stops if a file is
in both places).

Nothing generated from the game is ever written outside the workshop, and nothing in this
folder holds game code.
