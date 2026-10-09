"""One bounded local run; all navigation goes through the native XInput test API.
The explicit menu-preview driving bypass is unchanged. No menu RAM is edited.
"""
from pathlib import Path
import os,subprocess,time,shutil,re
root=Path(__file__).resolve().parents[1]
artifacts=root/'analysis/checkpoint-39';artifacts.mkdir(exist_ok=True)
run_archive=artifacts/time.strftime('run-%Y%m%d-%H%M%S');run_archive.mkdir()
(artifacts/'current-run.txt').write_text(str(run_archive))
buttons=artifacts/'buttons.txt';buttons.write_text('0')
env={k.upper():v for k,v in os.environ.items()}
env.update(NIGHTFIRE_MENU_PREVIEW='1',NIGHTFIRE_INPUT_TEST='1',NIGHTFIRE_INPUT_TEST_READY_START='1',NIGHTFIRE_INPUT_TEST_FILE=str(buttons),RECOMP_WATCHDOG_SECS=env.get('NIGHTFIRE_CONTROL_SECONDS','900'),NIGHTFIRE_VERTEX_PROGRAM='1',NIGHTFIRE_DEPTH='1',NIGHTFIRE_PROFILE='1',NIGHTFIRE_GPU_DIAGNOSTICS='1')
env.update(NIGHTFIRE_HW_GPU='1',NIGHTFIRE_TEXTURE_FILTER='1',NIGHTFIRE_TEXTURE_CACHE='1')
env.pop('NIGHTFIRE_SKY_TRACE',None)
env.pop('NIGHTFIRE_WORLD_CAPTURE_IMAGES',None)
env.pop('NIGHTFIRE_WORLD_DRAW_CAPTURE',None)
env.pop('NIGHTFIRE_INPUT_TEST_DELAY_MS',None)
log=root/'logs/nightfire-startup.log'
with (artifacts/'route-console.log').open('w') as output:
    proc=subprocess.Popen(['cmd.exe','/d','/c','run-windows.cmd'],cwd=root,env=env,stdout=output,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
    start=time.monotonic()
    def report(s):
        line=f'{time.monotonic()-start:.1f}s {s}'
        print(line,flush=True)
        with (run_archive/'events.log').open('a') as record:record.write(line+'\n')
    def wait_for(s,timeout=150):
        until=time.monotonic()+timeout
        while time.monotonic()<until:
            if log.exists() and s in log.read_text(errors='replace'):report(s);return
            if proc.poll() is not None:raise RuntimeError(f'game ended {proc.returncode} while waiting for {s}')
            time.sleep(.25)
        raise TimeoutError(s)
    def press(mask):
        buttons.write_text(f'{mask:x}');time.sleep(.7);buttons.write_text('0');time.sleep(.7)
    try:
        # Let the new process replace the previous run's log before observing it.
        time.sleep(3)
        wait_for('handler=000852F0 page=00000000 event=0000004C')
        time.sleep(20);press(0x10)
        wait_for('[MENU-PAGE] id=40000002')
        time.sleep(3);press(0x10000);press(2);press(2);press(0x10000)
        wait_for('[MENU-PAGE] id=40000025')
        time.sleep(3);press(2);press(0x10000)
        wait_for('event=00000044 arg=4000002F')
        time.sleep(3);press(2);press(0x10000)
        wait_for('[MENU-PAGE] id=40000023')
        time.sleep(3);press(2);press(1);press(0x10000)
        wait_for('[MENU-PAGE] id=4000001C')
        time.sleep(3);press(2);press(1);press(0x10000)
        wait_for('07100005.xmv',timeout=160)
        time.sleep(12);press(0x10)
        report('Briefing skip sent; observing level load until exit/watchdog')
        # The configured game watchdog bounds this diagnostic session.
        wait_for('[WORLD-DEPTH] enable=1 func=203',timeout=90)
        def capture(name,after):
            until=time.monotonic()+45
            while time.monotonic()<until:
                files=[p for p in (root/'captures/screenshots').glob('nightfire-framebuffer-flip-*.bmp') if p.stat().st_mtime>after]
                if files:
                    newest=max(files,key=lambda p:p.stat().st_mtime)
                    time.sleep(.3);shutil.copy2(newest,artifacts/(name+'.bmp'));shutil.copy2(newest,run_archive/(name+'.bmp'));report('Captured '+name);return
                if proc.poll() is not None:raise RuntimeError('game ended during '+name)
                time.sleep(.5)
            raise TimeoutError('capture '+name)
        capture('before-input',time.time())
        report('INTERACTIVE WORLD READY: controller file now available for bounded navigation')
        if env.get('NIGHTFIRE_REPLAY_ROUTE')=='recorded':
            rows=[]
            route=Path(env.get('NIGHTFIRE_ROUTE_FILE',str(root/'analysis/checkpoint-36/run-20260910-174250/actions.log')))
            shutil.copy2(route,run_archive/'replayed-actions.log')
            for line in route.read_text().splitlines():
                m=re.fullmatch(r'([\d.]+) ([\w-]+): (.*) for ([\d.]+)s',line)
                if not m:raise ValueError(line)
                rows.append((float(m[1]),m[2],m[3],float(m[4])))
            origin=time.monotonic()+20
            for stamp,name,packet,hold in rows:
                time.sleep(max(0,origin+stamp-rows[0][0]-time.monotonic()))
                if proc.poll() is not None:break
                report('Recorded begin '+name);buttons.write_text(packet)
                time.sleep(hold);buttons.write_text('0')
                files=list((root/'captures/screenshots').glob('nightfire-framebuffer-flip-*.bmp'))
                if files:
                    latest=max(files,key=lambda p:p.stat().st_mtime)
                    time.sleep(.3);shutil.copy2(latest,artifacts/('recorded-'+name+'.bmp'));shutil.copy2(latest,run_archive/('recorded-'+name+'.bmp'))
        elif env.get('NIGHTFIRE_REPLAY_ROUTE'):
            raise ValueError('NIGHTFIRE_REPLAY_ROUTE must be recorded or unset')
        proc.wait(timeout=max(1,int(env['RECOMP_WATCHDOG_SECS'])+10-(time.monotonic()-start)))
        report(f'exit={proc.returncode}')
    finally:
        buttons.write_text('0')
        if proc.poll() is not None:
            output.flush()
            for source in (log,root/'logs/xbox_kernel.log',artifacts/'route-console.log'):
                if source.exists():shutil.copy2(source,run_archive/source.name)
        # A failed navigation assertion leaves the bounded game run available
        # for inspection; its own watchdog still terminates it.







