"""One-window play: the launcher's persistent game window (native-driving/one-window).

A plain black Win32 window owned by play-nightfire-native.py. Its handle goes to each
engine as NIGHTFIRE_HOST_HWND; the engine (runtime/nightfire_host_window.h) then creates
its game window as a child filling this window instead of opening its own. So the
player keeps one window (position, size, maximised/fullscreen state) through every
Action <-> Driving hand-over, with black in between.

- Close (X button, Alt+F4 in the game, taskbar close): WM_CLOSE is passed to the
  engine's child window, so the engine exits exactly as when its own window is closed;
  close_requested tells the launcher to end the session instead of starting the next
  engine.
- Keyboard focus is handed to the engine's child window whenever this window gets it.
- Resizing / maximising resizes the child (asynchronously; never waits on the engine).
- NF_OVERLAY=1 (unless NF_DPI_AWARE=0): this window is per-monitor DPI aware like the
  engines then are (nightfire_host_window.h nf_host_dpi_setup), opens at the same size on
  screen, follows WM_DPICHANGED, and fullscreen covers the panel's real pixels.
  Otherwise it stays DPI-unaware, as before.
- NF_OVERLAY=1 display mode (F10 "Display mode" / Action's PC Graphics page): the engine
  posts NF_HOST_SET_MODE (wParam 0 windowed, 1 fullscreen kept on top, 2 borderless
  fullscreen) and this window switches its style live; windowed restores the last
  window size and position. A mode saved in overlay-settings.ini ("display=") is used
  at start instead of the launcher's windowed/fullscreen choice. Without a saved
  choice, or without NF_OVERLAY, the launcher's start mode stays.
"""
import ctypes, os, threading, time
from ctypes import wintypes as W

user32 = ctypes.WinDLL('user32', use_last_error=True)
gdi32 = ctypes.WinDLL('gdi32')
kernel32 = ctypes.WinDLL('kernel32')
LRESULT = ctypes.c_ssize_t
WNDPROC = ctypes.WINFUNCTYPE(LRESULT, W.HWND, W.UINT, W.WPARAM, W.LPARAM)

WM_DESTROY, WM_SIZE, WM_SETFOCUS, WM_CLOSE, WM_APP = 0x0002, 0x0005, 0x0007, 0x0010, 0x8000
NF_HOST_CHILD_READY = WM_APP + 0x4E46          # runtime/nightfire_host_window.h
HOST_QUIT = WM_APP + 0x4E47
NF_HOST_SET_MODE = WM_APP + 0x4E48             # runtime/nightfire_host_window.h (display mode)
GWL_STYLE, GWL_EXSTYLE, WS_EX_TOPMOST = -16, -20, 0x00000008
HWND_TOPMOST, HWND_NOTOPMOST = -1, -2
SWP_NOSIZE, SWP_NOMOVE, SWP_FRAMECHANGED, SWP_SHOWWINDOW = 0x0001, 0x0002, 0x0020, 0x0040
WS_OVERLAPPEDWINDOW, WS_POPUP, WS_VISIBLE, WS_CLIPCHILDREN = 0x00CF0000, 0x80000000, 0x10000000, 0x02000000
CW_USEDEFAULT = -0x80000000
SWP_NOZORDER, SWP_NOACTIVATE, SWP_ASYNCWINDOWPOS = 0x0004, 0x0010, 0x4000
GW_CHILD, GW_HWNDNEXT = 5, 2
WM_DPICHANGED = 0x02E0
OVERLAY_DPI = (os.environ.get('NF_OVERLAY', '')[:1] == '1' and   # see the module notes;
               os.environ.get('NF_DPI_AWARE', '')[:1] != '0')     # NF_DPI_AWARE=0 turns it off


def saved_display_mode():
    """The display mode saved by the F10 overlay / PC Graphics page (NF_OVERLAY=1 only):
    0 windowed, 1 fullscreen, 2 borderless fullscreen; None when not chosen yet."""
    if os.environ.get('NF_OVERLAY', '')[:1] != '1':
        return None
    path = os.path.join(os.environ.get('LOCALAPPDATA', ''), 'NightfirePC', 'overlay-settings.ini')
    try:
        with open(path, encoding='ascii', errors='replace') as f:
            for line in f:
                k, _, v = line.strip().partition('=')
                if k.strip() == 'display' and v.strip() in ('0', '1', '2'):
                    return int(v)
    except OSError:
        pass
    return None


class WNDCLASSEXW(ctypes.Structure):
    _fields_ = [('cbSize', W.UINT), ('style', W.UINT), ('lpfnWndProc', WNDPROC), ('cbClsExtra', ctypes.c_int),
                ('cbWndExtra', ctypes.c_int), ('hInstance', W.HINSTANCE), ('hIcon', W.HICON), ('hCursor', W.HANDLE),
                ('hbrBackground', W.HBRUSH), ('lpszMenuName', W.LPCWSTR), ('lpszClassName', W.LPCWSTR), ('hIconSm', W.HICON)]


class MONITORINFO(ctypes.Structure):
    _fields_ = [('cbSize', W.DWORD), ('rcMonitor', W.RECT), ('rcWork', W.RECT), ('dwFlags', W.DWORD)]


def _sig(f, res, *args):
    f.restype, f.argtypes = res, list(args)

_sig(user32.DefWindowProcW, LRESULT, W.HWND, W.UINT, W.WPARAM, W.LPARAM)
_sig(user32.RegisterClassExW, W.ATOM, ctypes.POINTER(WNDCLASSEXW))
_sig(user32.CreateWindowExW, W.HWND, W.DWORD, W.LPCWSTR, W.LPCWSTR, W.DWORD, ctypes.c_int, ctypes.c_int,
     ctypes.c_int, ctypes.c_int, W.HWND, W.HMENU, W.HINSTANCE, W.LPVOID)
_sig(user32.GetMessageW, W.BOOL, ctypes.POINTER(W.MSG), W.HWND, W.UINT, W.UINT)
_sig(user32.TranslateMessage, W.BOOL, ctypes.POINTER(W.MSG))
_sig(user32.DispatchMessageW, LRESULT, ctypes.POINTER(W.MSG))
_sig(user32.PostMessageW, W.BOOL, W.HWND, W.UINT, W.WPARAM, W.LPARAM)
_sig(user32.GetWindow, W.HWND, W.HWND, W.UINT)
_sig(user32.SetWindowPos, W.BOOL, W.HWND, W.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, W.UINT)
_sig(user32.SetFocus, W.HWND, W.HWND)
_sig(user32.GetClientRect, W.BOOL, W.HWND, ctypes.POINTER(W.RECT))
_sig(user32.GetWindowRect, W.BOOL, W.HWND, ctypes.POINTER(W.RECT))
_sig(user32.IsWindow, W.BOOL, W.HWND)
_sig(user32.DestroyWindow, W.BOOL, W.HWND)
_sig(user32.PostQuitMessage, None, ctypes.c_int)
_sig(user32.AdjustWindowRectEx, W.BOOL, ctypes.POINTER(W.RECT), W.DWORD, W.BOOL, W.DWORD)
_sig(user32.LoadCursorW, W.HANDLE, W.HINSTANCE, W.LPVOID)
_sig(user32.SetForegroundWindow, W.BOOL, W.HWND)
_sig(user32.GetForegroundWindow, W.HWND)
_sig(user32.MonitorFromWindow, W.HANDLE, W.HWND, W.DWORD)
_sig(user32.GetMonitorInfoW, W.BOOL, W.HANDLE, ctypes.POINTER(MONITORINFO))
_sig(user32.SetWindowTextW, W.BOOL, W.HWND, W.LPCWSTR)
_sig(user32.GetWindowThreadProcessId, W.DWORD, W.HWND, ctypes.POINTER(W.DWORD))
_sig(user32.SetWindowLongPtrW, ctypes.c_ssize_t, W.HWND, ctypes.c_int, ctypes.c_ssize_t)
_sig(user32.GetWindowLongPtrW, ctypes.c_ssize_t, W.HWND, ctypes.c_int)
_sig(gdi32.GetStockObject, W.HANDLE, ctypes.c_int)
_sig(kernel32.GetModuleHandleW, W.HMODULE, W.LPCWSTR)
try:   # match the engines' DPI awareness so the child fills the window exactly
    _sig(user32.SetThreadDpiAwarenessContext, W.HANDLE, W.HANDLE)
    _sig(user32.GetDpiForSystem, W.UINT)
    _sig(user32.AdjustWindowRectExForDpi, W.BOOL, ctypes.POINTER(W.RECT), W.DWORD, W.BOOL, W.DWORD, W.UINT)
except AttributeError:
    pass


class HostWindow:
    def __init__(self, title='Nightfire', client=(960, 720), fullscreen=False, log=None):
        self.title, self.client, self.fullscreen, self._log = title, client, fullscreen, log
        self.mode = saved_display_mode()          # display mode saved by the overlay (None: launcher's choice)
        if self.mode is None:
            self.mode = 1 if fullscreen else 0
        else:
            self.fullscreen = self.mode != 0
        self.saved_rect = None                    # windowed position/size to restore
        self.hwnd = None
        self.close_requested = False
        self.close_time = None
        self.child = None
        self.children = []                       # (time, pid, child hwnd) evidence
        self._ready = threading.Event()
        self._proc = WNDPROC(self._wndproc)      # keep the callback alive
        self._thread = threading.Thread(target=self._run, name='nightfire-host-window', daemon=True)
        self._thread.start()
        self._ready.wait(10)
        if not self.hwnd:
            raise RuntimeError('host window creation failed')

    def log(self, text):
        if self._log:
            self._log(text)

    def _children(self):
        c = user32.GetWindow(self.hwnd, GW_CHILD)
        while c:
            yield c
            c = user32.GetWindow(c, GW_HWNDNEXT)

    def _fit(self, child):
        r = W.RECT()
        user32.GetClientRect(self.hwnd, ctypes.byref(r))
        user32.SetWindowPos(child, None, 0, 0, r.right, r.bottom, SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS)

    def _wndproc(self, h, m, w, l):
        try:
            if m == WM_CLOSE:
                self.close_requested = True
                self.close_time = self.close_time or time.monotonic()
                live = [c for c in self._children()]
                for c in live:
                    user32.PostMessageW(c, WM_CLOSE, 0, 0)
                self.log(f'window closed by the player; WM_CLOSE passed to {len(live)} engine window(s)')
                return 0
            if m == WM_DPICHANGED and OVERLAY_DPI and l:   # another monitor's scaling: the suggested size
                r = ctypes.cast(l, ctypes.POINTER(W.RECT)).contents
                user32.SetWindowPos(h, None, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE)
                self.log(f'window now at {(w >> 16) & 0xFFFF} DPI: {r.right - r.left}x{r.bottom - r.top}')
                return 0
            if m == WM_SIZE:
                for c in self._children():
                    self._fit(c)
            elif m == WM_SETFOCUS:
                for c in self._children():
                    user32.SetFocus(c)
                    return 0
            elif m == NF_HOST_CHILD_READY:
                child = W.HWND(l).value
                self.child = child
                self.children.append((time.monotonic(), int(w), child))
                self.log(f'engine window {child:#x} (pid {int(w)}) is now inside host {self.hwnd:#x}')
                if user32.IsWindow(child):
                    self._fit(child)
                    if user32.GetForegroundWindow() == self.hwnd:
                        user32.SetFocus(child)
                    if self.close_requested:      # closed while that engine was still loading
                        user32.PostMessageW(child, WM_CLOSE, 0, 0)
                return 0
            elif m == NF_HOST_SET_MODE:
                self.set_mode(h, int(w))
                return 0
            elif m == HOST_QUIT:
                user32.DestroyWindow(h)
                return 0
            elif m == WM_DESTROY:
                user32.PostQuitMessage(0)
                return 0
        except Exception as e:                    # never let an exception cross the callback
            self.log(f'host window error: {e!r}')
        return user32.DefWindowProcW(h, m, w, l)

    def set_mode(self, h, mode):
        """Display mode, on this window's thread: 0 windowed (last size and position),
        1 fullscreen kept on top, 2 borderless fullscreen."""
        if mode not in (0, 1, 2):
            return
        style = user32.GetWindowLongPtrW(h, GWL_STYLE) & 0xFFFFFFFF
        if mode == 0:
            style = (style & ~WS_POPUP & 0xFFFFFFFF) | WS_OVERLAPPEDWINDOW | WS_VISIBLE
            user32.SetWindowLongPtrW(h, GWL_STYLE, ctypes.c_ssize_t(style if style < 0x80000000 else style - 0x100000000).value)
            if self.saved_rect:
                l, t, r, b = self.saved_rect
            else:   # started fullscreen: the launcher's window size, centred on the monitor
                mi = MONITORINFO(); mi.cbSize = ctypes.sizeof(MONITORINFO)
                user32.GetMonitorInfoW(user32.MonitorFromWindow(h, 2), ctypes.byref(mi))
                rr = W.RECT(0, 0, self.client[0] * self._dpi // 96, self.client[1] * self._dpi // 96)
                user32.AdjustWindowRectEx(ctypes.byref(rr), WS_OVERLAPPEDWINDOW, False, 0)
                cw, ch = rr.right - rr.left, rr.bottom - rr.top
                wa = mi.rcWork
                l, t = wa.left + max(0, (wa.right - wa.left - cw) // 2), wa.top + max(0, (wa.bottom - wa.top - ch) // 2)
                r, b = l + cw, t + ch
            user32.SetWindowPos(h, W.HWND(HWND_NOTOPMOST), l, t, r - l, b - t, SWP_FRAMECHANGED | SWP_SHOWWINDOW)
        else:
            if not (style & WS_POPUP):
                self.saved_rect = self.rect()
            style = (style & ~WS_OVERLAPPEDWINDOW & 0xFFFFFFFF) | WS_POPUP | WS_VISIBLE
            user32.SetWindowLongPtrW(h, GWL_STYLE, ctypes.c_ssize_t(style if style < 0x80000000 else style - 0x100000000).value)
            mi = MONITORINFO(); mi.cbSize = ctypes.sizeof(MONITORINFO)
            user32.GetMonitorInfoW(user32.MonitorFromWindow(h, 2), ctypes.byref(mi))
            r = mi.rcMonitor
            user32.SetWindowPos(h, W.HWND(HWND_TOPMOST if mode == 1 else HWND_NOTOPMOST), r.left, r.top,
                                r.right - r.left, r.bottom - r.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW)
        self.mode = mode
        self.log(f'display mode {("windowed", "fullscreen", "borderless fullscreen")[mode]}')

    def _run(self):
        dpi = 96
        try:
            if OVERLAY_DPI:
                user32.SetThreadDpiAwarenessContext(W.HANDLE(-4))  # DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
                dpi = max(96, user32.GetDpiForSystem())
            else:
                user32.SetThreadDpiAwarenessContext(W.HANDLE(-1))  # DPI_AWARENESS_CONTEXT_UNAWARE
        except Exception:
            dpi = 96
        self._dpi = dpi
        if OVERLAY_DPI:
            self.log(f'host window DPI aware, opens at {dpi} DPI ({dpi / 96:.2f}x)')
        inst = kernel32.GetModuleHandleW(None)
        wc = WNDCLASSEXW()
        wc.cbSize = ctypes.sizeof(WNDCLASSEXW)
        wc.lpfnWndProc = self._proc
        wc.hInstance = inst
        wc.hCursor = user32.LoadCursorW(None, W.LPVOID(32512))    # IDC_ARROW
        wc.hbrBackground = gdi32.GetStockObject(4)                # BLACK_BRUSH: black between engines
        wc.lpszClassName = 'NightfireOneWindowHost'
        user32.RegisterClassExW(ctypes.byref(wc))
        if self.fullscreen:   # borderless fullscreen on the primary monitor's area
            mi = MONITORINFO(); mi.cbSize = ctypes.sizeof(MONITORINFO)
            user32.GetMonitorInfoW(user32.MonitorFromWindow(None, 1), ctypes.byref(mi))
            r = mi.rcMonitor
            style, x, y, cw, ch = WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN, r.left, r.top, r.right - r.left, r.bottom - r.top
        else:
            style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN
            r = W.RECT(0, 0, self.client[0] * dpi // 96, self.client[1] * dpi // 96)
            if dpi != 96:
                user32.AdjustWindowRectExForDpi(ctypes.byref(r), style, False, 0, dpi)
            else:
                user32.AdjustWindowRectEx(ctypes.byref(r), style, False, 0)
            x = y = CW_USEDEFAULT
            cw, ch = r.right - r.left, r.bottom - r.top
        self.hwnd = user32.CreateWindowExW(0, wc.lpszClassName, self.title, style, x, y, cw, ch, None, None, inst, None)
        self._ready.set()
        if not self.hwnd:
            return
        if self.mode == 1 and os.environ.get('NF_OVERLAY', '')[:1] == '1' and saved_display_mode() == 1:
            user32.SetWindowPos(self.hwnd, W.HWND(HWND_TOPMOST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE)   # saved "Fullscreen": on top
        if saved_display_mode() is not None:
            self.log(f'saved display mode {("windowed", "fullscreen", "borderless fullscreen")[self.mode]} used at start')
        user32.SetForegroundWindow(self.hwnd)
        msg = W.MSG()
        while user32.GetMessageW(ctypes.byref(msg), None, 0, 0) > 0:
            user32.TranslateMessage(ctypes.byref(msg))
            user32.DispatchMessageW(ctypes.byref(msg))

    def has_child(self):
        return any(True for _ in self._children())

    def rect(self):
        r = W.RECT()
        user32.GetWindowRect(self.hwnd, ctypes.byref(r))
        return (r.left, r.top, r.right, r.bottom)

    def set_title(self, text):
        user32.SetWindowTextW(self.hwnd, text)

    def close(self):
        if self.hwnd:
            user32.PostMessageW(self.hwnd, HOST_QUIT, 0, 0)
            self._thread.join(5)
