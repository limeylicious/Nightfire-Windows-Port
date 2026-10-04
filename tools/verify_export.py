"""Verify all working files, or the complete proposed Git index with --staged."""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
from export_policy import MANUAL_FILES, check_text, read_allowlist


def verify(files: dict[str, bytes]) -> tuple[int, int]:
    entries = read_allowlist(files['tools/source_allowlist.json'])
    allow = {entry['path']: entry for entry in entries}
    manifest = json.loads(files['SOURCE-MANIFEST.json'])
    if manifest.get('schema') != 'reviewed-source-export-v2' or not manifest['scope'].startswith('strict source-only'):
        raise ValueError('unexpected manifest scope/schema')
    listed = {entry['path']: entry for entry in manifest['files']}
    if len(listed) != len(manifest['files']) or len(listed) != manifest['file_count']:
        raise ValueError('duplicate entries or incorrect manifest count')
    if set(listed) != set(allow) or set(files) != set(allow) | MANUAL_FILES:
        raise ValueError('file inventory differs from reviewed allowlist/manual files')
    for name, data in files.items():
        check_text(name, data)
        if name in listed:
            entry, approved = listed[name], allow[name]
            digest = hashlib.sha256(data).hexdigest()
            if (len(data) != entry['bytes'] or digest != entry['sha256']
                    or digest != approved['reviewed_sha256']
                    or entry['source'] != approved['source'] or entry['group'] != approved['group']):
                raise ValueError(f'export provenance mismatch: {name}')
    return len(files), len(listed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged', action='store_true', help='check Git blobs, not just working-tree files')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    files = {}
    if args.staged:
        rows = subprocess.check_output(['git', 'ls-files', '--stage', '-z'], cwd=repo).split(b'\0')
        for row in filter(None, rows):
            metadata, path = row.split(b'\t', 1)
            mode, oid, stage = metadata.split()
            if mode not in {b'100644', b'100755'} or stage != b'0':
                raise ValueError('linked/submodule/unmerged index entry')
            files[path.decode('utf-8')] = subprocess.check_output(['git', 'cat-file', 'blob', oid.decode()], cwd=repo)
    else:
        for file in repo.rglob('*'):
            relative = file.relative_to(repo)
            if '.git' in relative.parts:
                continue
            if file.is_symlink() or (hasattr(file, 'is_junction') and file.is_junction()):
                raise ValueError(f'linked export path: {relative.as_posix()}')
            if file.is_file():
                files[relative.as_posix()] = file.read_bytes()
    total, sources = verify(files)
    print(f'PASS ({"Git index" if args.staged else "working tree"}): {total} text files; {sources} reviewed source mappings; hashes and exclusion checks match.')


if __name__ == '__main__':
    main()
