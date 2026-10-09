"""Bounded controller-file action and a fresh, game-produced capture."""
from pathlib import Path
import argparse, shutil, time

p=argparse.ArgumentParser()
p.add_argument('name')
p.add_argument('packet')
p.add_argument('seconds',type=float)
a=p.parse_args()
if not 0 <= a.seconds <= 60:
    raise ValueError('Use a bounded action of at most 60 seconds')
root=Path(__file__).resolve().parents[1]
art=root/'analysis/checkpoint-36'
buttons=art/'buttons.txt'
archive=Path((art/'current-run.txt').read_text().strip()) if (art/'current-run.txt').exists() else art
line=f'{time.time():.3f} {a.name}: {a.packet} for {a.seconds}s\n'
for folder in {art,archive}:
    with (folder/'actions.log').open('a') as log:log.write(line)
try:
    buttons.write_text(a.packet)
    time.sleep(a.seconds)
finally:
    buttons.write_text('0')
after=time.time()
until=time.monotonic()+40
while time.monotonic()<until:
    files=[f for f in (root/'captures/screenshots').glob('nightfire-framebuffer-flip-*.bmp')
           if f.stat().st_mtime>after]
    if files:
        latest=max(files,key=lambda f:f.stat().st_mtime)
        time.sleep(.4)
        target=art/(a.name+'.bmp')
        shutil.copy2(latest,target)
        if archive!=art:shutil.copy2(latest,archive/target.name)
        print(target,flush=True)
        break
    time.sleep(.5)
else:
    raise TimeoutError('No fresh published framebuffer')
