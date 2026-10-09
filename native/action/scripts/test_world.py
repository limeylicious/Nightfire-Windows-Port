"""Focused development runs through run-windows.cmd; normal engine initialization.
Use existing full-startup scripts periodically, or when testing menus/videos.
"""
from pathlib import Path
import argparse,os,subprocess,time,shutil,json,hashlib,ctypes
def power_status():
 class Power(ctypes.Structure):
  _fields_=[('ac',ctypes.c_ubyte),('flags',ctypes.c_ubyte),('percent',ctypes.c_ubyte),('saver',ctypes.c_ubyte),('remaining',ctypes.c_uint32),('full',ctypes.c_uint32)]
 status=Power()
 if not ctypes.windll.kernel32.GetSystemPowerStatus(ctypes.byref(status)):return None
 return {key:getattr(status,key) for key in ('ac','percent','saver')}
p=argparse.ArgumentParser()
p.add_argument('--seconds',type=int,default=300)
p.add_argument('--benchmark',action='store_true')
p.add_argument('--manual',action='store_true',help='normal keyboard/mouse/controller input for a user play test')
p.add_argument('--exchange-section',type=int,choices=(1,2,3,4),default=1,help='explicit fresh development section; later sections do not preserve door carryover')
p.add_argument('--mission-id',choices=('07000005','07000001','07000009','0700000C','07000011','07000014','0700001B'),help='CP137 explicit fresh PAL action mission; requires a CP137 executable')
p.add_argument('--label',default='exchange')
p.add_argument('--executable',type=Path,help='saved reference executable; current build by default')
p.add_argument('--cpu-vertices',action='store_true',help='comparison path: transform vertices on CPU, retain GPU rasterization')
p.add_argument('--capture-world',action='store_true',help='bounded first world frame draw-state capture; not for FPS measurements')
a=p.parse_args()
if not 30<=a.seconds<=1800:raise ValueError('seconds must be 30..1800')
if not a.label.replace('-','').replace('_','').isalnum():raise ValueError('simple label required')
root=Path(__file__).resolve().parents[1];base=root/'analysis/development';base.mkdir(exist_ok=True)
executable=(a.executable or root/'build-windows/Debug/nightfire_diagnostic.exe').resolve()
if not executable.is_file() or executable.suffix.lower()!='.exe':raise ValueError('existing executable required')
archive=base/time.strftime('%Y%m%d-%H%M%S-')/a.label;archive.mkdir(parents=True)
(base/'current-run.txt').write_text(str(archive));buttons=archive/'buttons.txt';buttons.write_text('0')
power_record={'start':power_status()}
(archive/'power-status.json').write_text(json.dumps(power_record,indent=2))
env={k.upper():v for k,v in os.environ.items()}
env['NIGHTFIRE_EXECUTABLE']=str(executable)
env['NIGHTFIRE_HISTORY116_DIR']=str(archive.resolve())
env['NIGHTFIRE_DEV_EXCHANGE_SECTION']=str(a.exchange_section)
env.pop('NIGHTFIRE_DEV_MISSION137',None)
if a.mission_id:
 if a.exchange_section!=1:raise ValueError('mission-id cannot select an Exchange subsection')
 env['NIGHTFIRE_DEV_MISSION137']=a.mission_id
env.update(NIGHTFIRE_DEV_START_EXCHANGE='1',NIGHTFIRE_INPUT_TEST='1',NIGHTFIRE_INPUT_TEST_FILE=str(buttons),RECOMP_WATCHDOG_SECS=str(a.seconds),NIGHTFIRE_VERTEX_PROGRAM='1',NIGHTFIRE_DEPTH='1',NIGHTFIRE_PROFILE='1',NIGHTFIRE_GPU_DIAGNOSTICS='1',NIGHTFIRE_HW_GPU='1',NIGHTFIRE_GPU_VERTEX='1',NIGHTFIRE_TEXTURE_FILTER='1',NIGHTFIRE_TEXTURE_CACHE='1',NIGHTFIRE_NATIVE_VIDEO_AUDIO='1')
if a.cpu_vertices:env['NIGHTFIRE_GPU_VERTEX']='0'
# Existing test input has timed menu-navigation pulses. Defer those beyond this
# bounded direct-level run; file-backed controls remain active immediately.
env['NIGHTFIRE_INPUT_TEST_DELAY_MS']=str(a.seconds*1000)
if a.manual:
 for key in list(env):
  if key.startswith('NIGHTFIRE_INPUT_TEST'):env.pop(key)
 env['NIGHTFIRE_NATIVE_INPUT']='1'
for key in ('NIGHTFIRE_INPUT_TEST_READY_START','NIGHTFIRE_REPLAY_ROUTE','NIGHTFIRE_WORLD_DRAW_CAPTURE','NIGHTFIRE_WORLD_CAPTURE_IMAGES','NIGHTFIRE_SKY_TRACE'):env.pop(key,None)
if a.capture_world:
 if a.benchmark:raise ValueError('draw capture changes timing; use a separate run')
 env['NIGHTFIRE_WORLD_DRAW_CAPTURE']='1'
(archive/'run-metadata.json').write_text(json.dumps(dict(executable=str(executable),sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),seconds=a.seconds,benchmark=a.benchmark,development_entry='PAL action mission137' if a.mission_id else 'Exchange',mission_id=a.mission_id,exchange_section=a.exchange_section,fresh_start_without_carryover=True,environment={k:v for k,v in env.items() if k.startswith(('NIGHTFIRE_','RECOMP_'))}),indent=2))
log=root/'logs/nightfire-startup.log';start=time.monotonic()
def report(message):
 line=f'{time.monotonic()-start:.1f}s {message}';print(line,flush=True)
 with (archive/'events.log').open('a') as f:f.write(line+'\n')
def capture(name):
 after=time.time();until=time.monotonic()+30
 while time.monotonic()<until:
  fresh=[f for f in (root/'captures/screenshots').glob('nightfire-framebuffer-flip-*.bmp') if f.stat().st_mtime>after]
  if fresh:
   time.sleep(.4);shutil.copy2(max(fresh,key=lambda f:f.stat().st_mtime),archive/(name+'.bmp'));report('Captured '+name);return
  if proc.poll() is not None:raise RuntimeError('game exited before capture')
  time.sleep(.25)
 raise TimeoutError('no fresh published frame')
with (archive/'console.log').open('w') as output:
 proc=subprocess.Popen(['cmd.exe','/d','/c','run-windows.cmd'],cwd=root,env=env,stdout=output,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
 try:
  time.sleep(2);until=time.monotonic()+90
  while time.monotonic()<until:
   data=log.read_text(errors='replace') if log.exists() else ''
   if '[WORLD-DEPTH] enable=1 func=203' in data:
    if a.mission_id and ('[DEV-START] PAL action mission137 level='+a.mission_id) not in data:raise RuntimeError('Executable did not select requested mission137')
    assert '[DEV-START]' in data;report('WORLD READY');break
   if proc.poll() is not None:raise RuntimeError(f'game exited {proc.returncode} during development startup')
   time.sleep(.25)
  else:raise TimeoutError('development startup did not reach world')
  capture('start')
  if a.benchmark:
   report('PERFORMANCE SAMPLE START');capture('sample-start');time.sleep(60);capture('sample-end');report('PERFORMANCE SAMPLE END')
   subprocess.run(['powershell.exe','-NoProfile','-Command','Get-Process nightfire_diagnostic -ErrorAction SilentlyContinue | ForEach-Object { $_.CloseMainWindow() }'],creationflags=subprocess.CREATE_NO_WINDOW,check=True)
  else:report('Normal keyboard/mouse/controller input enabled' if a.manual else 'Native input file ready: '+str(buttons))
  proc.wait(timeout=max(1,a.seconds+15-(time.monotonic()-start)));report(f'exit={proc.returncode}')
 finally:
  power_record['end']=power_status()
  (archive/'power-status.json').write_text(json.dumps(power_record,indent=2))
  buttons.write_text('0')
  if proc.poll() is not None:
   output.flush()
   if a.capture_world:
    draw_trace=root/'analysis/checkpoint-34/world-batches.txt'
    if draw_trace.exists():shutil.copy2(draw_trace,archive/'world-batches.txt')
   for f in (log,root/'logs/xbox_kernel.log'):
    if f.exists():shutil.copy2(f,archive/f.name)
   fresh=[f for f in (root/'captures/screenshots').glob('nightfire-framebuffer-flip-*.bmp') if f.stat().st_mtime>time.time()-(time.monotonic()-start)]
   if fresh:shutil.copy2(max(fresh,key=lambda f:f.stat().st_mtime),archive/'final.bmp')

# A completed wrapper is not a successful game run. Preserve the actual game
# exit code after saving evidence, including explicit translation/renderer stops.
raise SystemExit(proc.returncode)
