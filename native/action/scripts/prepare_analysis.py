"""Apply only byte-verified Nightfire boundary corrections to the baseline."""
import hashlib
import json
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
baseline = root.parent / 'nightfire-analysis'
binary = (baseline / 'game_files/default.xbe').read_bytes()
assert hashlib.sha256(binary).hexdigest() == 'b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1'
# These addresses are in .text, whose VA-to-file offset is -0x10000.
def raw(va, size):
    return binary[va - 0x10000:va - 0x10000 + size]

assert raw(0x128e6, 7) == bytes.fromhex('ff2495102a0100')
assert raw(0x335bd, 7) == bytes.fromhex('ff2485d0370300')
assert struct.unpack('<4I', raw(0x12a10, 16)) == (0x128ed, 0x12962, 0x1299a, 0x128ed)
assert raw(0x50000, 9) == bytes.fromhex('8b4424108b0883f921')
assert raw(0x5010d, 4) == bytes.fromhex('32c05ec3')
functions = json.loads((baseline / 'disassembly/functions.json').read_text())
removed = {0x12a0e, 0x337ce, 0x50005, 0x5001a}
# Preserve aligned alternate entries; let the original code execute to ret.
extended = {0x50000, 0x50004, 0x50006, 0x50009, 0x50100}
corrected = []
for f in functions:
    address = int(f['start'], 16)
    if address in removed:
        continue
    if address in extended:
        f = dict(f, end='0x00050111', size=0x50111-address,
                 detection_method='nightfire_verified_boundary')
    corrected.append(f)
(root / 'analysis/functions.json').write_text(json.dumps(corrected, indent=2))
(root / 'analysis/boundary-corrections.json').write_text(json.dumps({
    'removed_candidates': [hex(a) for a in sorted(removed)],
    'bounded_candidates': [hex(a) for a in sorted(extended)],
    'bounded_end': '0x50111',
    'note': 'Two table/padding seeds and two mid-instruction seeds removed. Other uncertain candidates remain.'
}, indent=2))
print(f'Prepared {len(corrected)} candidates; byte assertions passed.')
