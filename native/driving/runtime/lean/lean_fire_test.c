/* Test-only scripted rifle fire for unattended Paris runs (ported from the
 * rifle copy's lean_fire_test.c). Off unless LEAN_FIRE_TEST is set; the real
 * keyboard/mouse sample is kept and only the fire trigger (out[9]) is
 * overridden while a window is active.
 *
 * LEAN_FIRE_TEST=all          fire for the whole run
 * LEAN_FIRE_TEST=a-b[,c-d..]  fire inside these windows (ms since the first
 *                             pad poll)
 * Fire is 100 ms down / 400 ms up. Each press and each change of the
 * ray-shell (bullet) count at guest 0x1E84E4 is logged as [LEAN-FIRE]. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ptrdiff_t g_xbox_mem_offset;

#define FIRE_MAX_WIN 8
static int fire_init, fire_n, fire_all, fire_held;
static ULONGLONG fire_a[FIRE_MAX_WIN], fire_b[FIRE_MAX_WIN], fire_t0;
static unsigned fire_presses;
static uint32_t fire_shells = 0xFFFFFFFFu;

static void fire_parse(void)
{
    const char *v = getenv("LEAN_FIRE_TEST");
    fire_init = 1;
    fire_t0 = GetTickCount64();
    if (v && !strcmp(v, "all")) { fire_all = 1; fprintf(stderr, "[LEAN-FIRE] test fire for the whole run, 100 ms on / 400 ms off\n"); return; }
    while (v && *v && fire_n < FIRE_MAX_WIN) {
        char *e; unsigned long long a = strtoull(v, &e, 10), b;
        if (*e != '-') break;
        b = strtoull(e + 1, &e, 10);
        fire_a[fire_n] = a; fire_b[fire_n] = b; fire_n++;
        if (*e != ',') break;
        v = e + 1;
    }
    for (int i = 0; i < fire_n; i++)
        fprintf(stderr, "[LEAN-FIRE] window %d: %llu-%llu ms, 100 ms on / 400 ms off\n", i, fire_a[i], fire_b[i]);
}

/* Called from driving_input224.c with the real sample already in out. */
void lean_fire_test_sample(unsigned char out[18])
{
    if (!fire_init) fire_parse();
    {   /* LEAN_PAD_TEST=<A|B|X|Y|K|W|L|R>:a-b[,<btn>:a-b..] (test only): press that pad button
         * 100 ms in every 1000 ms between a and b ms after the first pad poll (K = black,
         * W = white, L/R = triggers; report bytes 2..9 = XINPUT analog[0..7]). Each press is
         * logged as [LEAN-PAD] with the game frame. */
        static int init, n; static int idx[6]; static char nm[6]; static ULONGLONG pa[6], pb[6], pon[6], pper[6]; static unsigned presses; static int held[6];
        if (!init) { const char *e = getenv("LEAN_PAD_TEST"); init = 1;
            while (e && e[0] && e[1] == ':' && n < 6) { const char *m = strchr("ABXYKWLR^v<>", e[0]); unsigned long long a, b;
                if (!m || sscanf(e + 2, "%llu-%llu", &a, &b) != 2) break;
                pon[n] = 100; pper[n] = 1000;   /* optional @on/period in ms, e.g. X:150000-170000@1500/3000 */
                { const char *at = strchr(e + 2, '@'), *cm = strchr(e + 2, ','); if (at && (!cm || at < cm)) sscanf(at + 1, "%llu/%llu", &pon[n], &pper[n]); }
                { int k = (int)(m - "ABXYKWLR^v<>"); idx[n] = k < 8 ? 2 + k : -(1 << (k - 8)); }   /* ^ v < > = D-pad bits in byte 0 */ nm[n] = e[0]; pa[n] = a; pb[n] = b;
                fprintf(stderr, "[LEAN-PAD] test presses of %c (byte %d) %llu-%llu ms, %llu ms every %llu ms\n", e[0], idx[n], a, b, pon[n], pper[n]); n++;
                e = strchr(e, ','); if (e) e++; } }
        for (int i = 0; i < n; i++) {
            extern volatile LONG lean_present_count;
            ULONGLONG t = GetTickCount64() - fire_t0;
            int h = t >= pa[i] && t < pb[i] && ((t - pa[i]) % pper[i]) < pon[i];
            if (h) { if (idx[i] >= 0) out[idx[i]] = 255; else out[0] |= (unsigned char)(-idx[i]); }
            if (h && !held[i]) fprintf(stderr, "[LEAN-PAD] t=%llu f=%ld press %u %c\n", t, lean_present_count, ++presses, nm[i]);
            held[i] = h;
        }
    }
    if (!fire_all && !fire_n) return;
    ULONGLONG now = GetTickCount64() - fire_t0;
    int in = fire_all, held = 0;
    if (fire_all) held = (now % 500) < 100;
    for (int i = 0; i < fire_n; i++)
        if (now >= fire_a[i] && now < fire_b[i]) { in = 1; held = ((now - fire_a[i]) % 500) < 100; }
    if (in) out[9] = held ? 255 : 0;
    if (held != fire_held) {
        fire_held = held;
        if (held && ++fire_presses <= 2000 && (fire_presses <= 5 || !(fire_presses % 20)))
            fprintf(stderr, "[LEAN-FIRE] t=%llu press %u\n", now, fire_presses);
    }
    {   /* bullets in flight: a change means the game accepted a shot */
        uint32_t n = *(volatile uint32_t *)(g_xbox_mem_offset + 0x1E84E4u);
        static unsigned logged;
        if (n != fire_shells) {
            if (fire_shells != 0xFFFFFFFFu && n > fire_shells && logged++ < 2000)
                fprintf(stderr, "[LEAN-FIRE] t=%llu shells %u -> %u (press %u)\n", now, fire_shells, n, fire_presses);
            fire_shells = n;
        }
    }
}
