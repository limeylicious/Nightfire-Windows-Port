"""One bounded local run; all navigation goes through the native XInput test API.
The explicit menu-preview driving bypass is unchanged. No menu RAM is edited.
"""
from pathlib import Path
import os,subprocess,time
root=Path(__file__).resolve().parents[1]
artifacts=root/'analysis/checkpoint-28';artifacts.mkdir(exist_ok=True)
buttons=artifacts/'buttons.txt';buttons.write_text('0')
env={k.upper():v for k,v in os.environ.items()}
env.update(NIGHTFIRE_MENU_PREVIEW='1',NIGHTFIRE_INPUT_TEST='1',NIGHTFIRE_INPUT_TEST_READY_START='1',NIGHTFIRE_INPUT_TEST_FILE=str(buttons),RECOMP_WATCHDOG_SECS='480')
env.pop('NIGHTFIRE_INPUT_TEST_DELAY_MS',None)
log=root/'logs/nightfire-startup.log'
with (artifacts/'route-console.log').open('w') as output:
    proc=subprocess.Popen(['cmd.exe','/d','/c','run-windows.cmd'],cwd=root,env=env,stdout=output,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
    start=time.monotonic()
    def report(s): print(f'{time.monotonic()-start:.1f}s {s}',flush=True)
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
        # Allow the game's 480-second watchdog to produce its own diagnostic.
        proc.wait(timeout=max(1,490-(time.monotonic()-start)))
        report(f'exit={proc.returncode}')
    finally:
        buttons.write_text('0')
        # A failed navigation assertion leaves the bounded game run available
        # for inspection; its own watchdog still terminates it.
