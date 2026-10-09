"""Compare shared published-frame intervals and opt-in transfer timings."""
from pathlib import Path
import json
import re
import sys

root=Path(__file__).resolve().parents[1]
prefix=sys.argv[1] if len(sys.argv)>1 else 'transfer72'
runs=[]
for path in sorted((root/'analysis/development').glob('*/'+prefix+'-*')):
    log=path/'nightfire-startup.log'
    if not log.exists():continue
    text=log.read_text(errors='replace')
    times={int(n):int(t) for n,t in re.findall(r'\[PRESENT\] published=(\d+) at_ms=(\d+)',text)}
    if 300 not in times:continue
    matches=re.findall(r'\[GPU-TRANSFER\] count=(\d+) ms [^\n:]+:([^\n]+)',text)
    transfer=dict(zip(['enqueue','color_wait','color_copy','depth_wait','depth_pack','color_upload','depth_write_map','depth_upload'],map(float,matches[-1][1].split()))) if matches else None
    runs.append((path,times,transfer,int(matches[-1][0]) if matches else None))
end=min(max(times) for _,times,_,_ in runs)
rows=[]
for path,times,transfer,count in runs:
    seconds=(times[end]-times[300])/1000
    rows.append(dict(run=str(path.relative_to(root)),frames=[300,end],seconds=seconds,
                     fps=(end-300)/seconds,transfer_count=count,transfer_ms=transfer,
                     events=(path/'events.log').read_text()))
(root/('analysis/checkpoint-72/'+prefix+'-comparison.json')).write_text(json.dumps(rows,indent=2))
for row in rows:print(row['run'],round(row['fps'],3),'FPS',row['transfer_ms'])
