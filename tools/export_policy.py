"""Shared fail-closed checks for the reviewed source-only GitHub snapshot."""

from __future__ import annotations

import json
import re
from pathlib import PurePosixPath

MANUAL_FILES = {
    '.gitignore', '.gitattributes', 'README.md', 'CREDITS.md', 'SOURCE-EXPORT.md',
    'SOURCE-MANIFEST.json', 'tools/export_source.py', 'tools/verify_export.py',
    'tools/export_policy.py', 'tools/source_allowlist.json', 'tools/test_export_policy.py',
    'licenses/xboxrecomp-MIT.txt', 'licenses/xboxrecomp-NOTICE.txt',
    'docs/STATUS-510.md', 'experiments/driving-510/README.md',
    'experiments/driving-lean/README.md', 'native/README.md', 'release/README.md',
}
DENIED_NAMES = {
    'recomp_manual.c', 'driving_native247.h', 'driving_admission283.h',
    'driving_native_effects350.h', 'driving_native_remaining352.h',
    'driving_sprite_contract396.h', 'driving_font_plan348.h', 'driving_fog455.h',
}
DENIED_PARTS = {
    'game_files', 'recomp', 'gen', 'analysis', 'captures', 'logs', 'recovery',
    'cache', 'releases', 'generic263', 'packaging', 'node_modules', '__pycache__',
    'sessions', 'linked-sessions', 'runs', 'saves', 'lockstep',
}
# Native workspaces: source prefix -> repository prefix, relative paths kept.
NATIVE_GROUPS = {
    'native-driving': ('nightfire-driving-native/', 'native/driving/'),
    'native-action': ('nightfire-port-native/', 'native/action/'),
    'native-launch': ('native-driving/', 'native/launchers/'),
}
# The owner's alpha folder (launchers and set-up scripts only; Setup copies engines built locally).
RELEASE_GROUPS = {
    'release-alpha': ('releases/Nightfire-PC-Alpha/', 'release/alpha/'),
}
ALLOWED_SUFFIXES = {'.c', '.h', '.inc', '.py', '.cmd', '.txt', '.md', '.json', '.cmake'}
SECRET = re.compile(
    rb'[A-Z]:[\\/]+Users[\\/]+|/' rb'Users/[^/\s]+/|/' rb'home/[^/\s]+/|'
    rb'-----BEGIN (?:[A-Z ]+ )?PRIVATE KEY-----|'
    rb'\b(?:ghp_|github_pat_|AIza)[A-Za-z0-9_-]{15,}|'
    rb'\bsk-(?:proj-|ant-)?[A-Za-z0-9_-]{24,}|'
    rb'\bxox[baprs]-[A-Za-z0-9-]{15,}|\bAKIA[0-9A-Z]{16}\b|'
    rb'https?://[^\s/@:]+:[^\s/@]+@', re.I)


def relative_name(name: str) -> str:
    path = PurePosixPath(name)
    if (not name or path.is_absolute() or '\\' in name or ':' in name
            or any(part in {'', '.', '..'} for part in name.split('/'))):
        raise ValueError(f'not a safe relative path: {name}')
    return path.as_posix()


def check_destination(name: str) -> None:
    path = PurePosixPath(relative_name(name))
    parts = [part.lower() for part in path.parts]
    if any(part in DENIED_PARTS for part in parts) or any(part.startswith('build') for part in parts[:-1]):
        raise ValueError(f'forbidden destination: {name}')
    basename = path.name.lower()
    if basename in DENIED_NAMES or basename.startswith('driving_contract'):
        raise ValueError(f'excluded derived input: {name}')
    if path.suffix.lower() not in ALLOWED_SUFFIXES and name not in {'.gitignore', '.gitattributes'}:
        raise ValueError(f'forbidden file type: {name}')


def check_text(name: str, data: bytes) -> None:
    check_destination(name)
    # 270 kB: native/driving/runtime/kernel_bridge.c (261 kB) was reviewed on 2026-10-09.
    if len(data) > 270_000 and name != 'SOURCE-MANIFEST.json':
        raise ValueError(f'oversized text requires separate review: {name}')
    if b'\0' in data or data.startswith((b'MZ', b'XBEH', b'BM', b'PK\x03\x04', b'DDS ', b'\x89PNG')):
        raise ValueError(f'binary content: {name}')
    data.decode('utf-8-sig')
    if SECRET.search(data):
        # Do not print a matching credential or private path.
        raise ValueError(f'private path or credential pattern: {name}')


def read_allowlist(data: bytes) -> list[dict]:
    config = json.loads(data)
    if config.get('schema') != 'reviewed-source-export-v2':
        raise ValueError('unknown allowlist schema')
    seen = set()
    for row in config['files']:
        source = relative_name(row['source'])
        destination = relative_name(row['path'])
        check_destination(destination)
        if destination in seen or destination in MANUAL_FILES:
            raise ValueError(f'duplicate/reserved destination: {destination}')
        seen.add(destination)
        group = row['group']
        if group == 'normal-source':
            if source != destination or not source.startswith(('nightfire-port/', 'nightfire-driving/')):
                raise ValueError(f'invalid normal source mapping: {destination}')
            check_destination(source)
        elif group == 'driving-510-bridge':
            prefix = 'nightfire-driving/analysis/perf510/build-source/'
            suffix = source.removeprefix(prefix)
            if (not source.startswith(prefix) or not suffix.startswith(('runtime/', 'src/'))
                    or destination != 'experiments/driving-510/' + suffix
                    or PurePosixPath(suffix).suffix not in {'.c', '.h'}):
                raise ValueError(f'invalid bridge mapping: {destination}')
        elif group == 'driving-510-tool':
            prefix = 'nightfire-driving/analysis/perf510/'
            suffix = source.removeprefix(prefix)
            if (not source.startswith(prefix) or '/' in suffix or not suffix.endswith('.py')
                    or destination != 'experiments/driving-510/tools/' + suffix):
                raise ValueError(f'invalid tool mapping: {destination}')
        elif group == 'driving-lean':
            prefix = 'nightfire-driving-lean/'
            suffix = source.removeprefix(prefix)
            if not source.startswith(prefix) or destination != 'experiments/driving-lean/' + suffix:
                raise ValueError(f'invalid lean mapping: {destination}')
            check_destination(source)
        elif group in NATIVE_GROUPS:
            prefix, target = NATIVE_GROUPS[group]
            suffix = source.removeprefix(prefix)
            if not source.startswith(prefix) or destination != target + suffix:
                raise ValueError(f'invalid native mapping: {destination}')
            check_destination(source)
        elif group in RELEASE_GROUPS:
            # The source sits under the workspace's releases folder, so only the destination is
            # checked against the denied names; the exact prefix keeps it to that one folder.
            prefix, target = RELEASE_GROUPS[group]
            suffix = source.removeprefix(prefix)
            if not source.startswith(prefix) or destination != target + suffix:
                raise ValueError(f'invalid release mapping: {destination}')
        else:
            raise ValueError(f'unknown source group: {group}')
    return config['files']
