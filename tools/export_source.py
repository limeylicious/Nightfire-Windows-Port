"""Stage an allowlisted, game-file-free source snapshot from sibling projects.

Run from this private repository. Review `git status` and SOURCE-MANIFEST.json
before any push. This intentionally does not copy generated game translations.
"""

from __future__ import annotations

import hashlib
import json
import re
import shutil
from pathlib import Path

repo = Path(__file__).resolve().parents[1]
workspace = repo.parent

fixed = [
    'nightfire-port/CMakeLists.txt',
    'nightfire-port/build-windows.cmd',
    'nightfire-port/build-optimized-windows.cmd',
    'nightfire-port/run-windows.cmd',
    'nightfire-port/output-paths.cmd',
    'nightfire-port/src/main.c',
    'nightfire-port/src/nightfire_diagnostics.h',
    'nightfire-port/runtime/renderer/CMakeLists.txt',
    'nightfire-driving/CMakeLists.txt',
    'nightfire-driving/build-windows.cmd',
    'nightfire-driving/run-windows.cmd',
    'nightfire-driving/scripts/generate.py',
]

paths = [workspace / name for name in fixed]
paths.extend((workspace / 'nightfire-driving/src').glob('*.c'))
paths.extend((workspace / 'nightfire-driving/src').glob('*.h'))
for project in ('nightfire-port', 'nightfire-driving'):
    paths.extend((workspace / project / 'runtime').rglob('*.c'))
    paths.extend((workspace / project / 'runtime').rglob('*.h'))

excluded_names = {'recomp_manual.c', 'driving_native247.h', 'driving_admission283.h'}
prohibited_parts = {'game_files', 'recomp', 'gen', 'analysis', 'captures',
                    'logs', 'recovery', 'build-windows', 'generic263'}
secret = re.compile(r'(?:C:[\\/]Users[\\/]|-----BEGIN [A-Z ]*PRIVATE KEY-----|'
                    r'\b(?:ghp_|github_pat_|AIza)[A-Za-z0-9_-]{15,})', re.I)

manifest = []
for source in sorted(set(paths)):
    relative = source.relative_to(workspace)
    if source.name in excluded_names or source.name.startswith('driving_contract'):
        continue
    if any(part.lower() in prohibited_parts for part in relative.parts):
        continue
    if not source.is_file() or source.is_symlink():
        raise ValueError(f'missing or linked input: {relative}')
    if source.stat().st_size > 260_000:
        raise ValueError(f'oversized input requires review: {relative}')
    data = source.read_bytes()
    if b'\x00' in data or data.startswith((b'MZ', b'BM', b'PK\x03\x04')):
        raise ValueError(f'binary content: {relative}')
    content = data.decode('utf-8-sig')
    if secret.search(content):
        raise ValueError(f'private path or credential pattern: {relative}')
    output = repo / relative
    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, output)
    manifest.append({'path': relative.as_posix(), 'bytes': len(data),
                     'sha256': hashlib.sha256(data).hexdigest()})

(repo / 'SOURCE-MANIFEST.json').write_text(
    json.dumps({'scope': 'strict source-only export; no game or generated files',
                'file_count': len(manifest), 'files': manifest}, indent=2) + '\n',
    encoding='utf-8')
print(f'Staged {len(manifest)} allowlisted text files; review SOURCE-MANIFEST.json')
