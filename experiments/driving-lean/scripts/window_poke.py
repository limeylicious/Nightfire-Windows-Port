"""Resize, minimise and restore the lean game window during a run (test aid).

Usage: python scripts/window_poke.py [delay_seconds]
Finds the window whose title starts with "Nightfire Driving - experimental GPU preview".
"""
import ctypes, sys, time
from ctypes import wintypes

u = ctypes.windll.user32
TITLE = "Nightfire Driving - experimental GPU preview"
time.sleep(float(sys.argv[1]) if len(sys.argv) > 1 else 30)
found = []
@ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
def _enum(hwnd, _):
    buf = ctypes.create_unicode_buffer(300); u.GetWindowTextW(hwnd, buf, 300)
    if buf.value.startswith(TITLE): found.append(hwnd)
    return True
u.EnumWindows(_enum, 0)
h = found[0] if found else 0
print('window', hex(h or 0), flush=True)
if not h:
    sys.exit(1)
r = wintypes.RECT(); u.GetWindowRect(h, ctypes.byref(r)); print('rect', r.left, r.top, r.right, r.bottom, flush=True)
SWP_NOZORDER, SWP_NOMOVE = 4, 2
for w, hh in ((1300, 1000), (500, 400), (976, 759)):
    u.SetWindowPos(h, 0, 0, 0, w, hh, SWP_NOZORDER | SWP_NOMOVE); time.sleep(2)
    u.GetClientRect(h, ctypes.byref(r)); print('client', r.right, r.bottom, flush=True)
u.ShowWindow(h, 6); time.sleep(3); print('minimised', u.IsIconic(h), flush=True)   # SW_MINIMIZE
u.ShowWindow(h, 9); time.sleep(3); print('restored', not u.IsIconic(h), flush=True)  # SW_RESTORE
