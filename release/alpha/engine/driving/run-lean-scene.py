"""Run the Driving engine (Paris) with the clean comparison profile (perf501 config501 OFF list).

Nightfire PC alpha copy of the workshop's nightfire-driving-native/run-lean-scene.py (10 October 2026):
the base settings come from base-environment-paris.json in this folder (a frozen copy of
nightfire-driving/analysis/runs/20260927-174835-vehicle439-paris-regression/environment.json,
without its two path entries, which this script always sets), and the exe sits next to this file.

Usage: python run-lean-scene.py paris [--seconds 120]
Writes everything under runs/<stamp>-<scene>.
Refuses to launch when any nightfire* process is already running.
"""
from pathlib import Path
import argparse, datetime, json, os, re, shutil, subprocess, time

LEAN = Path(__file__).resolve().parent
EXES = {'lean': LEAN / 'nightfire_driving_lean.exe'}
SOURCES = {'paris': 'base-environment-paris.json'}
# Copied from nightfire-driving/analysis/perf501/config501.py (accepted 491 profile).
OFF = (
    'PC450', 'PC451', 'PC452', 'PC453', 'ACK457', 'FOG455', 'SUBMIT456',
    'CLEAR461', 'COLOR462', 'CAPTURE_EXTRA453', 'QUERY464', 'HINT465',
    'PROOF466', 'FONT469', 'COMMAND_SNAPSHOT471', 'REFUSE472', 'OBSERVE454',
    'PRESENT458', 'TIMING250', 'QUEUE_TIMING251', 'BATCH_TIMING244',
    'READ_SAMPLE326', 'METHOD_TIMING327', 'GPU_SAMPLE297', 'GPU_SAMPLE254',
    'NATIVE_TIMING247', 'GENERIC_TIMING326', 'HUD_COST397', 'SPRITE_COST399',
    'PAIR_COLOR_CENSUS401', 'BEGIN_PROBE334', 'IMPORT_PROBE335', 'OPENGL493',
    'OPENGL494', 'READBACK_ROWS495', 'API_CENSUS496', 'GRAPH_JOURNAL498',
    'WRITER_JOURNAL499', 'FONT_JOURNAL501',
)

def settings(scene):
    env = json.loads((LEAN / SOURCES[scene]).read_text())
    env.update({f'DRIVING_{k}': '0' for k in OFF})
    env.update(DRIVING_REGION_CACHE445='1', DRIVING_CPU448='1', DRIVING_COLOR_SEED276='1',
               DRIVING_RESOLVE459='1', DRIVING_ASYNC_TAIL295='0')
    return env

def running():
    q = subprocess.run(['powershell', '-NoProfile', '-Command',
                        '@(Get-Process nightfire* -ErrorAction SilentlyContinue).Count'],
                       capture_output=True, text=True)
    return q.stdout.strip()

def keep_awake(on):
    """While the game runs: SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED |
    ES_DISPLAY_REQUIRED) so the PC and display stay awake; cleared when it exits."""
    try:
        import ctypes
        ctypes.windll.kernel32.SetThreadExecutionState(0x80000000 | (0x00000003 if on else 0))
    except Exception:
        pass


def main():
    p = argparse.ArgumentParser()
    p.add_argument('scene', choices=sorted(SOURCES))
    p.add_argument('--seconds', type=int, default=120)
    p.add_argument('--build', choices=sorted(EXES), default='lean')
    p.add_argument('--env', action='append', default=[], help='extra NAME=VALUE')
    a = p.parse_args()
    assert running() == '0', 'A nightfire game is already running; not launching.'
    env = {k.upper(): v for k, v in os.environ.items()
           if not k.upper().startswith(('DRIVING_', 'NIGHTFIRE_', 'RECOMP_', 'LEAN_'))}
    env.update(settings(a.scene))
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    run = LEAN / 'runs' / f'{stamp}-{a.build}-{a.scene}'
    run.mkdir(parents=True)
    for kv in a.env:
        k, v = kv.split('=', 1); env[k] = v
    # Engine hand-over (native-driving/play-nightfire-native.py): the Action engine's own launch
    # page replaces the captured payload, so development shortcuts of the scene profile go.
    if env.get('NIGHTFIRE_LAUNCH_PAGE'):
        for k in [k for k in env if k.startswith('DRIVING_DEV_')]:
            env.pop(k)
    # Native audio needs the stream reader fix: without it the vehicle level hangs at
    # start-up (STRM reader race, 6/6 runs 2026-10-06). Explicit LEAN_STRM_FIX=0 wins.
    if env.get('LEAN_AUDIO_NATIVE') == '1' and 'LEAN_STRM_FIX' not in env:
        env['LEAN_STRM_FIX'] = '1'
    env.update(DRIVING_EXECUTABLE=str(EXES[a.build]), DRIVING_CAPTURE_DIR=str(run),
               RECOMP_WATCHDOG_SECS=str(a.seconds))
    (run / 'environment.json').write_text(json.dumps(
        {k: v for k, v in env.items() if k.startswith(('DRIVING_', 'NIGHTFIRE_', 'RECOMP_', 'LEAN_'))}, indent=2))
    start = time.monotonic()
    keep_awake(True)
    with (run / 'launcher.log').open('w') as log:
        proc = subprocess.Popen([env['COMSPEC'], '/d', '/c', str(LEAN / 'run-windows.cmd')],
                                cwd=LEAN, env=env, stdout=log, stderr=subprocess.STDOUT,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            code = proc.wait(timeout=a.seconds + 60)
        except subprocess.TimeoutExpired:
            subprocess.run(['taskkill', '/F', '/T', '/PID', str(proc.pid)], capture_output=True)
            code = 'killed-by-outer-guard'
    keep_awake(False)
    elapsed = time.monotonic() - start
    for name in ('driving-startup.log', 'xbox_kernel.log'):
        if (LEAN / 'logs' / name).exists():
            shutil.copy2(LEAN / 'logs' / name, run / name)
    text = (run / 'driving-startup.log').read_text(errors='replace') if (run / 'driving-startup.log').exists() else ''
    pres = [tuple(map(int, m)) for m in re.findall(r'\[PRESENT227\] frame=(\d+) elapsed_ms=(\d+) last30_ms=(\d+)', text)]
    batch = re.findall(r'\[BATCH236-TIME\] (.*)', text)
    result = dict(build=a.build, scene=a.scene, exit=code, wall_seconds=round(elapsed, 1), presents=len(pres),
                  last_present=pres[-1] if pres else None,
                  mean_ms_per_present=round(pres[-1][1] / pres[-1][0], 1) if pres else None,
                  last5_windows_ms_per_present=[round(x[2] / 30, 1) for x in pres[-5:]],
                  batch236_last=batch[-1] if batch else None, run=str(run))
    (run / 'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
