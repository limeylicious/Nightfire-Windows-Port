/* NIGHTFIRE_LOCKSTEP=1 (off by default): groundwork for online play, where every
 * PC runs the same match and only controller presses travel between them. That
 * only works if two copies given the same presses stay identical, and this file
 * is how we find out.
 *
 * 1. Game clock. Every clock the game can read (KeTickCount, KeQuerySystemTime,
 *    KeQueryPerformanceCounter/Frequency, KeQueryInterruptTime, rdtsc) counts
 *    game frames instead of real time: 1/60 s per pass of the frame routine
 *    0xDD1D0, plus 1 microsecond per clock read on the game thread inside a
 *    frame (so a loop waiting for time to pass still ends, the same way on every
 *    PC). Real-time pacing is untouched: the game still shows 60 frames a second.
 *    If the game thread makes no frame for 250 ms (loading, or a wait on
 *    KeTickCount, which the game reads straight from memory), clocks move with
 *    real time until the next frame; that is logged as a stall, since it is not
 *    the same on every PC.
 * 2. NF_INPUT_RECORD=<file> / NF_INPUT_REPLAY=<file>: each controller port's
 *    state, once per input frame (0x6CF50), is written to or read from <file>.
 *    Within one input frame every read of a port returns the same state. In
 *    lockstep the mouse moves the right stick (no direct mouse), so all input is
 *    in those states. Keep the mouse out of menus while recording (the PC menu
 *    cursor is moved directly, not through the controller).
 * 3. NF_STATE_HASH=<file>: a hash of every 64 KB block of game memory, one
 *    sixtieth of the blocks after each frame (so each block once a second, no
 *    hitch). scripts/nf_state_diff.py compares two such files and names the first
 *    frame and the blocks that differ.
 *
 * Test: play with NF_INPUT_RECORD + NF_STATE_HASH, then run again with
 * NF_INPUT_REPLAY + NF_STATE_HASH (another file) and compare the two hash files. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ptrdiff_t g_xbox_mem_offset;
size_t xbox_GetMappedSize(void);

#define LS_UNIT        10000000ll             /* clock unit: 100 ns */
#define LS_BOOT        (60ll * LS_UNIT)       /* virtual uptime at the first frame */
#define LS_FILETIME    126858528000000000ll   /* 2003-01-01 00:00 UTC */
#define LS_QPF         10000000ll
#define LS_STALL_MS    250
#define LS_BLOCK       0x10000u
#define LS_HASH_SPREAD 60u

static int s_on = -1;
static DWORD s_main_tid;
static LONG64 s_frames;                 /* frame-routine passes (game thread only) */
static unsigned s_depth;
static volatile LONG64 s_frame_t;       /* virtual time at this frame's start */
static volatile LONG64 s_main_t;        /* last time handed to the game thread */
static volatile LONG64 s_stall_t;       /* real time added during stalls */
static volatile LONG64 s_frame_real;    /* QPC at this frame's start */
static volatile LONG s_stalled, s_stalls;
static LONG64 s_qpf;

int nf_lockstep_on(void)
{
    if (s_on < 0) {
        const char *e = getenv("NIGHTFIRE_LOCKSTEP");
        s_on = e && e[0] == '1';
        if (s_on) fprintf(stderr, "[LOCKSTEP] on: game clocks count frames (1/60 s each); direct mouse off\n");
    }
    return s_on;
}

static LONG64 qpc(void) { LARGE_INTEGER c; QueryPerformanceCounter(&c); return c.QuadPart; }

/* Virtual time in 100 ns units since the virtual boot. */
static LONG64 ls_time(void)
{
    if (!s_main_tid || GetCurrentThreadId() == s_main_tid) {
        LONG64 t = s_main_t + 10;
        if (t < s_frame_t) t = s_frame_t;
        s_main_t = t;
        return LS_BOOT + t + s_stall_t;
    }
    return LS_BOOT + s_frame_t + s_stall_t;
}

uint64_t nf_lockstep_system_time(void) { return (uint64_t)(LS_FILETIME + ls_time()); }
uint64_t nf_lockstep_interrupt_time(void) { return (uint64_t)ls_time(); }
uint64_t nf_lockstep_qpc(void) { return (uint64_t)ls_time(); }
uint64_t nf_lockstep_qpf(void) { return (uint64_t)LS_QPF; }
/* The Xbox's rdtsc runs at 733.33 MHz = 220/3 counts per 100 ns. */
uint64_t nf_lockstep_tsc(void) { uint64_t t = (uint64_t)ls_time(); return t / 3u * 220u + t % 3u * 220u / 3u; }

/* KeTickCount (ms) is read straight from game memory. The game thread writes it at
 * each frame start (so every read in a frame sees the same value on every PC);
 * this thread only moves it while the game thread is stalled. */
static volatile uint32_t *s_tick;
static DWORD WINAPI tick_thread(LPVOID p)
{
    (void)p;
    HANDLE t = CreateWaitableTimerExW(NULL, NULL, 0x00000002 /* HIGH_RESOLUTION */, TIMER_ALL_ACCESS);
    LONG64 last = qpc();
    while (s_tick) {
        LONG64 now = qpc();
        if ((now - s_frame_real) * 1000 / s_qpf > LS_STALL_MS) {
            if (!s_stalled) { InterlockedExchange(&s_stalled, 1); InterlockedIncrement(&s_stalls); }
            InterlockedAdd64(&s_stall_t, (now - last) * LS_UNIT / s_qpf);
            *s_tick = (uint32_t)((LS_BOOT + s_frame_t + s_stall_t) / 10000);
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

/* ---- input record / replay ------------------------------------------------ */
typedef struct { uint32_t frame; uint8_t port, pad; uint8_t state[18]; } ls_pad;   /* 24 bytes */
static FILE *s_rec, *s_play;
static uint32_t s_input_frame;
static uint32_t s_latched_frame[4];
static uint8_t s_latched[4][18];
static int s_replay_ok = 1;
static volatile LONG s_hash_stop;      /* the replay ran out of presses: hashes end where the recording did */

static FILE *s_hash;
static uint8_t s_readable[0x10000];      /* per 64 KB block: 0 unknown, 1 readable, 2 not */

void nf_lockstep_arm(void)
{
    if (!nf_lockstep_on()) return;
    s_main_tid = GetCurrentThreadId();
    const char *r = getenv("NF_INPUT_RECORD"), *p = getenv("NF_INPUT_REPLAY"), *h = getenv("NF_STATE_HASH");
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

/* Called with each port's final state, just before the game gets it. */
void nf_lockstep_pad(unsigned port, unsigned char state[18])
{
    if (s_on <= 0 || port >= 4) return;
    if (s_latched_frame[port] == s_input_frame) { memcpy(state, s_latched[port], 18); return; }
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
    fwrite(&frame, 4, 1, s_hash);
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
        uint16_t id = (uint16_t)b;
        fwrite(&id, 2, 1, s_hash); fwrite(&h, 8, 1, s_hash);
    }
    uint16_t end = 0xFFFF; fwrite(&end, 2, 1, s_hash);
    if ((frame & 63) == 0) fflush(s_hash);
}

/* The frame routine 0xDD1D0, before (after=0) and after (after=1), game thread. */
void nf_lockstep_frame(unsigned after)
{
    if (s_on <= 0 || after > 1) return;
    if (!after) {
        if (s_depth++) return;
        s_frames++;
        LONG64 base = s_frames * LS_UNIT / 60;
        if (base < s_main_t) base = s_main_t;
        s_frame_t = base; s_main_t = base; s_frame_real = qpc();
        if (s_tick) *s_tick = (uint32_t)((LS_BOOT + s_frame_t + s_stall_t) / 10000);
        if (s_stalled) {
            InterlockedExchange(&s_stalled, 0);
            fprintf(stderr, "[LOCKSTEP] frame %lld resumed after a stall; clocks moved %.3f s with real time so far (stalls %ld)\n",
                    (long long)s_frames, s_stall_t / (double)LS_UNIT, (long)s_stalls);
        }
        return;
    }
    if (!s_depth || --s_depth) return;
    if (s_hash && !s_hash_stop) hash_some((uint32_t)s_frames);
    if (s_frames % 3600 == 0)
        fprintf(stderr, "[LOCKSTEP] frame %lld, input frame %u, stalls %ld (%.3f s)\n",
                (long long)s_frames, s_input_frame, (long)s_stalls, s_stall_t / (double)LS_UNIT);
}
