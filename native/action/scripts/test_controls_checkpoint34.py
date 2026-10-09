from pathlib import Path
import time,shutil
root=Path(__file__).resolve().parents[1];art=root/'analysis/checkpoint-34';buttons=art/'buttons.txt'
def capture(name):
 after=time.time();until=time.monotonic()+30
 while time.monotonic()<until:
  files=[p for p in (root/'captures/screenshots').glob('nightfire-framebuffer-flip-*.bmp') if p.stat().st_mtime>after]
  if files:
   latest=max(files,key=lambda p:p.stat().st_mtime);time.sleep(.3);shutil.copy2(latest,art/(name+'.bmp'));print('Captured '+name,flush=True);return
  time.sleep(.25)
 raise TimeoutError(name)
try:
 for name,packet,hold in [('look','0 0 0 24000 0 0 0',6),('walk','0 0 30000 0 0 0 0',6),('fire','0 0 0 0 0 0 255',2),('reload','10000',8),('advance','0 0 30000 0 0 0 0',12)]:
  buttons.write_text(packet);print('Begin '+name,flush=True);time.sleep(hold);buttons.write_text('0');capture(name)
finally:buttons.write_text('0')
