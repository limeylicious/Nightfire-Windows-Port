"""Guided play session: run a lean scene with session capture and collect
everything needed to analyse crashes, freezes and marked moments.

Usage: python run-lean-guided.py paris|vehicle [--seconds 3600] [--env NAME=VALUE ...]

Creates sessions/<YYYYMMDD-HHMMSS>-<scene>/ with:
  summary.txt     duration, exit reason, FPS min/avg, marks, freezes
  game.log        full game log
  settings.json   every LEAN_* / DRIVING_* / RECOMP_* setting in effect
  build.txt       exe path, time and SHA-256
  system.txt      GPU + driver, CPU, RAM, power, refresh rate, Ghidra/java
  stats.csv       once a second: presented FPS, game FPS, audio
  events.txt      start, marks, freezes, crash
  marks.txt + mark-NN.bmp   F9 presses
  crash.txt / abort.txt (+ last 200 log lines) and crash.dmp / abort.dmp
  audio.wav       the mixed sound output (48 kHz stereo); voices.txt = each sound start/stop
  freeze.txt (+ freeze-N.dmp) when the freeze monitor fired
Only the newest 5 sessions keep their .dmp and .wav files.
"""
from pathlib import Path
import argparse, csv, datetime, hashlib, json, re, shutil, subprocess, sys, time

LEAN = Path(__file__).resolve().parent
EXE = LEAN / 'build-lean/RelWithDebInfo/nightfire_driving_lean.exe'
KEEP_DUMPS = 5

PS_SYSTEM = r'''
$g = Get-CimInstance Win32_VideoController | Select-Object -First 3
foreach ($v in $g) { "GPU: $($v.Name) | driver $($v.DriverVersion) ($($v.DriverDate)) | $($v.CurrentHorizontalResolution)x$($v.CurrentVerticalResolution) @ $($v.CurrentRefreshRate) Hz" }
$c = Get-CimInstance Win32_Processor | Select-Object -First 1
"CPU: $($c.Name) | $($c.NumberOfCores) cores / $($c.NumberOfLogicalProcessors) threads | clock now $($c.CurrentClockSpeed) of $($c.MaxClockSpeed) MHz"
$os = Get-CimInstance Win32_OperatingSystem
"RAM: {0:N1} GB total, {1:N1} GB free" -f ($os.TotalVisibleMemorySize/1MB), ($os.FreePhysicalMemory/1MB)
"OS: $($os.Caption) $($os.Version)"
$b = Get-CimInstance Win32_Battery
if ($b) { $s = @{1='on battery';2='on AC power';3='fully charged';6='charging'}[[int]$b.BatteryStatus]; if(-not $s){$s="status $($b.BatteryStatus)"}; "Power: $s, battery $($b.EstimatedChargeRemaining)%" } else { "Power: no battery reported" }
$j = Get-Process java,javaw -ErrorAction SilentlyContinue
if ($j) { "Ghidra/java: running (" + (($j | ForEach-Object { "{0} pid {1}, {2:N1} GB" -f $_.Name, $_.Id, ($_.WorkingSet64/1GB) }) -join '; ') + ")" } else { "Ghidra/java: not running" }
'''


def system_info():
    q = subprocess.run(['powershell', '-NoProfile', '-Command', PS_SYSTEM], capture_output=True, text=True)
    return q.stdout.strip() + ('\n' + q.stderr.strip() if q.stderr.strip() else '')


def build_info():
    h = hashlib.sha256(EXE.read_bytes()).hexdigest()
    t = datetime.datetime.fromtimestamp(EXE.stat().st_mtime).strftime('%Y-%m-%d %H:%M:%S')
    return f'exe: {EXE}\nbuilt: {t}\nsha256: {h}\nsize: {EXE.stat().st_size} bytes\n'


def prune_dumps():
    sessions = sorted(p for p in (LEAN / 'sessions').iterdir() if p.is_dir())
    for old in sessions[:-KEEP_DUMPS]:
        for d in list(old.glob('*.dmp')) + list(old.glob('*.wav')):
            d.unlink()


def main():
    p = argparse.ArgumentParser()
    p.add_argument('scene', choices=['paris', 'vehicle'])
    p.add_argument('--seconds', type=int, default=3600)
    p.add_argument('--env', action='append', default=[])
    a = p.parse_args()
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    sess = LEAN / 'sessions' / f'{stamp}-{a.scene}'
    sess.mkdir(parents=True)
    (sess / 'system.txt').write_text(system_info() + '\n')
    (sess / 'build.txt').write_text(build_info())
    print(f'Session folder: {sess}')
    print('Press F9 in the game window to mark a moment (the title shows "MARK n SAVED").')
    env = list(a.env) + ['LEAN_SESSION_CAPTURE=1', f'LEAN_SESSION_DIR={sess}', 'LEAN_HANG_DUMP=1',
           'LEAN_AUDIO_VOICE_LOG=1', f'LEAN_AUDIO_WAV={sess / "audio.wav"}']   # sound evidence for F9 marks
    cmd = [sys.executable, str(LEAN / 'run-lean-scene.py'), a.scene, '--build', 'lean', '--seconds', str(a.seconds)]
    for kv in env:
        cmd += ['--env', kv]
    start = time.monotonic()
    out = subprocess.run(cmd, cwd=LEAN, capture_output=True, text=True)
    duration = time.monotonic() - start
    m = re.search(r'\{.*\}', out.stdout, re.S)
    result = json.loads(m.group(0)) if m else {}
    if not result:
        (sess / 'runner-output.txt').write_text(out.stdout + out.stderr)
    run = Path(result.get('run', '')) if result.get('run') else None
    log = ''
    if run and (run / 'driving-startup.log').exists():
        shutil.copy2(run / 'driving-startup.log', sess / 'game.log')
        log = (sess / 'game.log').read_text(errors='replace')
    if run and (run / 'environment.json').exists():
        shutil.copy2(run / 'environment.json', sess / 'settings.json')
    lines = log.splitlines()

    # Freeze text from the monitor, and the log tail for crashes.
    vev = [l for l in lines if l.startswith('[LEAN-VEV]')]
    if vev:
        (sess / 'voices.txt').write_text('\n'.join(vev) + '\n')
    hang = [l for l in lines if l.startswith('[LEAN-HANG]') and 'freeze monitor on' not in l]
    if hang:
        (sess / 'freeze.txt').write_text('\n'.join(hang) + '\n')
    for name in ('crash.txt', 'abort.txt'):
        if (sess / name).exists():
            with (sess / name).open('a') as f:
                f.write('\nLast 200 log lines:\n' + '\n'.join(lines[-200:]) + '\n')

    # Stats.
    rows = []
    if (sess / 'stats.csv').exists():
        with (sess / 'stats.csv').open() as f:
            rows = [r for r in csv.DictReader(f)]
    play = [r for r in rows if float(r['game_fps']) > 0]
    def stat(key, rr):
        v = [float(r[key]) for r in rr]
        return (min(v), sum(v) / len(v), max(v)) if v else (0, 0, 0)
    pmin, pavg, pmax = stat('presented_fps', play)
    gmin, gavg, gmax = stat('game_fps', play)
    live = [r for r in rows if float(r['game_fps']) >= 40]   # gameplay (not movies or loading)
    lpmin, lpavg, _ = stat('presented_fps', live)
    lgmin, lgavg, _ = stat('game_fps', live)
    stalls = sum(1 for l in lines if '[LEAN-HANG] no frame presented' in l)
    # Frozen at the end: the last 5 samples show no game frames after a freeze dump.
    frozen_end = stalls and len(rows) >= 5 and all(float(r['game_fps']) == 0 for r in rows[-5:])
    marks = sum(1 for l in (sess / 'marks.txt').read_text().splitlines() if l.startswith('mark ')) if (sess / 'marks.txt').exists() else 0

    if (sess / 'crash.txt').exists():
        first = (sess / 'crash.txt').read_text().splitlines()[1]
        reason = f'crash -- {first}'
    elif (sess / 'abort.txt').exists():
        reason = 'crash -- abort() (see abort.txt)'
    elif frozen_end:
        how = 'then closed' if 'window closed' in log else 'then stopped by the time limit' if '[WATCHDOG]' in log else 'then killed'
        reason = f'freeze -- game stopped presenting frames ({how}); see freeze.txt'
    elif '[PRESENT227] window closed' in log:
        reason = 'closed normally (window closed)'
    elif '[WATCHDOG]' in log:
        reason = f'time limit reached ({a.seconds} s)'
    else:
        reason = f'killed or exited unexpectedly (exit code {result.get("exit")})'

    summary = [
        f'Session: {sess.name}',
        f'Scene: {a.scene}',
        f'Duration: {duration:.0f} s',
        f'Exit reason: {reason}',
        f'Presented FPS (while the game ran): min {pmin:.0f}, avg {pavg:.0f}, max {pmax:.0f}',
        f'Game FPS (while the game ran): min {gmin:.0f}, avg {gavg:.1f}, max {gmax:.0f}',
        f'During gameplay ({len(live)} s with game >= 40 FPS): presented min {lpmin:.0f} avg {lpavg:.0f}; game min {lgmin:.0f} avg {lgavg:.1f}',
        f'Freeze-monitor stalls: {stalls}' + (' (game did not recover)' if frozen_end else ''),
        f'Marks (F9): {marks}',
        f'Audio: dropped={rows[-1]["xa2_dropped_total"] if rows else "?"} glitches={rows[-1]["xa2_glitches_total"] if rows else "?"} (totals)',
        f'Raw run folder: {run}',
        'Files: ' + ', '.join(sorted(x.name for x in sess.iterdir())),
    ]
    (sess / 'summary.txt').write_text('\n'.join(summary) + '\n')
    prune_dumps()
    print('\n'.join(summary))


if __name__ == '__main__':
    main()
