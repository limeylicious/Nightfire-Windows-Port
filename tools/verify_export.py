"""Fail closed if a source export contains a game file or an unlisted source file."""

from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path

repo = Path(__file__).resolve().parents[1]
manifest = json.loads((repo / 'SOURCE-MANIFEST.json').read_text(encoding='utf-8'))
assert manifest['scope'].startswith('strict source-only')
listed = {item['path']: item for item in manifest['files']}
assert len(listed) == manifest['file_count'] == 257

manually_reviewed = {
    '.gitignore', '.gitattributes', 'README.md', 'CREDITS.md', 'SOURCE-EXPORT.md',
    'SOURCE-MANIFEST.json', 'tools/export_source.py', 'tools/verify_export.py',
    'licenses/xboxrecomp-MIT.txt', 'licenses/xboxrecomp-NOTICE.txt',
    'licenses/LGPL-2.1.txt',
}
tracked_candidates = {}
for file in repo.rglob('*'):
    if '.git' in file.relative_to(repo).parts:
        continue
    if file.is_symlink():
        raise ValueError(f'symlink: {file}')
    if not file.is_file():
        continue
    relative = file.relative_to(repo).as_posix()
    tracked_candidates[relative] = file
    if relative not in listed and relative not in manually_reviewed:
        raise ValueError(f'unlisted file: {relative}')
    if any(part in {'game_files', 'recomp', 'gen', 'analysis', 'recovery',
                    'captures', 'logs', 'cache', 'build-windows'}
           for part in file.relative_to(repo).parts):
        raise ValueError(f'forbidden path: {relative}')
    data = file.read_bytes()
    if b'\x00' in data or data.startswith((b'MZ', b'BM', b'PK\x03\x04')):
        raise ValueError(f'binary content: {relative}')
    if re.search(rb'C:[\\/]Users[\\/]|-----BEGIN [A-Z ]*PRIVATE KEY-----|'
                 rb'\b(?:ghp_|github_pat_|AIza)[A-Za-z0-9_-]{15,}', data, re.I):
        raise ValueError(f'private path or credential pattern: {relative}')
    if relative in listed:
        item = listed[relative]
        assert len(data) == item['bytes'] and hashlib.sha256(data).hexdigest() == item['sha256'], relative

assert set(tracked_candidates) == set(listed) | manually_reviewed
assert not any(p.suffix.lower() in {'.xbe', '.exe', '.dll', '.pdb', '.bmp',
                                    '.png', '.jpg', '.mp4', '.raw', '.bin', '.zip'}
               for p in tracked_candidates.values())
print(f'PASS: {len(tracked_candidates)} text files; {len(listed)} source files match manifest; no forbidden paths/signatures')
