/* NIGHTFIRE_LOCKSTEP=1 (off by default): groundwork for online play, where every
 * PC runs the same match and only controller presses travel between them. That
 * only works if two copies given the same presses stay identical, and this file
 * is how we find out. NF_NET=host|join (online play, part 4) turns it on too.
 *
 * 1. Game clock. Every clock the game can read (KeTickCount, KeQuerySystemTime,
 *    KeQueryPerformanceCounter/Frequency, KeQueryInterruptTime, rdtsc) counts
 *    game frames instead of real time: 1/60 s per pass of the frame routine
 *    0xDD1D0, plus 1 microsecond per clock read on the game thread inside a
 *    frame (so a loop waiting for time to pass still ends, the same way on every
 *    PC). Real-time pacing is untouched: the game still shows 60 frames a second.
 *    Without frames (a movie, a loading screen, a wait loop) the game thread's
 *    clock moves by what it does, never by real time, so it is the same on
 *    every PC: after 2000 reads outside the frame routine since the last
 *    frame each read moves it 1 ms, a sleep (KeDelayExecutionThread,
 *    kernel_bridge.c) moves it by the time asked for, a picture shown without
 *    a frame (nf_lockstep_present, from the renderer's present) moves it
 *    1/60 s, and a poll of sound state counts as a read. KeTickCount follows.
 *    Time a frameless stretch adds carries on into the frames after it. Real speed comes from the sound: in lockstep the game thread mixes up
 *    to its clock and waits while 43 ms are waiting to be played
 *    (nds_action.c), so movies, menus and play run at the sound card's pace.
 *    Stalls (NF_LOCKSTEP_STALL=frames, the default): real time is added only
 *    when there has been no frame for 3 s and the game clock moved less than a
 *    quarter of real time over the last 3 s (the game thread is stuck, or
 *    waits in a way none of the above counts); that is logged as a stall,
 *    since it is not the same on every PC. NF_LOCKSTEP_STALL=real = lockstep1's
 *    rule: real time whenever the game thread makes no frame for 250 ms (each
 *    PC loads at its own speed, so not for online play). Waiting for the other
 *    PCs never counts as a stall. Every frameless
 *    stretch over 1 s is logged with what moved its clock.
 * 2. NF_INPUT_RECORD=<file> / NF_INPUT_REPLAY=<file>: each controller port's
 *    state, once per input frame (0x6CF50), is written to or read from <file>.
 *    Within one input frame every read of a port returns the same state. In
 *    lockstep the mouse moves the right stick (no direct mouse), so all input is
 *    in those states. The PC menu pointer is off in lockstep (it moves the
 *    menu cursor directly, not through the controller; pcg_menu.c).
 *    Online, the recording holds the agreed presses of every player, so a
 *    match can be played again on one PC.
 * 3. NF_STATE_HASH=<file>: a hash of every 64 KB block of game memory, one
 *    sixtieth of the blocks after each frame (so each block once a second, no
 *    hitch). scripts/nf_state_diff.py compares two such files and names the first
 *    frame and the blocks that differ.
 * 4. Online play (NF_NET=host|join, nf_net.c): at boot this PC joins the match
 *    (a small window shows who is still missing). Then, once per input frame,
 *    this PC's own players' presses go to the others and every port gets the
 *    agreed state of that port's player, the same on every PC. This PC's player
 *    k is the physical player k here (keyboard/mouse + controller 0 = player 1).
 *    Every 60 frames the same memory hash as (3) is compared between the PCs
 *    (NF_SYNC=0 off; NF_SYNC_SKIP=hex block list, e.g. 0x10-0x1F,0x3F0, leaves
 *    blocks out); a difference stops the match on every PC ("desync"). Joining
 *    is refused unless the exe, default.xbe and the play switches are the same.
 *
 * Test: play with NF_INPUT_RECORD + NF_STATE_HASH, then run again with
 * NF_INPUT_REPLAY + NF_STATE_HASH (another file) and compare the two hash files. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nf_net.h"

extern ptrdiff_t g_xbox_mem_offset;
size_t xbox_GetMappedSize(void);

#define LS_UNIT        10000000ll             /* clock unit: 100 ns */
#define LS_BOOT        (60ll * LS_UNIT)       /* virtual uptime at the first frame */
#define LS_FILETIME    126858528000000000ll   /* 2003-01-01 00:00 UTC */
#define LS_QPF         10000000ll
#define LS_STALL_MS    250
#define LS_STUCK_MS    3000                   /* NF_LOCKSTEP_STALL=frames */
#define LS_FINE_READS  2000u                  /* reads outside the frame routine at 1 us each... */
#define LS_COARSE      10000                  /* ...then 1 ms each */
#define LS_BLOCK       0x10000u
#define LS_HASH_SPREAD 60u

static int s_on = -1;
static DWORD s_main_tid;
static LONG64 s_frames;                 /* frame-routine passes (game thread only) */
static unsigned s_depth;
static volatile LONG64 s_frame_t;       /* virtual time at this frame's start */
static volatile LONG64 s_main_t;        /* last time handed to the game thread */
static volatile LONG64 s_stall_t;       /* real time added during stalls */
static volatile LONG64 s_frame_real;    /* QPC at this frame's start (or the end of a network wait) */
static volatile LONG s_stalled, s_stalls;
static volatile uint32_t *s_tick;       /* KeTickCount in game memory */
static volatile LONG s_net_wait;        /* waiting for the other PCs: not a stall */
static int s_stall_frames;              /* NF_LOCKSTEP_STALL=frames */
static LONG64 s_qpf;
static unsigned s_reads;                /* game-thread reads outside the frame routine since the frame started */
static unsigned s_frame_reads, s_max_frame_reads;   /* reads inside the frame routine: this frame, most in one */
static LONG64 s_frame_off;              /* frameless time carried into the frames after it */
static void (*s_audio_hook)(void);      /* nds_action.c: mix up to the frame start */
static struct { unsigned reads, sleeps, polls, presents; LONG64 slept; } s_fl;   /* this frameless stretch */
static LONG64 s_present_frame = -1;     /* s_frames at the last picture shown */
static int s_online;                    /* online match running */
static int s_pace_us;                   /* online: how far this PC should have slowed down */
static unsigned s_held_back;

static int net_wanted(void)
{
    const char *n = getenv("NF_NET");
    return n && (!_stricmp(n, "host") || !_stricmp(n, "join") || !_stricmp(n, "lobby"));
}

int nf_lockstep_on(void)
{
    if (s_on < 0) {
        const char *e = getenv("NIGHTFIRE_LOCKSTEP"), *st = getenv("NF_LOCKSTEP_STALL");
        s_on = (e && e[0] == '1') || net_wanted();
        /* the clocks must not follow real time (each PC loads at its own speed; a
         * replay must see the same movie and load times as the recording) */
        s_stall_frames = st && *st ? _stricmp(st, "real") != 0 : 1;
        if (s_on) fprintf(stderr, "[LOCKSTEP] on: game clocks count frames (1/60 s each); direct mouse and menu pointer off; stalls: %s\n",
                          s_stall_frames ? "real time only when stuck (no frame for 3 s and the game clock crawling)" : "real time after 250 ms with no frame");
    }
    return s_on;
}

static LONG64 qpc(void) { LARGE_INTEGER c; QueryPerformanceCounter(&c); return c.QuadPart; }

static int game_thread(void) { return !s_main_tid || GetCurrentThreadId() == s_main_tid; }

/* The game thread's clock moves by d (100 ns units); not during a stall, when it
 * follows real time. */
static LONG64 ls_advance(LONG64 d)
{
    if (s_stalled) d = 0;
    LONG64 t = s_main_t + d;
    if (t < s_frame_t) t = s_frame_t;
    s_main_t = t;
    /* without frames KeTickCount follows (never backwards) */
    if ((s_reads > LS_FINE_READS || d >= LS_COARSE) && s_tick) {
        uint32_t ms = (uint32_t)((LS_BOOT + t + s_stall_t) / 10000);
        if ((int32_t)(ms - *s_tick) > 0) *s_tick = ms;
    }
    return t;
}
/* Virtual time in 100 ns units since the virtual boot. */
static LONG64 ls_time(void)
{
    if (game_thread()) {
        s_fl.reads++;
        /* inside the frame routine a read is always 1 us (a frame may read the clock often);
         * outside it (a movie or wait loop with no frame) the 2000-read budget applies */
        if (s_depth) s_frame_reads++;
        LONG64 t = ls_advance(s_depth || s_reads++ < LS_FINE_READS ? 10 : LS_COARSE);
        return LS_BOOT + t + s_stall_t;
    }
    return LS_BOOT + s_frame_t + s_stall_t;
}

/* KeDelayExecutionThread on the game thread (kernel_bridge.c, before the real
 * sleep): the game clock moves by the time asked for. Xbox intervals: negative =
 * relative, in 100 ns; positive = an absolute system time. */
void nf_lockstep_sleep(int64_t interval)
{
    if (s_on <= 0 || !game_thread()) return;
    LONG64 d = interval < 0 ? -interval : interval - (LS_FILETIME + LS_BOOT + s_main_t + s_stall_t);
    if (d <= 0) return;
    if (d > 10ll * LS_UNIT) d = 10ll * LS_UNIT;
    s_fl.sleeps++; s_fl.slept += d;
    ls_advance(d);
}
/* For nds_action.c: the calling thread's game clock (100 ns, from the virtual
 * boot, stalls included) without moving it, and a poll of sound state, which
 * counts as a clock read on the game thread. */
uint64_t nf_lockstep_now(void) { return (uint64_t)((game_thread() ? s_main_t : s_frame_t) + s_stall_t); }
void nf_lockstep_poll(void) { if (s_on > 0 && game_thread()) { s_fl.polls++; s_fl.reads--; (void)ls_time(); } }
int nf_lockstep_game_thread(void) { return game_thread(); }
/* The clocks follow real time (a stall): the sound must not hold the game thread back then. */
int nf_lockstep_stalled(void) { return s_stalled != 0; }
/* The renderer showed a picture (game thread). In a frame that is part of the
 * frame; a second one with no frame in between (a movie or loading loop that
 * shows a picture every pass) moves the game clock 1/60 s. */
void nf_lockstep_present(void)
{
    if (s_on <= 0 || !game_thread()) return;
    if (s_present_frame == s_frames) { s_fl.presents++; ls_advance(LS_UNIT / 60); }
    s_present_frame = s_frames;
}
void nf_lockstep_set_audio_hook(void (*fn)(void)) { s_audio_hook = fn; }
/* Online: this PC's players' game ports (first .. first + count - 1, counted from 0 as in
 * nf_net_local_ports) and how many ports the match has (a PC that left keeps its ports), so
 * the renderer can show only this PC's views of a split screen (lean_d3d.c ov_frame_done;
 * NF_OWN_VIEW=0 shows the whole split screen). 0 offline, in local play and in a replay. */
int nf_lockstep_view_ports(unsigned *first, unsigned *count, unsigned *total)
{
    static int want = -1;
    if (want < 0) { const char *v = getenv("NF_OWN_VIEW"); want = !(v && v[0] == '0'); }
    if (!want || !s_online || !nf_net_on()) return 0;
    unsigned f0 = 0, n0 = 0, t = 0;
    nf_net_local_ports(&f0, &n0);
    for (int i = 0; i < NF_NET_PEERS; i++) {
        unsigned f = 0, c = 0;
        if (nf_net_peer(i, NULL, &f, &c, NULL, NULL) && f + c > t) t = f + c;
    }
    *first = f0; *count = n0; *total = t;
    return n0 > 0 && f0 + n0 <= t && t >= 2;
}

uint64_t nf_lockstep_system_time(void) { return (uint64_t)(LS_FILETIME + ls_time()); }
uint64_t nf_lockstep_interrupt_time(void) { return (uint64_t)ls_time(); }
uint64_t nf_lockstep_qpc(void) { return (uint64_t)ls_time(); }
uint64_t nf_lockstep_qpf(void) { return (uint64_t)LS_QPF; }
/* The Xbox's rdtsc runs at 733.33 MHz = 220/3 counts per 100 ns. */
uint64_t nf_lockstep_tsc(void) { uint64_t t = (uint64_t)ls_time(); return t / 3u * 220u + t % 3u * 220u / 3u; }

/* KeTickCount (ms) is read straight from game memory. The game thread writes it at
 * each frame start (so every read in a frame sees the same value on every PC) and
 * without frames as its clock moves; this thread only moves it during stalls. */
static DWORD WINAPI tick_thread(LPVOID p)
{
    (void)p;
    HANDLE t = CreateWaitableTimerExW(NULL, NULL, 0x00000002 /* HIGH_RESOLUTION */, TIMER_ALL_ACCESS);
    LONG64 last = qpc(), win_at = last, win_t = s_main_t, win_frames = s_frames;
    int crawl = 0;
    while (s_tick) {
        LONG64 now = qpc();
        LONG64 idle_ms = (now - s_frame_real) * 1000 / s_qpf;
        if (s_stall_frames) {
            /* every 3 s: did the game clock move at least a quarter of real time? */
            if (s_frames != win_frames || s_net_wait) { win_frames = s_frames; win_at = now; win_t = s_main_t; crawl = 0; }
            else if ((now - win_at) * 1000 / s_qpf >= LS_STUCK_MS) {
                crawl = (s_main_t - win_t) * 4 < (now - win_at) * LS_UNIT / s_qpf;
                win_at = now; win_t = s_main_t;
            }
        }
        int stalled = s_stall_frames ? crawl && idle_ms > LS_STUCK_MS : idle_ms > LS_STALL_MS;
        if (stalled && !s_net_wait) {
            if (!s_stalled) { InterlockedExchange(&s_stalled, 1); InterlockedIncrement(&s_stalls); }
            InterlockedAdd64(&s_stall_t, (now - last) * LS_UNIT / s_qpf);
            LONG64 t = s_main_t > s_frame_t ? s_main_t : s_frame_t;
            uint32_t ms = (uint32_t)((LS_BOOT + t + s_stall_t) / 10000);
            if ((int32_t)(ms - *s_tick) > 0) *s_tick = ms;
        }
        last = now;
        if (t) { LARGE_INTEGER d; d.QuadPart = -10000; if (SetWaitableTimer(t, &d, 0, NULL, NULL, FALSE)) { WaitForSingleObject(t, 5); continue; } }
        Sleep(1);
    }
    return 0;
}
void nf_lockstep_tick_start(volatile uint32_t *tick)
{
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); s_qpf = f.QuadPart ? f.QuadPart : 1;
    s_tick = tick; s_frame_real = qpc();
    *s_tick = (uint32_t)(LS_BOOT / 10000);
    HANDLE h = CreateThread(NULL, 0, tick_thread, NULL, 0, NULL);
    if (h) CloseHandle(h);
    fprintf(stderr, "[LOCKSTEP] KeTickCount counts game frames\n");
}

/* ---- online: ids, waiting window, end of the match ------------------------- */
static uint64_t mix64(uint64_t h, uint64_t v) { h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2); h *= 0xFF51AFD7ED558CCDull; return h ^ (h >> 31); }
static uint64_t hash_bytes(uint64_t h, const uint8_t *p, size_t n)
{
    for (; n >= 8; p += 8, n -= 8) { uint64_t v; memcpy(&v, p, 8); h = mix64(h, v); }
    while (n--) h = mix64(h, *p++);
    return h;
}
/* A whole file's hash, or 0 if it cannot be read. */
static uint64_t file_hash(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    static uint8_t buf[1 << 20];
    uint64_t h = 0x84222325CBF29CE4ull; size_t n, total = 0;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) { h = hash_bytes(h, buf, n); total += n; }
    fclose(f);
    return mix64(h, total) | 1;
}

/* Switches that only say where files go, or how this PC shows and controls the
 * game, are left out of the settings id; every other NIGHTFIRE_ / NF_ / LEAN_ /
 * RECOMP_ switch has to be the same on every PC. */
static int local_only(const char *name, const char *value)
{
    static const char *const names[] = { "NF_INPUT_RECORD", "NF_INPUT_REPLAY", "NF_STATE_HASH", "NF_KEYBOARD_PLAYER",
        "NIGHTFIRE_EXECUTABLE", "NIGHTFIRE_SESSION_STAMP", "NIGHTFIRE_SAVE_DIR", "LEAN_PRESENT_VSYNC", "RECOMP_WATCHDOG_SECS",
        "NIGHTFIRE_HANG_SECS", "NIGHTFIRE_HANG_ARM", "LEAN_HANG_DUMP", "NF_ONLINE_SEAT", "NF_WINDOW_LABEL", "NF_OWN_VIEW", "LEAN_INTERP", NULL };
    static const char *const ends[] = { "_DIR", "_FILE", "_PATH", "_LOG", NULL };
    if (!_strnicmp(name, "NF_NET", 6)) return 1;
    for (int i = 0; names[i]; i++) if (!_stricmp(name, names[i])) return 1;
    size_t n = strlen(name);
    for (int i = 0; ends[i]; i++) { size_t e = strlen(ends[i]); if (n > e && !_stricmp(name + n - e, ends[i])) return 1; }
    return strstr(value, ":\\") || strstr(value, ":/") || !strncmp(value, "\\\\", 2);
}
static int cmp_str(const void *a, const void *b) { return _stricmp(*(const char *const *)a, *(const char *const *)b); }
int nf_overlay_widescreen(void);   /* lean_overlay.inc; kernel_xbox.c gives a default */
static uint64_t settings_id(void)
{
    static const char *const prefixes[] = { "NIGHTFIRE_", "NF_", "LEAN_", "RECOMP_", NULL };
    char *list[512]; int n = 0;
    for (char **ep = _environ; ep && *ep && n < 512; ep++) {
        char *e = *ep;
        if (*e == '=') continue;
        char *eq = strchr(e, '='); if (!eq) continue;
        int want = 0;
        for (int i = 0; prefixes[i]; i++) if (!_strnicmp(e, prefixes[i], strlen(prefixes[i]))) want = 1;
        if (!want) continue;
        char name[128]; size_t ln = (size_t)(eq - e) < sizeof name - 1 ? (size_t)(eq - e) : sizeof name - 1;
        memcpy(name, e, ln); name[ln] = 0;
        if (!local_only(name, eq + 1)) list[n++] = e;
    }
    qsort(list, n, sizeof list[0], cmp_str);
    uint64_t h = 0x5E77196500000001ull;
    for (int i = 0; i < n; i++) {
        char up[1024]; size_t k = 0; int in_name = 1;   /* names are case-blind on Windows */
        for (const char *c = list[i]; *c && k < sizeof up - 1; c++) {
            if (*c == '=') in_name = 0;
            up[k++] = in_name && *c >= 'a' && *c <= 'z' ? (char)(*c - 32) : *c;
        }
        up[k] = 0;
        h = hash_bytes(h, (const uint8_t *)up, k + 1);
        fprintf(stderr, "[NETPLAY]   switch %s\n", up);
    }
    /* The F10 overlay's Widescreen choice reaches the game (kernel_xbox.c XC_VIDEO). */
    const char *ov = getenv("NF_OVERLAY"), *vf = getenv("NF_VIDEO_FLAGS");
    int ws = ov && ov[0] == '1' && !(vf && *vf) ? nf_overlay_widescreen() : 0;
    h = mix64(h, (uint64_t)ws);
    fprintf(stderr, "[NETPLAY] settings id %016llx from %d switches, widescreen %s\n", (unsigned long long)h, n, ws ? "on" : "off");
    return h;
}

/* A small window while this PC joins (and later whenever the others keep this PC
 * waiting for more than 2 s), so the black game window is not a mystery. */
static volatile LONG s_joining, s_cancel;
static volatile LONG64 s_wait_since;
static HWND s_wait_wnd, s_wait_label, s_wait_button;
static int s_wait_shown;
static LRESULT CALLBACK wait_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if ((m == WM_COMMAND && LOWORD(w) == IDCANCEL) || m == WM_CLOSE) {
        if (!s_cancel) fprintf(stderr, "[NETPLAY] %s\n", s_joining ? "cancelled while waiting for the match to start" : "left the match while waiting for the others");
        InterlockedExchange(&s_cancel, 1);
        nf_net_leave();
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}
static void wait_text(char *t, size_t n)
{
    const char *role = getenv("NF_NET"), *addr = getenv("NF_NET_ADDR"), *need = getenv("NF_NET_PLAYERS");
    if (s_joining) {
        if (role && !_stricmp(role, "host"))
            snprintf(t, n, "Hosting an online match.\n%d of %s PCs are here. The match starts when everyone has joined.\n\nOthers join with this PC's address (UDP port %s).",
                     nf_net_peers() ? nf_net_peers() : 1, need && *need ? need : "2", getenv("NF_NET_PORT") ? getenv("NF_NET_PORT") : "47610");
        else
            snprintf(t, n, nf_net_joined() ? "Joined the match at %s.\nWaiting for the host to start it (when everyone is in)."
                                           : "Joining the match at %s...", addr ? addr : "?");
        return;
    }
    const char *st = nf_net_status();
    snprintf(t, n, "%s\n\nThe match carries on when they are back. A PC that stays silent for 20 s is dropped.", *st ? st : "Waiting for the other players...");
}
static DWORD WINAPI wait_thread(LPVOID p)
{
    (void)p;
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wait_proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "NightfireOnlineWait";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassA(&wc);
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN), ww = 460, wh = 190;
    s_wait_wnd = CreateWindowExA(WS_EX_TOPMOST, wc.lpszClassName, "Nightfire Online", WS_CAPTION | WS_SYSMENU,
                                 (sw - ww) / 2, (sh - wh) / 3, ww, wh, NULL, NULL, wc.hInstance, NULL);
    if (!s_wait_wnd) return 0;
    s_wait_label = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 14, 12, ww - 40, 100, s_wait_wnd, NULL, wc.hInstance, NULL);
    s_wait_button = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, ww - 140, wh - 80, 110, 28, s_wait_wnd, (HMENU)IDCANCEL, wc.hInstance, NULL);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessageA(s_wait_label, WM_SETFONT, (WPARAM)font, TRUE); SendMessageA(s_wait_button, WM_SETFONT, (WPARAM)font, TRUE);
    char last[400] = "";
    for (;;) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        LONG64 since = s_wait_since;
        int show = s_joining || (s_net_wait && since && (qpc() - since) * 1000 / s_qpf > 2000);
        if (show) {
            char t[400]; wait_text(t, sizeof t);
            if (strcmp(t, last)) { strcpy(last, t); SetWindowTextA(s_wait_label, t); }
            if (!s_wait_shown) {
                SetWindowTextA(s_wait_button, s_joining ? "Cancel" : "Leave match");
                ShowWindow(s_wait_wnd, SW_SHOWNORMAL); s_wait_shown = 1;
            }
        } else if (s_wait_shown) { ShowWindow(s_wait_wnd, SW_HIDE); s_wait_shown = 0; }
        if (!s_online && !s_joining) break;
        if (s_cancel) nf_net_leave();      /* until the network side has started too */
        MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);
    }
    DestroyWindow(s_wait_wnd);
    return 0;
}

static DWORD WINAPI ended_box(LPVOID p)
{
    MessageBoxA(NULL, (const char *)p, "Nightfire Online", MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);
    return 0;
}
static void net_ended(void)
{
    static char text[600];
    const char *st = nf_net_status();
    fprintf(stderr, "[NETPLAY] match ended: %s This PC's players carry on here; the others stand still.\n", *st ? st : "The connection closed.");
    snprintf(text, sizeof text, "The online match ended:\n%s\n\nThe game keeps running here with only this PC's players. "
             "Close the game window to quit. The logs are in this game's online folder.", *st ? st : "the connection closed.");
    HANDLE h = CreateThread(NULL, 0, ended_box, text, 0, NULL);
    if (h) CloseHandle(h);
}
static void net_exit(void) { if (s_online) { fprintf(stderr, "[NETPLAY] leaving: telling the others\n"); fflush(stderr); } nf_net_shutdown(); }

/* At boot, on the game thread: join the match. Exits the game if it cannot. */
static void net_start(void)
{
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); if (!s_qpf) s_qpf = f.QuadPart ? f.QuadPart : 1;
    char exe[MAX_PATH]; DWORD ln = GetModuleFileNameA(NULL, exe, sizeof exe);
    uint64_t build = ln && ln < sizeof exe ? file_hash(exe) : 0, game = file_hash("game_files\\default.xbe");
    fprintf(stderr, "[NETPLAY] build id %016llx (this exe), game id %016llx (game_files\\default.xbe)\n", (unsigned long long)build, (unsigned long long)game);
    nf_net_set_ids(build, game, settings_id());
    /* NF_NET=lobby: the lobby window is the joining window; this one is only for waits in the match */
    const char *role = getenv("NF_NET");
    int lobby = role && !_stricmp(role, "lobby");
    if (!lobby) InterlockedExchange(&s_joining, 1);
    InterlockedExchange(&s_net_wait, 1);
    HANDLE w = lobby ? NULL : CreateThread(NULL, 0, wait_thread, NULL, 0, NULL);
    int r = nf_net_init();
    s_frame_real = qpc();
    InterlockedExchange(&s_net_wait, 0);
    if (r == 1) {
        s_online = 1;
        unsigned first = 0, count = 0; nf_net_local_ports(&first, &count);
        fprintf(stderr, "[NETPLAY] match started: %d PCs; this PC's players are players %u-%u; input delay %u frames\n",
                nf_net_peers(), first + 1, first + count, nf_net_delay());
        atexit(net_exit);
        const char *lb = getenv("NF_WINDOW_LABEL");
        if (lobby && !(lb && *lb)) {       /* the game window's title says which players this PC has */
            char t[64];
            if (count > 1) snprintf(t, sizeof t, "online, players %u-%u%s", first + 1, first + count, first == 0 ? " (host)" : "");
            else snprintf(t, sizeof t, "online, player %u%s", first + 1, first == 0 ? " (host)" : "");
            _putenv_s("NF_WINDOW_LABEL", t);
        }
        if (lobby) w = CreateThread(NULL, 0, wait_thread, NULL, 0, NULL);
    }
    InterlockedExchange(&s_joining, 0);
    if (w) { if (r != 1) WaitForSingleObject(w, 2000); CloseHandle(w); }
    if (r == 1) return;
    const char *st = nf_net_status();
    if (nf_net_cancelled()) fprintf(stderr, "[NETPLAY] the online lobby was closed, so the game closes\n");
    else fprintf(stderr, "[NETPLAY] could not start: %s\n", *st ? st : "see the [NF-NET] lines");
    fflush(stderr);
    if (!s_cancel && !nf_net_cancelled()) {
        char text[600];
        snprintf(text, sizeof text, "Online play could not start:\n%s\n\nThe game will close. Details are in the log.",
                 *st ? st : "the network could not be opened (see the log).");
        MessageBoxA(NULL, text, "Nightfire Online", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
    }
    nf_net_shutdown();
    ExitProcess(s_cancel || nf_net_cancelled() ? 0 : 3);
}

/* ---- input record / replay ------------------------------------------------ */
typedef struct { uint32_t frame; uint8_t port, pad; uint8_t state[18]; } ls_pad;   /* 24 bytes */
static FILE *s_rec, *s_play;
static uint32_t s_input_frame;
static uint32_t s_latched_frame[4];
static uint8_t s_latched[4][18];
static int s_replay_ok = 1;
static volatile LONG s_hash_stop;      /* the replay ran out of presses: hashes end where the recording did */
static uint8_t s_phys[4][NF_NET_PAD];  /* online: this PC's own presses, latest read of each port */
static uint8_t s_agreed[NF_NET_PORTS][NF_NET_PAD];
static uint32_t s_net_frame = ~0u;     /* input frame the agreed states are for */
static int s_net_over;

static FILE *s_hash;
static uint8_t s_readable[0x10000];      /* per 64 KB block: 0 unknown, 1 readable, 2 not */
static int s_sync;                       /* online: compare memory hashes */
static uint8_t s_sync_skip[0x10000];     /* NF_SYNC_SKIP blocks */
static uint64_t s_sync_h;

static void parse_skip(const char *v)
{
    unsigned n = 0;
    while (v && *v) {
        char *e; unsigned long a = strtoul(v, &e, 16), b = a;
        if (e == v) break;
        if (*e == '-') { const char *q = e + 1; b = strtoul(q, &e, 16); if (e == q) b = a; }
        for (unsigned long i = a; i <= b && i < 0x10000; i++) { s_sync_skip[i] = 1; n++; }
        v = e; while (*v == ',' || *v == ' ' || *v == ';') v++;
    }
    if (n) fprintf(stderr, "[NETPLAY] sync check leaves out %u memory blocks (NF_SYNC_SKIP)\n", n);
}

void nf_lockstep_arm(void)
{
    if (!nf_lockstep_on()) return;
    s_main_tid = GetCurrentThreadId();
    static int armed;
    if (armed++) return;
    const char *r = getenv("NF_INPUT_RECORD"), *p = getenv("NF_INPUT_REPLAY"), *h = getenv("NF_STATE_HASH");
    if (net_wanted()) {
        net_start();       /* returns only with the match running */
        const char *sy = getenv("NF_SYNC");
        s_sync = !(sy && sy[0] == '0');
        if (s_sync) parse_skip(getenv("NF_SYNC_SKIP"));
        fprintf(stderr, "[NETPLAY] sync check %s\n", s_sync ? "on: memory hashes compared every 60 frames" : "off (NF_SYNC=0)");
        if (p && *p) { fprintf(stderr, "[LOCKSTEP] NF_INPUT_REPLAY ignored online\n"); p = NULL; }
    }
    if (p && *p) {
        s_play = fopen(p, "rb");
        char head[8];
        if (!s_play || fread(head, 1, 8, s_play) != 8 || memcmp(head, "NFINPUT1", 8)) {
            fprintf(stderr, "[LOCKSTEP] cannot replay %s (missing or not a recording)\n", p);
            if (s_play) fclose(s_play);
            s_play = NULL;
        } else fprintf(stderr, "[LOCKSTEP] replaying presses from %s\n", p);
    } else if (r && *r) {
        s_rec = fopen(r, "wb");
        if (s_rec) { fwrite("NFINPUT1", 1, 8, s_rec); fprintf(stderr, "[LOCKSTEP] recording presses to %s\n", r); }
        else fprintf(stderr, "[LOCKSTEP] cannot write %s\n", r);
    }
    if (h && *h) {
        s_hash = fopen(h, "wb");
        if (s_hash) { fwrite("NFHASH01", 1, 8, s_hash); fprintf(stderr, "[LOCKSTEP] state hashes to %s\n", h); }
    }
    for (int i = 0; i < 4; i++) s_latched_frame[i] = ~0u;
}

void nf_lockstep_input_frame(void) { if (s_on > 0) s_input_frame++; }

/* Online: once per input frame, send this PC's presses and get every port's
 * agreed state (waits while another PC is behind). */
static void net_frame(void)
{
    if (s_net_frame == s_input_frame) return;
    s_net_frame = s_input_frame;
    uint8_t local[NF_NET_PORTS][NF_NET_PAD];
    unsigned first = 0, count = 0; nf_net_local_ports(&first, &count);
    for (unsigned k = 0; k < count && k < NF_NET_PORTS; k++) memcpy(local[k], s_phys[k], NF_NET_PAD);
    s_wait_since = qpc();
    InterlockedExchange(&s_net_wait, 1);
    int ok = nf_net_frame(s_input_frame, (const uint8_t (*)[NF_NET_PAD])local, s_agreed);
    s_frame_real = qpc();
    InterlockedExchange(&s_net_wait, 0);
    s_wait_since = 0;
    if (!ok && !s_net_over) {
        s_net_over = 1;
        fprintf(stderr, "[NETPLAY] input frame %u, frame %lld:\n", s_input_frame, (long long)s_frames);
        net_ended();
        s_online = 0;
    }
}

/* Called with each port's final state, just before the game gets it. */
void nf_lockstep_pad(unsigned port, unsigned char state[18])
{
    if (s_on <= 0 || port >= 4) return;
    if (s_latched_frame[port] == s_input_frame) { memcpy(state, s_latched[port], 18); return; }
    if (s_online || s_net_over) {
        memcpy(s_phys[port], state, 18);
        if (s_online) net_frame();
        /* after the match ended nf_net_frame keeps this PC's players live and the others still */
        else if (s_net_frame != s_input_frame) {
            s_net_frame = s_input_frame;
            uint8_t local[NF_NET_PORTS][NF_NET_PAD];
            for (unsigned k = 0; k < NF_NET_PORTS; k++) memcpy(local[k], s_phys[k], NF_NET_PAD);
            nf_net_frame(s_input_frame, (const uint8_t (*)[NF_NET_PAD])local, s_agreed);
        }
        memcpy(state, s_agreed[port], 18);
    }
    if (s_play && s_replay_ok) {
        ls_pad e;
        if (fread(&e, sizeof e, 1, s_play) != 1) {
            fprintf(stderr, "[LOCKSTEP] replay finished at input frame %u; live input from here, state hashes stop\n", s_input_frame);
            s_replay_ok = 0;
            InterlockedExchange(&s_hash_stop, 1);
        } else if (e.frame != s_input_frame || e.port != port) {
            fprintf(stderr, "[LOCKSTEP] REPLAY DIVERGED at input frame %u port %u (recording has frame %u port %u); live input from here\n",
                    s_input_frame, port, e.frame, e.port);
            s_replay_ok = 0;
        } else memcpy(state, e.state, 18);
    } else if (s_rec) {
        ls_pad e; memset(&e, 0, sizeof e);
        e.frame = s_input_frame; e.port = (uint8_t)port; memcpy(e.state, state, 18);
        fwrite(&e, sizeof e, 1, s_rec);
        if ((s_input_frame & 63) == 0) fflush(s_rec);
    }
    s_latched_frame[port] = s_input_frame;
    memcpy(s_latched[port], state, 18);
}

/* ---- state hashes ---------------------------------------------------------- */
static uint64_t hash_block(const uint64_t *p, size_t n)
{
    uint64_t h = 0x9E3779B97F4A7C15ull;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 0xFF51AFD7ED558CCDull; h ^= h >> 29; }
    return h;
}
static void hash_some(uint32_t frame)
{
    size_t size = xbox_GetMappedSize();
    uint32_t blocks = (uint32_t)(size / LS_BLOCK);
    if (blocks > sizeof s_readable) blocks = sizeof s_readable;
    int file = s_hash && !s_hash_stop, sync = s_sync && s_online;
    if (file) fwrite(&frame, 4, 1, s_hash);
    for (uint32_t b = frame % LS_HASH_SPREAD; b < blocks; b += LS_HASH_SPREAD) {
        const uint8_t *p = (const uint8_t *)(g_xbox_mem_offset + (uintptr_t)b * LS_BLOCK);
        if (!s_readable[b]) {
            MEMORY_BASIC_INFORMATION mi;
            int ok = VirtualQuery(p, &mi, sizeof mi) == sizeof mi && mi.State == MEM_COMMIT &&
                     (uintptr_t)mi.BaseAddress + mi.RegionSize >= (uintptr_t)p + LS_BLOCK &&
                     (mi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) &&
                     !(mi.Protect & PAGE_GUARD);
            s_readable[b] = ok ? 1 : 2;
        }
        uint64_t h = s_readable[b] == 1 ? hash_block((const uint64_t *)p, LS_BLOCK / 8) : 0;
        if (file) { uint16_t id = (uint16_t)b; fwrite(&id, 2, 1, s_hash); fwrite(&h, 8, 1, s_hash); }
        if (sync && !s_sync_skip[b]) s_sync_h = mix64(s_sync_h, h ^ ((uint64_t)b << 48));
    }
    if (file) {
        uint16_t end = 0xFFFF; fwrite(&end, 2, 1, s_hash);
        if ((frame & 63) == 0) fflush(s_hash);
    }
    /* every block once per 60 frames: one value for the others to compare */
    if (sync && frame % LS_HASH_SPREAD == LS_HASH_SPREAD - 1) { nf_net_hash(frame, s_sync_h); s_sync_h = 0; }
}

/* The frame routine 0xDD1D0, before (after=0) and after (after=1), game thread. */
void nf_lockstep_frame(unsigned after)
{
    if (s_on <= 0 || after > 1) return;
    if (!after) {
        if (s_depth++) return;
        /* Online: a PC running ahead of the others (e.g. after it loaded faster) would wait
         * for them a little every frame. nf_net_pace_us says by how much it should slow
         * down. The 60 Hz pacer (nightfire_pacer93.h) makes up any delay shorter than one
         * frame, so the slowdown is saved up to one frame and taken as one frame started
         * two frames late (36 ms), which moves its timeline back by about one frame. */
        if (s_online) {
            s_pace_us += nf_net_pace_us();
            if (s_pace_us >= 16667) {
                s_pace_us = 0;
                if (++s_held_back <= 20 || s_held_back % 100 == 0)
                    fprintf(stderr, "[NETPLAY] running ahead of the others: one frame held back at frame %lld (%u so far)\n", (long long)s_frames + 1, s_held_back);
                Sleep(36);
            }
        }
        LONG64 now = qpc(), real_ms = s_qpf ? (now - s_frame_real) * 1000 / s_qpf : 0;
        if (real_ms > 1000)
            fprintf(stderr, "[LOCKSTEP] %.1f s without a frame before frame %lld: game clock moved %.3f s (%u clock reads, %u sound polls, %u pictures, %u sleeps = %.3f s)%s\n",
                    real_ms / 1000.0, (long long)s_frames + 1, (s_main_t - s_frame_t) / (double)LS_UNIT, s_fl.reads, s_fl.polls, s_fl.presents, s_fl.sleeps,
                    s_fl.slept / (double)LS_UNIT, s_stalled ? ", plus real time (stall)" : "");
        memset(&s_fl, 0, sizeof s_fl);
        s_frames++;
        /* 1/60 s per frame; a frameless stretch that moved the clock further moves the timeline on */
        LONG64 base = s_frame_off + s_frames * LS_UNIT / 60;
        if (base < s_main_t) { s_frame_off += s_main_t - base; base = s_main_t; }
        s_frame_t = base; s_main_t = base; s_frame_real = now; s_reads = 0;
        if (s_frame_reads > s_max_frame_reads) s_max_frame_reads = s_frame_reads;
        s_frame_reads = 0;
        if (s_tick) *s_tick = (uint32_t)((LS_BOOT + s_frame_t + s_stall_t) / 10000);
        if (s_stalled) {
            InterlockedExchange(&s_stalled, 0);
            fprintf(stderr, "[LOCKSTEP] frame %lld resumed after a stall; clocks moved %.3f s with real time so far (stalls %ld)\n",
                    (long long)s_frames, s_stall_t / (double)LS_UNIT, (long)s_stalls);
        }
        if (s_audio_hook) s_audio_hook();
        return;
    }
    if (!s_depth || --s_depth) return;
    if ((s_hash && !s_hash_stop) || (s_sync && s_online)) hash_some((uint32_t)s_frames);
    if (s_frames % 3600 == 0)
        fprintf(stderr, "[LOCKSTEP] frame %lld, input frame %u, stalls %ld (%.3f s), most clock reads in one frame %u%s\n",
                (long long)s_frames, s_input_frame, (long)s_stalls, s_stall_t / (double)LS_UNIT, s_max_frame_reads, s_online ? ", online" : "");
    if (s_online && s_frames % 3600 == 0)
        fprintf(stderr, "[NETPLAY] frame %lld: input delay %u frames, held back %u times%s%s\n", (long long)s_frames, nf_net_delay(), s_held_back,
                *nf_net_status() ? "; " : "", nf_net_status());
}
