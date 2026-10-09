"""Screen-rate measurements for DPI awareness (test aid, 2026-10-09; DPI-PERF.md).

Runs Paris (Driving only, run-lean-guided.py) once per configuration with NF_PRESENT_STATS=1 and
LEAN_STAGE_TIMING=1, checks before each run that the display is on (a non-black desktop sample),
sets the overlay ini for the run and puts the original back afterwards (always, byte for byte).
Usage: python dpi_perf_runs.py <out_dir> [config names...]
Run at most ONE fullscreen config per process: one_window_host registers its window class once,
so a second HostWindow in the same process gets the first one's window procedure and the game
window is not fitted (its swap chain stays 640x480). Windowed configs can share a process.
"""
import ctypes, os, re, shutil, subprocess, sys, threading, time
from ctypes import wintypes
from pathlib import Path

os.environ['NF_OVERLAY'] = '1'; os.environ['NF_DPI_AWARE'] = '1'   # the fullscreen host is created DPI aware
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'native-driving'))
from one_window_host import HostWindow
del os.environ['NF_OVERLAY'], os.environ['NF_DPI_AWARE']   # read at import; each run passes its own --env

u = ctypes.windll.user32; g = ctypes.windll.gdi32
u.SetProcessDPIAware()
OUT = Path(sys.argv[1]); OUT.mkdir(parents=True, exist_ok=True)
INI = Path(os.environ['LOCALAPPDATA']) / 'NightfirePC' / 'overlay-settings.ini'
SECONDS = 45
BASE = ['LEAN_NATIVE_D3D=1', 'LEAN_DISPLAY_GAMMA=0.8', 'LEAN_FPS_COUNTER=1', 'LEAN_PRESENT_VSYNC=1', 'LEAN_AUDIO_NATIVE=1',
        'LEAN_PRECISE_TICK=1', 'LEAN_AUDIO_LEAD=1600', 'LEAN_STRM_FIX=1', 'LEAN_FIX_CMPSD=audio', 'LEAN_VBLANK_HZ=60',
        'LEAN_VIDEO_MODE=pal60', 'LEAN_INTERP_GAMECLOCK=1', 'LEAN_AUDIO_EMIT_LOG=1', 'LEAN_FIX_CLIPSEARCH=1',
        'LEAN_AUDIO_NO_CHIP=1', 'LEAN_AUDIO_OWN_DSP=1', 'NF_OVERLAY=1', 'NF_PRESENT_STATS=1', 'LEAN_STAGE_TIMING=1']
# name: (window: 'default' | (w, h) screen pixels | 'fullscreen', resolution ini value, DPI aware, smooth, extra env)
CONFIGS = {
    'base-960x720-2x':        ('default', 1, False, True, []),
    'dpi-default-match':      ('default', 4, True, True, []),
    'dpi-default-2x':         ('default', 1, True, True, []),
    'dpi-2560x1440-match':    ((2560, 1440), 4, True, True, []),
    'dpi-2560x1440-2x':       ((2560, 1440), 1, True, True, []),
    'dpi-full-match':         ('fullscreen', 4, True, True, []),
    'dpi-full-2x':            ('fullscreen', 1, True, True, []),
    'dpi-full-2x-smoothoff':  ('fullscreen', 1, True, False, []),
    'dpi-full-2x-swap1080':   ('fullscreen', 1, True, True, ['NF_SWAP_FIXED=1920x1080']),
}

t0 = time.time()
def log(*a):
    line = '[%7.1f] ' % (time.time() - t0) + ' '.join(str(x) for x in a)
    print(line, flush=True)
    with open(OUT / 'harness.log', 'a', encoding='utf-8') as f: f.write(line + '\n')

def display_on():
    """Mean brightness of a 400x300 sample at the screen centre (0 = black)."""
    sw, sh = u.GetSystemMetrics(0), u.GetSystemMetrics(1)
    w, h = 400, 300; x, y = (sw - w) // 2, (sh - h) // 2
    sdc = u.GetDC(0); mdc = g.CreateCompatibleDC(sdc); bmp = g.CreateCompatibleBitmap(sdc, w, h)
    g.SelectObject(mdc, bmp); ok = g.BitBlt(mdc, 0, 0, w, h, sdc, x, y, 0x00CC0020)
    class BIH(ctypes.Structure):
        _fields_ = [('biSize', wintypes.DWORD), ('biWidth', ctypes.c_long), ('biHeight', ctypes.c_long),
                    ('biPlanes', wintypes.WORD), ('biBitCount', wintypes.WORD), ('biCompression', wintypes.DWORD),
                    ('biSizeImage', wintypes.DWORD), ('a', ctypes.c_long), ('b', ctypes.c_long), ('c', wintypes.DWORD), ('d', wintypes.DWORD)]
    bi = BIH(); bi.biSize = 40; bi.biWidth = w; bi.biHeight = h; bi.biPlanes = 1; bi.biBitCount = 32
    buf = ctypes.create_string_buffer(w * h * 4); g.GetDIBits(mdc, bmp, 0, h, buf, ctypes.byref(bi), 0)
    g.DeleteObject(bmp); g.DeleteDC(mdc); u.ReleaseDC(0, sdc)
    raw = buf.raw; mean = sum(raw[0::16]) / (len(raw) / 16)
    return bool(ok) and mean > 3, mean

def game_running():
    r = subprocess.run(['tasklist'], capture_output=True, text=True).stdout.lower()
    return 'nightfire' in r or 'driving' in r

def find_game_window(title='Nightfire Driving - experimental GPU preview', timeout=60):
    found = []
    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def _enum(hwnd, _):
        buf = ctypes.create_unicode_buffer(300); u.GetWindowTextW(hwnd, buf, 300)
        if buf.value.startswith(title): found.append(hwnd)
        return True
    end = time.time() + timeout
    while time.time() < end:
        found.clear(); u.EnumWindows(_enum, 0)
        if found: return found[0]
        time.sleep(0.25)
    return 0

def resize(h, cw, ch):
    for _ in range(2):
        c = wintypes.RECT(); u.GetClientRect(h, ctypes.byref(c)); wr = wintypes.RECT(); u.GetWindowRect(h, ctypes.byref(wr))
        if (c.right, c.bottom) == (cw, ch): break
        u.SetWindowPos(h, 0, 20, 20, wr.right - wr.left + cw - c.right, wr.bottom - wr.top + ch - c.bottom, 4); time.sleep(1)
    c = wintypes.RECT(); u.GetClientRect(h, ctypes.byref(c)); return c.right, c.bottom

def run(name):
    window, res, dpi, smooth, extra = CONFIGS[name]
    if game_running(): log(name, 'SKIPPED: a Nightfire game is running'); return
    on, mean = display_on()
    if not on: log(name, 'SKIPPED: display looks off (mean %.1f)' % mean); return
    orig = INI.read_bytes()
    keep = [l for l in orig.decode('ascii', 'replace').splitlines() if not l.startswith(('shape=', 'resolution='))]
    INI.write_text('\n'.join(keep + ['shape=2', 'resolution=%d' % res]) + '\n', encoding='ascii')
    try:
        env = BASE + ['LEAN_INTERP=%d' % (1 if smooth else 0)] + (['NF_DPI_AWARE=1'] if dpi else []) + extra
        host = None
        if window == 'fullscreen':
            host = HostWindow(title='Nightfire (DPI test)', fullscreen=True, log=lambda s: log(name, 'host:', s))
            env.append(f'NIGHTFIRE_HOST_HWND={host.hwnd}')
        cmd = [sys.executable, 'run-lean-guided.py', 'paris', '--seconds', str(SECONDS)]
        for e in env: cmd += ['--env', e]
        log(name, 'start; display mean %.1f' % mean)
        p = subprocess.Popen(cmd, cwd=str(ROOT / 'nightfire-driving-native'), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if isinstance(window, tuple):
            h = find_game_window()
            if h: time.sleep(2); log(name, 'client', resize(h, *window))
        out, _ = p.communicate()
        if host: host.close()
        on2, mean2 = display_on()
        log(name, 'end; display %s (mean %.1f)' % ('on' if on2 else 'OFF: run invalid', mean2))
        (OUT / f'{name}.summary.txt').write_text(out, encoding='utf-8')
        m = re.search(r'Session folder: (.*)', out) or re.search(r'Raw run folder: (.*)', out)
        sess = None
        for line in out.splitlines():
            if line.startswith('Session: '): sess = ROOT / 'nightfire-driving-native' / 'sessions' / line.split(': ', 1)[1].strip()
        if sess and (sess / 'game.log').exists():
            keepre = re.compile(r'\[NF-PRESENT\]|\[NF-OVERLAY\] (internal|swap|DPI)|\[LEAN-PRESENT\] Direct3D|\[LEAN-INTERP\] (5s|game waited)|STAGE|\[HOST-WINDOW\] child')
            lines = [l for l in (sess / 'game.log').read_text(encoding='utf-8', errors='replace').splitlines() if keepre.search(l)]
            (OUT / f'{name}.stats.txt').write_text('session ' + str(sess) + '\n' + '\n'.join(lines) + '\n', encoding='utf-8')
            log(name, 'session', sess.name, len(lines), 'stat lines')
        for line in out.splitlines():
            if 'FPS' in line: log(name, line.strip())
    finally:
        INI.write_bytes(orig)
        log(name, 'ini restored', INI.read_bytes() == orig)

names = sys.argv[2:] or list(CONFIGS)
for n in names:
    run(n)
log('all done')
