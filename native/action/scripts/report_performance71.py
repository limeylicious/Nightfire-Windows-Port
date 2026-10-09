"""Report matched published-frame intervals for the vertex-fetch experiment."""
from pathlib import Path
import json
import re
import sys

root = Path(__file__).resolve().parents[1]
runs = []
prefix = sys.argv[1] if len(sys.argv)>1 else 'fetch71'
for path in sorted((root / 'analysis/development').glob('*/'+prefix+'-*')):
    logfile = path / 'nightfire-startup.log'
    if not logfile.exists():
        continue
    times = {int(n): int(t) for n, t in re.findall(
        r'\[PRESENT\] published=(\d+) at_ms=(\d+)', logfile.read_text(errors='replace'))}
    if 300 in times:
        runs.append((path, times))
end = min(max(times) for _, times in runs)
rows = []
for path, times in runs:
    seconds = (times[end] - times[300]) / 1000
    rows.append(dict(run=str(path.relative_to(root)), frames=[300, end],
                     seconds=seconds, fps=(end-300)/seconds,
                     events=(path/'events.log').read_text()))
(root / ('analysis/checkpoint-71/'+prefix+'-performance.json')).write_text(json.dumps(rows, indent=2))
for row in rows:
    print(row['run'], row['frames'], round(row['fps'], 3), 'FPS')
