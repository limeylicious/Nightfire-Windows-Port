"""Copy only explicitly reviewed source mappings from the sibling private workspace.

Run with python -B tools/export_source.py. No discovery globs, deletion, game
execution or Git mutation. Validate every input before writing any export file.
"""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

sys.dont_write_bytecode = True
from export_policy import check_text, read_allowlist


def main():
    repo = Path(__file__).resolve().parents[1]
    workspace = repo.parent
    entries = read_allowlist((repo / 'tools/source_allowlist.json').read_bytes())
    prepared = []
    for entry in entries:
        source = workspace / entry['source']
        output = repo / entry['path']
        for boundary, path in ((workspace, source), (repo, output)):
            if not path.resolve().is_relative_to(boundary):
                raise ValueError(f'path escapes boundary: {entry["path"]}')
            current = path
            while current != boundary:
                if current.is_symlink() or (hasattr(current, 'is_junction') and current.is_junction()):
                    raise ValueError(f'linked path: {entry["path"]}')
                current = current.parent
        data = source.read_bytes()
        check_text(entry['path'], data)
        digest = hashlib.sha256(data).hexdigest()
        if digest != entry['reviewed_sha256']:
            raise ValueError(f'input changed since review: {entry["source"]}')
        prepared.append((entry, output, data, digest))
    manifest = []
    for entry, output, data, digest in prepared:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(data)
        manifest.append(dict(path=entry['path'], source=entry['source'],
                             group=entry['group'], bytes=len(data), sha256=digest))
    (repo / 'SOURCE-MANIFEST.json').write_text(json.dumps({
        'scope': 'strict source-only export; no game assets or generated game translations',
        'schema': 'reviewed-source-export-v2', 'through_checkpoint': 510,
        'file_count': len(manifest), 'files': manifest,
    }, indent=2) + '\n', encoding='utf-8')
    print(f'Exported {len(manifest)} reviewed text files. Run verify_export.py before staging/pushing.')


if __name__ == '__main__':
    main()
