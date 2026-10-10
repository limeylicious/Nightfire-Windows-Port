"""Nightfire PC alpha set-up (run by Setup.cmd).

This alpha is a personal test build: it copies the two engines the workshop last built, and it
links the game files. It does not compile anything and it never changes the workshop folders.

  1. Finds the workshop: the folder that holds nightfire-port-native and nightfire-driving-native
     (by default the folder above releases\\; or pass --workshop PATH).
  2. Copies into engine\\:
       action\\nightfire_native.exe + .pdb        from nightfire-port-native\\build-native\\RelWithDebInfo
       action\\cache\\shaders\\*.dxbc             from nightfire-port-native\\cache\\shaders (saves first-run hitches)
       driving\\nightfire_driving_lean.exe + .pdb from nightfire-driving-native\\build-native\\RelWithDebInfo
       driving\\launch-data\\original-first-paris144.bin  from nightfire-driving-native\\launch-data
     and writes engine\\build-info.txt (sizes, times, SHA-256 of both exes).
  3. Game files. If "Game Files" already holds the extracted disc (default.xbe and Driving.xbe), or
     links to it, it is used as it is. Otherwise "Game Files" becomes a link (junction) to the
     workshop's nightfire-port\\game_files. Both XBEs are checked against the PAL hashes.
  4. Links engine\\action\\game_files and engine\\nightfire-port\\game_files to the game files (the
     Driving engine reads ..\\nightfire-port\\game_files from engine\\driving).
  5. Makes engine\\driving\\saves (the Driving engine creates its save images there on first start).

Run it again any time to pick up newer workshop builds. It refuses while a Nightfire game is running.
Only the alpha folder is written; links are removed with rmdir, which never touches their target.
"""
from pathlib import Path
import argparse, datetime, hashlib, os, shutil, stat, subprocess, sys

ALPHA = Path(__file__).resolve().parent.parent
ENGINE = ALPHA / 'engine'
GAME = ALPHA / 'Game Files'
PLACEHOLDER = 'PUT YOUR GAME FILES HERE.txt'
XBE_SHA = {
    'default.xbe': 'b464b787250ffc0ea7443a0e0848689c03a6cdaf6eb593fa0dba509bd2641aa1',   # PAL Action engine
    'Driving.xbe': '0f4c50e4f84b1edeedab4667933b36c9c228da463cc89180e4c70cf3a88bfcef',   # PAL Driving engine
}


def say(t=''):
    print(t, flush=True)


def is_link(p):
    try:
        st = os.lstat(p)
    except OSError:
        return False
    return stat.S_ISLNK(st.st_mode) or bool(getattr(st, 'st_file_attributes', 0) & 0x400)


def sha256(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def running():
    q = subprocess.run(['powershell', '-NoProfile', '-Command',
                        '@(Get-Process nightfire* -ErrorAction SilentlyContinue).Count'],
                       capture_output=True, text=True)
    return q.stdout.strip() != '0'


def junction(link, target):
    """Make `link` a junction to `target`. An existing junction is replaced; a real folder is never touched."""
    if is_link(link):
        os.rmdir(link)            # removes the link only
    elif link.exists():
        raise SystemExit(f'{link} is a real folder, not a link; move it away and run Setup again.')
    link.parent.mkdir(parents=True, exist_ok=True)
    r = subprocess.run(['cmd', '/d', '/c', 'mklink', '/J', str(link), str(target)], capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f'Could not link {link} to {target}: {r.stdout.strip()} {r.stderr.strip()}')


def copy(src, dst):
    if not src.exists():
        raise SystemExit(f'Missing in the workshop: {src}')
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    say(f'  copied {dst.relative_to(ALPHA)} ({dst.stat().st_size:,} bytes)')


def check_runtime():
    sysdir = Path(os.environ.get('SystemRoot', r'C:\Windows')) / 'System32'
    need = ['vcruntime140.dll', 'msvcp140.dll', 'ucrtbase.dll', 'd3d11.dll', 'd3dcompiler_47.dll',
            'xinput1_4.dll', 'xaudio2_9.dll', 'mfplat.dll', 'mfreadwrite.dll']
    missing = [n for n in need if not (sysdir / n).exists()]
    if missing:
        say('Warning: these Windows files were not found: ' + ', '.join(missing))
        say('  The Microsoft Visual C++ 2015-2022 x64 runtime (vcruntime140, msvcp140) and, on Windows N,')
        say('  the Media Feature Pack (mfplat, mfreadwrite) provide them.')
    else:
        say('Windows runtime files: all present.')


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--workshop', help='folder holding nightfire-port-native and nightfire-driving-native')
    a = p.parse_args()
    ws = Path(a.workshop).resolve() if a.workshop else ALPHA.parent.parent
    if not (ws / 'nightfire-port-native').is_dir() or not (ws / 'nightfire-driving-native').is_dir():
        raise SystemExit(f'No workshop found at {ws} (it needs nightfire-port-native and nightfire-driving-native).\n'
                         'Run: Setup.cmd --workshop "C:\\path\\to\\Nightfire-Windows-Diagnostic-Project"')
    if running():
        raise SystemExit('A Nightfire game is running. Close it, then run Setup again.')
    say(f'Nightfire PC alpha set-up\n  alpha folder: {ALPHA}\n  workshop:     {ws}\n')
    check_runtime()

    say('\nCopying the engines (the workshop is only read):')
    an, dn = ws / 'nightfire-port-native', ws / 'nightfire-driving-native'
    for name in ('nightfire_native.exe', 'nightfire_native.pdb'):
        copy(an / 'build-native' / 'RelWithDebInfo' / name, ENGINE / 'action' / name)
    shaders = an / 'cache' / 'shaders'
    n = 0
    if shaders.is_dir():
        (ENGINE / 'action' / 'cache' / 'shaders').mkdir(parents=True, exist_ok=True)
        for f in shaders.glob('*.dxbc'):
            shutil.copy2(f, ENGINE / 'action' / 'cache' / 'shaders' / f.name); n += 1
    say(f'  copied {n} compiled shaders to engine\\action\\cache\\shaders')
    for name in ('nightfire_driving_lean.exe', 'nightfire_driving_lean.pdb'):
        copy(dn / 'build-native' / 'RelWithDebInfo' / name, ENGINE / 'driving' / name)
    copy(dn / 'launch-data' / 'original-first-paris144.bin', ENGINE / 'driving' / 'launch-data' / 'original-first-paris144.bin')

    info = [f'Nightfire PC alpha engines, set up {datetime.datetime.now():%Y-%m-%d %H:%M:%S} from {ws}']
    for rel in ('action/nightfire_native.exe', 'driving/nightfire_driving_lean.exe'):
        f = ENGINE / rel
        info.append(f'{rel}: {f.stat().st_size} bytes, built {datetime.datetime.fromtimestamp(f.stat().st_mtime):%Y-%m-%d %H:%M:%S}, sha256 {sha256(f)}')
    (ENGINE / 'build-info.txt').write_text('\n'.join(info) + '\n')

    say('\nGame files:')
    if (GAME / 'default.xbe').exists():
        target = Path(os.path.realpath(GAME))
        say(f'  using the game files in "{GAME.name}" ({target})')
    else:
        target = ws / 'nightfire-port' / 'game_files'
        if not (target / 'default.xbe').exists():
            raise SystemExit(f'No game files found. Copy the contents of your extracted PAL disc into\n  {GAME}\n'
                             'so that default.xbe and Driving.xbe sit directly inside it, then run Setup again.')
        if GAME.exists() and not is_link(GAME):
            extra = [f.name for f in GAME.iterdir() if f.name != PLACEHOLDER]
            if extra:
                raise SystemExit(f'"{GAME.name}" has files but no default.xbe. Fix it or empty it, then run Setup again.')
            (GAME / PLACEHOLDER).unlink(missing_ok=True)
            GAME.rmdir()   # the empty placeholder folder
        junction(GAME, target)
        say(f'  "{GAME.name}" now links to the workshop\'s game files ({target})')
    for name, want in XBE_SHA.items():
        f = target / name
        if not f.exists():
            raise SystemExit(f'Missing {f}. The alpha needs the PAL disc files, including {name}.')
        got = sha256(f)
        if got != want:
            raise SystemExit(f'{name} is not the PAL version this alpha was built for (sha256 {got}).')
        say(f'  {name}: PAL version confirmed')
    junction(ENGINE / 'action' / 'game_files', target)
    junction(ENGINE / 'nightfire-port' / 'game_files', target)
    say('  engine links made: engine\\action\\game_files, engine\\nightfire-port\\game_files')
    for d in (ENGINE / 'driving' / 'saves', ENGINE / 'driving' / 'logs', ENGINE / 'action' / 'logs'):
        d.mkdir(parents=True, exist_ok=True)
    say('\nSet-up finished. Start the game with "Play Nightfire.cmd".')
    return 0


if __name__ == '__main__':
    sys.exit(main())
