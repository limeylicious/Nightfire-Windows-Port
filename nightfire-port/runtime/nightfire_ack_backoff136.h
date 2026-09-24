/* CP136 opt-in ACK idle backoff. ACK-thread owned state only.
 * No guest reads, pointer caching, GPU work or external wakeup dependency here. */
#ifndef NIGHTFIRE_ACK_BACKOFF136_H
#define NIGHTFIRE_ACK_BACKOFF136_H
#include <stdint.h>

/* Pure policy, also compiled by the isolated logic fixture. QPC units throughout.
 * A rollback starts a new grace period. No elapsed-time debt is accumulated. */
typedef struct NFAck136Policy {
    uint64_t last, quiet_since, grace;
    unsigned valid;
} NFAck136Policy;
static int nf_ack136_quiet(NFAck136Policy *p, uint64_t now,
                          int progress, int eligible)
{
    if (!eligible || !p->grace) { p->valid = 0; return 0; }
    if (!p->valid || now < p->last || progress) {
        p->valid = 1; p->last = p->quiet_since = now; return 0;
    }
    p->last = now;
    return now - p->quiet_since >= p->grace;
}
/* ceil(freq * 100us), avoiding multiplication/addition overflow. */
static uint64_t nf_ack136_grace(uint64_t frequency)
{
    return frequency / 10000u + (frequency % 10000u != 0);
}
static int nf_ack136_exact_enabled(const char *v)
{
    return v && v[0] == '1' && v[1] == '\0';
}

#if defined(NIGHTFIRE_ACK_BACKOFF136) && !defined(NF_ACK136_POLICY_ONLY)
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct NFAck136State {
    NFAck136Policy policy;
    HANDLE timer;
    uint64_t frequency, start, cpu_start, last_report;
    uint64_t loops, idle, progress_loops, grace_yields, waits, requested_us;
    uint64_t failures, wait_ticks, longest_wait_ticks, timeouts;
    unsigned timeout_streak, max_timeout_streak;
    unsigned requested, active, progress, cpu_valid, attempted, iteration_eligible;
    DWORD error;
    const char *fallback;
} NFAck136State;
/* Singleton belongs exclusively to nv2a_ack_thread and its mirror helpers.
 * init/final run on that thread; no other thread may inspect or mutate it. */
static NFAck136State nf_ack136;
static void nf_ack136_report(void);
static uint64_t nf_ack136_filetime(FILETIME t)
{
    return ((uint64_t)t.dwHighDateTime << 32) | t.dwLowDateTime;
}
static int nf_ack136_cpu(uint64_t *out)
{
    FILETIME c,e,k,u;
    if (!GetThreadTimes(GetCurrentThread(), &c,&e,&k,&u)) return 0;
    *out = nf_ack136_filetime(k) + nf_ack136_filetime(u); return 1;
}
static void nf_ack136_disable(const char *why, DWORD error)
{
    nf_ack136.active = 0; nf_ack136.policy.valid = 0;
    nf_ack136.attempted = 1; /* permanent failure, including loss of eligibility */
    nf_ack136.fallback = why; nf_ack136.error = error;
    if (nf_ack136.timer) {
        CancelWaitableTimer(nf_ack136.timer);
        CloseHandle(nf_ack136.timer); nf_ack136.timer = NULL;
    }
    nf_ack136_report();
}
static void nf_ack136_activate(void)
{
    nf_ack136.attempted = 1;
    /* HIGH_RESOLUTION=2 (Windows 10 1803+). No normal-resolution substitute:
     * failure retains Sleep(0). Only the private ACK thread owns this timer. */
    nf_ack136.timer = CreateWaitableTimerExW(NULL,NULL,2,
                                           TIMER_MODIFY_STATE|SYNCHRONIZE);
    if (!nf_ack136.timer) {
        DWORD error = GetLastError();
        ++nf_ack136.failures; nf_ack136_disable("timer-create",error); return;
    }
    nf_ack136.active = 1;
    nf_ack136.fallback = NULL;
}
static void nf_ack136_init(int apu_trapped)
{
    LARGE_INTEGER f,n;
    memset(&nf_ack136,0,sizeof(nf_ack136));
    nf_ack136.requested = nf_ack136_exact_enabled(getenv("NIGHTFIRE_ACK_BACKOFF136"));
    /* Single metadata line even runtime OFF allows external GetThreadTimes for
     * matched baselines, without hot-loop metrics or shared-stat readers. */
    fprintf(stderr,"[ACK136] startup tid=%lu requested=%u trapped=%d\n",
            GetCurrentThreadId(),nf_ack136.requested,apu_trapped);
    if (!nf_ack136.requested) return;
    nf_ack136.cpu_valid = nf_ack136_cpu(&nf_ack136.cpu_start);
    fprintf(stderr,"[ACK136] grace_us=100 due_us=250 watchdog_ms=1 timeout_limit=4; simulated ACK only\n");
    if (!QueryPerformanceFrequency(&f) || f.QuadPart <= 0 ||
        !QueryPerformanceCounter(&n) || n.QuadPart < 0) {
        ++nf_ack136.failures; nf_ack136_disable("qpc-init",0); return;
    }
    nf_ack136.frequency = (uint64_t)f.QuadPart;
    nf_ack136.start = nf_ack136.last_report = (uint64_t)n.QuadPart;
    nf_ack136.policy.grace = nf_ack136_grace(nf_ack136.frequency);
    if (apu_trapped) nf_ack136_activate();
    else nf_ack136.fallback = "waiting-for-trap";
}
static void nf_ack136_begin_iteration(int apu_trapped)
{
    if (!nf_ack136.requested) return;
    if (!nf_ack136.attempted && apu_trapped) nf_ack136_activate();
    nf_ack136.iteration_eligible = apu_trapped != 0;
    if (nf_ack136.active) nf_ack136.progress = 0;
}
static void nf_ack136_mark_progress(void)
{
    if (nf_ack136.active) nf_ack136.progress = 1;
}
/* Called AFTER the entire original ACK iteration, including KeTickCount.
 * Return 1 only after a successful timer wait; caller retains original Sleep(0)
 * on all other paths. Rearming each quiet iteration supplies periodic polling:
 * lPeriod cannot express 250us because its unit is whole milliseconds.
 * The 1ms watchdog requests a fresh poll on timeout. Four consecutive timed-out
 * wait attempts disable the timer; only a successful wait clears that streak.
 * A due time of 250us does NOT guarantee thread scheduling within 250us. */
static int nf_ack136_after_iteration(int apu_trapped)
{
    LARGE_INTEGER n, due, end;
    DWORD result, wait_error;
    uint64_t elapsed;
    if (!nf_ack136.active) return 0;
    if (!apu_trapped || !nf_ack136.iteration_eligible) {
        nf_ack136_disable("trap-lost",0); return 0;
    }
    ++nf_ack136.loops;
    if (nf_ack136.progress) ++nf_ack136.progress_loops;
    else ++nf_ack136.idle;
    if (!QueryPerformanceCounter(&n) || n.QuadPart < 0) {
        ++nf_ack136.failures; nf_ack136_disable("qpc-loop",0); return 0;
    }
    if ((uint64_t)n.QuadPart < nf_ack136.last_report)
        nf_ack136.last_report = (uint64_t)n.QuadPart;
    if (((uint64_t)n.QuadPart - nf_ack136.last_report) / nf_ack136.frequency >= 5) {
        nf_ack136_report(); nf_ack136.last_report = (uint64_t)n.QuadPart;
    }
    if (!nf_ack136_quiet(&nf_ack136.policy,(uint64_t)n.QuadPart,
                        nf_ack136.progress,1)) {
        ++nf_ack136.grace_yields; return 0;
    }
    due.QuadPart = -2500; /* exactly 250us requested, no adaptive growth */
    if (!SetWaitableTimer(nf_ack136.timer,&due,0,NULL,NULL,FALSE)) {
        DWORD error = GetLastError();
        ++nf_ack136.failures; nf_ack136_disable("timer-arm",error); return 0;
    }
    ++nf_ack136.waits; nf_ack136.requested_us += 250;
    result = WaitForSingleObject(nf_ack136.timer,1);
    wait_error = result == WAIT_FAILED ? GetLastError() : result;
    /* Include timed-out/failed waits in the aggregate; they consumed time too. */
    if (QueryPerformanceCounter(&end) && end.QuadPart >= n.QuadPart) {
        elapsed = (uint64_t)(end.QuadPart - n.QuadPart);
        nf_ack136.wait_ticks += elapsed;
        if (elapsed > nf_ack136.longest_wait_ticks)
            nf_ack136.longest_wait_ticks = elapsed;
    }
    if (result == WAIT_TIMEOUT) {
        ++nf_ack136.timeouts;
        ++nf_ack136.timeout_streak;
        if (nf_ack136.timeout_streak > nf_ack136.max_timeout_streak)
            nf_ack136.max_timeout_streak = nf_ack136.timeout_streak;
        /* A new complete polling iteration starts a fresh100us grace period.
         * No extra wait here, no elapsed-time debt and no progress is invented. */
        nf_ack136.policy.valid = 0;
        if (nf_ack136.timeout_streak >= 4) {
            ++nf_ack136.failures;
            nf_ack136_disable("timer-timeout-streak",WAIT_TIMEOUT); return 0;
        }
        if (!CancelWaitableTimer(nf_ack136.timer)) {
            DWORD error = GetLastError();
            ++nf_ack136.failures; nf_ack136_disable("timer-cancel",error); return 0;
        }
        /* Cancel does not clear an already-signaled timer. This is safe: the
         * next attempt calls SetWaitableTimer, resetting it to nonsignaled. */
        return 0;
    }
    if (result != WAIT_OBJECT_0) {
        ++nf_ack136.failures; nf_ack136_disable("timer-wait",wait_error); return 0;
    }
    nf_ack136.timeout_streak = 0;
    return 1;
}
/* Only the ACK thread reports. Cumulative 5s reports survive ordinary UI paths
 * that call ExitProcess without running the ACK-thread exit epilogue. */
static void nf_ack136_report(void)
{
    LARGE_INTEGER n;
    uint64_t cpu = 0;
    double cpu_ms = -1, elapsed_ms = -1, wait_ms = -1, max_wait_us = -1;
    if (!nf_ack136.requested) return;
    if (nf_ack136.cpu_valid && nf_ack136_cpu(&cpu) && cpu >= nf_ack136.cpu_start)
        cpu_ms = (double)(cpu - nf_ack136.cpu_start) / 10000.0;
    if (nf_ack136.frequency) {
        if (QueryPerformanceCounter(&n) && n.QuadPart >= 0 &&
            (uint64_t)n.QuadPart >= nf_ack136.start)
            elapsed_ms = 1000.0 * ((uint64_t)n.QuadPart - nf_ack136.start) / nf_ack136.frequency;
        wait_ms = 1000.0 * nf_ack136.wait_ticks / nf_ack136.frequency;
        max_wait_us = 1000000.0 * nf_ack136.longest_wait_ticks / nf_ack136.frequency;
    }
    fprintf(stderr,"[ACK136] loops=%llu idle=%llu progress=%llu grace_yields=%llu "
        "waits=%llu requested_us=%llu failures=%llu cpu_ms=%.3f elapsed_ms=%.3f "
        "wait_ms=%.3f max_wait_us=%.3f timeouts=%llu timeout_streak=%u "
        "max_timeout_streak=%u fallback=%s error=%lu\n",
        (unsigned long long)nf_ack136.loops,(unsigned long long)nf_ack136.idle,
        (unsigned long long)nf_ack136.progress_loops,(unsigned long long)nf_ack136.grace_yields,
        (unsigned long long)nf_ack136.waits,(unsigned long long)nf_ack136.requested_us,
        (unsigned long long)nf_ack136.failures,cpu_ms,elapsed_ms,wait_ms,max_wait_us,
        (unsigned long long)nf_ack136.timeouts,nf_ack136.timeout_streak,nf_ack136.max_timeout_streak,
        nf_ack136.fallback ? nf_ack136.fallback : "none",(unsigned long)nf_ack136.error);
}
static void nf_ack136_finish(void)
{
    nf_ack136_report();
    if (nf_ack136.timer) {
        CancelWaitableTimer(nf_ack136.timer); CloseHandle(nf_ack136.timer);
        nf_ack136.timer = NULL;
    }
    nf_ack136.active = 0;
}
#elif !defined(NF_ACK136_POLICY_ONLY)
/* Compile OFF: arguments are not evaluated; no timer, clocks, metrics or changes
 * to legacy volatile reads/writes. Runtime OFF: no per-loop QPC or statistics. */
#define nf_ack136_init(trapped) ((void)0)
#define nf_ack136_begin_iteration(trapped) ((void)0)
#define nf_ack136_mark_progress() ((void)0)
#define nf_ack136_after_iteration(trapped) 0
#define nf_ack136_finish() ((void)0)
#endif
#endif
