"""Add the LEAN_AUDIO_NATIVE hook to generated code (idempotent).

Generated code calls functions directly (RECOMP_ABI_CALL(va, fn) -> fn()), so
there is no runtime table to override a guest function from the lean bridge.
This adds one address-keyed check to the plain RECOMP_ABI_CALL in
src/recomp/gen/recomp_types.h: calls into the DSOUND section
(0x17AC40-0x183AA4) ask lean_ds_override(va) (runtime/lean/lean_dsound_glue.c)
first. For direct calls va is a constant, so every call outside that range
compiles to exactly what it was. With LEAN_AUDIO_NATIVE off the override
returns 0 and the original function runs.

Usage: python scripts/apply_native_audio_hook.py [path to recomp_types.h]
Run again after regenerating src/recomp/gen.
"""
import sys, pathlib

path = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else
                    pathlib.Path(__file__).resolve().parent.parent / 'src/recomp/gen/recomp_types.h')
raw = path.read_bytes()
crlf = b'\r\n' in raw
text = raw.decode('latin-1').replace('\r\n', '\n')
MARK = '/* LEAN_AUDIO_NATIVE hook */'
OLD = '#define RECOMP_ABI_CALL(va, fn) do { LEAN_COV(va); (fn)(); } while (0)'
NEW = ('int lean_ds_override(uint32_t va); ' + MARK + '\n'
       '#define LEAN_DS_HOOKED(va) ((uint32_t)(va) >= 0x0017AC40u && (uint32_t)(va) < 0x00183AA4u)\n'
       '#define RECOMP_ABI_CALL(va, fn) do { LEAN_COV(va); '
       'if (LEAN_DS_HOOKED(va) && lean_ds_override((uint32_t)(va))) break; (fn)(); } while (0)')
if MARK in text:
    print('already applied:', path)
    sys.exit(0)
if text.count(OLD) != 1:
    sys.exit('expected RECOMP_ABI_CALL definition not found exactly once in %s' % path)
text = text.replace(OLD, NEW)
if crlf:
    text = text.replace('\n', '\r\n')
path.write_bytes(text.encode('latin-1'))
print('applied:', path)
