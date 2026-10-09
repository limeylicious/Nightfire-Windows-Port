/* Diagnostic: execution counts for the generated-code branch sites whose
 * condition reads the never-assigned `_flags` fallback (the lifter emits it for
 * a conditional jump at a label reached from more than one path, so the jump
 * is never taken). Sites are numbered in analysis-flags-sites.json.
 * LEAN_FLAGS_LOG=1 prints each site's first execution and a 10 s summary. */
#include <windows.h>
#include <intrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>

/* Coverage: one bit per guest code address, set the first time a function
 * is called (RECOMP_ABI_CALL in recomp_types.h). LEAN_COVERAGE=1 logs each
 * first call with a timestamp, so code that only runs on a player action
 * (e.g. firing) shows up as new lines at that moment. */
uint8_t g_lean_cov[0x80000];
/* LEAN_CALL_CHAIN=from,to,va[,va..] (diagnostic): host call chain and first stack
 * arguments for every call to those guest functions inside that game-frame range.
 * Their coverage bit is kept clear so each call reaches lean_cov_first. */
static int cc_init, cc_n;
static long cc_from, cc_to;
static uint32_t cc_va[8];
static void cc_parse(void)
{
    const char *e = getenv("LEAN_CALL_CHAIN");
    cc_init = 1;
    if (!e || sscanf(e, "%ld,%ld", &cc_from, &cc_to) != 2) return;
    for (const char *q = strchr(strchr(e, ',') + 1, ','); q && cc_n < 8; q = strchr(q + 1, ','))
        cc_va[cc_n++] = (uint32_t)strtoul(q + 1, NULL, 16);
}
/* LEAN_WATCH_GRANT=frames (diagnostic, with LEAN_GADGET_GRANT): for that many game frames after
 * a grant, every guest call comes here (coverage bits are cleared and not set) and a change of
 * the granted count is logged with the calls just before and at the change. Slow while active. */
volatile LONG lean_watch_on;
static uint32_t watch_addr, watch_last_va, watch_last_ecx;
static long watch_until;
static int32_t watch_val;
void lean_watch_start(uint32_t addr)
{
    extern volatile LONG lean_present_count;
    extern ptrdiff_t g_xbox_mem_offset;
    const char *e = getenv("LEAN_WATCH_GRANT");
    if (!e || atoi(e) <= 0 || lean_watch_on) return;
    watch_addr = addr; watch_until = lean_present_count + atoi(e);
    watch_val = *(const int32_t *)(g_xbox_mem_offset + addr);
    memset(g_lean_cov, 0, sizeof g_lean_cov);
    lean_watch_on = 1;
    fprintf(stderr, "[LEAN-WATCH] watching %08X (now %d) until f=%ld\n", addr, watch_val, watch_until);
}
void lean_cov_first(uint32_t va)
{
    static int cov_on = -1;
    static ULONGLONG cov_t0;
    if (lean_watch_on) {
        extern volatile LONG lean_present_count;
        extern ptrdiff_t g_xbox_mem_offset;
        extern __declspec(thread) uint32_t g_ecx, g_esp;
        static __declspec(thread) uint32_t t_last_va, t_last_ret;   /* this thread's previous call */
        uint32_t ret = *(const uint32_t *)(g_xbox_mem_offset + g_esp);   /* guest return address = call site + 5 */
        int32_t v = *(const int32_t *)(g_xbox_mem_offset + watch_addr);
        if (v != watch_val) {
            fprintf(stderr, "[LEAN-WATCH] f=%ld %08X %d -> %d after call sub_%08X (ecx %08X), seen at call sub_%08X (ecx %08X) | thread %lu: prev call sub_%08X ret %08X, this call ret %08X\n",
                    lean_present_count, watch_addr, watch_val, v, watch_last_va, watch_last_ecx, va, g_ecx,
                    GetCurrentThreadId(), t_last_va, t_last_ret, ret);
            watch_val = v;
        }
        watch_last_va = va; watch_last_ecx = g_ecx; t_last_va = va; t_last_ret = ret;
        if (lean_present_count > watch_until) { lean_watch_on = 0; fprintf(stderr, "[LEAN-WATCH] stopped at f=%ld\n", lean_present_count); }
        /* else fall through to the call-chain log; the bit is kept clear below */
    }
    if (!cc_init) cc_parse();
    for (int i = 0; i < cc_n; i++)
        if (cc_va[i] == va) {
            extern volatile LONG lean_present_count;
            extern __declspec(thread) uint32_t g_esp, g_ecx;
            extern ptrdiff_t g_xbox_mem_offset;
            extern void lean_print_host_chain(const char *tag);
            static unsigned n;
            if (lean_present_count >= cc_from && lean_present_count <= cc_to && n++ < (getenv("LEAN_CALL_CHAIN_NOSTACK") ? 400000u : 2000u)) {
                const uint32_t *a = (const uint32_t *)(g_xbox_mem_offset + (uintptr_t)g_esp);
                char tag[160];
                snprintf(tag, sizeof tag, "call sub_%08X f=%ld ecx %08X args %08X %08X %08X %08X", va, lean_present_count, g_ecx, a[1], a[2], a[3], a[4]);
                {   /* LEAN_CALL_CHAIN_NOSTACK=1: one short line per call (with the vblank counter 0x1E5204) */
                    static int ns = -1; if (ns < 0) { const char *e = getenv("LEAN_CALL_CHAIN_NOSTACK"); ns = e && e[0] == '1'; }
                    if (ns) { LARGE_INTEGER q; QueryPerformanceCounter(&q);
                        fprintf(stderr, "[LEAN-CALL] %s qpc=%lld vblank=%u\n", tag, (long long)q.QuadPart, *(const uint32_t *)(g_xbox_mem_offset + 0x1E5204u)); }
                    else lean_print_host_chain(tag);
                }
                {   /* LEAN_CALL_PEEK=1: also print the LEAN_PEEK entries at this call */
                    static int cp = -1; if (cp < 0) { const char *e = getenv("LEAN_CALL_PEEK"); cp = e && e[0] == '1'; }
                    if (cp) { extern void lean_peek_dump(void); lean_peek_dump(); }
                }
                if ((g_ecx >= 0x10000u && g_ecx < 0x04000000u) || (g_ecx >= 0x80000000u && g_ecx < 0x84000000u)) {
                    const uint32_t *o = (const uint32_t *)(g_xbox_mem_offset + (uintptr_t)g_ecx);
                    fprintf(stderr, "[LEAN-TRACE]   ecx[0..9] %08X %08X %08X %08X %08X %08X %08X %08X %08X(%g) %08X\n",
                            o[0], o[1], o[2], o[3], o[4], o[5], o[6], o[7], o[8], *(const float *)&o[8], o[9]);
                }
            }
            return;   /* bit stays clear */
        }
    if (lean_watch_on) return;   /* LEAN_WATCH_GRANT: keep the bit clear so the next call comes back */
    g_lean_cov[va >> 3] |= (uint8_t)(1u << (va & 7));
    if (cov_on < 0) {
        const char *v = getenv("LEAN_COVERAGE");
        cov_on = v && v[0] == '1';
        cov_t0 = GetTickCount64();
    }
    if (cov_on)
        { extern volatile LONG lean_present_count; fprintf(stderr, "[LEAN-COV] t=%llu f=%ld first sub_%08X\n", GetTickCount64() - cov_t0, lean_present_count, va); }
}

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
    { extern volatile LONG lean_present_count;
        fprintf(stderr, "[LEAN-FLAGS] first site=%d t=%llu ms f=%ld\n", id, GetTickCount64() - t0, lean_present_count); }
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

/* LEAN_FIX_CMPSD=audio | d3d | all (comma list allowed; off by default): correct the
 * `xor reg,reg; repe cmpsd; jcc` mis-lifts (scripts/fix_cmpsd_flags.py), where the jcc
 * tested the zeroed register instead of the string compare result.
 * group 1 audio: 0x13BC20 (stream slot format check); group 2 d3d: 0x15E149, 0x165DE0;
 * group 3 dsound: 0x17C4A9 (only with "all"). */
int lean_fix_cmpsd(int group)
{
    static int mask = -1;
    if (mask < 0) {
        const char *e = getenv("LEAN_FIX_CMPSD");
        mask = 0;
        if (e) {
            if (strstr(e, "all") || e[0] == '1') mask = 0xE;
            if (strstr(e, "audio")) mask |= 2;
            if (strstr(e, "d3d")) mask |= 4;
        }
        if (mask) fprintf(stderr, "[LEAN] repe cmpsd fixes on (%s%s%s)\n", mask & 2 ? "audio " : "", mask & 4 ? "d3d " : "", mask & 8 ? "dsound" : "");
    }
    return (mask >> group) & 1;
}
/* counts 0x13BC20 "stream header changed" results (fix on) */
volatile long lean_cmpsd_changed;
int lean_cmpsd_note(int changed)
{
    if (changed) InterlockedIncrement(&lean_cmpsd_changed);
    return changed;
}

/* LEAN_TIMER_EXACT60=1 (enhanced option, off by default; scripts/fix_timer_exact60.py):
 * the game's step timer asks the XAPI multimedia timer (0x10EED3) for 1000/60 = 16 ms,
 * which is 62.5 Hz on the Xbox. Called at 0x10EF9A with edx:eax = period in -100 ns:
 * turn exactly 16 ms into exactly 1/60 s (-166667), so logic runs 60.00 steps/s. */
void lean_timer_exact60(void)
{
    extern __declspec(thread) uint32_t g_eax, g_edx;
    static int on = -1;
    if (on < 0) { const char *v = getenv("LEAN_TIMER_EXACT60"); on = v && v[0] == '1'; }
    if (!on) return;
    int64_t p = (int64_t)(((uint64_t)g_edx << 32) | g_eax);
    if (p == -160000) {
        p = -166667;
        g_eax = (uint32_t)p; g_edx = (uint32_t)((uint64_t)p >> 32);
        fprintf(stderr, "[LEAN] game step timer: 16 ms -> exactly 1/60 s (LEAN_TIMER_EXACT60)\n");
    }
}

/* LEAN_FIX_SAHF=1 (off by default; scripts/fix_sahf_unordered.py): correct NaN (unordered)
 * handling of the 8 branches after `fnstsw ax; sahf` in Driving's CRT maths. */
int lean_fix_sahf(void)
{
    static int on = -1;
    if (on < 0) { const char *v = getenv("LEAN_FIX_SAHF"); on = v && v[0] == '1'; }
    {   /* LEAN_SAHF_COUNT=1 (diagnostic): count, per call site, how often the compare result is
         * unordered (g_fp_cmp == 2, a NaN operand) when a site is evaluated; summary every 10 s */
        static int cnt = -1;
        if (cnt < 0) { const char *c = getenv("LEAN_SAHF_COUNT"); cnt = c && c[0] == '1'; }
        if (cnt) {
            extern __declspec(thread) int g_fp_cmp;
            static void *site[16]; static volatile LONG evals[16], nans[16]; static ULONGLONG last;
            void *ra = _ReturnAddress(); int i;
            for (i = 0; i < 16 && site[i] && site[i] != ra; i++) ;
            if (i < 16) { if (!site[i]) site[i] = ra; InterlockedIncrement(&evals[i]); if (g_fp_cmp == 2) InterlockedIncrement(&nans[i]); }
            ULONGLONG now = GetTickCount64();
            if (now - last >= 10000) { last = now;
                char line[1024]; int n = snprintf(line, sizeof line, "[LEAN-SAHF] per site (evaluations/NaN):");
                for (int k = 0; k < 16 && site[k]; k++) n += snprintf(line + n, sizeof line - n, " %p:%ld/%ld", site[k], evals[k], nans[k]);
                fprintf(stderr, "%s fix=%s\n", line, on ? "on" : "off"); }
        }
    }
    return on;
}

/* LEAN_GADGET_GRANT=n (test only, off by default; scripts/fix_gadget_grant.py): called at
 * 0xBAED9 after the gadget select routine 0xBAE50 stores type eax in weapons+0x08. If that
 * gadget slot (table weapons+0x10, 0x54 bytes per type) has a charge count (+0x18) of 0,
 * give it n charges. The Paris tutorial selects the Q-Smoke but never grants one in our build. */
volatile uint32_t lean_grant_watch;   /* guest address of the last granted count (for test captures) */
void lean_gadget_grant(void)
{
    extern __declspec(thread) uint32_t g_eax, g_ecx;
    extern ptrdiff_t g_xbox_mem_offset;
    extern volatile LONG lean_present_count;
    static int n = -1;
    if (n < 0) {
        const char *v = getenv("LEAN_GADGET_GRANT");
        n = v ? atoi(v) : 0;
        if (n > 0) fprintf(stderr, "[LEAN] gadget grant on: %d charges when a gadget is selected with none (test only)\n", n);
    }
    if (n <= 0) return;
    int32_t t = (int32_t)g_eax;
    if (t < 0 || t >= 64) return;
    uint32_t slots = *(const uint32_t *)(g_xbox_mem_offset + g_ecx + 0x10u);
    volatile int32_t *count = (volatile int32_t *)(g_xbox_mem_offset + slots + (uint32_t)t * 0x54u + 0x18u);
    if (*count == 0) {
        *count = n;
        lean_grant_watch = slots + (uint32_t)t * 0x54u + 0x18u;
        { extern void lean_watch_start(uint32_t); lean_watch_start(lean_grant_watch); }
        fprintf(stderr, "[LEAN-GRANT] f=%ld gadget type %02X slot %08X: count 0 -> %d\n", lean_present_count, t, slots + (uint32_t)t * 0x54u, n);
    }
}

/* LEAN_FIX_CLIPSEARCH=1 (off by default; scripts/fix_clip_search_setb.py): correct the setb in the
 * clip-track binary search 0x76C60, which always returned the first track (e.g. no Q-Smoke fire clip). */
int lean_fix_clipsearch(void)
{
    static int on = -1; static long from;
    if (on < 0) { const char *v = getenv("LEAN_FIX_CLIPSEARCH"), *f = getenv("LEAN_FIX_CLIPSEARCH_FROM"); on = v && v[0] == '1';
        from = f ? atol(f) : 0;   /* test only: fix active from this game frame on */
        if (on) fprintf(stderr, "[LEAN] clip track search setb fix on (LEAN_FIX_CLIPSEARCH) from f=%ld\n", from); }
    if (on && from) { extern volatile LONG lean_present_count; return lean_present_count >= from; }
    return on;
}

/* LEAN_CLIPSEARCH_LOG=1 (diagnostic; scripts/fix_clip_search_note.py): at the return of the clip-track
 * search 0x76C60, log once per (caller, table, key) each lookup whose result is not the first track. */
void lean_clipsearch_note(uint32_t result, uint32_t begin, uint32_t end, uint32_t key, uint32_t ret)
{
    static int on = -1;
    static uint64_t seen[4096];
    static unsigned nseen;
    extern ptrdiff_t g_xbox_mem_offset;
    extern volatile LONG lean_present_count;
    if (on < 0) { const char *v = getenv("LEAN_CLIPSEARCH_LOG"); on = v && v[0] == '1'; }
    if (!on) return;
    const uint8_t *m = (const uint8_t *)g_xbox_mem_offset;
    if (!lean_fix_clipsearch()) {   /* fix off: log what the corrected search would return (shadow lower_bound) */
        uint32_t lo = begin; int32_t n = (int32_t)(end - begin) / 16;
        while (n > 0) { int32_t h = n / 2; uint32_t e = lo + (uint32_t)h * 16;
            int below = m[e + 0xC] != m[key + 0xC] ? m[e + 0xC] < m[key + 0xC] : m[e + 0xD] < m[key + 0xD];
            if (below) { lo = e + 16; n -= h + 1; } else n = h; }
        if (lo < end && m[lo + 0xC] == m[key + 0xC] && m[lo + 0xD] == m[key + 0xD] && lo != result) result = lo;   /* would now find a match */
        else return;
    }
    if (result == begin) return;
    uint8_t ka = m[key + 0xC], kv = m[key + 0xD];
    uint64_t id = ((uint64_t)ret << 40) ^ ((uint64_t)begin << 16) ^ ((uint64_t)ka << 8) ^ kv;
    for (unsigned i = 0; i < nseen; i++) if (seen[i] == id) return;
    if (nseen >= 4096) return;
    seen[nseen++] = id;
    int found = result < end;
    fprintf(stderr, "[LEAN-CLIPSEARCH] f=%ld caller %08X table %08X (%u tracks) key action %02X variant %02X -> track %u%s%s action %02X variant %02X\n",
            lean_present_count, ret - 5, begin, (end - begin) / 16, ka, kv, (result - begin) / 16,
            found ? "" : " (past end)", found && m[result + 0xC] == ka && m[result + 0xD] == kv ? " match" : " no-match",
            found ? m[result + 0xC] : 0, found ? m[result + 0xD] : 0);
}

/* Float->int rounding fixes (scripts/fix_cvtss2si_round.py), off by default.
 * site = enclosing function address * 16 + occurrence. Shadow logs count, per site, how often
 * the lifted result differs from the hardware result; a summary prints every 10 s. */
#include <emmintrin.h>
#include <math.h>
typedef struct { uint32_t site; volatile LONG evals, diffs; } round_site;
static void round_note(round_site *t, uint32_t site, int diff, const char *tag)
{
    static volatile LONG lock; static ULONGLONG last[2];
    int i, h = (int)((site * 2654435761u) >> 24);   /* 256-entry open table */
    for (i = 0; i < 256; i++) { round_site *e = &t[(h + i) & 255];
        if (e->site == site) { InterlockedIncrement(&e->evals); if (diff) InterlockedIncrement(&e->diffs); break; }
        if (!e->site && !InterlockedCompareExchange(&lock, 1, 0)) {
            if (!e->site) e->site = site; InterlockedExchange(&lock, 0);
            if (e->site == site) { InterlockedIncrement(&e->evals); if (diff) InterlockedIncrement(&e->diffs); break; } } }
    ULONGLONG now = GetTickCount64(); int w = tag[0] == 'f';
    if (now - last[w] >= 10000) { last[w] = now;
        char line[2048]; int n = snprintf(line, sizeof line, "[LEAN-ROUND] %s sites with differences (func#occ evals/diffs):", tag), shown = 0;
        for (i = 0; i < 256 && n < (int)sizeof line - 48; i++) if (t[i].site && t[i].diffs) {
            n += snprintf(line + n, sizeof line - n, " %06X#%u %ld/%ld", t[i].site >> 4, t[i].site & 15, t[i].evals, t[i].diffs); shown++; }
        fprintf(stderr, "%s%s\n", line, shown ? "" : " none"); }
}
int32_t lean_cvtss2si(float x, uint32_t site)
{
    static int fix = -1, log = -1; static round_site tab[256];
    if (fix < 0) { const char *e = getenv("LEAN_FIX_CVTSS2SI"); fix = e && e[0] == '1'; const char *l = getenv("LEAN_CVTSS2SI_LOG"); log = l && l[0] == '1';
        if (fix) fprintf(stderr, "[LEAN-ROUND] cvtss2si rounds by MXCSR (LEAN_FIX_CVTSS2SI)\n"); }
    int32_t hw = _mm_cvtss_si32(_mm_set_ss(x)), old = (int32_t)x;
    if (log) round_note(tab, site, hw != old, "cvtss2si");
    {   /* LEAN_FIX_CVTSS2SI_SITES=func[#occ],... (diagnostic bisect): fix only these sites, e.g.
         * "0AD4A0,0CC880#1" (hex function address; optional occurrence). Overrides LEAN_FIX_CVTSS2SI. */
        static int init; static uint32_t sel[64]; static int nsel = -1;
        if (!init) { const char *e = getenv("LEAN_FIX_CVTSS2SI_SITES"); init = 1;
            if (e && *e) { nsel = 0; while (*e && nsel < 64) { char *q; uint32_t f = (uint32_t)strtoul(e, &q, 16), occ = 0xFF;
                    if (*q == '#') occ = (uint32_t)strtoul(q + 1, &q, 10);
                    sel[nsel++] = (f << 8) | occ; e = *q ? q + 1 : q; }
                fprintf(stderr, "[LEAN-ROUND] cvtss2si fix limited to %d site entries (LEAN_FIX_CVTSS2SI_SITES)\n", nsel); } }
        if (nsel >= 0) {
            for (int i = 0; i < nsel; i++)
                if ((sel[i] >> 8) == (site >> 4) && ((sel[i] & 0xFF) == 0xFF || (sel[i] & 0xFF) == (site & 15))) return hw;
            return old;
        }
    }
    return fix ? hw : old;
}
long long lean_fist_rc(double x, uint32_t site)
{
    extern __declspec(thread) uint16_t g_fp_control_word;
    static int fix = -1, log = -1; static round_site tab[256];
    if (fix < 0) { const char *e = getenv("LEAN_FIX_FIST"); fix = e && e[0] == '1'; const char *l = getenv("LEAN_FIST_LOG"); log = l && l[0] == '1';
        if (fix) fprintf(stderr, "[LEAN-ROUND] fist/fistp round by the x87 control word (LEAN_FIX_FIST)\n"); }
    long long old = llrint(x), hw;
    switch ((g_fp_control_word >> 10) & 3) {
    case 1: hw = (long long)floor(x); break;
    case 2: hw = (long long)ceil(x); break;
    case 3: hw = (long long)trunc(x); break;
    default: hw = old; break;   /* nearest-even, as llrint in the default host mode */
    }
    if (log) round_note(tab, site, hw != old, "fist");
    return fix ? hw : old;
}
