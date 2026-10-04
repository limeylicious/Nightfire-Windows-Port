# Driving bridge and diagnostic source snapshot — checkpoint510

This is an **isolated, incomplete source snapshot**, not a replacement for the
normal Driving reference and not a runnable game. It copies reviewed C/H files
from the actual checkpoint510 build and selected source-only diagnostic parsers.
Source-relative origins and exact SHA256 values are recorded in the root
`SOURCE-MANIFEST.json` and `tools/source_allowlist.json`.

The cumulative bridge includes work through491, the unpromoted508 residency
candidate, and510's default-OFF observations:

- `DRIVING_TEXT510=1`: records draw inputs, text replay and present associations.
- `DRIVING_STALL510=1`: samples thread contexts/waits after a no-present gap.
- `DRIVING_PC508`: experimental access/publication safeguards; not a promoted
  comparison or player profile. This snapshot supplies no preset that enables it.

See [the measured status](../../docs/STATUS-510.md) for the NO-GO result, diagnostic
limitations and next repair. Neither diagnostic is an FPS optimization. The
generic510 hooks in `runtime/gpu144/nv2a_pb_exec.c` were not linked in the tested
game; the actual linked wrapper uses an excluded upstream executor. Absence of
those events must not be interpreted as no draw activity.

Excluded dependencies include game executables/assets, generated game functions,
large captured shader/program/state tables, the copied generic263 executor, private
build configuration, toolkit source, captures, logs and test input payloads.
Consequently, includes may intentionally refer to files absent here. The original
source bytes are preserved; excluded functions are not replaced with fake stubs.

The Python tools parse locally supplied diagnostic data. No captured game data
is included with them. Refer to their CLI help/source for input requirements;
this export does not claim their live tests can be reproduced without the private
development environment and a compatible privately supplied PAL game.

This source retains per-file provenance and notices. See the root CREDITS.md and
SOURCE-EXPORT.md for rights, restrictions and short legacy admission fingerprints.
