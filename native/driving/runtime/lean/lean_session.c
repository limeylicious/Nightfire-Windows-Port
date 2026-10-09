/* Guided play-session capture (lean build). Off unless LEAN_SESSION_CAPTURE=1
 * and LEAN_SESSION_DIR names the session folder (run-lean-guided.py sets both).
 *
 * In the session folder it writes:
 *   stats.csv     once a second: presented FPS, game FPS, audio level and
 *                 XAudio2 queue/drop/glitch counters
 *   events.txt    session start, marks, freezes, crash/abort
 *   marks.txt     one line per F9 press with a state snapshot; mark-NN.bmp is
 *                 the game picture at that moment
 *   crash.txt + crash.dmp   unhandled exception: code, address, function,
 *                 host/guest call chain, guest registers, small minidump
 *   abort.txt + abort.dmp   abort()/assert path
 *   freeze-N.dmp  minidump when the freeze monitor (lean_hang.c) fires
 * Test-only: LEAN_TEST_CRASH=<s> / LEAN_TEST_FREEZE=<s> / LEAN_TEST_ABORT=<s>
 * crash, freeze or abort() the game thread that many seconds after the first
 * frame. */
#include <windows.h>
#include <dbghelp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma comment(lib, "dbghelp.lib")

extern volatile LONG lean_present_count;    /* game frames presented (lean_hang.c) */
extern volatile LONG lean_screen_presents;  /* frames shown, incl. in-between (lean_present.inc) */
extern volatile int lean_audio_peak, lean_audio_nonsilent;
extern volatile float lean_audio_rms;
extern void xa2_get_stats(int *queued, int *dropped, int *submitted);
extern void xa2_get_health(int *glitches, int *min_queued);
extern void lean_apu_state_line(char *buf, size_t n);

volatile LONG lean_mark_request;            /* mark number whose picture is still to be saved */
volatile ULONGLONG lean_mark_flash_until;   /* window title shows "MARK n saved" until then */
volatile LONG lean_mark_count;

static int s_on = -1;
static char s_dir[MAX_PATH];
static FILE *s_events, *s_marks;
static ULONGLONG s_t0;
static volatile LONG s_pfps10, s_gfps10;    /* last second's rates x10 */
static volatile LONG s_freezes, s_crashed;
static CRITICAL_SECTION s_lock;

static ULONGLONG ms_now(void) { return GetTickCount64() - s_t0; }

static void event(const char *fmt, const char *a, long b)
{
    if (!s_events) return;
    EnterCriticalSection(&s_lock);
    fprintf(s_events, "%10.3f  ", ms_now() / 1000.0);
    fprintf(s_events, fmt, a, b);
    fputc('\n', s_events);
    fflush(s_events);
    LeaveCriticalSection(&s_lock);
}

static void minidump(const char *name, PEXCEPTION_POINTERS ep)
{
    char path[MAX_PATH];
    HANDLE f;
    MINIDUMP_EXCEPTION_INFORMATION mei;
    snprintf(path, sizeof path, "%s\\%s", s_dir, name);
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    mei.ThreadId = GetCurrentThreadId(); mei.ExceptionPointers = ep; mei.ClientPointers = FALSE;
    /* Threads, stacks and the memory they point at: a few MB, not the 64 MB
     * guest RAM or the GPU heaps. */
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f,
                      (MINIDUMP_TYPE)(MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules |
                                      MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
                      ep ? &mei : NULL, NULL, NULL);
    CloseHandle(f);
}

static void symbol(FILE *f, DWORD64 pc)
{
    char buf[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO *sym = (SYMBOL_INFO *)buf;
    DWORD64 disp = 0;
    memset(buf, 0, sizeof buf); sym->SizeOfStruct = sizeof(SYMBOL_INFO); sym->MaxNameLen = 255;
    if (SymFromAddr(GetCurrentProcess(), pc, &disp, sym)) fprintf(f, "%s+0x%llX", sym->Name, (unsigned long long)disp);
    else fprintf(f, "%016llX", (unsigned long long)pc);
}

/* Host call chain from a context; recompiled frames are named sub_XXXXXXXX
 * after the guest function, so this is also the guest call chain. A deep
 * chain (runaway recursion) prints its innermost 40 frames, the frame count,
 * then the outermost 24, which show where the recursion started. */
static int unwind_step(CONTEXT *c)
{
    DWORD64 base = 0;
    PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(c->Rip, &base, NULL);
    if (fn) { PVOID hd; DWORD64 ef; RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c->Rip, fn, c, &hd, &ef, NULL); return 1; }
    {   DWORD64 ret = 0; SIZE_T got = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)c->Rsp, &ret, 8, &got) || got != 8) return 0;
        c->Rip = ret; c->Rsp += 8; return 1; }
}
static void chain(FILE *f, const CONTEXT *start)
{
    static CONTEXT c;   /* static: may run with little stack to spare */
    static DWORD64 tail[24];
    unsigned n = 0, k;
    c = *start;
    for (; n < 40 && c.Rip; n++) {
        fprintf(f, "  #%-2u ", n); symbol(f, c.Rip); fputc('\n', f);
        if (!unwind_step(&c)) return;
    }
    for (; n < 2000000 && c.Rip; n++) {
        tail[n % 24] = c.Rip;
        if (!unwind_step(&c)) { n++; break; }
    }
    if (n <= 40) return;
    fprintf(f, "  ... %u frames in all; outermost:\n", n);
    for (k = n > 64 ? n - 24 : 40; k < n; k++) { fprintf(f, "  #%-2u ", k); symbol(f, tail[k % 24]); fputc('\n', f); }
}

static const char *code_name(DWORD c)
{
    switch (c) {
    case EXCEPTION_ACCESS_VIOLATION: return "access violation";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
    case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
    case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
    case EXCEPTION_BREAKPOINT: return "breakpoint";
    default: return "exception";
    }
}

int lean_session_on(void) { return s_on > 0; }

/* Crash report writer. The report is written by a thread started at session
 * start, not on the faulting thread: after a stack overflow the faulting
 * thread has only the guard area left, and writing the report there crashed
 * the handler itself (an empty crash.txt and no dump). The faulting thread
 * only copies the exception and waits. */
static EXCEPTION_RECORD s_cer;
static CONTEXT s_cctx;
static EXCEPTION_POINTERS s_cep;
static uint32_t s_cgr[7];
static DWORD s_ctid;
static HANDLE s_crash_req, s_crash_done;

static void crash_write(void)
{
    char path[MAX_PATH];
    FILE *f;
    EXCEPTION_RECORD *er = &s_cer;
    fflush(stderr);
    snprintf(path, sizeof path, "%s\\crash.txt", s_dir);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "Crash at %.3f s (game frame %ld)\n", ms_now() / 1000.0, lean_present_count);
        fprintf(f, "Exception 0x%08lX (%s) at ", er->ExceptionCode, code_name(er->ExceptionCode));
        symbol(f, (DWORD64)(uintptr_t)er->ExceptionAddress);
        fprintf(f, " thread %lu\n", s_ctid);
        if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
            fprintf(f, "  %s of host address 0x%llX\n", er->ExceptionInformation[0] == 1 ? "write" : er->ExceptionInformation[0] == 8 ? "execute" : "read",
                    (unsigned long long)er->ExceptionInformation[1]);
        fprintf(f, "Guest registers: eax=%08X ecx=%08X edx=%08X ebx=%08X esi=%08X edi=%08X esp=%08X\n",
                s_cgr[0], s_cgr[1], s_cgr[2], s_cgr[3], s_cgr[4], s_cgr[5], s_cgr[6]);
        fprintf(f, "Host call chain (sub_XXXXXXXX = recompiled guest function):\n");
        fflush(f);
        chain(f, &s_cctx);
        fclose(f);
    }
    event("crash: %s %ld", code_name(er->ExceptionCode), (long)er->ExceptionCode);
    {   /* minidump() names the calling thread; here that is the writer. */
        HANDLE h;
        MINIDUMP_EXCEPTION_INFORMATION mei;
        snprintf(path, sizeof path, "%s\\crash.dmp", s_dir);
        h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            mei.ThreadId = s_ctid; mei.ExceptionPointers = &s_cep; mei.ClientPointers = FALSE;
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), h,
                              (MINIDUMP_TYPE)(MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules |
                                              MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
                              &mei, NULL, NULL);
            CloseHandle(h);
        }
    }
}

static DWORD WINAPI crash_writer(LPVOID p)
{
    (void)p;
    WaitForSingleObject(s_crash_req, INFINITE);
    crash_write();
    SetEvent(s_crash_done);
    return 0;
}

/* Unhandled exception (called from main.c's filter on the faulting thread,
 * with that thread's guest registers). Uses almost no stack. */
void lean_session_crash(PEXCEPTION_POINTERS ep, const uint32_t gr[7])
{
    if (s_on <= 0 || InterlockedExchange(&s_crashed, 1)) return;
    s_cer = *ep->ExceptionRecord; s_cer.ExceptionRecord = NULL;
    s_cctx = *ep->ContextRecord;
    s_cep.ExceptionRecord = &s_cer; s_cep.ContextRecord = &s_cctx;
    s_cgr[0] = gr[0]; s_cgr[1] = gr[1]; s_cgr[2] = gr[2]; s_cgr[3] = gr[3]; s_cgr[4] = gr[4]; s_cgr[5] = gr[5]; s_cgr[6] = gr[6];
    s_ctid = GetCurrentThreadId();
    if (s_crash_req && s_crash_done) { SetEvent(s_crash_req); WaitForSingleObject(s_crash_done, 30000); }
    else crash_write();
}

static void on_abort(int sig)
{
    char path[MAX_PATH];
    FILE *f;
    (void)sig;
    if (s_on <= 0 || InterlockedExchange(&s_crashed, 1)) return;
    fflush(stderr);
    snprintf(path, sizeof path, "%s\\abort.txt", s_dir);
    f = fopen(path, "w");
    if (f) {
        CONTEXT c;
        RtlCaptureContext(&c);
        fprintf(f, "abort() at %.3f s (game frame %ld) thread %lu\n", ms_now() / 1000.0, lean_present_count, GetCurrentThreadId());
        fprintf(f, "Host call chain (sub_XXXXXXXX = recompiled guest function):\n");
        chain(f, &c);
        fclose(f);
    }
    event("abort%s %ld", "", 0);
    minidump("abort.dmp", NULL);
}

/* Freeze monitor (lean_hang.c) fired after its text dump. Keeps 2 dumps. */
void lean_session_freeze(unsigned secs, long presents)
{
    char name[32];
    LONG n;
    if (s_on <= 0) return;
    n = InterlockedIncrement(&s_freezes);
    {   char t[48]; snprintf(t, sizeof t, "%u s at game frame ", secs);
        event("freeze: no game frame for %s%ld (could also be a long loading screen; check the picture)", t, presents); }

    if (n > 2) return;
    snprintf(name, sizeof name, "freeze-%ld.dmp", n);
    minidump(name, NULL);
}

/* F9 from the game window (driving_present201.c). */
void lean_session_mark(void)
{
    char apu[256];
    LONG n;
    int queued = 0, dropped = 0, submitted = 0, glitches = 0, minq = 0;
    if (s_on <= 0) return;
    n = InterlockedIncrement(&lean_mark_count);
    lean_apu_state_line(apu, sizeof apu);
    xa2_get_stats(&queued, &dropped, &submitted); xa2_get_health(&glitches, &minq);
    if (s_marks) {
        EnterCriticalSection(&s_lock);
        fprintf(s_marks, "mark %ld  t=%.3f s  game_frame=%ld  presented_fps=%.1f game_fps=%.1f  audio peak=%d rms=%.1f nonsilent=%d/188 queued=%d dropped=%d glitches=%d  picture=mark-%02ld.bmp\n"
                         "        apu: %s\n",
                n, ms_now() / 1000.0, lean_present_count, s_pfps10 / 10.0, s_gfps10 / 10.0,
                lean_audio_peak, lean_audio_rms, lean_audio_nonsilent, queued, dropped, glitches, n, apu);
        fflush(s_marks);
        LeaveCriticalSection(&s_lock);
    }
    event("mark %s%ld", "", (long)n);
    InterlockedExchange(&lean_mark_request, n);
    lean_mark_flash_until = GetTickCount64() + 2000;
    MessageBeep(MB_ICONASTERISK);
}

/* lean_gpu.c: the picture for mark n (640x480 BGRA). */
void lean_session_mark_picture(long n, const uint32_t *pix)
{
    char path[MAX_PATH];
    FILE *f;
    unsigned char h[54] = {0};
    uint32_t size = 54 + 640 * 480 * 4, off = 54, dib = 40, w = 640;
    int32_t ht = -480;
    uint16_t planes = 1, bits = 32;
    snprintf(path, sizeof path, "%s\\mark-%02ld.bmp", s_dir, n);
    f = fopen(path, "wb");
    if (!f) return;
    memcpy(h, "BM", 2); memcpy(h + 2, &size, 4); memcpy(h + 10, &off, 4); memcpy(h + 14, &dib, 4);
    memcpy(h + 18, &w, 4); memcpy(h + 22, &ht, 4); memcpy(h + 26, &planes, 2); memcpy(h + 28, &bits, 2);
    fwrite(h, 1, 54, f); fwrite(pix, 4, 640 * 480, f); fclose(f);
}

static DWORD WINAPI sampler(LPVOID p)
{
    char path[MAX_PATH];
    FILE *csv;
    LONG lp = lean_screen_presents, lg = lean_present_count;
    ULONGLONG lt = GetTickCount64();
    (void)p;
    snprintf(path, sizeof path, "%s\\stats.csv", s_dir);
    csv = fopen(path, "w");
    if (!csv) return 0;
    fprintf(csv, "t_s,presented_fps,game_fps,audio_peak,audio_rms,audio_nonsilent_of_188,xa2_queued,xa2_dropped_total,xa2_glitches_total\n");
    for (;;) {
        Sleep(1000);
        ULONGLONG t = GetTickCount64();
        LONG sp = lean_screen_presents, gp = lean_present_count;
        double dt = (t - lt) / 1000.0;
        int queued = 0, dropped = 0, submitted = 0, glitches = 0, minq = 0;
        if (dt <= 0) continue;
        s_pfps10 = (LONG)((sp - lp) * 10 / dt); s_gfps10 = (LONG)((gp - lg) * 10 / dt);
        xa2_get_stats(&queued, &dropped, &submitted); xa2_get_health(&glitches, &minq);
        fprintf(csv, "%.1f,%.1f,%.1f,%d,%.1f,%d,%d,%d,%d\n", ms_now() / 1000.0, s_pfps10 / 10.0, s_gfps10 / 10.0,
                lean_audio_peak, lean_audio_rms, lean_audio_nonsilent, queued, dropped, glitches);
        fflush(csv);
        lp = sp; lg = gp; lt = t;
    }
}

/* LEAN_TEST_CRASH_KIND=stack: the forced crash is a stack overflow. */
static __declspec(noinline) int test_recurse(volatile int d)
{
    volatile char pad[512];
    pad[0] = (char)d; pad[511] = (char)d;
    if (d > 0x7FFFFFF0) return 0;
    return test_recurse(d + 1) + pad[(unsigned)d & 511];
}

/* Test-only hooks, called by lean_gpu.c for every presented game frame. */
void lean_session_test_tick(void)
{
    static int init;
    static double crash_s, freeze_s, abort_s;
    static ULONGLONG t0;
    if (!init) {
        const char *c = getenv("LEAN_TEST_CRASH"), *z = getenv("LEAN_TEST_FREEZE");
        const char *ab = getenv("LEAN_TEST_ABORT");
        init = 1; crash_s = c ? atof(c) : 0; freeze_s = z ? atof(z) : 0; abort_s = ab ? atof(ab) : 0; t0 = GetTickCount64();
        if (crash_s > 0) fprintf(stderr, "[LEAN-SESSION] TEST: forced crash in %.0f s\n", crash_s);
        if (freeze_s > 0) fprintf(stderr, "[LEAN-SESSION] TEST: forced freeze in %.0f s\n", freeze_s);
    }
    if (!crash_s && !freeze_s && !abort_s) return;
    double el = (GetTickCount64() - t0) / 1000.0;
    if (crash_s > 0 && el >= crash_s) {
        fprintf(stderr, "[LEAN-SESSION] TEST: forcing a crash now\n"); fflush(stderr);
        if (getenv("LEAN_TEST_CRASH_KIND") && getenv("LEAN_TEST_CRASH_KIND")[0] == 's') test_recurse(0);   /* =stack: runaway recursion */
        *(volatile int *)(uintptr_t)8 = 1;
    }
    if (abort_s > 0 && el >= abort_s) {
        fprintf(stderr, "[LEAN-SESSION] TEST: calling abort() now\n"); fflush(stderr);
        abort();
    }
    if (freeze_s > 0 && el >= freeze_s) {
        fprintf(stderr, "[LEAN-SESSION] TEST: forcing a freeze now (game thread stops)\n"); fflush(stderr);
        for (;;) Sleep(1000);
    }
}

/* Called once from WinMain, after the symbol handler is set up. */
void lean_session_init(void)
{
    const char *on = getenv("LEAN_SESSION_CAPTURE"), *dir = getenv("LEAN_SESSION_DIR");
    char path[MAX_PATH];
    s_on = on && on[0] == '1' && dir && *dir;
    if (!s_on) return;
    InitializeCriticalSection(&s_lock);
    s_t0 = GetTickCount64();
    snprintf(s_dir, sizeof s_dir, "%s", dir);
    CreateDirectoryA(s_dir, NULL);
    snprintf(path, sizeof path, "%s\\events.txt", s_dir); s_events = fopen(path, "w");
    snprintf(path, sizeof path, "%s\\marks.txt", s_dir); s_marks = fopen(path, "w");
    signal(SIGABRT, on_abort);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    { HANDLE h = CreateThread(NULL, 0, sampler, NULL, 0, NULL); if (h) CloseHandle(h); }
    s_crash_req = CreateEventA(NULL, FALSE, FALSE, NULL); s_crash_done = CreateEventA(NULL, TRUE, FALSE, NULL);
    { HANDLE h = s_crash_req && s_crash_done ? CreateThread(NULL, 1 << 20, crash_writer, NULL, 0, NULL) : NULL;
      if (h) CloseHandle(h); else { s_crash_req = s_crash_done = NULL; } }
    event("session start (pid %s%ld)", "", (long)GetCurrentProcessId());
    fprintf(stderr, "[LEAN-SESSION] capture on: %s (F9 = mark)\n", s_dir);
}
