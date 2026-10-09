"""Private saved-action141 run to observe its original driving handoff.
Uses the existing main-menu/run script settings, with documented bypasses cleared.
Root drives the existing file-input interface; the debugger owns only its child.
"""
from pathlib import Path
import hashlib,json,os,re,subprocess,time
root=Path(__file__).resolve().parents[1];action=root.parent/'nightfire-port'
folder=root/'analysis/launch-capture144';runs=folder/'runs';runs.mkdir(exist_ok=True)
stamp=time.strftime('%Y%m%d-%H%M%S');run=runs/stamp
buttons=runs/(stamp+'-buttons.txt');buttons.write_text('0',encoding='ascii')
env={k.upper():v for k,v in os.environ.items() if not k.upper().startswith(('NIGHTFIRE_','RECOMP_','DRIVING_'))}
for line in (action/'show-nightfire-main-menu141.cmd').read_text(encoding='utf-8').splitlines():
    m=re.fullmatch(r'set "([A-Z0-9_]+)=(.*)"',line)
    if m and m[1].startswith(('NIGHTFIRE_','RECOMP_')):
        env[m[1]]=m[2].replace('%~dp0',str(action)+'\\')
for key in ('NIGHTFIRE_MENU_PREVIEW','NIGHTFIRE_DEV_START_EXCHANGE','NIGHTFIRE_DEV_MISSION137',
            'NIGHTFIRE_DEV_EXCHANGE_SECTION','NIGHTFIRE_MENU_MISSIONS139','RECOMP_CMDLINE'):
    env.pop(key,None)
env.update(RECOMP_WATCHDOG_SECS='175',RECOMP_TRACE_BUDGET='2000',RECOMP_APU_TRACE='1',
    RECOMP_FB_WINDOW='1',RECOMP_PB_SCAN='1',RECOMP_PB_EXEC='1',RECOMP_PB_EXEC_VERBOSE='1',
    NIGHTFIRE_DIAGNOSTIC_FENCES='1',NIGHTFIRE_DIAGNOSTIC_DSP='1',
    NIGHTFIRE_INPUT_TEST='1',NIGHTFIRE_INPUT_TEST_FILE_ONLY='1',NIGHTFIRE_INPUT_TEST_READY_START='1',
    NIGHTFIRE_INPUT_TEST_FILE=str(buttons),RECOMP_FB_DUMP=str(run/'framebuffer'),RECOMP_TEX_DUMP='',
    NIGHTFIRE_KERNEL_LOG=str(run/'xbox_kernel.log'),NIGHTFIRE_NATIVE_VIDEO_AUDIO='1',NIGHTFIRE_GPU_MOVIE='1',
    NIGHTFIRE_NATIVE_GAME_AUDIO='1',NIGHTFIRE_NATIVE_GAME_STREAM='1',NIGHTFIRE_NATIVE_GAME_SPATIAL='1')
exe=action/'analysis/checkpoint-141/candidate-build/nightfire_diagnostic.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='45934d39faa2b7f85b9312a279b8243375d58352f0261ba615cb33c230ce8826'
metadata=dict(run=str(run),buttons=str(buttons),executable=str(exe),
    environment={k:v for k,v in env.items() if k.startswith(('NIGHTFIRE_','RECOMP_'))},
    note='Main-menu141 and run-windows settings; original opening drive retained. Saved executable unmodified.')
(folder/'current-run.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
print(run,flush=True)
with (runs/(stamp+'-debugger.log')).open('w',encoding='utf-8') as log:
    proc=subprocess.Popen([str(folder/'capture144.exe'),'--launch',str(exe),str(action),str(run),'180'],
        cwd=action,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        deadline=time.monotonic()+195;started=False
        while proc.poll() is None:
            if time.monotonic()>deadline:raise subprocess.TimeoutExpired(proc.args,195)
            game_log=run/'action-stdout.log'
            if not started and game_log.exists():
                text=game_log.read_text(encoding='utf-8',errors='replace')
                if '[INPUT-READY]' in text:
                    # Fresh run's original title gate; respond before attract timeout.
                    buttons.write_text('10',encoding='ascii');time.sleep(.25)
                    buttons.write_text('0',encoding='ascii');started=True
                    (run/'route-events.log').write_text('Start pressed at first original INPUT-READY, then released.\n',encoding='utf-8')
                    print('Original title Start sent',flush=True)
            time.sleep(.1)
        code=proc.returncode
    except subprocess.TimeoutExpired:
        subprocess.run(['taskkill','/PID',str(proc.pid),'/T','/F'],stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
        code=proc.wait(timeout=10)
buttons.write_text('0',encoding='ascii')
if run.exists():(run/'root-environment.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
print('Debugger exit',code,flush=True)
raise SystemExit(code)
