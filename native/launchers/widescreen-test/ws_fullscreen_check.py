"""Borderless fullscreen check with Driving only (test aid, 2026-10-09).

Creates the overlay launcher's host window (one_window_host.HostWindow, fullscreen, as
play-nightfire-native-overlay-fullscreen.cmd does) with NF_OVERLAY=1, runs Paris inside it
through run-lean-guided.py (no Action), and screenshots the whole screen at 30 s and 50 s.
Usage: python ws_fullscreen_check.py <out_dir> <seconds> [--env NAME=VALUE ...]
"""
import ctypes, os, subprocess, sys, threading, time
from ctypes import wintypes
from pathlib import Path

os.environ['NF_OVERLAY'] = '1'                     # before the import: the host reads it
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'native-driving'))
from one_window_host import HostWindow

u = ctypes.windll.user32; g = ctypes.windll.gdi32
u.SetProcessDPIAware()
out, seconds, extra = sys.argv[1], sys.argv[2], sys.argv[3:]
t0 = time.time()
def log(*a): print('[%6.1f]' % (time.time() - t0), *a, flush=True)

host = HostWindow(title='Nightfire (fullscreen check)', fullscreen=True, log=lambda s: log('host:', s))
r = wintypes.RECT(); u.GetClientRect(host.hwnd, ctypes.byref(r)); log('host client', r.right, r.bottom)

def shot(name):
    r = wintypes.RECT(); u.GetClientRect(host.hwnd, ctypes.byref(r))
    p = wintypes.POINT(0, 0); u.ClientToScreen(host.hwnd, ctypes.byref(p))
    w, hh = r.right, r.bottom
    sdc = u.GetDC(0); mdc = g.CreateCompatibleDC(sdc); bmp = g.CreateCompatibleBitmap(sdc, w, hh)
    g.SelectObject(mdc, bmp); g.BitBlt(mdc, 0, 0, w, hh, sdc, p.x, p.y, 0x00CC0020)
    class BIH(ctypes.Structure):
        _fields_ = [('biSize', wintypes.DWORD), ('biWidth', ctypes.c_long), ('biHeight', ctypes.c_long),
                    ('biPlanes', wintypes.WORD), ('biBitCount', wintypes.WORD), ('biCompression', wintypes.DWORD),
                    ('biSizeImage', wintypes.DWORD), ('a', ctypes.c_long), ('b', ctypes.c_long),
                    ('c', wintypes.DWORD), ('d', wintypes.DWORD)]
    bi = BIH(); bi.biSize = 40; bi.biWidth = w; bi.biHeight = hh; bi.biPlanes = 1; bi.biBitCount = 24
    stride = (w * 3 + 3) & ~3; buf = ctypes.create_string_buffer(stride * hh)
    g.GetDIBits(mdc, bmp, 0, hh, buf, ctypes.byref(bi), 0)
    path = os.path.join(out, name + '.bmp')
    with open(path, 'wb') as f:
        f.write(b'BM' + (54 + len(buf.raw)).to_bytes(4, 'little') + b'\0\0\0\0' + (54).to_bytes(4, 'little'))
        f.write(bytes(bi)); f.write(buf.raw)
    g.DeleteObject(bmp); g.DeleteDC(mdc); u.ReleaseDC(0, sdc)
    log('shot', path, w, hh)

def shots():
    while not host.child and time.time() - t0 < 60: time.sleep(0.25)
    tc = time.time(); log('engine child', hex(host.child or 0))
    if host.child:
        c = wintypes.RECT(); u.GetClientRect(host.child, ctypes.byref(c)); log('child client', c.right, c.bottom)
    for sec, name in ((30, 'fullscreen-30s-intro'), (50, 'fullscreen-50s-sniper')):
        d = tc + sec - time.time()
        if d > 0: time.sleep(d)
        shot(name)
threading.Thread(target=shots, daemon=True).start()

cmd = [sys.executable, 'run-lean-guided.py', 'paris', '--seconds', seconds, '--env', 'NF_OVERLAY=1',
       '--env', f'NIGHTFIRE_HOST_HWND={host.hwnd}'] + extra
p = subprocess.run(cmd, cwd=str(ROOT / 'nightfire-driving-native'), capture_output=True, text=True)
print('\n'.join(p.stdout.splitlines()[-12:]))
host.close()
log('done')
