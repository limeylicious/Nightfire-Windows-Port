"""Write a manifest of a gen folder: SHA-256 of each file's text with line endings normalised.

    python make_manifest.py GEN_FOLDER OUT.json [--note TEXT]

Hashes only; no content. regen.py checks a rebuilt folder against it.
"""
import argparse, json, time
from pathlib import Path
from regen import norm_sha

ap = argparse.ArgumentParser(); ap.add_argument('gen'); ap.add_argument('out'); ap.add_argument('--note', default='')
a = ap.parse_args()
gen = Path(a.gen)
files = {p.name: norm_sha(p) for p in sorted(gen.iterdir()) if p.suffix in ('.c', '.h')}
Path(a.out).write_text(json.dumps({'made': time.strftime('%Y-%m-%d'), 'note': a.note,
                                   'normalisation': 'CRLF/CR to LF, trailing spaces removed, UTF-8',
                                   'files': files}, indent=1) + '\n')
print(len(files), 'files')
