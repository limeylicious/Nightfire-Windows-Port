/* NIGHTFIRE_FAST_CALLS=1 (native copy; off by default): the per-call checkpoint
 * (nightfire_thread_check, run before and after every translated call) only for the
 * calls something actually watches.
 *
 * The checkpoint costs about a quarter of the Action game thread (runs/profile3), yet
 * nearly all of its work is keyed to a few dozen routines. recomp_types.h (patched by
 * scripts/nf_fast_calls_hook.py) now asks NF_CALL_HOT(va) first: a bit per routine
 * address below 0x200000. With the switch off every bit is set and nothing changes.
 * With it on, only the routines listed below reach the checkpoint; for every other
 * call the per-call call-boundary history and thread-context comparison are skipped
 * (the session crash capture still records crashes), and the window-close poll runs at
 * the hooked present (0x103730) instead of at every call.
 *
 * The switch is ignored, so everything stays watched, when a setting needs every call:
 * RECOMP_PB_EXEC (push-buffer drain per call), NIGHTFIRE_RENDER_RETURN_WATCH,
 * NIGHTFIRE_VIDEO_WATCH, NIGHTFIRE_CALL_SURVEY. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char nf_hot_map[0x40000];   /* one bit per VA below 0x200000 */
int nf_hot_far = 1;                  /* VAs at or above 0x200000 (kernel thunks) */

static void mark(uint32_t va) { if (va < 0x200000u) nf_hot_map[va >> 3] |= (unsigned char)(1u << (va & 7)); }
static void mark_range(uint32_t lo, uint32_t hi) { for (uint32_t va = lo; va <= hi; va++) mark(va); }

/* NIGHTFIRE_GAME_COUNTERS=1 (diagnostic): every 5 s, the game's own update and render
 * counters per second (PAL 0x1F65B4 updates, 0x1F65B0 renders; rate 0x17C0F4, step
 * 0x17C104, as nightfire_game_rate94.h reads them), to check game speed against wall time. */
#include <windows.h>
extern ptrdiff_t g_xbox_mem_offset;
#define GM32(a) (*(volatile uint32_t *)(g_xbox_mem_offset + (uintptr_t)(a)))
static DWORD WINAPI counters(LPVOID p)
{
    (void)p;
    uint32_t u0 = 0, r0 = 0; ULONGLONG t0 = 0;
    for (;;) {
        Sleep(5000);
        if (!g_xbox_mem_offset) continue;
        uint32_t u = GM32(0x1f65b4), r = GM32(0x1f65b0); ULONGLONG t = GetTickCount64();
        if (t0) {
            double s = (t - t0) / 1000.0; uint32_t step = GM32(0x17c104); float f; memcpy(&f, &step, 4);
            fprintf(stderr, "[GAME-COUNTERS] updates/s %.1f renders/s %.1f rate %u step %.4f s\n",
                    (u - u0) / s, (r - r0) / s, GM32(0x17c0f4), f);
        }
        u0 = u; r0 = r; t0 = t;
    }
}

void nightfire_fast_calls_init(void)
{
    {   const char *c = getenv("NIGHTFIRE_GAME_COUNTERS");
        if (c && c[0] == '1') { HANDLE h = CreateThread(NULL, 0, counters, NULL, 0, NULL); if (h) CloseHandle(h); } }
    const char *e = getenv("NIGHTFIRE_FAST_CALLS");
    const char *needs_all = getenv("RECOMP_PB_EXEC") ? "RECOMP_PB_EXEC" :
                            getenv("NIGHTFIRE_RENDER_RETURN_WATCH") ? "NIGHTFIRE_RENDER_RETURN_WATCH" :
                            getenv("NIGHTFIRE_VIDEO_WATCH") ? "NIGHTFIRE_VIDEO_WATCH" :
                            getenv("NIGHTFIRE_CALL_SURVEY") ? "NIGHTFIRE_CALL_SURVEY" : NULL;
    if (!e || e[0] != '1' || needs_all) {
        memset(nf_hot_map, 0xFF, sizeof nf_hot_map); nf_hot_far = 1;
        if (e && e[0] == '1') fprintf(stderr, "[FAST-CALLS] off: %s needs every call\n", needs_all);
        return;
    }
    memset(nf_hot_map, 0, sizeof nf_hot_map); nf_hot_far = 0;
    static const uint32_t hot[] = {
        /* bot trace 231 */ 0x4e180, 0x1b6b0, 0x3d670, 0xa12b0, 0x9d700, 0x1b120, 0x1cb20,
        /* input frame, mouse 122/127, dev damage 134, game pacing 93/94 */ 0x6cf50, 0xb7a50, 0xd5030, 0xac0a0, 0xdd1d0, 0x6b040,
        /* input test output */ 0xe76a0, 0xde5c0,
        /* menu, frontend and game-flow logs */ 0x95a80, 0x72e20, 0x92c40, 0x6abf0, 0x6abc0,
        /* profile boundaries, present (GPU present + window-close poll) */
        0x130624, 0x1328e1, 0x130d2f, 0x103730, 0x1065a0, 0x106650, 0x1035e0, 0x1036f0, 0x1064b0, 0xdffb0, 0x85240,
        /* video mapping */ 0x130eaf,
        /* game music ring producer (old audio layer) */ 0xe0dd0,
        /* thread-pointer validations (after == 2) */ 0xeb0b4, 0xeb238, 0xec8c5, 0xed07c, 0xedf7b, 0x105ac0,
    };
    for (size_t i = 0; i < sizeof hot / sizeof hot[0]; i++) mark(hot[i]);
    mark_range(0x85000, 0x85fff);       /* frontend handlers */
    mark_range(0x15800f, 0x1581c6);     /* input calls */
    mark_range(0x112600, 0x11c801);     /* DirectSound: old audio layers, movie audio, audio set-up checks */
    unsigned n = 0;
    for (size_t i = 0; i < sizeof nf_hot_map; i++) { unsigned b = nf_hot_map[i]; while (b) { n += b & 1; b >>= 1; } }
    fprintf(stderr, "[FAST-CALLS] on: checkpoint only for %u watched routine addresses\n", n);
}
