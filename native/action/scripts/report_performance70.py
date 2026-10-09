"""Compare common published-frame intervals, not UI snapshots or unlike scenes."""
from pathlib import Path
import re,json
root=Path(__file__).resolve().parents[1]
labels=['movie70-separated','movie70-reference68','movie70-reference68-paused','movie70-separated-paused']
rows=[]
for label in labels:
 dirs=sorted((root/'analysis/development').glob('*/'+label))
 if not dirs:continue
 path=dirs[-1];log=path/'nightfire-startup.log'
 if not log.exists():continue
 text=log.read_text(errors='replace');times={int(n):int(t) for n,t in re.findall(r'\[PRESENT\] published=(\d+) at_ms=(\d+)',text)}
 if 300 not in times or 1300 not in times:continue
 elapsed=(times[1300]-times[300])/1000
 hw=re.findall(r'\[HW-TIME\] begin_ms=([\d.]+) draw_ms=([\d.]+) sync_ms=([\d.]+) texture_ms=([\d.]+)',text)
 rows.append(dict(label=label,run=str(path.relative_to(root)),frames=[300,1300],seconds=elapsed,fps=1000/elapsed,normal_close='exit=0' in (path/'events.log').read_text(),last_hardware_totals_ms=list(map(float,hw[-1])) if hw else None))
(root/'analysis/checkpoint-70/gameplay-performance.json').write_text(json.dumps(rows,indent=2))
for row in rows:print(row['label'],round(row['fps'],3),'FPS',row['seconds'],'seconds')
