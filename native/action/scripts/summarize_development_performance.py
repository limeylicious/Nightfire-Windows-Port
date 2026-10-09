"""Compare focused stationary runs using their common published-frame interval."""
from pathlib import Path
import argparse,json,re
p=argparse.ArgumentParser()
p.add_argument('runs',nargs='+',type=Path)
p.add_argument('--output',type=Path)
a=p.parse_args()
samples=[]
for run in a.runs:
 text=(run/'nightfire-startup.log').read_text(errors='replace')
 if '[DEV-START]' not in text:raise ValueError(f'not a focused run: {run}')
 if 'PERFORMANCE SAMPLE END' not in (run/'events.log').read_text():raise ValueError(f'incomplete benchmark: {run}')
 frames={int(n):int(ms) for n,ms in re.findall(r'\[PRESENT\] published=(\d+) at_ms=(\d+)',text)}
 if len(frames)<5:raise ValueError('too few samples')
 samples.append((run,frames))
# Remove two initial samples (loading/first frames); compare the same frame range
# rather than assuming a CPU vertex count, which can legitimately be zero on GPU.
common=set.intersection(*(set(sorted(f)[2:]) for _,f in samples))
selected=sorted(common)
if len(selected)<2:raise ValueError('too few common samples')
first,last=selected[0],selected[-1]
results=[]
for run,frames in samples:
 duration=(frames[last]-frames[first])/1000
 results.append(dict(run=str(run),first_present=first,last_present=last,seconds=duration,
  published_fps=(last-first)/duration,
  sample_intervals_fps=[1000*(y-x)/(frames[y]-frames[x]) for x,y in zip(selected,selected[1:])]))
out=json.dumps(results,indent=2)
print(out)
if a.output:a.output.write_text(out+'\n')
