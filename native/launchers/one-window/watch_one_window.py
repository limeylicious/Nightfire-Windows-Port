"""Test helper for one-window play: logs every second which Nightfire top-level windows
exist, the host's child window, keyboard focus and foreground; saves a screenshot of the
host window rect every few seconds; optionally sends WM_CLOSE to the host N seconds after
the Driving child window appears (simulates the player closing the window).

Usage: python watch_one_window.py OUTDIR [--close-after-driving N] [--max-seconds N]
"""
import argparse, base64, ctypes, subprocess, sys, time
from ctypes import wintypes as W
from pathlib import Path

u = ctypes.WinDLL('user32')
u.SetProcessDPIAware()   # real pixel rects for the screenshots
ENUM = ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
u.EnumWindows.argtypes = [ENUM, W.LPARAM]
u.EnumChildWindows.argtypes = [W.HWND, ENUM, W.LPARAM]
for f in ('GetClassNameW', 'GetWindowTextW'):
    getattr(u, f).argtypes = [W.HWND, W.LPWSTR, ctypes.c_int]
u.IsWindowVisible.argtypes = [W.HWND]
u.GetWindowRect.argtypes = [W.HWND, ctypes.POINTER(W.RECT)]
u.GetWindowThreadProcessId.argtypes = [W.HWND, ctypes.POINTER(W.DWORD)]
u.GetWindowThreadProcessId.restype = W.DWORD
u.GetForegroundWindow.restype = W.HWND
u.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]


class GUITHREADINFO(ctypes.Structure):
    _fields_ = [('cbSize', W.DWORD), ('flags', W.DWORD), ('hwndActive', W.HWND), ('hwndFocus', W.HWND),
                ('hwndCapture', W.HWND), ('hwndMenuOwner', W.HWND), ('hwndMoveSize', W.HWND),
                ('hwndCaret', W.HWND), ('rcCaret', W.RECT)]


def cls(h):
    b = ctypes.create_unicode_buffer(128); u.GetClassNameW(h, b, 128); return b.value


def title(h):
    b = ctypes.create_unicode_buffer(256); u.GetWindowTextW(h, b, 256); return b.value


GAME = ('NightfireOneWindowHost', 'NightfireFrameObserver', 'NightfireDriving201')


def snapshot():
    tops, kids = [], {}
    def top(h, _):
        c = cls(h)
        if c in GAME and u.IsWindowVisible(h):
            tops.append((h, c))
        return True
    cb = ENUM(top); u.EnumWindows(cb, 0)
    for h, c in tops:
        lst = []
        def kid(k, _):
            lst.append((k, cls(k))); return True
        kb = ENUM(kid); u.EnumChildWindows(h, kb, 0)
        kids[h] = lst
    return tops, kids


def main():
    p = argparse.ArgumentParser()
    p.add_argument('out')
    p.add_argument('--close-after-driving', type=float, default=0)
    p.add_argument('--max-seconds', type=float, default=240)
    a = p.parse_args()
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    log = (out / 'watch.txt').open('w', buffering=1)
    t0 = time.monotonic(); seen_game = False; driving_at = None; closed = False; shot_at = 0; n = 0
    while time.monotonic() - t0 < a.max_seconds:
        t = time.monotonic() - t0
        tops, kids = snapshot()
        fg = u.GetForegroundWindow()
        parts = []
        for h, c in tops:
            r = W.RECT(); u.GetWindowRect(h, ctypes.byref(r))
            tid = u.GetWindowThreadProcessId(h, None)
            gi = GUITHREADINFO(); gi.cbSize = ctypes.sizeof(gi); u.GetGUIThreadInfo(tid, ctypes.byref(gi))
            parts.append(f'{c}:{h:#x} rect=({r.left},{r.top},{r.right},{r.bottom}) fg={int(fg == h)} '
                         f'focus={gi.hwndFocus or 0:#x} children=[{", ".join(f"{k:#x}:{kc}" for k, kc in kids[h])}] '
                         f'title="{title(h)}"')
            if c == 'NightfireOneWindowHost' and any(kc == 'NightfireDriving201' for _, kc in kids[h]) and driving_at is None:
                driving_at = t
        log.write(f'{t:7.1f} top-level game windows={len(tops)} ' + ' | '.join(parts) + '\n')
        if tops:
            seen_game = True
        elif seen_game and t > 20:
            log.write(f'{t:7.1f} no game window left; stopping\n'); break
        host = [h for h, c in tops if c == 'NightfireOneWindowHost']
        if host and t - shot_at >= 3:
            shot_at = t; n += 1
            r = W.RECT(); u.GetWindowRect(host[0], ctypes.byref(r))
            ps = (f"Add-Type -AssemblyName System.Drawing;Add-Type -Namespace W -Name U -MemberDefinition '[DllImport(\"user32.dll\")] public static extern bool SetProcessDPIAware();';[void][W.U]::SetProcessDPIAware();$b=New-Object System.Drawing.Bitmap({r.right - r.left},{r.bottom - r.top});"
                  f"$g=[System.Drawing.Graphics]::FromImage($b);$g.CopyFromScreen({r.left},{r.top},0,0,$b.Size);"
                  f"$b.Save('{out / f'shot-{n:03d}-{int(t):04d}s.png'}');")
            subprocess.Popen(['powershell', '-NoProfile', '-EncodedCommand', base64.b64encode(ps.encode('utf-16-le')).decode()], creationflags=0x08000000)
        if a.close_after_driving and driving_at is not None and not closed and t - driving_at >= a.close_after_driving and host:
            u.PostMessageW(host[0], 0x0010, 0, 0); closed = True
            log.write(f'{t:7.1f} WM_CLOSE sent to host {host[0]:#x} (simulated player close)\n')
        time.sleep(1)
    log.close()


if __name__ == '__main__':
    main()
