"""Install scoped guest event/timer waits after prepare_runtime.py; no toolkit edits.

The three runtime/driving_dispatcher143 files are checked-in implementation
inputs, not generated guest code. The core must match the isolated 73-check
candidate exactly. Root build wiring: add runtime/driving_dispatcher143.c to
xbox_kernel; do not replace kernel_sync.c (Nt HANDLE services still use it).
"""
from pathlib import Path
import hashlib, json
root=Path(__file__).resolve().parents[1]
source=root/'runtime/kernel_bridge.c'
core=root/'runtime/driving_dispatcher143_core.h'
expected='e86703a4a333f404d1cc375871a6b8fc67ca1cf77c9c5bb2f4247711094cc707'
assert hashlib.sha256(core.read_bytes()).hexdigest()==expected
text=source.read_text(encoding='utf-8')
before=hashlib.sha256(source.read_bytes()).hexdigest()
marker='/* WAIT143: guest dispatcher objects; Nt HANDLE APIs stay unchanged. */'

def replace(old,new):
    global text
    assert text.count(old)==1,(old[:100],text.count(old))
    text=text.replace(old,new)

def function(signature,new):
    global text
    start=text.index(signature+'\n{')
    end=text.index('\n}',start)+2
    text=text[:start]+new.strip()+text[end:]

if marker not in text:
    replace('#include "kernel.h"','#include "kernel.h"\n#include "driving_dispatcher143.h"\n'+marker)
    function('static void bridge_KeSetEvent(void)',r'''
static void bridge_KeSetEvent(void)
{
    /* Increment affects scheduling priority on Xbox; the host scheduler has no
     * equivalent. WaitNext!=0 is rejected rather than losing its IRQL contract. */
    g_eax = driving_wait143_set_event(STACK_ARG(0), STACK_ARG(2));
}''')
    function('static void bridge_KeWaitForSingleObject(void)',r'''
static void bridge_KeWaitForSingleObject(void)
{
    /* Object is a guest dispatcher header, not an Nt handle token. */
    g_eax = driving_wait143_single(STACK_ARG(0), STACK_ARG(3), STACK_ARG(4));
}''')
    function('static void bridge_KeWaitForMultipleObjects(void)',r'''
static void bridge_KeWaitForMultipleObjects(void)
{
    /* WaitReason/WaitMode/WaitBlockArray do not select host HANDLE semantics.
     * Nonalertable event/timer waits only; unknown cases fail visibly. */
    g_eax = driving_wait143_multiple(STACK_ARG(0), STACK_ARG(1), STACK_ARG(2),
                                    STACK_ARG(5), STACK_ARG(6));
}''')
    replace(' * KeWaitForSingleObject is routed; the multiple-object sibling was not. Like\n * NtWaitForMultipleObjectsEx, the Objects[] array in guest memory holds 32-bit\n * handle tokens, so each is resolved before the native wait sees it. */',
            ' * Objects[] holds 32-bit guest dispatcher-object pointers, NOT handle tokens.\n * WAIT143 preserves NtWaitForMultipleObjectsEx on the separate handle path. */')
    replace('#define BRIDGE_MAXIMUM_WAIT_OBJECTS 64\n','')
    replace('#undef BRIDGE_MAXIMUM_WAIT_OBJECTS\n','')
    signature='static void bridge_KeInitializeTimerEx(void)'
    function(signature,r'''
static void bridge_KeInitializeTimerEx(void)
{
    kernel_timer_initialize143(STACK_ARG(0), STACK_ARG(1));
    g_eax = 0;
}''')
    replace(signature+'\n{','static void kernel_timer_initialize143(uint32_t timer_va, unsigned type);\n\n'+signature+'\n{')
    replace('static int g_timer_started;',r'''static INIT_ONCE g_timer_lock_once = INIT_ONCE_STATIC_INIT;
static INIT_ONCE g_timer_thread_once = INIT_ONCE_STATIC_INIT;
static DWORD WINAPI kernel_timer_thread(LPVOID unused);

static BOOL CALLBACK kernel_timer_lock_init143(PINIT_ONCE once, PVOID arg, PVOID *context)
{
    (void)once; (void)arg; (void)context;
    InitializeCriticalSection(&g_timer_lock);
    return TRUE;
}

static void kernel_timer_ensure_lock143(void)
{
    if (!InitOnceExecuteOnce(&g_timer_lock_once, kernel_timer_lock_init143, NULL, NULL))
        driving_wait143_stop("timer lock initialization", 0);
}

static BOOL CALLBACK kernel_timer_thread_init143(PINIT_ONCE once, PVOID arg, PVOID *context)
{
    HANDLE thread;
    (void)once; (void)arg; (void)context;
    thread = CreateThread(NULL, 0, kernel_timer_thread, NULL, 0, NULL);
    if (!thread) driving_wait143_stop("timer thread creation", 0);
    CloseHandle(thread);
    return TRUE;
}

static void kernel_timer_initialize143(uint32_t timer_va, unsigned type)
{
    int i;
    kernel_timer_ensure_lock143();
    EnterCriticalSection(&g_timer_lock);
    for (i = 0; i < XBOX_MAX_TIMERS; ++i)
        if (g_timers[i].timer_va == timer_va)
            driving_wait143_stop("reinitialize armed timer", timer_va);
    driving_wait143_initialize_timer(timer_va, type);
    LeaveCriticalSection(&g_timer_lock);
}''')
    replace('        return 0;\n    }\n    g_esp = XBOX_WORKER_STACK_TOP(slot);',
            '        driving_wait143_stop("timer worker stack unavailable", 0);\n    }\n    g_esp = XBOX_WORKER_STACK_TOP(slot);')
    replace('            return 0;\n        }\n        g_fs_base = tib;\n    }\n\n    for (;;) {',
            '            driving_wait143_stop("timer worker TIB unavailable", 0);\n        }\n        g_fs_base = tib;\n    }\n\n    for (;;) {')
    replace('            dpc = g_timers[i].dpc_va;\n            if (g_timers[i].period_ms > 0)',
            '            /* Publish while this expiry is still ordered against rearm/cancel.\n             * DPC execution stays outside the scheduler lock. */\n            driving_wait143_signal_timer(g_timers[i].timer_va);\n            dpc = g_timers[i].dpc_va;\n            if (g_timers[i].period_ms > 0)')
    function('static void kernel_set_timer(uint32_t timer_va, long long due_100ns,\n                             long period_ms, uint32_t dpc_va)',r'''
static void kernel_set_timer(uint32_t timer_va, long long due_100ns,
                             long period_ms, uint32_t dpc_va)
{
    /* Preserve the existing 10ms scheduler and relative-time floor. Positive
     * absolute due times retain its immediate approximation; not fixed here. */
    uint64_t ticks = due_100ns < 0 ? (uint64_t)(-(due_100ns + 1)) + 1 : 0;
    uint64_t delay_ms = ticks / 10000;
    long long now;
    int i, free_slot = -1;
    uint32_t was_set = 0;
    if (period_ms < 0) driving_wait143_stop("negative timer period", timer_va);
    driving_wait143_validate_timer(timer_va);
    kernel_timer_ensure_lock143();
    if (!InitOnceExecuteOnce(&g_timer_thread_once, kernel_timer_thread_init143, NULL, NULL))
        driving_wait143_stop("timer thread initialization", timer_va);
    EnterCriticalSection(&g_timer_lock);
    for (i = 0; i < XBOX_MAX_TIMERS; i++) {
        if (g_timers[i].timer_va == timer_va) { free_slot = i; was_set = 1; break; }
        if (!g_timers[i].timer_va && free_slot < 0) free_slot = i;
    }
    if (free_slot < 0) driving_wait143_stop("active timer table exhausted", timer_va);
    /* A successful rearm clears a previous expiry signal under both locks. */
    driving_wait143_reset_timer(timer_va);
    now = (long long)GetTickCount64();
    g_timers[free_slot].timer_va  = timer_va;
    g_timers[free_slot].dpc_va    = dpc_va;
    g_timers[free_slot].due_ms    = delay_ms > (uint64_t)(INT64_MAX - now)
                                ? INT64_MAX : now + (long long)delay_ms;
    g_timers[free_slot].period_ms = period_ms;
    LeaveCriticalSection(&g_timer_lock);
    g_eax = was_set;
}''')
    function('int xbox_kernel_cancel_timer(uint32_t timer_va)',r'''
int xbox_kernel_cancel_timer(uint32_t timer_va)
{
    int i, was_set = 0;
    driving_wait143_validate_timer(timer_va);
    kernel_timer_ensure_lock143();
    EnterCriticalSection(&g_timer_lock);
    for (i = 0; i < XBOX_MAX_TIMERS; i++)
        if (g_timers[i].timer_va == timer_va) {
            g_timers[i].timer_va = 0;
            was_set = 1;
        }
    /* Cancellation prevents future expiry; it does not unsignal an expiry
     * that already occurred, and cannot retract an already dispatched DPC. */
    LeaveCriticalSection(&g_timer_lock);
    return was_set;
}''')
    function('static void bridge_KeCancelTimer(void)',r'''
static void bridge_KeCancelTimer(void)
{
    /* Guest KTIMER is 40 bytes. The native xbox_KeCancelTimer expects the
     * toolkit's larger host shadow struct, which must not overlay guest RAM. */
    g_eax = (uint32_t)xbox_kernel_cancel_timer(STACK_ARG(0));
}''')
    source.write_text(text,encoding='utf-8')
else:
    assert 'driving_wait143_multiple(STACK_ARG(0)' in text
    assert 'driving_wait143_signal_timer(g_timers[i].timer_va);' in text
    assert 'g_timer_started' not in text

report=dict(core_sha256=expected,bridge_before_sha256=before,
    bridge_after_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    target_addition='target_sources(xbox_kernel PRIVATE runtime/driving_dispatcher143.c)',
    unchanged='Nt HANDLE APIs; generated PAL code; timer cadence; DPC call order outside lock')
(root/'analysis/wait143/install.json').write_text(json.dumps(report,indent=2)+'\n')
print('Installed coordinated WAIT143 guest event/timer boundaries; add dispatcher143.c to xbox_kernel.')
