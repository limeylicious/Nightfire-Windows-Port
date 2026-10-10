/* Freeze monitor (diagnostic, lean build). LEAN_HANG_DUMP=1 starts a watcher
 * thread when the first frame is presented. If no frame is presented for
 * LEAN_HANG_SECS seconds (default 4), after the first 200 frames, it prints
 * (once per stall):
 *   - every other thread's host call chain, symbolised (the recompiled
 *     sub_XXXXXXXX frames are the guest call chain);
 *   - our own mixer's output ring (lean_ds_ring_dump);
 *   - the IRQL lock owners and the pending DPC queue (lean_kernel_dump_state).
 * Each thread is suspended only while its stack is unwound; names are looked
 * up after it is resumed, so a suspended thread holding the heap or the
 * symbol lock cannot block the dump. */
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile LONG lean_present_count;
extern void lean_kernel_dump_state(void);

#define HANG_FRAMES 48

static int unwind(HANDLE th, DWORD64 *pcs)
{
    CONTEXT c;
    int n = 0;
    memset(&c, 0, sizeof c);
    c.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(th, &c)) return 0;
    while (n < HANG_FRAMES && c.Rip) {
        DWORD64 base = 0;
        PRUNTIME_FUNCTION f;
        pcs[n++] = c.Rip;
        f = RtlLookupFunctionEntry(c.Rip, &base, NULL);
        if (f) {
            PVOID hd; DWORD64 ef;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c.Rip, f, &c, &hd, &ef, NULL);
        } else {   /* leaf function: return address is at the stack pointer */
            DWORD64 ret = 0; SIZE_T got = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)c.Rsp, &ret, 8, &got) || got != 8) break;
            c.Rip = ret; c.Rsp += 8;
        }
    }
    return n;
}

static void dump_threads(void)
{
    DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te;
    if (snap == INVALID_HANDLE_VALUE) return;
    te.dwSize = sizeof te;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        DWORD64 pcs[HANG_FRAMES];
        int n;
        HANDLE th;
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
        th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
        if (!th) continue;
        if (SuspendThread(th) == (DWORD)-1) { CloseHandle(th); continue; }
        n = unwind(th, pcs);
        ResumeThread(th);
        CloseHandle(th);
        fprintf(stderr, "[LEAN-HANG] thread %lu (%d frames):\n", te.th32ThreadID, n);
        for (int i = 0; i < n; i++) {
            char buf[sizeof(SYMBOL_INFO) + 256];
            SYMBOL_INFO *sym = (SYMBOL_INFO *)buf;
            DWORD64 disp = 0;
            memset(buf, 0, sizeof buf); sym->SizeOfStruct = sizeof(SYMBOL_INFO); sym->MaxNameLen = 255;
            if (SymFromAddr(GetCurrentProcess(), pcs[i], &disp, sym))
                fprintf(stderr, "[LEAN-HANG]   #%d %s+0x%llX\n", i, sym->Name, (unsigned long long)disp);
            else
                fprintf(stderr, "[LEAN-HANG]   #%d %016llX\n", i, (unsigned long long)pcs[i]);
        }
    }
    CloseHandle(snap);
}

static DWORD WINAPI hang_thread(LPVOID p)
{
    const char *v = getenv("LEAN_HANG_SECS");
    DWORD secs = v ? (DWORD)atoi(v) : 4, still = 0;
    LONG last = -1;
    (void)p;
    if (!secs) secs = 4;
    for (;;) {
        Sleep(500);
        LONG now = lean_present_count;
        if (now != last) { last = now; still = 0; continue; }
        { static long arm = -1; if (arm < 0) { const char *ae = getenv("LEAN_HANG_ARM"); arm = ae ? atol(ae) : 200; }
          if (now < arm) continue; }   /* startup loading stalls are normal (LEAN_HANG_ARM=frames, default 200) */
        if (++still == secs * 2) {
            fprintf(stderr, "[LEAN-HANG] no game frame for %lu s (game frames=%ld; smooth-mode presents may continue); dumping state\n", secs, now);
            lean_kernel_dump_state();
            { extern void lean_ds_ring_dump(void); lean_ds_ring_dump(); }
            dump_threads();
            { extern void lean_strm_dump(uint32_t); lean_strm_dump(0); }
            lean_kernel_dump_state();   /* again, to show whether anything moved */
            fprintf(stderr, "[LEAN-HANG] dump end\n");
            { extern void lean_session_freeze(unsigned, long); lean_session_freeze(secs, now); }
            fflush(stderr);
        }
    }
}

/* LEAN_PEEK (diagnostic): printed each logged frame (LEAN_GAMETIME_LOG) and, with
 * LEAN_CALL_PEEK=1, at every LEAN_CALL_CHAIN call. */
void lean_peek_dump(void)
{
    extern ptrdiff_t g_xbox_mem_offset;
    {   /* LEAN_PEEK=addr[>off..],count[;..] (diagnostic): dwords as hex and float each logged frame.
         * Each ">off" follows a pointer: a = *(u32 *)a + off (80E3F61C>10 is (*0x80E3F61C)+0x10). */
        static int pinit, pn; static uint32_t pa[4][6], pd[4], pc[4];
        if (!pinit) { const char *e = getenv("LEAN_PEEK"); pinit = 1;
            while (e && *e && pn < 4) { char *q; pa[pn][0] = (uint32_t)strtoul(e, &q, 16); pd[pn] = 0;
                while (*q == '>' && pd[pn] < 5) { pa[pn][++pd[pn]] = (uint32_t)strtoul(q + 1, &q, 16); }
                if (*q != ',') break; pc[pn] = (uint32_t)strtoul(q + 1, &q, 10); if (pc[pn] > 16) pc[pn] = 16; pn++;
                e = strchr(q, ';'); if (e) e++; } }
        for (int i = 0; i < pn; i++) {
            uint32_t a = pa[i][0]; int ok = 1;
            for (uint32_t k = 1; k <= pd[i] && ok; k++) {
                if (!((a >= 0x10000u && a < 0x04000000u) || (a >= 0x80000000u && a < 0x84000000u))) { ok = 0; break; }
                a = *(const uint32_t *)(g_xbox_mem_offset + a) + pa[i][k]; }
            if (!ok || !((a >= 0x10000u && a < 0x04000000u) || (a >= 0x80000000u && a < 0x84000000u))) { fprintf(stderr, "[LEAN-PEEK] f=%ld entry %d: bad pointer\n", lean_present_count, i); continue; }
            char line[640]; int len = snprintf(line, sizeof line, "[LEAN-PEEK] f=%ld %08X:", lean_present_count, a);
            for (uint32_t k = 0; k < pc[i]; k++) { uint32_t v = *(const uint32_t *)(g_xbox_mem_offset + a + 4 * k);
                len += snprintf(line + len, sizeof line - len, " %08X(%.3g)", v, *(const float *)&v); }
            fprintf(stderr, "%s\n", line);
        }
    }
}

/* Called from present_physical for each presented frame. */
void lean_hang_frame(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("LEAN_HANG_DUMP"), *sc = getenv("LEAN_SESSION_CAPTURE");
        /* the monitor counts game frames (this call), not smooth-mode presents, so a stalled
         * game behind a still-presenting window is caught; session capture turns it on too
         * (LEAN_HANG_DUMP=0 keeps it off) */
        on = (v && v[0] == '1') || (!v && sc && sc[0] == '1');
        if (on) { HANDLE h = CreateThread(NULL, 0, hang_thread, NULL, 0, NULL); if (h) CloseHandle(h);
            fprintf(stderr, "[LEAN-HANG] freeze monitor on\n"); }
    }
    { static void stage_frame(void); stage_frame(); }   /* LEAN_STAGE_TIMING */
    InterlockedIncrement(&lean_present_count);
    {   /* LEAN_POKE=addr:value@frame[;..] (diagnostic only): write a guest dword once at that game frame */
        static int init, n; static uint32_t pa[4], pv[4]; static long pf[4]; static int done[4];
        extern ptrdiff_t g_xbox_mem_offset;
        if (!init) { const char *e = getenv("LEAN_POKE"); init = 1;
            while (e && *e && n < 4) { char *q; pa[n] = (uint32_t)strtoul(e, &q, 16); if (*q != ':') break;
                pv[n] = (uint32_t)strtoul(q + 1, &q, 16); if (*q != '@') break; pf[n] = strtol(q + 1, &q, 10); n++;
                e = strchr(q, ';'); if (e) e++; } }
        for (int i = 0; i < n; i++)
            if (!done[i] && lean_present_count >= pf[i]) { done[i] = 1;
                *(uint32_t *)(g_xbox_mem_offset + pa[i]) = pv[i];
                fprintf(stderr, "[LEAN-POKE] f=%ld wrote %08X = %08X\n", lean_present_count, pa[i], pv[i]); }
    }
    {   /* LEAN_GAMETIME_LOG=from,to (diagnostic): game time 0x243A6C (1/60 s steps) and
         * step count 0x243A68 against real time, each frame in that game-frame range */
        static int init; static long gf = -1, gt = -1; static LARGE_INTEGER hz, t0;
        extern ptrdiff_t g_xbox_mem_offset;
        if (!init) { const char *e = getenv("LEAN_GAMETIME_LOG"); if (e) sscanf(e, "%ld,%ld", &gf, &gt); QueryPerformanceFrequency(&hz); QueryPerformanceCounter(&t0); init = 1; }
        if (gf >= 0 && lean_present_count >= gf && lean_present_count <= gt) {
            LARGE_INTEGER now; QueryPerformanceCounter(&now);
            fprintf(stderr, "[LEAN-GAMETIME] f=%ld real=%.3f game=%.4f steps=%u vblanks=%u sched_last=%u lock=%02X/%02X\n", lean_present_count,
                    (double)(now.QuadPart - t0.QuadPart) / hz.QuadPart,
                    *(const float *)(g_xbox_mem_offset + 0x243A6Cu), *(const uint32_t *)(g_xbox_mem_offset + 0x243A68u),
                    *(const uint32_t *)(g_xbox_mem_offset + 0x1E5204u), *(const uint32_t *)(g_xbox_mem_offset + 0x80107314u),
                    *(const uint8_t *)(g_xbox_mem_offset + 0x8010731Cu), *(const uint8_t *)(g_xbox_mem_offset + 0x8010731Du));
            lean_peek_dump();
        }
    }
}

/* Called once from sub_0014B520's block walk (src/recomp/gen/recomp_0013.c)
 * after a million passes: dump the chain it is stuck on. */
extern ptrdiff_t g_xbox_mem_offset;
void lean_strm_dump(uint32_t walker);
void lean_loop_probe(uint32_t ecx, uint32_t edx, uint32_t ebx, uint32_t base)
{
    const uint32_t *g = (const uint32_t *)(g_xbox_mem_offset + (uintptr_t)(ecx & ~15u));
    const uint32_t *b = (const uint32_t *)(g_xbox_mem_offset + (uintptr_t)ebx);
    fprintf(stderr, "[LEAN-LOOP] sub_0014B520 spinning: block=%08X want_tag=%08X list=%08X base=%08X list words: %08X %08X %08X %08X\n",
            ecx, edx, ebx, base, b[0], b[1], b[2], b[3]);
    if ((ecx & ~15u) >= 0x40) /* the block pointer can be 0 here; don't read below guest memory */
    for (int r = -2; r < 6; r++)
        fprintf(stderr, "[LEAN-LOOP]   %08X: %08X %08X %08X %08X\n", (ecx & ~15u) + r * 16,
                g[r * 4], g[r * 4 + 1], g[r * 4 + 2], g[r * 4 + 3]);
    lean_strm_dump(ecx);
    fflush(stderr);
}

/* LEAN_STRM_LOCK=1 (diagnostic): host lock around the STRM stream ring's
 * producer and consumer entries (wrappers in src/recomp/gen/recomp_0013.c). */
static CRITICAL_SECTION strm_cs;
static INIT_ONCE strm_once = INIT_ONCE_STATIC_INIT;
static int strm_on;
static BOOL CALLBACK strm_init(PINIT_ONCE o, PVOID p, PVOID *c)
{
    const char *v = getenv("LEAN_STRM_LOCK");
    (void)o; (void)p; (void)c;
    InitializeCriticalSection(&strm_cs);
    strm_on = v && v[0] == '1';
    if (strm_on) fprintf(stderr, "[LEAN] STRM producer/consumer lock on\n");
    return TRUE;
}
int lean_strm_lock(void)
{
    InitOnceExecuteOnce(&strm_once, strm_init, NULL, NULL);
    if (!strm_on) return 0;
    EnterCriticalSection(&strm_cs);
    return 1;
}
void lean_strm_unlock(int locked) { if (locked) LeaveCriticalSection(&strm_cs); }

/* LEAN_STRM_CHECK=1 (diagnostic): the STRM consumer walk (sub_0014B520)
 * reports every packet header it visits. A header with size 0 that is not a
 * wrap marker (word0 = -1) is a "hit" (the ~91 s freeze spins on one). Hits
 * are counted; the first 5 are logged with the stream object's ring fields and
 * the last 32 producer/consumer operations (function, enter/leave, thread).
 * Every 5 s: hits, headers checked, and a histogram of packet tags. */
int lean_strm_check_on = -1;
static volatile LONG strm_hits, strm_checked;
static volatile LONG strm_tags[256];
static struct { uint32_t fn; int leave; DWORD tid; ULONGLONG t; } strm_ops[32];
static volatile LONG strm_opn;
/* Freeze capture: last STRM object and readers seen, and 64 ops with ring pointers. */
static volatile uint32_t strm_obj, strm_readers[8]; static volatile LONG strm_nreaders;
static struct { uint32_t fn; int leave; DWORD tid; ULONGLONG t; uint32_t rd, parse, wr, state, rcount, rptr; } strm_ops64[64];
static volatile LONG strm_op64n;
static const uint32_t *strm_g(uint32_t a) { return (const uint32_t *)(g_xbox_mem_offset + (uintptr_t)a); }
void lean_strm_reader(uint32_t reader, uint32_t obj)
{
    LONG k;
    strm_obj = obj;
    for (k = 0; k < strm_nreaders; k++) if (strm_readers[k] == reader) return;
    if (strm_nreaders < 8) { strm_readers[strm_nreaders] = reader; InterlockedIncrement(&strm_nreaders); }
}
void lean_strm_op(uint32_t fn, int leave)
{
    if (lean_strm_check_on <= 0) return;
    { uint32_t o = strm_obj; LONG j = InterlockedIncrement(&strm_op64n) & 63;
      strm_ops64[j].fn = fn; strm_ops64[j].leave = leave; strm_ops64[j].tid = GetCurrentThreadId(); strm_ops64[j].t = GetTickCount64();
      if (o) { const uint32_t *g = strm_g(o); strm_ops64[j].rd = g[0x60/4]; strm_ops64[j].parse = g[0x64/4]; strm_ops64[j].wr = g[0x68/4]; strm_ops64[j].state = g[0x48/4]; }
      if (strm_nreaders) { const uint32_t *r = strm_g(strm_readers[0]); strm_ops64[j].rcount = r[2]; strm_ops64[j].rptr = r[3]; } }
    LONG i = InterlockedIncrement(&strm_opn) & 31;
    strm_ops[i].fn = fn; strm_ops[i].leave = leave; strm_ops[i].tid = GetCurrentThreadId(); strm_ops[i].t = GetTickCount64();
}
static DWORD WINAPI strm_report(LPVOID p)
{
    (void)p;
    for (;;) {
        Sleep(5000);
        char buf[512]; int n = 0;
        for (int t = 0; t < 256; t++) { LONG c = InterlockedExchange(&strm_tags[t], 0); if (c && n < 480) n += snprintf(buf + n, sizeof buf - n, " %02X:%ld", t, c); }
        { extern long lean_strm_stale_count(void), lean_strm_negative_count(void), lean_strm_dry_count(void), lean_strm_credit0_count(void), lean_strm_close_count(void);
          extern long lean_strm_pad_unmatched(void), lean_strm_pad_matched(void);
          fprintf(stderr, "[LEAN-STRM] 5s: hits=%ld checked=%ld stale=%ld negative=%ld dry=%ld credit0=%ld close=%ld pads=%ld/%ld(unmatched/matched) tags:%s\n", strm_hits, InterlockedExchange(&strm_checked, 0),
                  lean_strm_stale_count(), lean_strm_negative_count(), lean_strm_dry_count(), lean_strm_credit0_count(), lean_strm_close_count(),
                  lean_strm_pad_unmatched(), lean_strm_pad_matched(), n ? buf : " none"); }
        { extern void lean_strm_outstanding_line(void); lean_strm_outstanding_line(); }
    }
}
static void strm_check_init(void)
{
    const char *v = getenv("LEAN_STRM_CHECK");
    int on = v && v[0] == '1';
    if (on) { extern void lean_snd_watch_start(void); lean_snd_watch_start(); }
    if (on) { HANDLE h = CreateThread(NULL, 0, strm_report, NULL, 0, NULL); if (h) CloseHandle(h); fprintf(stderr, "[LEAN-STRM] validator on\n"); }
    lean_strm_check_on = on;
}
void lean_strm_check(uint32_t hdr, uint32_t w0, uint32_t w1, uint32_t obj)
{
    if (lean_strm_check_on < 0) { strm_check_init(); if (!lean_strm_check_on) return; }
    InterlockedIncrement(&strm_checked);
    InterlockedIncrement(&strm_tags[w1 >> 24]);
    if ((w1 & 0xFFFFFF) || w0 == 0xFFFFFFFFu) return;
    LONG h = InterlockedIncrement(&strm_hits);
    if (h > 5) return;
    const uint32_t *o = (const uint32_t *)(g_xbox_mem_offset + (uintptr_t)obj);
    fprintf(stderr, "[LEAN-STRM] HIT %ld: header %08X = {%08X, %08X} thread %lu obj %08X magic=%08X +3C=%08X +40=%08X +44=%08X +48=%08X +60=%08X +64=%08X +68=%08X +180=%08X\n",
            h, hdr, w0, w1, GetCurrentThreadId(), obj, o[0], o[0x3C/4], o[0x40/4], o[0x44/4], o[0x48/4], o[0x60/4], o[0x64/4], o[0x68/4], o[0x180/4]);
    LONG last = strm_opn;
    for (int k = 31; k >= 0; k--) {
        int i = (int)((last - k) & 31);
        if (strm_ops[i].fn) fprintf(stderr, "[LEAN-STRM]   op %08X %s thread %lu t=%llu\n", strm_ops[i].fn, strm_ops[i].leave ? "leave" : "enter", strm_ops[i].tid, strm_ops[i].t);
    }
    fflush(stderr);
}
void lean_pump_caller(void)
{
    static volatile LONG n; static DWORD seen[8]; static int ns;
    if (lean_strm_check_on <= 0) { if (lean_strm_check_on < 0) strm_check_init(); if (lean_strm_check_on <= 0) return; }
    DWORD t = GetCurrentThreadId();
    for (int i = 0; i < ns; i++) if (seen[i] == t) return;
    if (ns < 8) { seen[ns++] = t; fprintf(stderr, "[LEAN-STRM] task pump 0x10AC40 called from thread %lu (%d distinct so far)\n", t, ns); }
    (void)n;
}

/* LEAN_STRM_FIX=1: lock-first hand fix in sub_0014B520 (recomp_0013.c). */
int lean_strm_fix_on(void)
{
    static int on = -1;
    if (on < 0) {
        /* forced on with LEAN_AUDIO_NATIVE (start-up STRM reader race) unless LEAN_STRM_FIX=0 */
        extern int lean_audio_native_on(void);
        const char *v = getenv("LEAN_STRM_FIX");
        on = v ? v[0] == '1' : lean_audio_native_on();
        if (on) fprintf(stderr, "[LEAN-STRM] reader lock-first fix on%s\n", v ? "" : " (forced by LEAN_AUDIO_NATIVE)");
        else if (lean_audio_native_on()) fprintf(stderr, "[LEAN-STRM] WARNING: LEAN_STRM_FIX=0 with native audio (vehicle start-up can hang)\n");
    }
    return on;
}
/* LEAN_STRM_JITTER=1 (test only): spin ~30 us between the parser's count and
 * pointer stores, to make the reader race frequent. */
void lean_strm_jitter(void)
{
    static int on = -1; static LARGE_INTEGER hz;
    if (on < 0) { const char *v = getenv("LEAN_STRM_JITTER"); on = v && v[0] == '1'; QueryPerformanceFrequency(&hz); if (on) fprintf(stderr, "[LEAN-STRM] jitter amplifier on\n"); }
    if (!on) return;
    LARGE_INTEGER a, b; QueryPerformanceCounter(&a);
    do QueryPerformanceCounter(&b); while ((b.QuadPart - a.QuadPart) * 1000000 / hz.QuadPart < 30);
}
/* LEAN_STRM_CHECK: packet the reader is about to return. Stale = tag byte
 * not the reader's tag, or word0 = -2 (consumed). */
static volatile LONG strm_stale, strm_negative, strm_stale_tag[256];
void lean_strm_handout(uint32_t pkt, uint32_t w0, uint32_t w1, uint32_t tag, uint32_t obj);
void lean_strm_stale(uint32_t pkt, uint32_t w0, uint32_t w1, uint32_t rtag, uint32_t count, uint32_t obj)
{
    if (lean_strm_check_on < 0) { strm_check_init(); if (!lean_strm_check_on) return; }
    lean_strm_handout(pkt, w0, w1, rtag, obj);
    if ((w1 >> 24) == (rtag & 0xFF) && w0 != 0xFFFFFFFEu) return;
    LONG n = InterlockedIncrement(&strm_stale);
    InterlockedIncrement(&strm_stale_tag[rtag & 0xFF]);
    if (n <= 5) fprintf(stderr, "[LEAN-STRM] STALE %ld: pkt %08X {%08X, %08X} reader tag %02X count %d obj %08X thread %lu t=%llu\n",
                        n, pkt, w0, w1, rtag & 0xFF, (int32_t)count, obj, GetCurrentThreadId(), GetTickCount64());
}
void lean_strm_negative(int32_t count)
{
    LONG n = InterlockedIncrement(&strm_negative);
    if (n <= 5) fprintf(stderr, "[LEAN-STRM] NEGATIVE count %d thread %lu t=%llu\n", count, GetCurrentThreadId(), GetTickCount64());
}
long lean_strm_stale_count(void) { return strm_stale; }
long lean_strm_negative_count(void) { return strm_negative; }

/* LEAN_STRM_CHECK: dry polls (reader saw count 0) and credits from count 0
 * (worker stored a new next-packet pointer); "close" = a credit within 1 ms
 * of a dry poll on another thread, i.e. the window the reader race needs. */
static volatile LONG strm_dry, strm_credit0, strm_close;
static volatile LONGLONG strm_last_dry_qpc; static volatile DWORD strm_last_dry_tid;
void lean_strm_dry(void)
{
    LARGE_INTEGER q; QueryPerformanceCounter(&q);
    InterlockedIncrement(&strm_dry); strm_last_dry_qpc = q.QuadPart; strm_last_dry_tid = GetCurrentThreadId();
}
void lean_strm_credit0(void)
{
    static LARGE_INTEGER hz; LARGE_INTEGER q;
    if (!hz.QuadPart) QueryPerformanceFrequency(&hz);
    QueryPerformanceCounter(&q);
    InterlockedIncrement(&strm_credit0);
    if (strm_last_dry_tid != GetCurrentThreadId() && q.QuadPart - strm_last_dry_qpc < hz.QuadPart / 1000) InterlockedIncrement(&strm_close);
}
long lean_strm_dry_count(void) { return strm_dry; }
long lean_strm_credit0_count(void) { return strm_credit0; }
long lean_strm_close_count(void) { return strm_close; }

/* LEAN_STRM_CHECK: end/pad packets seen by the parser 0x14AD20. */
static volatile LONG strm_pads_matched, strm_pads_unmatched;
void lean_strm_pad(uint32_t hdr, uint32_t w0, uint32_t w1, uint32_t rounded, uint32_t obj, int matched)
{
    LONG n = InterlockedIncrement(matched ? &strm_pads_matched : &strm_pads_unmatched);
    if (n > 8) return;
    const uint32_t *o = (const uint32_t *)(g_xbox_mem_offset + (uintptr_t)obj);
    const uint32_t *h = (const uint32_t *)(g_xbox_mem_offset + (uintptr_t)hdr);
    fprintf(stderr, "[LEAN-STRM] PAD %s %ld: hdr %08X w0=%08X w1=%08X rounded=%u thread %lu t=%llu ring +40=%08X +44=%08X +60=%08X +64=%08X +68=%08X entry(+70)=%08X\n",
            matched ? "matched" : "UNMATCHED", n, hdr, w0, w1, rounded, GetCurrentThreadId(), GetTickCount64(),
            o[0x40/4], o[0x44/4], o[0x60/4], o[0x64/4], o[0x68/4], o[0x70/4]);
    fprintf(stderr, "[LEAN-STRM]   padding after header: %08X %08X %08X %08X %08X %08X %08X %08X\n", h[2], h[3], h[4], h[5], h[6], h[7], h[8], h[9]);
}
int lean_strm_padfix_on(void)
{
    static int on = -1;
    if (on < 0) { const char *v = getenv("LEAN_STRM_PADFIX"); on = v && v[0] == '1'; if (on) fprintf(stderr, "[LEAN-STRM] pad fix on\n"); }
    return on;
}
long lean_strm_pad_unmatched(void) { return strm_pads_unmatched; }
long lean_strm_pad_matched(void) { return strm_pads_matched; }

/* Freeze capture (LEAN_STRM_CHECK): STRM object, entries, readers, ring memory
 * around the walker and 0x60/0x64/0x68, and the last 64 ops, written to
 * <DRIVING_CAPTURE_DIR>/strm-dump.txt (or stderr). Called by the freeze monitor
 * and the sub_0014B520 loop probe. */
static void strm_hex(FILE *f, const char *what, uint32_t at)
{
    uint32_t a = (at & ~15u) - 4096;
    fprintf(f, "-- %s %08X: ring bytes %08X..%08X\n", what, at, a, a + 8192);
    for (uint32_t x = a; x < a + 8192; x += 16) { const uint32_t *g = strm_g(x);
        fprintf(f, "%08X: %08X %08X %08X %08X%s\n", x, g[0], g[1], g[2], g[3], (at >= x && at < x + 16) ? "   <==" : ""); }
}
void lean_strm_dump(uint32_t walker)
{
    uint32_t o = strm_obj; char path[MAX_PATH]; const char *dir = getenv("DRIVING_CAPTURE_DIR"); FILE *f = NULL;
    if (lean_strm_check_on <= 0) return;
    if (dir) { snprintf(path, sizeof path, "%s\\strm-dump-%llu.txt", dir, GetTickCount64()); f = fopen(path, "w"); }
    if (!f) f = stderr;
    fprintf(f, "STRM freeze capture t=%llu walker=%08X obj=%08X\n", GetTickCount64(), walker, o);
    if (o) { const uint32_t *g = strm_g(o);
        fprintf(f, "obj magic=%08X\n", g[0]);
        for (unsigned k = 0x3C; k <= 0x78; k += 4) fprintf(f, "  +%03X = %08X\n", k, g[k/4]);
        for (unsigned k = 0x180; k <= 0x18C; k += 4) fprintf(f, "  +%03X = %08X\n", k, g[k/4]);
        for (unsigned e = 0x6C; e <= 0x74; e += 4) { uint32_t ent = g[e/4]; if (!ent || ent < 0x10000 || ent > 0x84000000u) continue;
            const uint32_t *x = strm_g(ent);
            fprintf(f, "entry via +%02X = %08X: +4 state=%08X +10 type=%08X +118=%08X +11C=%08X +120=%08X\n", e, ent, x[1], x[4], x[0x118/4], x[0x11C/4], x[0x120/4]); }
    }
    for (LONG k = 0; k < strm_nreaders; k++) { const uint32_t *r = strm_g(strm_readers[k]);
        fprintf(f, "reader %08X: obj=%08X tag=%08X count=%d ptr=%08X\n", strm_readers[k], r[0], r[1], (int32_t)r[2], r[3]); }
    { void lean_strm_dump_packets(FILE *, uint32_t, uint32_t); if (o) lean_strm_dump_packets(f, o, strm_g(o)[0x60/4]); }
    { extern void lean_snd_dump(FILE *); lean_snd_dump(f); }
    fprintf(f, "last 64 ops (fn enter/leave thread time rd parse wr state reader0.count reader0.ptr):\n");
    for (int k = 63; k >= 0; k--) { int i = (int)((strm_op64n - k) & 63); if (!strm_ops64[i].fn) continue;
        fprintf(f, "  %08X %s %5lu %llu rd=%08X parse=%08X wr=%08X st=%u cnt=%d ptr=%08X\n", strm_ops64[i].fn, strm_ops64[i].leave ? "leave" : "enter", strm_ops64[i].tid, strm_ops64[i].t,
                strm_ops64[i].rd, strm_ops64[i].parse, strm_ops64[i].wr, strm_ops64[i].state, (int32_t)strm_ops64[i].rcount, strm_ops64[i].rptr); }
    if (walker) strm_hex(f, "walker", walker);
    if (o) { const uint32_t *g = strm_g(o); strm_hex(f, "+60 read", g[0x60/4]); strm_hex(f, "+64 parse", g[0x64/4]); strm_hex(f, "+68 write", g[0x68/4]); }
    if (f != stderr) { fclose(f); fprintf(stderr, "[LEAN-STRM] freeze capture written to %s\n", path); }
}

/* LEAN_STRM_CHECK: outstanding-packet table. Hand-outs (0x14B520) and releases
 * (0x14B9F0 writes -2). Where the rd advance loop in 0x14B730 stops on a packet
 * the table says was released but whose word0 is no longer -2/-1, log
 * "OVERWRITTEN AFTER RELEASE". The freeze dump lists outstanding packets. */
#define PKT_N 1024
static struct { uint32_t addr, obj, tag, w0, w1; DWORD tid; ULONGLONG t; int released; DWORD rtid; ULONGLONG rt; uint32_t rw1; } pkts[PKT_N];
static SRWLOCK pkt_lock = SRWLOCK_INIT; static unsigned pkt_next;
static int pkt_find(uint32_t a) { for (int i = 0; i < PKT_N; i++) if (pkts[i].addr == a) return i; return -1; }
void lean_strm_handout(uint32_t pkt, uint32_t w0, uint32_t w1, uint32_t tag, uint32_t obj)
{
    AcquireSRWLockExclusive(&pkt_lock);
    int i = pkt_find(pkt); if (i < 0) { i = (int)(pkt_next++ % PKT_N); }
    pkts[i].addr = pkt; pkts[i].obj = obj; pkts[i].tag = tag & 0xFF; pkts[i].w0 = w0; pkts[i].w1 = w1;
    pkts[i].tid = GetCurrentThreadId(); pkts[i].t = GetTickCount64(); pkts[i].released = 0;
    ReleaseSRWLockExclusive(&pkt_lock);
}
void lean_strm_release(uint32_t pkt, uint32_t w1)
{
    AcquireSRWLockExclusive(&pkt_lock);
    int i = pkt_find(pkt);
    if (i >= 0) { pkts[i].released = 1; pkts[i].rtid = GetCurrentThreadId(); pkts[i].rt = GetTickCount64(); pkts[i].rw1 = w1; }
    ReleaseSRWLockExclusive(&pkt_lock);
}
void lean_strm_rdstop(uint32_t obj, uint32_t rd)
{
    static volatile LONG n;
    const uint32_t *g = strm_g(rd);
    if (g[0] == 0xFFFFFFFEu || g[0] == 0xFFFFFFFFu) return;
    AcquireSRWLockShared(&pkt_lock);
    int i = pkt_find(rd);
    if (i >= 0 && pkts[i].released && InterlockedIncrement(&n) <= 10)
        fprintf(stderr, "[LEAN-STRM] OVERWRITTEN AFTER RELEASE: obj %08X pkt %08X tag %u now %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X; handed out t=%llu thread %lu (w0=%08X w1=%08X), released %llu ms ago by thread %lu\n",
                obj, rd, pkts[i].tag, g[0], g[1], g[2], g[3], g[4], g[5], g[6], g[7], g[8], g[9], g[10], g[11], pkts[i].t, pkts[i].tid, pkts[i].w0, pkts[i].w1, GetTickCount64() - pkts[i].rt, pkts[i].rtid);
    ReleaseSRWLockShared(&pkt_lock);
}
void lean_strm_dump_packets(FILE *f, uint32_t obj, uint32_t rd)
{
    ULONGLONG now = GetTickCount64(); int found = -1;
    AcquireSRWLockShared(&pkt_lock);
    fprintf(f, "outstanding packets (handed out, not released) for obj %08X:\n", obj);
    for (int i = 0; i < PKT_N; i++) {
        if (!pkts[i].addr || pkts[i].obj != obj) continue;
        if (pkts[i].addr == rd) found = i;
        if (!pkts[i].released) fprintf(f, "  %08X tag %u age %llu ms thread %lu w0=%08X w1=%08X\n", pkts[i].addr, pkts[i].tag, now - pkts[i].t, pkts[i].tid, pkts[i].w0, pkts[i].w1);
    }
    if (found >= 0) fprintf(f, "rd packet %08X: tag %u handed out %llu ms ago by thread %lu (w0=%08X w1=%08X), %s\n", rd, pkts[found].tag, now - pkts[found].t, pkts[found].tid, pkts[found].w0, pkts[found].w1,
                            pkts[found].released ? "RELEASED (so overwritten after release)" : "NOT released (still held)");
    else fprintf(f, "rd packet %08X: not in the hand-out table (never handed out by 0x14B520, or evicted)\n", rd);
    ReleaseSRWLockShared(&pkt_lock);
}
/* 5 s line: packets handed out and still held, per tag, with the oldest age. */
void lean_strm_outstanding_line(void)
{
    unsigned held[4] = {0}, rel[4] = {0}; ULONGLONG oldest[4] = {0}, now = GetTickCount64();
    AcquireSRWLockShared(&pkt_lock);
    for (int i = 0; i < PKT_N; i++) {
        if (!pkts[i].addr) continue;
        unsigned t = pkts[i].tag & 3;
        if (pkts[i].released) rel[t]++;
        else { held[t]++; if (now - pkts[i].t > oldest[t]) oldest[t] = now - pkts[i].t; }
    }
    ReleaseSRWLockShared(&pkt_lock);
    fprintf(stderr, "[LEAN-STRM] packets held/released: tag1 %u/%u (oldest %llu ms) tag2 %u/%u (oldest %llu ms)\n",
            held[1], rel[1], oldest[1], held[2], rel[2], oldest[2]);
}

/* LEAN_STRM_CHECK sound watch (game software mixer, from Ghidra): mixer 0x13D820
 * mixes from its position [0x244CC0] up to the DSound play cursor + lead [0x244CBC],
 * wrapping at [0x244CB8]; done callbacks 0x13BB20 release stream packets.
 * Logged per 5 s: mixer calls, done callbacks, largest single-call advance vs the
 * lead. Sampled every 100 ms for the freeze dump. */
static volatile LONG snd_hist_ms[9]; static double snd_max_gap_ms; static volatile LONG snd_underruns;
static volatile LONG snd_mix_calls, snd_done_calls; static uint32_t snd_pos_before; static volatile LONG snd_max_step;
static struct { ULONGLONG t; uint32_t pos, lead, wrap, doneq; LONG mixes, dones; } snd_hist[64]; static volatile LONG snd_hn;
static uint32_t g32(uint32_t a) { return *strm_g(a); }
/* LEAN_AUDIO_GAPSTACK=1 (diagnostic): when the game's sound thread has not mixed for
 * more than 30 ms, snapshot its call stack once per gap (where it is blocked: lock,
 * sleep, or just runnable) plus the owner of the IRQL dispatch lock. */
static volatile DWORD gap_tid; static volatile LONGLONG gap_last_qpc; static volatile LONG gap_id, gap_sampled = -1, gap_lines;
static DWORD WINAPI gap_sampler(LPVOID p)
{
    LARGE_INTEGER hz; QueryPerformanceFrequency(&hz);
    HANDLE t = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    (void)p;
    for (;;) {
        LARGE_INTEGER due; due.QuadPart = -20000;
        if (t && SetWaitableTimer(t, &due, 0, NULL, NULL, FALSE)) WaitForSingleObject(t, 50); else Sleep(2);
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        double ms = (now.QuadPart - gap_last_qpc) * 1000.0 / hz.QuadPart;
        if (!gap_tid || ms < 30.0 || gap_sampled == gap_id || gap_lines >= 40) continue;
        gap_sampled = gap_id;
        HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, gap_tid);
        if (!th) continue;
        DWORD64 pcs[HANG_FRAMES]; int n = 0;
        if (SuspendThread(th) != (DWORD)-1) { n = unwind(th, pcs); ResumeThread(th); }
        CloseHandle(th);
        extern unsigned long lean_dispatch_owner(void);
        char line[2048]; int len = snprintf(line, sizeof line, "[LEAN-GAPSTACK] gap %.1f ms so far, dispatch lock owner %lu (sound thread %lu):", ms, lean_dispatch_owner(), gap_tid);
        for (int i = 0; i < n && len < (int)sizeof line - 80; i++) {
            char buf[sizeof(SYMBOL_INFO) + 256]; SYMBOL_INFO *sym = (SYMBOL_INFO *)buf; DWORD64 disp = 0;
            memset(buf, 0, sizeof buf); sym->SizeOfStruct = sizeof(SYMBOL_INFO); sym->MaxNameLen = 255;
            if (SymFromAddr(GetCurrentProcess(), pcs[i], &disp, sym)) len += snprintf(line + len, sizeof line - len, " %s+%llX", sym->Name, (unsigned long long)disp);
            else len += snprintf(line + len, sizeof line - len, " %llX", (unsigned long long)pcs[i]);
        }
        gap_lines++;
        fprintf(stderr, "%s\n", line);
    }
}
static void gap_note_mix(void)
{
    static int on = -1;
    if (on < 0) { const char *e = getenv("LEAN_AUDIO_GAPSTACK"); on = e && e[0] == '1';
        if (on) { HANDLE h = CreateThread(NULL, 0, gap_sampler, NULL, 0, NULL); if (h) CloseHandle(h); fprintf(stderr, "[LEAN-GAPSTACK] on\n"); } }
    if (!on) return;
    LARGE_INTEGER q; QueryPerformanceCounter(&q);
    gap_tid = GetCurrentThreadId(); gap_last_qpc = q.QuadPart; InterlockedIncrement(&gap_id);
}
void lean_snd_mix(int after)
{
    if (!after) gap_note_mix();
    if (!after) {
        InterlockedIncrement(&snd_mix_calls); snd_pos_before = g32(0x244CC0);
        /* interval between mixer calls, ms buckets <2,<4,<6,<8,<10,<15,<20,<30,>=30 */
        static LARGE_INTEGER f, last; LARGE_INTEGER now; QueryPerformanceCounter(&now);
        if (!f.QuadPart) QueryPerformanceFrequency(&f);
        if (last.QuadPart) { double ms = (now.QuadPart - last.QuadPart) * 1000.0 / f.QuadPart;
            static const double edge[8] = { 2, 4, 6, 8, 10, 15, 20, 30 }; int b = 0; while (b < 8 && ms >= edge[b]) b++;
            InterlockedIncrement(&snd_hist_ms[b]); if (ms > snd_max_gap_ms) snd_max_gap_ms = ms; }
        last = now; return; }
    uint32_t now = g32(0x244CC0), wrap = g32(0x244CB8); LONG step = (LONG)(now - snd_pos_before);
    if (step < 0 && wrap) step += (LONG)wrap;
    if (step > snd_max_step) snd_max_step = step;
    /* ring underrun: the mixer fills from its last position up to cursor + lead, so a
     * step beyond the lead (+32-sample write offset) means the play cursor had already
     * passed data the game had not written yet (music/dialogue glitch) */
    if (step > (LONG)g32(0x244CBC) + 32) InterlockedIncrement(&snd_underruns);
}
void lean_snd_done(void) { InterlockedIncrement(&snd_done_calls); }
static DWORD WINAPI snd_sampler(LPVOID p)
{
    int k = 0; LONG lm = 0, ld = 0;
    (void)p;
    for (;;) {
        Sleep(100);
        LONG i = InterlockedIncrement(&snd_hn) & 63;
        snd_hist[i].t = GetTickCount64(); snd_hist[i].pos = g32(0x244CC0); snd_hist[i].lead = g32(0x244CBC); snd_hist[i].wrap = g32(0x244CB8);
        snd_hist[i].doneq = g32(0x244FE0); snd_hist[i].mixes = snd_mix_calls; snd_hist[i].dones = snd_done_calls;
        if (++k == 50) { k = 0;
            fprintf(stderr, "[LEAN-SND] 5s: mixer calls %ld, done callbacks %ld, largest mix step %ld vs lead %u (wrap %u), pos %08X\n",
                    snd_mix_calls - lm, snd_done_calls - ld, snd_max_step, g32(0x244CBC), g32(0x244CB8), g32(0x244CC0));
            fprintf(stderr, "[LEAN-SND] 5s gaps ms <2:%ld <4:%ld <6:%ld <8:%ld <10:%ld <15:%ld <20:%ld <30:%ld >=30:%ld max %.1f\n",
                    snd_hist_ms[0], snd_hist_ms[1], snd_hist_ms[2], snd_hist_ms[3], snd_hist_ms[4], snd_hist_ms[5], snd_hist_ms[6], snd_hist_ms[7], snd_hist_ms[8], snd_max_gap_ms);
            { extern volatile long lean_cmpsd_changed; fprintf(stderr, "[LEAN-SND] 5s ring underruns %ld, stream header changes (0x13BC20) %ld total\n", InterlockedExchange(&snd_underruns, 0), lean_cmpsd_changed); }
            for (int q = 0; q < 9; q++) snd_hist_ms[q] = 0; snd_max_gap_ms = 0;
            lm = snd_mix_calls; ld = snd_done_calls; snd_max_step = 0; }
    }
}
void lean_snd_watch_start(void) { HANDLE h = CreateThread(NULL, 0, snd_sampler, NULL, 0, NULL); if (h) CloseHandle(h); }
void lean_snd_dump(FILE *f)
{
    fprintf(f, "sound watch, last 6.4 s (t pos lead wrap doneq mixercalls donecalls):\n");
    for (int k = 63; k >= 0; k--) { int i = (int)((snd_hn - k) & 63); if (!snd_hist[i].t) continue;
        fprintf(f, "  %llu %08X %u %u %u %ld %ld\n", snd_hist[i].t, snd_hist[i].pos, snd_hist[i].lead, snd_hist[i].wrap, snd_hist[i].doneq, snd_hist[i].mixes, snd_hist[i].dones); }
    unsigned n = *(const uint8_t *)(g_xbox_mem_offset + 0x244D0Fu);
    fprintf(f, "stream slots (count %u at 0x244D0F):\n", n);
    for (unsigned s2 = 0; s2 < n && s2 < 16; s2++) { uint32_t slot = g32(0x244BA8 + 4 * s2); if (!slot) continue;
        const uint8_t *sb = (const uint8_t *)(g_xbox_mem_offset + (uintptr_t)slot); uint32_t vid = g32(slot + 8);
        fprintf(f, "  slot %u at %08X: reader=%08X voice=%u state=%u active(+124)=%08X\n", s2, slot, g32(slot), vid, sb[0x10], g32(slot + 0x124));
        if (vid < 64) { uint32_t vv = g32(0x2452E4 + 4 * vid); if (vv) { const uint8_t *vb = (const uint8_t *)(g_xbox_mem_offset + (uintptr_t)vv);
            fprintf(f, "    voice %08X: +0=%08X +14=%08X +20=%08X +23=%02X +26=%04X +28=%08X +2C=%08X\n", vv, g32(vv), g32(vv + 0x14), g32(vv + 0x20), vb[0x23], *(const uint16_t *)(vb + 0x26), g32(vv + 0x28), g32(vv + 0x2C)); } }
    }
    fprintf(f, "done queue count (0x244FE0) = %u\n", g32(0x244FE0));
}

/* Symbolised host call chain of the calling thread (recompiled frames are
 * sub_XXXXXXXX = guest functions). Used by LEAN_AUDIO_TRACE_EBO. */
void lean_print_host_chain(const char *tag)
{
    void *frames[40]; USHORT n = RtlCaptureStackBackTrace(1, 40, frames, NULL);
    char line[2048]; int len = snprintf(line, sizeof line, "[LEAN-TRACE] %s thread %lu:", tag, GetCurrentThreadId());
    for (USHORT i = 0; i < n && len < (int)sizeof line - 80; i++) {
        char buf[sizeof(SYMBOL_INFO) + 256]; SYMBOL_INFO *sym = (SYMBOL_INFO *)buf; DWORD64 disp = 0;
        memset(buf, 0, sizeof buf); sym->SizeOfStruct = sizeof(SYMBOL_INFO); sym->MaxNameLen = 255;
        if (SymFromAddr(GetCurrentProcess(), (DWORD64)(uintptr_t)frames[i], &disp, sym) && !strncmp(sym->Name, "sub_", 4))
            len += snprintf(line + len, sizeof line - len, " %s+%llX", sym->Name, (unsigned long long)disp);
    }
    fprintf(stderr, "%s\n", line);
}

/* LEAN_AUDIO_EMIT_LOG=1 (diagnostic): world sound emitters (list 0x243AAC, updated per
 * frame by 0x123A30). Logs the setter 0x1244D0 (this, volume, p2, p3, delay, p5) with the
 * host call chain for the first few calls per emitter, and each emitter play with its
 * fields: +0/+4 sound ids, +8 restart flag, +10/+12, +14 countdown, +16 volume, +18. */
int lean_emit_log_on = -1;
static int emit_on(void) { if (lean_emit_log_on < 0) { const char *e = getenv("LEAN_AUDIO_EMIT_LOG"); lean_emit_log_on = e && atoi(e) > 0; } return lean_emit_log_on; }
static struct { uint32_t p; unsigned sets, plays, chains, flips; int on; uint32_t ids; } emit_seen[1024];
static int emit_slot(uint32_t p) { for (int i = 0; i < 1024; i++) { if (emit_seen[i].p == p) return i; if (!emit_seen[i].p) { emit_seen[i].p = p; return i; } } return -1; }
void lean_emit_set(uint32_t self, uint32_t esp)
{
    if (!emit_on()) return;
    const float *a = (const float *)(g_xbox_mem_offset + (uintptr_t)esp + 4);
    int s = emit_slot(self); unsigned n = s >= 0 ? ++emit_seen[s].sets : 0;
    const uint8_t *b = (const uint8_t *)(g_xbox_mem_offset + (uintptr_t)self);
    int on = a[0] > 0.0f, changed = s >= 0 && (n == 1 || emit_seen[s].on != on);
    if (s >= 0) emit_seen[s].on = on;
    if (changed) emit_seen[s].flips++;
    if (changed && emit_seen[s].flips > 60) changed = (emit_seen[s].flips % 100) == 0;
    {   /* LEAN_AUDIO_EMIT_CHAIN=from,to (diagnostic): chain of every sound-id change or switch-on in that frame range */
        static int init; static long cf = -1, ct = -1; static unsigned chains;
        if (!init) { const char *e = getenv("LEAN_AUDIO_EMIT_CHAIN"); if (e) sscanf(e, "%ld,%ld", &cf, &ct); init = 1; }
        uint32_t ids = g32(self) ^ (g32(self + 4) << 16);
        int idchg = s >= 0 && emit_seen[s].ids != ids;
        if (s >= 0) emit_seen[s].ids = ids;
        if (cf >= 0 && lean_present_count >= cf && lean_present_count <= ct && on && (idchg || changed) && chains++ < 800) {
            char tag[96]; snprintf(tag, sizeof tag, "emitset %08X ids %08X/%08X vol %.3f f=%ld", self, g32(self), g32(self + 4), a[0], lean_present_count);
            lean_print_host_chain(tag);
        }
    }
    if (changed && on && ++emit_seen[s].chains <= 3) { char tag[48]; snprintf(tag, sizeof tag, "emitter %08X switched on f=%ld", self, lean_present_count); lean_print_host_chain(tag); }
    if (changed)
        fprintf(stderr, "[LEAN-EMIT] set #%u f=%ld t=%llu emitter %08X ids %08X/%08X flag8=%u vol %.3f p2 %.3f p3 %.3f delay %.3f p5 %.3f (oldvol %d)\n",
                n, lean_present_count, GetTickCount64(), self, g32(self), g32(self + 4), b[8], a[0], a[1], a[2], a[3], a[4], (int)(int8_t)b[0x16]);
}
void lean_emit_play(uint32_t self, uint32_t elapsed)
{
    if (!emit_on()) return;
    int s = emit_slot(self); unsigned n = s >= 0 ? ++emit_seen[s].plays : 0;
    const uint8_t *b = (const uint8_t *)(g_xbox_mem_offset + (uintptr_t)self);
    if (n <= 40 || (n % 50) == 0)
        fprintf(stderr, "[LEAN-EMIT] play #%u f=%ld t=%llu emitter %08X ids %08X/%08X flag8=%u +10=%u +12=%u count=%d vol=%d +17=%d +18=%u +19=%u elapsed=%u\n",
                n, lean_present_count, GetTickCount64(), self, g32(self), g32(self + 4), b[8], *(const uint16_t *)(b + 0x10), *(const uint16_t *)(b + 0x12),
                (int)*(const int16_t *)(b + 0x14), (int)(int8_t)b[0x16], (int)(int8_t)b[0x17], b[0x18], b[0x19], elapsed);
}
/* LEAN_AUDIO_EMIT_LOG: car state read by the skid sound (0x11F9B0 call sites 0x11FFAF /
 * 0x120021, taken when a wheel surface is 2, 5 or 0xC). Logged every 10th hit per car. */
/* LEAN_AUDIO_EMIT_LOG: hardware write watch on the first car's skid targets (+0x104/+0x108),
 * to find the code that sets them. Debug registers are set from a helper thread. */
static uint32_t skid_watch_car; static uintptr_t skid_watch_host[2]; static volatile LONG skid_watch_hits;
static DWORD skid_watch_tid;
static LONG CALLBACK skid_watch_veh(PEXCEPTION_POINTERS ep)
{
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || !skid_watch_car) return EXCEPTION_CONTINUE_SEARCH;
    DWORD64 dr6 = ep->ContextRecord->Dr6;
    if (!(dr6 & 3)) return EXCEPTION_CONTINUE_SEARCH;
    ep->ContextRecord->Dr6 = 0;
    static float last[2] = { -1.0f, -1.0f };
    for (int k = 0; k < 2; k++) if (dr6 & (1ull << k)) {
        float v = *(const float *)skid_watch_host[k];
        if (v == last[k]) continue;
        last[k] = v;
        if (InterlockedIncrement(&skid_watch_hits) > 400) continue;
        char buf[sizeof(SYMBOL_INFO) + 256]; SYMBOL_INFO *sym = (SYMBOL_INFO *)buf; DWORD64 disp = 0;
        memset(buf, 0, sizeof buf); sym->SizeOfStruct = sizeof(SYMBOL_INFO); sym->MaxNameLen = 255;
        const char *nm = SymFromAddr(GetCurrentProcess(), ep->ContextRecord->Rip, &disp, sym) ? sym->Name : "?";
        fprintf(stderr, "[LEAN-SKIDW] t=%llu car %08X +%X = %.3f by %s+%llX\n", GetTickCount64(), skid_watch_car, k ? 0x108 : 0x104, v, nm, (unsigned long long)disp);
        if (skid_watch_hits <= 12) { char tag[64]; snprintf(tag, sizeof tag, "skid +%X write", k ? 0x108 : 0x104); lean_print_host_chain(tag); }
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}
static DWORD WINAPI skid_watch_arm(LPVOID p)
{
    HANDLE t = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, skid_watch_tid);
    (void)p;
    if (!t) return 1;
    SuspendThread(t);
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(t, &c)) {
        c.Dr0 = skid_watch_host[0]; c.Dr1 = skid_watch_host[1];
        c.Dr7 = (c.Dr7 & ~0xFF000Full) | 1 | 4 | (0x1ull << 16) | (0x3ull << 18) | (0x1ull << 20) | (0x3ull << 22); /* L0,L1 write, 4 bytes */
        SetThreadContext(t, &c);
    }
    ResumeThread(t); CloseHandle(t);
    return 0;
}
static void skid_watch_start(uint32_t car)
{
    if (skid_watch_car) return;
    skid_watch_car = car;
    skid_watch_host[0] = (uintptr_t)(g_xbox_mem_offset + (uintptr_t)car + 0x104);
    skid_watch_host[1] = (uintptr_t)(g_xbox_mem_offset + (uintptr_t)car + 0x108);
    skid_watch_tid = GetCurrentThreadId();
    AddVectoredExceptionHandler(1, skid_watch_veh);
    HANDLE h = CreateThread(NULL, 0, skid_watch_arm, NULL, 0, NULL);
    if (h) { WaitForSingleObject(h, 2000); CloseHandle(h); }
    fprintf(stderr, "[LEAN-SKIDW] watching car %08X (+104/+108)\n", car);
}
void lean_skid_state(uint32_t car, int site, float factor)
{
    if (!emit_on()) return;
    skid_watch_start(car);
    int s = emit_slot(car ^ 0x80000000u); unsigned n = s >= 0 ? ++emit_seen[s].plays : 0;
    if (n > 3 && n % 10) return;
    const float *f = (const float *)(g_xbox_mem_offset + (uintptr_t)car);
    fprintf(stderr, "[LEAN-SKID] t=%llu car %08X site %d hit #%u factor %.3f surf %u %u %u %u target104 %.3f target108 %.3f smooth10C %.3f smooth110 %.3f scale114 %.3f\n",
            GetTickCount64(), car, site, n, factor, g32(car + 0xD4), g32(car + 0xD8), g32(car + 0xDC), g32(car + 0xE0),
            f[0x104 / 4], f[0x108 / 4], f[0x10C / 4], f[0x110 / 4], f[0x114 / 4]);
}
/* LEAN_AUDIO_EMIT_LOG: wheel-slip averages in 0x6A840 (front [esp+2C], rear [esp+1C]) and the
 * vcall +0x48 result that selects the "any slip -> 1.0" branch. Logged on change per car. */
void lean_slip_state(uint32_t car, uint32_t esp, uint32_t v48, uint32_t ebx)
{
    if (!emit_on()) return;
    static struct { uint32_t car; float f, r; uint32_t v; } last[64]; static unsigned lines;
    const float *s = (const float *)(g_xbox_mem_offset + (uintptr_t)esp);
    float f = s[0x2C / 4], r = s[0x1C / 4];
    int i; for (i = 0; i < 64 && last[i].car && last[i].car != car; i++) ;
    if (i == 64) return;
    if (last[i].car == car && last[i].f == f && last[i].r == r && last[i].v == v48) return;
    last[i].car = car; last[i].f = f; last[i].r = r; last[i].v = v48;
    if (++lines > 3000) return;
    uint32_t vt = g32(car);
    fprintf(stderr, "[LEAN-SLIP] t=%llu car %08X vt %08X slip+108 fn %08X v48 %u ebx %u front %.3f rear %.3f\n",
            GetTickCount64(), car, vt, g32(vt + 0x108), v48, ebx, f, r);
}
/* LEAN_AUDIO_EMIT_LOG: per-frame volume the emitter update passes to 0x13CB40 for the skid
 * sound (ids 0x4C/1), logged on change, with the play handle. */
void lean_emit_vol(uint32_t self, uint32_t vol)
{
    if (!emit_on() || g32(self) != 0x4C || g32(self + 4) != 1) return;
    static struct { uint32_t p, v; } last[32]; static unsigned lines;
    int i; for (i = 0; i < 32 && last[i].p && last[i].p != self; i++) ;
    if (i == 32 || (last[i].p == self && last[i].v == vol)) return;
    last[i].p = self; last[i].v = vol;
    if (++lines > 3000) return;
    fprintf(stderr, "[LEAN-EVOL] t=%llu emitter %08X handle %08X game vol %d/127 pitch %u\n", GetTickCount64(), self, g32(self + 0xC), (int)vol, *(const uint16_t *)(g_xbox_mem_offset + (uintptr_t)self + 0x10));
}

/* LEAN_AUDIO_LEAD=n (off by default): the game's sound init 0x13D5E0 sets its software
 * mixer lead [0x244CBC] to 20 ms of samples (960 at 48 kHz) inside a 50 ms (2400) ring.
 * A larger lead tolerates a late mixer call without writing behind the play cursor.
 * Kept below 2000 so it stays clear of the ring wrap. */
uint32_t lean_audio_lead(uint32_t game_lead)
{
    const char *e = getenv("LEAN_AUDIO_LEAD");
    if (!e || !*e) return game_lead;
    long n = strtol(e, NULL, 10);
    if (n < (long)game_lead || n > 2000) n = (long)game_lead;
    fprintf(stderr, "[LEAN-SND] mixer lead %u -> %ld samples\n", game_lead, n);
    return (uint32_t)n;
}

/* LEAN_AUDIO_GAMEVOL=1 (diagnostic): the game's own per-voice volume changes (0x13CB40,
 * 0-127 scale, voice record 0x244F3C + idx*0x88) with the present count, plus which voice
 * each stream slot plays (slot table 0x244BA8, voice id at slot+8), so music ducking by
 * the game itself can be told from a starving stream. */
void lean_game_vol_note(uint32_t idx, uint32_t vol)
{
    static int on = -1; static unsigned lines;
    if (on < 0) { const char *e = getenv("LEAN_AUDIO_GAMEVOL"); on = e && e[0] == '1'; }
    if (!on || ++lines > 20000) return;
    char slots[160]; int len = 0; unsigned n = *(const uint8_t *)(g_xbox_mem_offset + 0x244D0Fu);
    for (unsigned s2 = 0; s2 < n && s2 < 8 && len < 140; s2++) { uint32_t slot = g32(0x244BA8 + 4 * s2); if (slot) len += snprintf(slots + len, sizeof slots - len, " %u:v%u/st%u", s2, g32(slot + 8), *(const uint8_t *)(g_xbox_mem_offset + (uintptr_t)slot + 0x10)); }
    slots[len] = 0;
    fprintf(stderr, "[LEAN-GAMEVOL] f=%ld voice %u vol %u slots%s\n", lean_present_count, idx, vol, slots);
    {   /* LEAN_AUDIO_GAMEVOL_CHAIN=voice,from,to: call chain for that voice's changes in that present range */
        static int init; static long cv = -1, cf = 0, ct = 0; static unsigned chains;
        if (!init) { const char *e = getenv("LEAN_AUDIO_GAMEVOL_CHAIN"); if (e) sscanf(e, "%ld,%ld,%ld", &cv, &cf, &ct); init = 1; }
        if ((long)idx == cv && lean_present_count >= cf && lean_present_count <= ct && chains++ < 60) {
            char tag[64]; snprintf(tag, sizeof tag, "gamevol voice %u -> %u f=%ld", idx, vol, lean_present_count);
            lean_print_host_chain(tag);
        }
    }
}

/* LEAN_STAGE_TIMING=1 (diagnostic, off by default): where a game frame's time goes.
 * Stages are fed with QueryPerformanceCounter time from lean_gpu.c (decode, D3D submit,
 * flip wait, readback) and kernel_bridge.c (kernel waits incl. critical sections, file
 * reads). Time on the game thread (the thread that presents game frames) and on other
 * threads is kept apart; "guest" = game-frame wall time minus the game thread's stages.
 * Every 5 s: per-frame averages, then the same breakdown averaged over the slowest 1%. */
enum { ST_DECODE, ST_SUBMIT, ST_FLIPWAIT, ST_READBACK, ST_KWAIT, ST_READ, ST_N };
static const char *st_name[ST_N] = { "decode", "submit", "flipwait", "readback", "kwait", "fileread" };
static int st_on = -1;
static DWORD st_game_tid;
static volatile LONG64 st_game[ST_N], st_other[ST_N];
int lean_stage_on(void)
{
    if (st_on < 0) { const char *e = getenv("LEAN_STAGE_TIMING"); st_on = e && e[0] == '1';
        if (st_on) fprintf(stderr, "[LEAN-STAGE] per-frame stage timing on\n"); }
    return st_on;
}
uint64_t lean_stage_t0(void) { LARGE_INTEGER q; if (!lean_stage_on()) return 0; QueryPerformanceCounter(&q); return (uint64_t)q.QuadPart; }
void lean_stage_add(int stage, uint64_t t0)
{
    LARGE_INTEGER q;
    if (!t0 || stage < 0 || stage >= ST_N) return;
    QueryPerformanceCounter(&q);
    InterlockedAdd64(GetCurrentThreadId() == st_game_tid ? &st_game[stage] : &st_other[stage], (LONG64)((uint64_t)q.QuadPart - t0));
}
void lean_stage_add_ticks(int stage, uint64_t ticks)
{
    if (!lean_stage_on() || stage < 0 || stage >= ST_N) return;
    InterlockedAdd64(GetCurrentThreadId() == st_game_tid ? &st_game[stage] : &st_other[stage], (LONG64)ticks);
}
#define ST_MAXF 2048
static void stage_frame(void)   /* called once per game frame on the game thread */
{
    static LARGE_INTEGER hz, last, win0;
    static float rec[ST_MAXF][2 * ST_N + 2];   /* wall, guest, game stages, other stages (ms) */
    static int n;
    LARGE_INTEGER now;
    if (!lean_stage_on()) return;
    QueryPerformanceCounter(&now);
    if (!hz.QuadPart) { QueryPerformanceFrequency(&hz); last = win0 = now; st_game_tid = GetCurrentThreadId(); return; }
    double k = 1000.0 / hz.QuadPart, wall = (now.QuadPart - last.QuadPart) * k, sum = 0;
    last = now;
    float *r = rec[n < ST_MAXF ? n : ST_MAXF - 1];
    for (int s = 0; s < ST_N; s++) {
        double g = InterlockedExchange64(&st_game[s], 0) * k, o = InterlockedExchange64(&st_other[s], 0) * k;
        r[2 + s] = (float)g; r[2 + ST_N + s] = (float)o;
        sum += g;   /* stages are fed exclusive of each other (decode excludes submit and flip waits) */
    }
    r[0] = (float)wall; r[1] = (float)(wall - sum);
    if (n < ST_MAXF) n++;
    if ((now.QuadPart - win0.QuadPart) * k >= 5000.0 && n > 10) {
        double avg[2 * ST_N + 2] = { 0 }, slow[2 * ST_N + 2] = { 0 };
        static float walls[ST_MAXF]; int idx[ST_MAXF];
        for (int i = 0; i < n; i++) { walls[i] = rec[i][0]; idx[i] = i; for (int c = 0; c < 2 * ST_N + 2; c++) avg[c] += rec[i][c] / n; }
        int m = n / 100 > 0 ? n / 100 : 1;   /* slowest 1% by wall time (partial selection) */
        for (int a = 0; a < m; a++) { int b = a; for (int i = a + 1; i < n; i++) if (walls[idx[i]] > walls[idx[b]]) b = i; int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
        for (int a = 0; a < m; a++) for (int c = 0; c < 2 * ST_N + 2; c++) slow[c] += rec[idx[a]][c] / m;
        for (int pass = 0; pass < 2; pass++) {
            double *v = pass ? slow : avg; char line[640]; int l;
            l = snprintf(line, sizeof line, "[LEAN-STAGE] %s %d frames: wall %.2f ms = guest %.2f", pass ? "slowest1%" : "5s avg", pass ? m : n, v[0], v[1]);
            for (int s = 0; s < ST_N; s++) l += snprintf(line + l, sizeof line - l, " + %s %.2f", st_name[s], v[2 + s]);
            l += snprintf(line + l, sizeof line - l, " | other threads:");
            for (int s = 0; s < ST_N; s++) l += snprintf(line + l, sizeof line - l, " %s %.2f", st_name[s], v[2 + ST_N + s]);
            fprintf(stderr, "%s (f=%ld)\n", line, lean_present_count);
        }
        n = 0; win0 = now;
    }
}
