/* Diagnostic: execution counts for the generated-code branch sites whose
 * condition reads the never-assigned `_flags` fallback (the lifter emits it for
 * a conditional jump at a label reached from more than one path, so the jump
 * is never taken). Sites are numbered in analysis-flags-sites.json.
 * LEAN_FLAGS_LOG=1 prints each site's first execution and a 10 s summary. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define LEAN_FLAGS_MAX 512
static volatile LONG counts[LEAN_FLAGS_MAX];
static int on = -1;
static ULONGLONG t0, last;

int lean_flags_hit(int id)
{
    if (on < 0) {
        const char *v = getenv("LEAN_FLAGS_LOG");
        on = v && v[0] == '1';
        t0 = last = GetTickCount64();
    }
    if (!on || id < 0 || id >= LEAN_FLAGS_MAX) return 0;
    if (InterlockedIncrement(&counts[id]) == 1)
        fprintf(stderr, "[LEAN-FLAGS] first site=%d t=%llu ms\n", id, GetTickCount64() - t0);
    ULONGLONG now = GetTickCount64();
    if (now - last >= 10000) {
        char line[4096]; int n = 0;
        last = now;
        for (int i = 0; i < LEAN_FLAGS_MAX && n < (int)sizeof(line) - 32; i++)
            if (counts[i]) n += snprintf(line + n, sizeof(line) - n, " %d:%ld", i, counts[i]);
        line[n] = 0;
        fprintf(stderr, "[LEAN-FLAGS] t=%llu ms counts:%s\n", now - t0, line);
    }
    return 0;
}
