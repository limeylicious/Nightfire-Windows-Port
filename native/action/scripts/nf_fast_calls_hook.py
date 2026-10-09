"""Add the NIGHTFIRE_FAST_CALLS filter to Action generated code (idempotent).

NIGHTFIRE_CALL_CHECK(va, after) runs nightfire_thread_check before and after every
translated call. This makes it ask NF_CALL_HOT(va) first (a bit per routine address,
runtime/native_action/nf_fast_calls.c). With NIGHTFIRE_FAST_CALLS off every bit is set
and behaviour is unchanged.

Usage: python scripts/nf_fast_calls_hook.py [path to recomp_types.h]
"""
import sys, pathlib

path = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else
                    pathlib.Path(__file__).resolve().parent.parent / 'src/recomp/gen/recomp_types.h')
raw = path.read_bytes()
crlf = b'\r\n' in raw
text = raw.decode('latin-1').replace('\r\n', '\n')
MARK = '/* NIGHTFIRE_FAST_CALLS hook */'
if MARK in text:
    print('already applied:', path)
    sys.exit(0)
OLD = '#define NIGHTFIRE_CALL_CHECK(va, after) nightfire_thread_check((va), (after))'
NEW = ('extern unsigned char nf_hot_map[0x40000]; extern int nf_hot_far; ' + MARK + '\n'
       '#define NF_CALL_HOT(va) ((uint32_t)(va) < 0x200000u ? '
       '(nf_hot_map[(uint32_t)(va) >> 3] >> ((uint32_t)(va) & 7u)) & 1 : nf_hot_far)\n'
       '#define NIGHTFIRE_CALL_CHECK(va, after) '
       'do { if (NF_CALL_HOT(va)) nightfire_thread_check((va), (after)); } while (0)')
if text.count(OLD) != 1:
    sys.exit('expected NIGHTFIRE_CALL_CHECK definition not found exactly once in %s' % path)
text = text.replace(OLD, NEW)
if crlf:
    text = text.replace('\n', '\r\n')
path.write_bytes(text.encode('latin-1'))
print('applied:', path)
