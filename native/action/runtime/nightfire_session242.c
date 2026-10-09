/* Checkpoint 242: play-session crash capture (diagnostic only).
 *
 * Off unless NIGHTFIRE_SESSION242=1 and NIGHTFIRE_SESSION_DIR names a folder
 * (show-nightfire-session242.cmd sets both). It never writes guest memory,
 * never changes game logic or timing, and adds no work to the game threads
 * beyond one counter increment per published frame.
 *
 * In the session folder:
 *   events.txt    session start, crash/abort, freezes and recoveries
 *   stats.csv     once a second: frames published to the window
 *   crash.txt + crash.dmp   unhandled exception: code, faulting function,
 *                 host call chain (sub_XXXXXXXX frames are guest functions),
 *                 guest registers, small minidump
 *   abort.txt + abort.dmp   abort()/assert path
 *   freeze.txt + freeze-N.dmp   no frame published for NIGHTFIRE_HANG_SECS
 *                 seconds (default 15) after NIGHTFIRE_HANG_ARM frames
 *                 (default 100): every thread's call chain, at most 2 dumps
 * Same layout as the Driving lean build's session capture (lean_session.c).
 * Test-only: NIGHTFIRE_TEST_CRASH / _FREEZE / _ABORT = seconds after the first
 * published frame, to prove each report is written. */
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile LONG nightfire_session242_frames;   /* incremented by fb_present.c per published frame */

static int s_on = -1;
static char s_dir[MAX_PATH];
static FILE *s_events;
static ULONGLONG s_t0;
static volatile LONG s_crashed, s_freezes;
static CRITICAL_SECTION s_lock;

static ULONGLONG ms_now(void) { return GetTickCount64() - s_t0; }

static void event(const char *fmt, ...)
{
    va_list ap;
    if (!s_events) return;
    EnterCriticalSection(&s_lock);
    fprintf(s_events, "%10.3f  ", ms_now() / 1000.0);
    va_start(ap, fmt); vfprintf(s_events, fmt, ap); va_end(ap);
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
    /* Threads, stacks and memory they reference: a few MB, not guest RAM. */
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

#define CHAIN_MAX 48
static int unwind(CONTEXT c, DWORD64 *pcs)
{
    int n = 0;
    while (n < CHAIN_MAX && c.Rip) {
        DWORD64 base = 0;
        PRUNTIME_FUNCTION fn;
        pcs[n++] = c.Rip;
        fn = RtlLookupFunctionEntry(c.Rip, &base, NULL);
        if (fn) { PVOID hd; DWORD64 ef; RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c.Rip, fn, &c, &hd, &ef, NULL); }
        else {   /* leaf: the return address is at the stack pointer */
            DWORD64 ret = 0; SIZE_T got = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)c.Rsp, &ret, 8, &got) || got != 8) break;
            c.Rip = ret; c.Rsp += 8;
        }
    }
    return n;
}

static void print_chain(FILE *f, const DWORD64 *pcs, int n)
{
    for (int i = 0; i < n; i++) { fprintf(f, "  #%-2d ", i); symbol(f, pcs[i]); fputc('\n', f); }
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

int nightfire_session242_on(void) { return s_on > 0; }

/* Crash report writer (checkpoint 243). The report is written by a thread
 * started with the session, not on the faulting thread: after a stack overflow
 * that thread has only the guard area left, and writing the report there
 * crashes the handler itself (empty crash.txt, no dump; seen in Driving). The
 * faulting thread only copies the exception and waits. A runaway recursion
 * prints its innermost 48 frames, the frame count and the outermost 24. */
static EXCEPTION_RECORD s_cer;
static CONTEXT s_cctx;
static EXCEPTION_POINTERS s_cep;
static uint32_t s_cgr[7];
static DWORD s_ctid;
static HANDLE s_crash_req, s_crash_done;

static void crash_tail(FILE *f, int shown)
{
    static CONTEXT c;
    static DWORD64 tail[24];
    unsigned n = 0, k;
    c = s_cctx;
    for (; n < 2000000 && c.Rip; n++) {
        DWORD64 base = 0;
        PRUNTIME_FUNCTION fn;
        tail[n % 24] = c.Rip;
        fn = RtlLookupFunctionEntry(c.Rip, &base, NULL);
        if (fn) { PVOID hd; DWORD64 ef; RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c.Rip, fn, &c, &hd, &ef, NULL); }
        else {
            DWORD64 ret = 0; SIZE_T got = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(uintptr_t)c.Rsp, &ret, 8, &got) || got != 8) { n++; break; }
            c.Rip = ret; c.Rsp += 8;
        }
    }
    if (n <= (unsigned)shown) return;
    fprintf(f, "  ... %u frames in all; outermost:\n", n);
    for (k = n > (unsigned)shown + 24 ? n - 24 : (unsigned)shown; k < n; k++) { fprintf(f, "  #%-2u ", k); symbol(f, tail[k % 24]); fputc('\n', f); }
}

static void crash_write(void)
{
    char path[MAX_PATH];
    FILE *f;
    EXCEPTION_RECORD *er = &s_cer;
    fflush(stderr);
    snprintf(path, sizeof path, "%s\\crash.txt", s_dir);
    f = fopen(path, "w");
    if (f) {
        static DWORD64 pcs[CHAIN_MAX];
        int n = unwind(s_cctx, pcs);
        fprintf(f, "Crash at %.3f s (published frame %ld)\n", ms_now() / 1000.0, nightfire_session242_frames);
        fprintf(f, "Exception 0x%08lX (%s) at ", er->ExceptionCode, code_name(er->ExceptionCode));
        symbol(f, (DWORD64)(uintptr_t)er->ExceptionAddress);
        fprintf(f, " thread %lu\n", s_ctid);
        if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
            fprintf(f, "  %s of host address 0x%llX\n",
                    er->ExceptionInformation[0] == 1 ? "write" : er->ExceptionInformation[0] == 8 ? "execute" : "read",
                    (unsigned long long)er->ExceptionInformation[1]);
        fprintf(f, "Guest registers: eax=%08X ecx=%08X edx=%08X ebx=%08X esi=%08X edi=%08X esp=%08X\n",
                s_cgr[0], s_cgr[1], s_cgr[2], s_cgr[3], s_cgr[4], s_cgr[5], s_cgr[6]);
        fprintf(f, "Host call chain (sub_XXXXXXXX = recompiled guest function):\n");
        print_chain(f, pcs, n);
        if (n == CHAIN_MAX) crash_tail(f, n);
        fclose(f);
    }
    event("crash: %s (code 0x%08lX)", code_name(er->ExceptionCode), (unsigned long)er->ExceptionCode);
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

/* Unhandled exception: called from main.c's top-level filter on the faulting
 * thread, with that thread's guest registers. Uses almost no stack. */
void nightfire_session242_crash(PEXCEPTION_POINTERS ep, const uint32_t gr[7])
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
        DWORD64 pcs[CHAIN_MAX];
        RtlCaptureContext(&c);
        fprintf(f, "abort() at %.3f s (published frame %ld) thread %lu\n",
                ms_now() / 1000.0, nightfire_session242_frames, GetCurrentThreadId());
        fprintf(f, "Host call chain (sub_XXXXXXXX = recompiled guest function):\n");
        print_chain(f, pcs, unwind(c, pcs));
        fclose(f);
    }
    event("abort");
    minidump("abort.dmp", NULL);
}

/* Every other thread's call chain. Each thread is suspended only while its
 * stack is unwound; symbols are looked up after it resumes, so a suspended
 * thread holding the heap or the symbol lock cannot block the report. */
static void dump_threads(FILE *f)
{
    DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te;
    if (snap == INVALID_HANDLE_VALUE) return;
    te.dwSize = sizeof te;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        DWORD64 pcs[CHAIN_MAX];
        CONTEXT c;
        int n = 0;
        HANDLE th;
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
        th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
        if (!th) continue;
        if (SuspendThread(th) != (DWORD)-1) {
            memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_FULL;
            if (GetThreadContext(th, &c)) n = unwind(c, pcs);
            ResumeThread(th);
        }
        CloseHandle(th);
        fprintf(f, "thread %lu (%d frames):\n", te.th32ThreadID, n);
        print_chain(f, pcs, n);
    }
    CloseHandle(snap);
}

static DWORD WINAPI monitor(LPVOID p)
{
    const char *v = getenv("NIGHTFIRE_HANG_SECS"), *a = getenv("NIGHTFIRE_HANG_ARM");
    DWORD secs = v && atoi(v) > 0 ? (DWORD)atoi(v) : 15, still = 0;
    LONG arm = a ? atol(a) : 100, last = -1, counted = 0;
    ULONGLONG stalled_at = 0;
    char path[MAX_PATH];
    FILE *csv;
    (void)p;
    snprintf(path, sizeof path, "%s\\stats.csv", s_dir);
    csv = fopen(path, "w");
    if (csv) { fprintf(csv, "t_s,frames_per_s,total_frames\n"); fflush(csv); }
    for (unsigned tick = 1;; tick++) {
        Sleep(500);
        LONG now = nightfire_session242_frames;
        if (csv && !(tick & 1)) {
            fprintf(csv, "%.1f,%ld,%ld\n", ms_now() / 1000.0, now - counted, now);
            fflush(csv);
            counted = now;
        }
        if (now != last) {
            if (stalled_at)
                event("resumed: frames again after %ld s", (long)((GetTickCount64() - stalled_at) / 1000));
            last = now; still = 0; stalled_at = 0;
            continue;
        }
        if (now < arm) continue;   /* start-up loading may publish nothing for a while */
        if (++still == secs * 2) {
            LONG k = InterlockedIncrement(&s_freezes);
            FILE *f;
            stalled_at = GetTickCount64() - secs * 1000;
            event("freeze: no frame published for %ld s (frame %ld)", (long)secs, now);
            snprintf(path, sizeof path, "%s\\freeze.txt", s_dir);
            f = fopen(path, "a");
            if (f) {
                fprintf(f, "Freeze %ld at %.3f s: no frame published for %lu s (published frame %ld)\n",
                        k, ms_now() / 1000.0, secs, now);
                dump_threads(f);
                fputc('\n', f);
                fclose(f);
            }
            if (k <= 2) { char name[32]; snprintf(name, sizeof name, "freeze-%ld.dmp", k); minidump(name, NULL); }
        }
    }
}

static __declspec(noinline) int test_recurse(volatile int d)
{
    volatile char pad[512];
    pad[0] = (char)d; pad[511] = (char)d;
    if (d > 0x7FFFFFF0) return 0;
    return test_recurse(d + 1) + pad[(unsigned)d & 511];
}

/* Test-only: called for every published frame. */
void nightfire_session242_frame(void)
{
    static int init;
    static double crash_s, freeze_s, abort_s;
    static ULONGLONG t0;
    InterlockedIncrement(&nightfire_session242_frames);
    if (s_on <= 0) return;
    if (!init) {
        const char *c = getenv("NIGHTFIRE_TEST_CRASH"), *z = getenv("NIGHTFIRE_TEST_FREEZE"), *ab = getenv("NIGHTFIRE_TEST_ABORT");
        init = 1; crash_s = c ? atof(c) : 0; freeze_s = z ? atof(z) : 0; abort_s = ab ? atof(ab) : 0; t0 = GetTickCount64();
    }
    if (!crash_s && !freeze_s && !abort_s) return;
    double el = (GetTickCount64() - t0) / 1000.0;
    if (crash_s > 0 && el >= crash_s) { fprintf(stderr, "[SESSION242] TEST: forcing a crash\n"); fflush(stderr);
        { const char *k = getenv("NIGHTFIRE_TEST_CRASH_KIND"); if (k && k[0] == 's') test_recurse(0); }   /* =stack: stack overflow */
        *(volatile int *)(uintptr_t)8 = 1; }
    if (abort_s > 0 && el >= abort_s) { fprintf(stderr, "[SESSION242] TEST: calling abort()\n"); fflush(stderr); abort(); }
    if (freeze_s > 0 && el >= freeze_s) { fprintf(stderr, "[SESSION242] TEST: stopping the frame thread\n"); fflush(stderr); for (;;) Sleep(1000); }
}

/* Called once from WinMain after the symbol handler is set up. */
void nightfire_session242_init(void)
{
    const char *on = getenv("NIGHTFIRE_SESSION242"), *dir = getenv("NIGHTFIRE_SESSION_DIR");
    char path[MAX_PATH];
    s_on = on && on[0] == '1' && dir && *dir;
    if (!s_on) return;
    InitializeCriticalSection(&s_lock);
    s_t0 = GetTickCount64();
    snprintf(s_dir, sizeof s_dir, "%s", dir);
    CreateDirectoryA(s_dir, NULL);
    snprintf(path, sizeof path, "%s\\events.txt", s_dir); s_events = fopen(path, "w");
    signal(SIGABRT, on_abort);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    { HANDLE h = CreateThread(NULL, 0, monitor, NULL, 0, NULL); if (h) CloseHandle(h); }
    s_crash_req = CreateEventA(NULL, FALSE, FALSE, NULL); s_crash_done = CreateEventA(NULL, TRUE, FALSE, NULL);
    { HANDLE h = s_crash_req && s_crash_done ? CreateThread(NULL, 1 << 20, crash_writer, NULL, 0, NULL) : NULL;
      if (h) CloseHandle(h); else { s_crash_req = s_crash_done = NULL; } }
    event("session start (pid %lu)", (unsigned long)GetCurrentProcessId());
    fprintf(stderr, "[SESSION242] crash capture on: %s\n", s_dir);
}
