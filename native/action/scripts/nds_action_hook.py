"""Add the NIGHTFIRE_NATIVE_SOUND hook to Action generated code (idempotent).

Generated code calls functions directly (RECOMP_ABI_CALL(va, fn) -> fn()). This
adds one address-keyed check to both RECOMP_ABI_CALL variants in
src/recomp/gen/recomp_types.h, before the per-call checkpoint: calls into the
DirectSound entry-point range (0x112700-0x114A00) ask nds_override(va)
(runtime/native_action/nds_action_glue.c) first. For direct calls va is a
constant, so every call outside the range compiles to what it was. With the
switch off nds_override returns 0 and the original routine runs.

Usage: python scripts/nds_action_hook.py [path to recomp_types.h]
"""
import sys, pathlib
BSNL = chr(92) + chr(10)   # backslash-newline (macro continuation)

path = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else
                    pathlib.Path(__file__).resolve().parent.parent / 'src/recomp/gen/recomp_types.h')
raw = path.read_bytes()
crlf = b'\r\n' in raw
text = raw.decode('latin-1').replace('\r\n', '\n')
MARK = '/* NIGHTFIRE_NATIVE_SOUND hook */'
if MARK in text:
    print('already applied:', path)
    sys.exit(0)
OLD_CHECK = ('    uint32_t _ab = g_ebx, _as = g_esi, _ad = g_edi, _ap = g_esp; ' + BSNL +
             '    NIGHTFIRE_CALL_CHECK((va), 0); ' + BSNL)
NEW_CHECK = ('    uint32_t _ab = g_ebx, _as = g_esi, _ad = g_edi, _ap = g_esp; ' + BSNL +
             '    if (NDS_HOOKED(va) && nds_override((uint32_t)(va))) break; ' + BSNL +
             '    NIGHTFIRE_CALL_CHECK((va), 0); ' + BSNL)
OLD_PLAIN = '#define RECOMP_ABI_CALL(va, fn) do { NIGHTFIRE_CALL_CHECK((va), 0); (fn)(); NIGHTFIRE_CALL_CHECK((va), 1); } while (0)'
NEW_PLAIN = ('#define RECOMP_ABI_CALL(va, fn) do { if (NDS_HOOKED(va) && nds_override((uint32_t)(va))) break; '
             'NIGHTFIRE_CALL_CHECK((va), 0); (fn)(); NIGHTFIRE_CALL_CHECK((va), 1); } while (0)')
ANCHOR = '#ifdef RECOMP_ABI_CHECK\nvoid recomp_abi_violation_log('
DECL = ('int nds_override(uint32_t va); ' + MARK + '\n'
        '#define NDS_HOOKED(va) ((uint32_t)(va) >= 0x00112700u && (uint32_t)(va) < 0x00114A00u)\n')
for old in (OLD_CHECK, OLD_PLAIN, ANCHOR):
    if text.count(old) != 1:
        sys.exit('expected text not found exactly once in %s: %r' % (path, old[:60]))
text = text.replace(OLD_CHECK, NEW_CHECK).replace(OLD_PLAIN, NEW_PLAIN).replace(ANCHOR, DECL + ANCHOR)
if crlf:
    text = text.replace('\n', '\r\n')
path.write_bytes(text.encode('latin-1'))
print('applied:', path)
