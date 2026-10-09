"""Read archive directories only, with bounds checks; do not extract assets."""
from pathlib import Path
import json, struct
root = Path(__file__).resolve().parents[1]
assets = root.parent / 'nightfire-port/game_files/driving'
result = []
for path in sorted(assets.iterdir()):
    with path.open('rb') as stream:
        header = stream.read(16)
        if header[:4] != b'BIGF':
            continue
        size = path.stat().st_size
        declared = {order: int.from_bytes(header[4:8], order) for order in ('little', 'big')}
        count, end = struct.unpack('>II', header[8:])
        assert 16 <= end <= min(size, 16 * 1024 * 1024), (path.name, end)
        directory = header + stream.read(end - 16)
    entries, cursor = [], 16
    for _ in range(count):
        assert cursor + 8 <= end
        offset, length = struct.unpack_from('>II', directory, cursor)
        cursor += 8
        terminator = directory.index(0, cursor)
        name = directory[cursor:terminator].decode('latin1')
        cursor = terminator + 1
        assert offset >= end and offset + length <= size, (path.name, name, offset, length)
        entries.append({'name': name, 'offset': offset, 'bytes': length})
    result.append({'archive': path.name, 'bytes': size, 'header_size_candidates': declared, 'entries': entries})
out = root / 'analysis/archive-directories.json'
out.write_text(json.dumps(result, indent=2), encoding='utf-8')
print(f'{len(result)} archive directories; {sum(len(a["entries"]) for a in result)} bounded entries')
for archive in result:
    if archive['archive'] in ('misc.viv', 'mis01.viv'):
        matches = [e['name'] for e in archive['entries'] if any(s in e['name'].lower() for s in ('paris', 'loading', '.ini', 'intro'))]
        print(archive['archive'], matches[:50])
