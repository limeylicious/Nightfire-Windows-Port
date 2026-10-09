"""Interpret a saved297 sample; never treat Draw time as pixel-shader-only time."""
from pathlib import Path
import json
import re
import sys

run = Path(sys.argv[1]).resolve()
log = (run / 'driving-startup.log').read_text(encoding='utf-8', errors='replace')
def fields(line):
    return dict(re.findall(r'(\w+)=([^\s]+)', line))
headers = [fields(line) for line in log.splitlines() if line.startswith('[GPU297]')]
assert len(headers) == 1, 'Expected one lifetime297 sample'
header = headers[0]
assert header['status'] == 'valid', header
events = [fields(line) for line in log.splitlines() if line.startswith('[GPU297-EVENT]')]
totals = [fields(line) for line in log.splitlines() if line.startswith('[GPU297-SUM]')]
assert len(totals) == 1 and len(events) == int(header['events'])
t = totals[0]
hz = int(t['gpu_hz'])
outer = {}
for i, e in enumerate(events):
    assert int(e['id']) == i and e['batch'] == header['batch']
    begin, end, parent = (int(e[k]) for k in ('begin', 'end', 'parent'))
    assert end >= begin
    duration = (end - begin) * 1000 / hz
    assert abs(duration - float(e['ms'])) < 0.000002
    if parent >= 0:
        p = events[parent]
        assert parent < i and int(p['begin']) <= begin <= end <= int(p['end'])
    else:
        outer[e['kind']] = outer.get(e['kind'], 0.0) + duration
material_pairs = {(int(e['lane']), int(e['draw'])) for e in events if e['kind'] == 'material'}
assert material_pairs == {(lane, draw) for lane in range(2) for draw in range(int(header['draws']))}
total = float(t['total_ms'])
assert abs(sum(outer.values()) + float(t['unclassified_ms']) - total) < 0.00001
raw = [int(x) for x in t['cpu_raw'].split('/')]
qpc = int(t['qpc_hz'])
cpu = dict(submission_ms=(raw[1] - raw[0]) * 1000 / qpc,
           existing_map_ms=[(raw[3+2*i] - raw[2+2*i]) * 1000 / qpc
                            for i in range(4) if raw[2+2*i] and raw[3+2*i]],
           through_owned_output_ms=(raw[10] - raw[0]) * 1000 / qpc)
report = dict(header=header, total_gpu_timeline_ms=total, nonoverlapping_gpu_ms=outer,
              unclassified_gpu_ms=float(t['unclassified_ms']), cpu=cpu,
              nested_kernel_ms={k: float(v) for k,v in t.items()
                                if k.endswith('_ms') and k.startswith(('depth', 'pack'))},
              events=events,
              limitations=['One private batch, not a whole frame or FPS benchmark.',
                           'Material Draw spans include vertex/pixel, raster/depth/blend, memory and scheduling.',
                           'Kernel timings are nested within envelopes; never add them twice.',
                           'Unclassified intervals include CPU submission gaps; not proven idle GPU time.',
                           'CPU through_owned_output ends before caller guest publication.',
                           'Timestamp queries may perturb execution. GPU and CPU clocks are distinct.'])
(run / 'gpu-timeline297.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k != 'events'}, indent=2))
