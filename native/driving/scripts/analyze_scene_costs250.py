"""Read saved coarse timings. Sparse endpoints are approximate, not GPU timings."""
from pathlib import Path
import copy
import json
import re
import sys

run = Path(sys.argv[1]).resolve()
latest = {}
frames = []
for line in (run / 'driving-startup.log').read_text(encoding='utf-8', errors='replace').splitlines():
    match = re.match(r'\[(TIMING250|OFFSCREEN250|SOFTWARE250|QUEUE251|PRESENT227)\]', line)
    if not match:
        continue
    marker = match.group(1)
    values = {k: float(v) for k, v in re.findall(r'\b(\w+)=(-?\d+(?:\.\d+)?)', line)}
    if marker == 'PRESENT227':
        frames.append(dict(frame=values['frame'], elapsed_ms=values['elapsed_ms'],
                           last30_ms=values['last30_ms'], counters=copy.deepcopy(latest)))
    else:
        if marker == 'QUEUE251':
            marker += '-accepted' if 'result=accepted' in line else '-refused'
        latest[marker] = values

intervals = []
for before, after in zip(frames, frames[1:]):
    delta = {}
    for marker in before['counters'].keys() & after['counters'].keys():
        a, b = before['counters'][marker], after['counters'][marker]
        delta[marker] = {k: round(b[k] - a[k], 3) for k in a.keys() & b.keys()
                         if k.endswith('_ms') or k in ('calls', 'drains', 'accepted', 'refused')}
    intervals.append(dict(from_frame=before['frame'], to_frame=after['frame'],
                          presentation_ms=after['elapsed_ms'] - before['elapsed_ms'],
                          approximate_sparse_counter_deltas=delta))

q = latest.get('QUEUE251-accepted', {})
phases = ('plan_ms', 'resources_ms', 'allocations_ms', 'textures_ms', 'vertices_ms',
          'expand_ms', 'stage_cleanup_ms')
report = dict(run=str(run), last_counters=latest,
              preparation_phase_percent={k: round(100 * q[k] / q['total_ms'], 2)
                                         for k in phases if q.get('total_ms') and k in q},
              intervals=intervals,
              limits='Sparse reports are not synchronized with presentation endpoints. '
                     'Phase totals overlap other inclusive counters; never sum all counters. '
                     'Elapsed CPU/driver-wait timings are not GPU execution duration. '
                     'Method-wrapper software coverage requires the noted timing250 audit.')
(run / 'cost-analysis250.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
