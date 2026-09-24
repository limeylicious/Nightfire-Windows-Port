/* Isolated proposal, NOT installed. One process-wide instance is required.
 * Scope: validated live guest dispatcher headers, type0/1 events and8/9 timers;
 * nonalertable waits only. Header memory must remain alive throughout each wait.
 * Call signal/reset under this API from EVERY relevant set/clear/timer boundary.
 * This does not implement NT handles, threads, semaphores, APCs, WaitNext/IRQL,
 * guest wait-list links, device behavior, or a timer scheduler. */
#ifndef DRIVING_WAIT143_CANDIDATE_H
#define DRIVING_WAIT143_CANDIDATE_H
#include <windows.h>
#include <stdint.h>
typedef struct Wait143Header {
    uint8_t type, absolute, size, inserted;
    volatile LONG signal;
    uint32_t flink, blink;
} Wait143Header;
typedef char wait143_header_size[(sizeof(Wait143Header)==16)?1:-1];
#define W143_INVALID ((uint32_t)0xC000000Du)
#define W143_UNSUPPORTED ((uint32_t)0xC00000BBu)
#define W143_FAILED ((uint32_t)0xC0000001u)
static SRWLOCK wait143_lock=SRWLOCK_INIT;
static CONDITION_VARIABLE wait143_cv=CONDITION_VARIABLE_INIT;
static int wait143_supported(const Wait143Header *h) {
    return h && ((uintptr_t)h%4==0) &&
        (h->type==0 || h->type==1 || h->type==8 || h->type==9);
}
/* All supported objects use boolean signal state; odd types auto-consume. */
static uint32_t wait143_set(Wait143Header *h, int signaled, LONG *previous) {
    uint32_t status=0;
    AcquireSRWLockExclusive(&wait143_lock);
    if (!wait143_supported(h)) status=W143_UNSUPPORTED;
    else {
        if (previous) *previous=h->signal;
        h->signal=signaled?1:0;
        if(signaled) WakeAllConditionVariable(&wait143_cv);
    }
    ReleaseSRWLockExclusive(&wait143_lock);
    return status;
}
/* Relative/absolute NT timeout to a monotonic interval. ceil avoids premature
 * sub-millisecond expiry; enormous timeouts are waited in bounded DWORD chunks. */
static uint64_t wait143_duration(const LARGE_INTEGER *timeout) {
    uint64_t ticks;
    if (timeout->QuadPart<0) ticks=(uint64_t)(-(timeout->QuadPart+1))+1;
    else {
        FILETIME ft; ULARGE_INTEGER now;
        GetSystemTimeAsFileTime(&ft); now.LowPart=ft.dwLowDateTime;now.HighPart=ft.dwHighDateTime;
        if((uint64_t)timeout->QuadPart<=now.QuadPart) return 0;
        ticks=(uint64_t)timeout->QuadPart-now.QuadPart;
    }
    return ticks/10000+(ticks%10000!=0);
}
static uint32_t wait143_wait(unsigned count,Wait143Header *const *objects,
                            unsigned wait_type,int alertable,const LARGE_INTEGER *timeout) {
    uint32_t result=W143_FAILED;
    uint64_t start=GetTickCount64(), duration=timeout?wait143_duration(timeout):0;
    unsigned i,j;
    if(!count||count>64||!objects||wait_type>1) return W143_INVALID;
    if(alertable) return W143_UNSUPPORTED;
    AcquireSRWLockExclusive(&wait143_lock);
    for(i=0;i<count;i++) {
        if(!wait143_supported(objects[i])) {result=W143_UNSUPPORTED;goto done;}
        for(j=0;j<i;j++) if(objects[j]==objects[i]) {result=W143_INVALID;goto done;}
    }
    for(;;) {
        unsigned selected=count, signaled=0;
        for(i=0;i<count;i++) if(objects[i]->signal>0) {
            if(selected==count) selected=i;
            ++signaled;
        }
        if((wait_type==1&&selected<count)||(wait_type==0&&signaled==count)) {
            if(wait_type==0) {for(i=0;i<count;i++) if(objects[i]->type&1) objects[i]->signal=0;result=0;}
            else {if(objects[selected]->type&1) objects[selected]->signal=0;result=selected;}
            goto done;
        }
        {
            DWORD ms=INFINITE;
            if(timeout) {
                uint64_t elapsed=GetTickCount64()-start, remaining;
                if(elapsed>=duration) {result=0x102;goto done;}
                remaining=duration-elapsed;
                ms=remaining>=INFINITE?(INFINITE-1):(DWORD)remaining;
            }
            if(!SleepConditionVariableSRW(&wait143_cv,&wait143_lock,ms,0)
               &&GetLastError()!=ERROR_TIMEOUT) {result=W143_FAILED;goto done;}
            /* Includes spurious/stolen wakes and deadline races: recheck state. */
        }
    }
done:
    ReleaseSRWLockExclusive(&wait143_lock);
    return result;
}
#endif
