"""Assemble action-address-map.json from the work-*.json outputs of
d3d_action_map.py, d3d_action_globals.py and d3d_action_survey.py (run those first)."""
import json
from pathlib import Path
HERE = Path(__file__).resolve().parent
rm = json.load(open(HERE / 'work-replace-map.json'))
gl = json.load(open(HERE / 'work-globals.json'))
sv = json.load(open(HERE / 'work-survey.json'))
fm = json.load(open(HERE / 'work-full-match.json'))
KEY = ['0x175418', '0x175424', '0x175428', '0x175628', '0x175798', '0x178500', '0x1758D0', '0x176528',
       '0x1786D8', '0x1785C0', '0x176408', '0x18E888', '0x189C04', '0x1D3558']
out = dict(
    meta=dict(driving_xbe_sha256=fm['meta']['driving_sha'], action_xbe_sha256=fm['meta']['action_sha'],
              method='masked machine-code comparison (capstone); image addresses and branch targets masked; '
                     'globals from aligned operands of exactly matched routines',
              note='Action VAs are leads until exercised in a run; nothing in nightfire-port was changed.'),
    replace_set=rm,
    globals={k: dict(action=gl[k]['action'], name=gl[k]['name'], how=gl[k]['how']) for k in KEY if k in gl},
    all_native_source_addresses={k: dict(action=v['action'], how=v['how'], name=v['name']) for k, v in gl.items()},
    action_only_hardware_routines=sv['residual_hw'],
    all_driving_d3d_function_matches=fm['match'],
)
json.dump(out, open(HERE / 'action-address-map.json', 'w'), indent=1)
print('wrote action-address-map.json:', len(rm), 'replace-set entries,', len(out['globals']), 'key globals,', len(gl), 'source addresses')
