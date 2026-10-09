"""Matched constant-mask experiment, including AC/battery metadata."""
from pathlib import Path
import json,re
root=Path(__file__).resolve().parents[1]
runs=[]
for path in sorted((root/'analysis/development').glob('*/constants73-*')):
 log=path/'nightfire-startup.log'
 if not log.exists():continue
 times={int(n):int(t) for n,t in re.findall(r'\[PRESENT\] published=(\d+) at_ms=(\d+)',log.read_text(errors='replace'))}
 if 300 in times:runs.append((path,times))
end=min(max(t) for p,t in runs)
rows=[]
for path,times in runs:
 seconds=(times[end]-times[300])/1000
 rows.append(dict(run=str(path.relative_to(root)),frames=[300,end],seconds=seconds,fps=(end-300)/seconds,power=json.loads((path/'power-status.json').read_text()),events=(path/'events.log').read_text()))
(root/'analysis/checkpoint-73/gameplay-performance.json').write_text(json.dumps(rows,indent=2))
for row in rows:print(row['run'],round(row['fps'],3),'FPS',row['power'])
