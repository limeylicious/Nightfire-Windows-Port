"""Hook fix (2026-10-10): lift frndint by the x87 control word in the Driving gen.

The lifter emits `fp_top() = rint(fp_top()); /* frndint */`, which always rounds to
nearest. The CRT ceil/floor (sub_00133040 / sub_0013310E) set the rounding field
with fldcw and then use frndint, so in this build they rounded to nearest. The
bullet grid walk sub_000C64B0 takes its first cell boundary from them and could
start one cell row off, skipping the Paris hook's cell.

This script points the 11 frndint sites at lean_frndint_rc (runtime/lean/lean_flags.c),
which honours (g_fp_control_word >> 10) & 3 (LEAN_FIX_FRNDINT=0 at run time restores
nearest). It also declares lean_frndint_rc after each file's LEAN_FIX_ROUND line.

  python scripts\\nf_frndint_fix.py --check   report, change nothing
  python scripts\\nf_frndint_fix.py --apply   patch (idempotent; keeps CRLF)
  python scripts\\nf_frndint_fix.py --undo    restore the original lines exactly
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GEN = ROOT / 'src' / 'recomp' / 'gen'
FILES = ['recomp_0008.c', 'recomp_0011.c', 'recomp_0012.c', 'recomp_0017.c', 'recomp_0018.c']
EXPECTED_SITES = 11

OLD_SITE = b'fp_top() = rint(fp_top()); /* frndint */'
NEW_SITE = b'fp_top() = lean_frndint_rc(fp_top()); /* frndint (LEAN_FIX_FRNDINT) */'
ROUND_DECL = b'/* LEAN_FIX_ROUND */'
NEW_DECL = b'double lean_frndint_rc(double x); /* LEAN_FIX_FRNDINT */'


def scan(data):
    return data.count(OLD_SITE), data.count(NEW_SITE), data.count(NEW_DECL)


def patch(data):
    nl = b'\r\n' if b'\r\n' in data else b'\n'
    if NEW_DECL not in data:
        i = data.find(ROUND_DECL)
        if i < 0:
            raise SystemExit('no LEAN_FIX_ROUND declaration line to anchor on')
        j = data.find(nl, i) + len(nl)
        data = data[:j] + NEW_DECL + nl + data[j:]
    return data.replace(OLD_SITE, NEW_SITE)


def unpatch(data):
    nl = b'\r\n' if b'\r\n' in data else b'\n'
    data = data.replace(NEW_DECL + nl, b'', 1)
    return data.replace(NEW_SITE, OLD_SITE)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else '--check'
    if mode not in ('--check', '--apply', '--undo'):
        raise SystemExit(__doc__)
    total_old = total_new = 0
    blobs = {}
    for name in FILES:
        p = GEN / name
        if not p.exists():
            raise SystemExit(f'missing {p}')
        data = p.read_bytes()
        o, n, d = scan(data)
        total_old += o
        total_new += n
        blobs[name] = data
        print(f'{name}: {o} unpatched, {n} patched, declaration {"yes" if d else "no"}')
    if total_old + total_new != EXPECTED_SITES:
        raise SystemExit(f'found {total_old + total_new} frndint sites, expected {EXPECTED_SITES}: '
                         'the generated code changed, nothing written')
    if mode == '--check':
        return
    for name, data in blobs.items():
        out = patch(data) if mode == '--apply' else unpatch(data)
        if out != data:
            (GEN / name).write_bytes(out)
            print(f'{name}: {"patched" if mode == "--apply" else "restored"}')
    print('frndint fix ' + ('applied' if mode == '--apply' else 'removed') +
          f' ({EXPECTED_SITES} sites)')


if __name__ == '__main__':
    main()
