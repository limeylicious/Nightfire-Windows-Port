"""Widescreen check driver (test aid, 2026-10-09).

Waits for the Driving window, sets a 1280x720 client area, screenshots the whole
client area (bars included) at given times, and sends F10 / Down / Right / Left /
Esc with SendInput to check the Picture shape row and its restart hint.
Usage: python drive_ws_check.py <out_dir>
"""
import ctypes, sys, time, os
from ctypes import wintypes

u = ctypes.windll.user32; g = ctypes.windll.gdi32
u.SetProcessDPIAware()
TITLE = "Nightfire Driving - experimental GPU preview"
out = sys.argv[1]
t0 = time.time()

def log(*a):
    print('[%6.1f]' % (time.time() - t0), *a, flush=True)

def find():
    found = []
    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def _enum(hwnd, _):
        buf = ctypes.create_unicode_buffer(300); u.GetWindowTextW(hwnd, buf, 300)
        if buf.value.startswith(TITLE): found.append(hwnd)
        return True
    u.EnumWindows(_enum, 0)
    return found[0] if found else 0

h = 0
while not h and time.time() - t0 < 60:
    h = find(); time.sleep(0.25)
if not h:
    log('window not found'); sys.exit(1)
tw = time.time()   # times below are from the window's appearance
log('window', hex(h))

def at(sec):
    d = tw + sec - time.time()
    if d > 0: time.sleep(d)

def fg():
    if u.GetForegroundWindow() == h:
        return True   # no Alt tap: it would put the window into menu mode and eat the next arrow keys
    u.keybd_event(0x12, 0, 0, 0); u.keybd_event(0x12, 0, 2, 0)   # Alt tap lets SetForegroundWindow work
    u.SetForegroundWindow(h); time.sleep(0.3)
    return u.GetForegroundWindow() == h

class KI(ctypes.Structure):
    _fields_ = [('wVk', wintypes.WORD), ('wScan', wintypes.WORD), ('dwFlags', wintypes.DWORD),
                ('time', wintypes.DWORD), ('dwExtraInfo', ctypes.c_size_t)]
class INP(ctypes.Structure):
    class _U(ctypes.Union):
        _fields_ = [('ki', KI), ('pad', ctypes.c_byte * 32)]
    _anonymous_ = ('u',)
    _fields_ = [('type', wintypes.DWORD), ('u', _U)]

EXT = {0x25, 0x26, 0x27, 0x28}   # arrows are extended keys
def key(vk, name):
    sc = u.MapVirtualKeyW(vk, 0)
    fl = 1 if vk in EXT else 0
    arr = (INP * 2)()
    for i, up in enumerate((0, 2)):
        arr[i].type = 1; arr[i].ki = KI(vk, sc, fl | up, 0, 0)
    ok = fg()
    n = u.SendInput(2, arr, ctypes.sizeof(INP))
    log('key', name, 'foreground' if ok else 'NOT foreground', 'sent', n)
    time.sleep(0.4)

def shot(name):
    fg()
    r = wintypes.RECT(); u.GetClientRect(h, ctypes.byref(r))
    p = wintypes.POINT(0, 0); u.ClientToScreen(h, ctypes.byref(p))
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

# 1280x720 client in the game's own pixels: the game is not DPI aware, so on a
# scaled display (200% here) its window is that many times larger on screen.
at(3)
try:   # 1280x720 at 96 DPI = this many screen pixels (same size on screen either way)
    sc = u.GetDpiForSystem() / 96.0
    log('system DPI', u.GetDpiForSystem(), 'window DPI', u.GetDpiForWindow(h))
except Exception:
    sc = 1.0
log('scale', sc)
r = wintypes.RECT(0, 0, int(1280 * sc), int(720 * sc))
style = u.GetWindowLongW(h, -16); exstyle = u.GetWindowLongW(h, -20)
u.AdjustWindowRectEx(ctypes.byref(r), style, False, exstyle)
u.SetWindowPos(h, 0, 20, 20, r.right - r.left, r.bottom - r.top, 4)
time.sleep(1); c = wintypes.RECT(); u.GetClientRect(h, ctypes.byref(c))
want_w, want_h = int(1280 * sc), int(720 * sc)
if (c.right, c.bottom) != (want_w, want_h):   # correct for the border sizes
    wr = wintypes.RECT(); u.GetWindowRect(h, ctypes.byref(wr))
    u.SetWindowPos(h, 0, 20, 20, wr.right - wr.left + want_w - c.right, wr.bottom - wr.top + want_h - c.bottom, 4)
    time.sleep(1); u.GetClientRect(h, ctypes.byref(c))
log('client (screen pixels)', c.right, c.bottom)
fg()
at(30); shot('screen-30s-intro')
at(50); shot('screen-50s-sniper')
# mouse aim: F1 captures the mouse (driving_input224), then relative moves (raw input sees device counts)
at(51); key(0x70, 'F1')
class MI(ctypes.Structure):
    _fields_ = [('dx', ctypes.c_long), ('dy', ctypes.c_long), ('mouseData', wintypes.DWORD), ('dwFlags', wintypes.DWORD),
                ('time', wintypes.DWORD), ('dwExtraInfo', ctypes.c_size_t)]
class MINP(ctypes.Structure):
    class _U(ctypes.Union):
        _fields_ = [('mi', MI), ('pad', ctypes.c_byte * 32)]
    _anonymous_ = ('u',)
    _fields_ = [('type', wintypes.DWORD), ('u', _U)]
for i in range(20):
    m = MINP(); m.type = 0; m.mi = MI(15, 0, 0, 0x0001, 0, 0)   # MOUSEEVENTF_MOVE, 15 counts right
    u.SendInput(1, ctypes.byref(m), ctypes.sizeof(MINP)); time.sleep(0.03)
log('mouse moved 300 counts right')
at(53); shot('screen-53s-sniper-after-mouse')
at(58); key(0x79, 'F10'); at(60); shot('screen-60s-menu-open')
at(62); key(0x28, 'Down')
at(63); key(0x27, 'Right'); at(65); shot('screen-65s-shape-stretch-hint')
at(67); key(0x25, 'Left'); at(69); shot('screen-69s-shape-widescreen')
at(72); key(0x1B, 'Esc'); at(74); shot('screen-74s-menu-closed')
log('done')
