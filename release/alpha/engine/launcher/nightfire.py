"""Nightfire PC alpha launcher: both engines native and linked, as the Xbox runs them.

Based on the workshop's native-driving/play-nightfire-native.py (10 October 2026), with the
paths changed for this folder and two modes:

  play   (Play Nightfire.cmd)  PC settings on (F10 menu, PC Graphics page, key binds); crash and
         freeze reports kept; no sound recording, no frame-counter overlay, no picture dumps.
  debug  (Debug\\*.cmd)        everything the workshop records today: audio.wav and sound logs,
         the frame counter, Action picture dumps, plus the options below.

The Action engine (default.xbe, engine/action) and the Driving engine (Driving.xbe,
engine/driving) are separate programs. When one asks the Xbox to switch images, its runtime
saves the launch page the game wrote (NIGHTFIRE_HANDOVER_DIR) and exits; this script then
starts the other engine with that page (NIGHTFIRE_LAUNCH_PAGE), exactly as the reboot would.
One window by default (one_window_host.py): both engines draw inside it.

Usage: python nightfire.py [--mode play|debug] [--fullscreen] [--two-windows]
                           [--skip-opening-drive] [--split-screen | --split-screen-sound]
                           [--record] [--smooth-off]
  --skip-opening-drive  start at the Action menus (the opening Paris drive is bypassed)
  --split-screen        up to four players on this PC (controllers 1-3 = players 2-4;
                        F6 moves keyboard+mouse to the next player, F5 back); implies
                        --skip-opening-drive and uses the workshop's tested split-screen sound setting
                        (the older sound path; show-nightfire-native-4pads.cmd)
  --split-screen-sound  the same with the Action engine's own new sound (not tried in split-screen yet)
  --record              Driving keeps the last 72 pictures; F9 saves them (debug)
  --smooth-off          Driving's Smooth Motion off (pictures only at game steps)
Writes engine/linked-sessions/<stamp>/steps.txt; each engine keeps its own session folder
(engine/action/sessions, engine/driving/sessions) with crash.txt / crash.dmp / freeze.txt.
Old session folders are pruned (newest 10 kept in play mode, 30 in debug mode).
Refuses to start when any nightfire* process is already running.
"""
from pathlib import Path
import argparse, datetime, os, shutil, stat, subprocess, sys, time

ENGINE = Path(__file__).resolve().parent.parent
ACTION = ENGINE / 'action'
DRIVING = ENGINE / 'driving'
LINKED = ENGINE / 'linked-sessions'
# The owner's Driving setup (workshop native-driving/play-native-paris-guided.cmd), less the
# two debug-only entries in DRIVING_DEBUG_ENV.
DRIVING_ENV = ['LEAN_NATIVE_D3D=1', 'LEAN_DISPLAY_GAMMA=0.8', 'LEAN_INTERP=1',
               'LEAN_PRESENT_VSYNC=1', 'LEAN_AUDIO_NATIVE=1', 'LEAN_PRECISE_TICK=1', 'LEAN_AUDIO_LEAD=1600',
               'LEAN_STRM_FIX=1', 'LEAN_FIX_CMPSD=audio', 'LEAN_VBLANK_HZ=60', 'LEAN_VIDEO_MODE=pal60',
               'LEAN_INTERP_GAMECLOCK=1', 'LEAN_FIX_CLIPSEARCH=1',
               'LEAN_AUDIO_NO_CHIP=1', 'LEAN_AUDIO_OWN_DSP=1']
DRIVING_DEBUG_ENV = ['LEAN_FPS_COUNTER=1', 'LEAN_AUDIO_EMIT_LOG=1']
LONG_SECONDS = 86400   # time limit per engine run: 24 hours (the workshop used 30 min / 1 h test limits)
KEEP = {'play': 10, 'debug': 30}
MAX_STEPS = 64
ENGINE_EXES = ('nightfire_native.exe', 'nightfire_driving_lean.exe')


def running():
    q = subprocess.run(['powershell', '-NoProfile', '-Command',
                        '@(Get-Process nightfire* -ErrorAction SilentlyContinue).Count'],
                       capture_output=True, text=True)
    return q.stdout.strip() != '0'


def is_link(p):
    """True for symlinks and junctions: never delete through them."""
    try:
        st = os.lstat(p)
    except OSError:
        return False
    return stat.S_ISLNK(st.st_mode) or bool(getattr(st, 'st_file_attributes', 0) & 0x400)


def prune(folder, keep, files=False):
    """Delete all but the newest `keep` entries of one output folder (session folders or files)."""
    if not folder.is_dir() or is_link(folder):
        return
    items = [p for p in folder.iterdir() if not is_link(p) and (p.is_file() if files else p.is_dir())]
    items.sort(key=lambda p: p.stat().st_mtime)
    for old in items[:-keep] if keep else items:
        try:
            if old.is_dir():
                shutil.rmtree(old)
            else:
                old.unlink()
        except OSError:
            pass


def prune_outputs(mode):
    k = KEEP[mode]
    prune(LINKED, k)
    prune(ACTION / 'sessions', k)
    prune(ACTION / 'logs' / 'game-history', 3 * k, files=True)
    prune(ACTION / 'captures' / 'screenshots', 200, files=True)
    prune(ACTION / 'captures' / 'textures', 200, files=True)
    prune(DRIVING / 'sessions', k)
    prune(DRIVING / 'runs', 5 if mode == 'play' else 20)   # each run folder holds about 60 MB of pictures


HOST_HWND = None   # one-window play: the launcher's window handle, passed to both engines


def run_action(hand, page, a, first):
    env = dict(os.environ)
    for k in list(env):
        if k.upper().startswith(('NIGHTFIRE_', 'LEAN_', 'RECOMP_', 'DRIVING_')):
            env.pop(k)
    skip_drive = a.skip_opening_drive and first
    env.update(NIGHTFIRE_MENU_PREVIEW='1' if skip_drive else '0', NIGHTFIRE_HANDOVER_DIR=str(hand),
               RECOMP_WATCHDOG_SECS=str(LONG_SECONDS))
    if a.mode == 'play':
        env['NIGHTFIRE_NO_AUTODUMPS140'] = '1'
    if HOST_HWND:
        env['NIGHTFIRE_HOST_HWND'] = str(HOST_HWND)
    if page:
        env['NIGHTFIRE_LAUNCH_PAGE'] = str(page)
    return subprocess.Popen([env.get('COMSPEC', 'cmd.exe'), '/d', '/c', str(ACTION / 'start-action.cmd')],
                            cwd=ACTION, env=env)


def run_driving(hand, page, a):
    cmd = [sys.executable, str(DRIVING / 'run-lean-guided.py'), 'paris', '--seconds', str(LONG_SECONDS)]
    if a.mode == 'play':
        cmd.append('--no-sound-recording')
    denv = list(DRIVING_ENV) + (DRIVING_DEBUG_ENV if a.mode == 'debug' else [])
    if a.smooth_off:
        denv = [kv for kv in denv if not kv.startswith('LEAN_INTERP=')] + ['LEAN_INTERP=0']
    extra = [f'NIGHTFIRE_HOST_HWND={HOST_HWND}'] if HOST_HWND else []
    for kv in denv + [f'NIGHTFIRE_HANDOVER_DIR={hand}', f'NIGHTFIRE_LAUNCH_PAGE={page}'] + extra:
        cmd += ['--env', kv]
    return subprocess.Popen(cmd, cwd=DRIVING)


def wait_engine(proc, host):
    """proc.wait(); in one-window play also ends the engine when the player closed the window.
    The engine gets WM_CLOSE (its normal close); if it has no window yet (still loading) or
    has not exited 10 s later, only the engine executable is ended, so the wrappers still
    save their session folders."""
    if not host:
        return proc.wait()
    killed = False
    while True:
        try:
            return proc.wait(timeout=0.25)
        except subprocess.TimeoutExpired:
            pass
        if host.close_requested and not killed:
            waited = time.monotonic() - host.close_time
            if (not host.has_child() and waited > 1) or waited > 10:
                for exe in ENGINE_EXES:
                    subprocess.run(['taskkill', '/F', '/IM', exe], capture_output=True)
                host.log(f'engine ended by the launcher after the window was closed ({waited:.0f} s)')
                killed = True


def setup_done():
    missing = [str(p.relative_to(ENGINE.parent)) for p in (
        ACTION / 'nightfire_native.exe', DRIVING / 'nightfire_driving_lean.exe',
        ACTION / 'game_files' / 'default.xbe', ENGINE / 'nightfire-port' / 'game_files' / 'Driving.xbe')
        if not p.exists()]
    if missing:
        print('Setup has not been completed. Missing:\n  ' + '\n  '.join(missing))
        print('Run Setup.cmd in the Nightfire-PC-Alpha folder first.')
    return not missing


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--mode', choices=sorted(KEEP), default='play')
    p.add_argument('--steps', type=int, default=MAX_STEPS, help='stop after this many engine runs')
    p.add_argument('--two-windows', action='store_true', help='each engine opens its own window')
    p.add_argument('--fullscreen', action='store_true', help='one window, borderless fullscreen')
    p.add_argument('--skip-opening-drive', action='store_true')
    p.add_argument('--split-screen', action='store_true')
    p.add_argument('--split-screen-sound', action='store_true')
    p.add_argument('--record', action='store_true')
    p.add_argument('--smooth-off', action='store_true')
    a = p.parse_args()
    if not setup_done():
        return 1
    if running():
        print('A Nightfire game is already running; not starting.'); return 1
    os.environ['NF_OVERLAY'] = '1'   # F10 settings, PC Graphics / Options pages, key binds, Local/Online pages
    if a.record:
        os.environ['NF_RECORD'] = '1'
    if a.split_screen or a.split_screen_sound:   # the workshop's show-nightfire-native-4pads.cmd settings
        a.skip_opening_drive = True
        os.environ.update(NF_PADS='4', NF_VIRTUAL_PADS='3')
        if not a.split_screen_sound:
            os.environ['NF_ALPHA_ACTION_SOUND'] = 'off'   # 4pads leaves NIGHTFIRE_NATIVE_SOUND unset
    prune_outputs(a.mode)
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    hand = LINKED / stamp
    hand.mkdir(parents=True)
    log = (hand / 'steps.txt').open('a', buffering=1)
    log.write(f'mode {a.mode}; options: ' + ' '.join(k for k in ('fullscreen', 'two_windows', 'skip_opening_drive',
              'split_screen', 'split_screen_sound', 'record', 'smooth_off') if getattr(a, k))
              + (f' NF_KEYBOARD_PLAYER={os.environ["NF_KEYBOARD_PLAYER"]}' if os.environ.get('NF_KEYBOARD_PLAYER') else '') + '\n')
    print(f'Nightfire PC alpha ({a.mode} mode). Session notes: {hand}')
    global HOST_HWND
    host = None
    if not a.two_windows:
        from one_window_host import HostWindow
        host = HostWindow(fullscreen=a.fullscreen,
                          log=lambda t: log.write(f'{datetime.datetime.now():%H:%M:%S} one-window: {t}\n'))
        HOST_HWND = host.hwnd
        log.write(f'{datetime.datetime.now():%H:%M:%S} one-window: host window {HOST_HWND:#x} rect {host.rect()}\n')
    engine, page = 'action', None
    for step in range(1, a.steps + 1):
        for name in ('next-page.bin', 'next-target.txt'):
            (hand / name).unlink(missing_ok=True)
        t0 = time.monotonic()
        log.write(f'{datetime.datetime.now():%H:%M:%S} step {step}: start {engine}'
                  + (f' with {page.name}' if page else '') + '\n')
        proc = run_action(hand, page, a, step == 1) if engine == 'action' else run_driving(hand, page, a)
        code = wait_engine(proc, host)
        target = (hand / 'next-target.txt').read_text(errors='replace').strip() if (hand / 'next-target.txt').exists() else ''
        log.write(f'{datetime.datetime.now():%H:%M:%S} step {step}: {engine} ended after {time.monotonic() - t0:.0f} s,'
                  f' exit {code}, next {target or "(none: game closed)"}\n')
        if host and host.close_requested:
            log.write(f'{datetime.datetime.now():%H:%M:%S} one-window: player closed the window; session ends\n')
            break
        if not target or not (hand / 'next-page.bin').exists():
            break
        engine = 'driving' if 'DRIVING' in target.upper() else 'action'
        page = hand / f'page-{step:02d}-to-{engine}.bin'
        shutil.copyfile(hand / 'next-page.bin', page)
        if running():   # the previous engine must be gone before the next one starts
            time.sleep(2)
        if host:
            host.set_title('Nightfire')
    if host:
        host.close()
    log.close()
    print((hand / 'steps.txt').read_text())
    return 0


if __name__ == '__main__':
    sys.exit(main())
