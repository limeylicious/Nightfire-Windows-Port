"""Summarize matched startup evidence; gaps are logged lower bounds, not a listening test."""
from pathlib import Path
import json,re
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parents[1]
out=root/'analysis/checkpoint-70'
runs=[('software','audio69-startup/run-20260911-214710'),('gpu_shared','movie70-gpu/run-20260911-215709'),('gpu_final_wallpaper_closed','movie70-final-startup/run-20260911-220934')]
rows=[]
for name,folder in runs:
 p=root/'analysis/checkpoint-42'/folder
 text=(p/'nightfire-startup.log').read_text(errors='replace')
 phases=[]
 for line in text.splitlines():
  if '[NATIVE-AUDIO] opened' in line:phases.append(dict(stream_number=len(phases)+1,gaps_ms=[]))
  gap=re.search(r'empty-to-refill=(\d+) ms',line)
  if gap and phases:phases[-1]['gaps_ms'].append(int(gap[1]))
 for phase in phases:
  gaps=phase['gaps_ms'];phase.update(gap_count=len(gaps),gap_sum_ms=sum(gaps),gap_max_ms=max(gaps,default=0))
 events=(p/'events.log').read_text()
 brief=float(re.search(r'([\d.]+)s Letting briefing',events)[1])
 world=float(re.search(r'([\d.]+)s \[WORLD-DEPTH\]',events)[1])
 end=float(re.search(r'([\d.]+)s exit=0',events)[1])
 rows.append(dict(name=name,run=str(p.relative_to(root)),briefing_to_world_seconds=world-brief,world_seconds=world,normal_exit_seconds=end,streams=phases))
a=np.array(Image.open(out/'game-frame-software.bmp').convert('RGB'),dtype=np.int16)
b=np.array(Image.open(out/'game-frame-gpu.bmp').convert('RGB'),dtype=np.int16)
d=np.abs(a-b)
report=dict(runs=rows,game_frame_comparison=dict(pixels=int(a.shape[0]*a.shape[1]),max_channel_error=int(d.max()),channels_over_one=int((d>1).sum()),mean_error=float(d.mean())),limitations=['Single runs; different render workload and wall pacing.','No audio transport, guest clock, codec or PAL translation changes.','User reports small remaining picture/audio hitches and gameplay FPS regression in shared-shader build.','Separate movie/gameplay shaders pass; matched gameplay benchmarks29.934 reference vs30.418 updated after wallpaper paused/closed.'])
(out/'startup-comparison.json').write_text(json.dumps(report,indent=2))
for row in rows:
 print(row['name'],'briefing->world',round(row['briefing_to_world_seconds'],1),'gaps:',[(s['gap_count'],s['gap_sum_ms']) for s in row['streams']])
print(report['game_frame_comparison'])

