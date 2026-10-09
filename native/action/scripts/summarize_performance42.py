from pathlib import Path
import re,json
root=Path(__file__).resolve().parents[1]
results={}
for mode in ('cpu','gpu','gpu-compact','gpu-indexed','gpu-address','gpu-state-display','diagnostic-lookup','video-map-cache','inactive-audio','optimized-game'):
 base=root/'analysis/checkpoint-42'/mode
 if not (base/'current-run.txt').exists():continue
 run=Path((base/'current-run.txt').read_text().strip());p=run/'nightfire-startup.log'
 if not p.exists():continue
 text=p.read_text(errors='replace')
 frames={int(n):int(ms) for n,ms in re.findall(r'\[PRESENT\] published=(\d+) at_ms=(\d+)',text)}
 world={int(n):(int(v),float(ms)) for n,v,ms in re.findall(r'\[WORLD-PROFILE\] present=(\d+) vertices=(\d+) vertex_ms=([\d.]+)',text)}
 eligible=[n for n in sorted(frames) if n in world and world[n][0]>0]
 # Discard the first two world samples to remove loading/first-frame work.
 selected=eligible[2:]
 if len(selected)<2:raise RuntimeError('too few stationary world samples')
 a,b=selected[0],selected[-1];duration=(frames[b]-frames[a])/1000
 fps=(b-a)/duration
 results[mode]=dict(run=str(run),first_present=a,last_present=b,seconds=duration,published_fps=fps,cpu_vertex_ms_per_frame=(world[b][1]-world[a][1])/(b-a),sample_intervals_fps=[100000/(frames[y]-frames[x]) for x,y in zip(selected,selected[1:])])
 print(mode,json.dumps(results[mode],indent=2))
if 'cpu' in results:
 for mode in ('gpu','gpu-compact','gpu-indexed','gpu-address','gpu-state-display','diagnostic-lookup','video-map-cache','inactive-audio','optimized-game'):
  if mode in results:print(mode,'FPS ratio',results[mode]['published_fps']/results['cpu']['published_fps'])
(root/'analysis/checkpoint-42/performance.json').write_text(json.dumps(results,indent=2))
