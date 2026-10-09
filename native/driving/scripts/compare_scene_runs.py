"""Read saved scene-summary.json files and compare cadence / normalized costs.

python scripts/compare_scene_runs.py RUN_A RUN_B
No equivalence claim: scene state, temperature and other workloads can differ.
"""
from pathlib import Path
import json
import re
import statistics
import sys

def load(path):
    p = Path(path).resolve()
    s = json.loads((p / 'scene-summary.json').read_text(encoding='utf-8'))
    line = s['final_diagnostics'].get('BATCH244-WALL', '')
    fields = {k: float(v) for k, v in re.findall(r'(\w+)=([0-9.]+)', line)}
    calls = fields.get('draw_calls', 0)
    normalized = {k: round(v / calls, 6) for k, v in fields.items()
                  if calls and k.endswith('_ms') and k != 'prepare_outside_flush_ms'}
    windows = [x['presentations_per_second'] for x in s['presentation_intervals']
               if x['frame'] > 30 and x['presentations_per_second'] is not None]
    return dict(run=str(p), executable=s['result']['exe_sha256'],
                result=s['result'], flags=s['environment']['optional_flags'],
                steady_windows_fps=windows,
                median_window_fps=statistics.median(windows) if windows else None,
                ms_per_lane_draw=normalized,
                final_diagnostics=s['final_diagnostics'])

a, b = map(load, sys.argv[1:3])
changed = {k: [a['flags'].get(k), b['flags'].get(k)]
           for k in sorted(a['flags'].keys() | b['flags'].keys())
           if a['flags'].get(k) != b['flags'].get(k)}
print(json.dumps(dict(a=a, b=b, same_executable=a['executable'] == b['executable'],
                     changed_flags=changed,
                     limits='Same executable and explicit settings improve comparison, '
                            'but do not guarantee identical game state or host conditions. '
                            'Timing subsets overlap; do not sum them. '
                            'CPU/wait time is not isolated GPU time.'), indent=2))
