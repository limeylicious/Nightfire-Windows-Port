"""Summarize the last complete native census and check same-scope totals."""
from pathlib import Path
import json
import re
import sys

run = Path(sys.argv[1]).resolve()
reports = []
pending = None
for line in (run / 'driving-startup.log').read_text(encoding='utf-8', errors='replace').splitlines():
    if line.startswith('[BATCH-CENSUS255] reason='):
        fields = dict(re.findall(r'(\w+)=([^\s]+)', line))
        reason = fields.pop('reason')
        row = {k: float(v) if k.endswith('_ms') else int(v) for k, v in fields.items()}
        if reason == 'all':
            pending = {'all': row, 'reasons': {}, 'capacity_causes': {}}
        elif pending is not None:
            key = 'capacity_causes' if reason.startswith('capacity-') else 'reasons'
            pending[key][reason] = row
    elif pending is not None and line.startswith('[BATCH-CENSUS255] histogram_draws_'):
        pending['histogram_1_to_16_then_over16'] = [int(x) for x in line.split('=', 1)[1].split()[0].split(',')]
    elif pending is not None and line.startswith('[BATCH-CAPACITY260] histogram_draws_'):
        pending['histogram_17_to_32_then_over32'] = [int(x) for x in line.split('=', 1)[1].split()[0].split(',')]
    elif pending is not None and line.startswith('[BATCH236-TIME]'):
        fields = dict(re.findall(r'(\w+)=([\d.]+)', line))
        pending['same_scope_batch236'] = {k: float(v) if k.endswith('_ms') else int(v) for k, v in fields.items()}
        reports.append(pending)
        pending = None

assert reports, 'No complete census followed by its existing batch report'
r = reports[-1]
total, reasons, capacity = r['all'], r['reasons'], r['capacity_causes']
for key in ('flushes', 'draws', 'queue_bytes_sum'):
    assert sum(row[key] for row in reasons.values()) == total[key], key
    assert sum(row[key] for row in capacity.values()) == reasons.get('capacity', {}).get(key, 0), ('capacity', key)
for key in ('backend_ms', 'publish_ms', 'total_ms'):
    assert abs(sum(row[key] for row in reasons.values()) - total[key]) <= (len(reasons) + 1) * .0011, key
assert sum(r['histogram_1_to_16_then_over16']) == total['flushes']
h16 = r['histogram_1_to_16_then_over16']
assert len(h16) == 17 and all(n >= 0 for n in h16)
h32 = r.get('histogram_17_to_32_then_over32', [0] * 17)
assert len(h32) == 17 and all(n >= 0 for n in h32)
assert sum(h32) == h16[16], 'Missing or inconsistent exact >16 histogram'
assert h32[16] == 0, 'Unexpected >32 batch; do not invent its draw count'
assert sum((i + 1) * n for i, n in enumerate(h16[:16])) + sum((i + 17) * n for i, n in enumerate(h32[:16])) == total['draws'], 'Exact histogram draw total mismatch'
for key in ('flushes', 'draws'):
    assert total[key] == r['same_scope_batch236'][key], key
for census_key, batch_key in (('backend_ms', 'flush_backend_ms'), ('publish_ms', 'publish_ms'), ('total_ms', 'flush_total_ms')):
    assert abs(total[census_key] - r['same_scope_batch236'][batch_key]) <= .0011, census_key
r['complete_reports'] = len(reports)
r['checks'] = 'Reason/capacity partitions, histogram and same-scope completed batch totals agree.'
r['limits'] = ('Cumulative through the last complete report, with a possible unreported tail. '
               'Queue bytes are accounting bytes, not transfer traffic. Wall time includes CPU and waits. '
               'Capacity counts alone do not predict larger-batch performance.')
(run / 'batch-census255.json').write_text(json.dumps(r, indent=2), encoding='utf-8')
print(json.dumps(r, indent=2))
