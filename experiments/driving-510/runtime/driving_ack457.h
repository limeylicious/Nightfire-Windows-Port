#ifndef DRIVING_ACK457_H
#define DRIVING_ACK457_H
/* Experimental scheduling only. Does not acknowledge registers, GPU commands,
   fences, or target memory. The unchanged worker sweep does all original work. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fenv.h>
#include <xmmintrin.h>
#ifndef ACK457_CREATE
#define ACK457_CREATE() CreateEventW(NULL,FALSE,FALSE,NULL)
#define ACK457_SET(h) SetEvent(h)
#define ACK457_WAIT(h,ms) WaitForSingleObject(h,ms)
#define ACK457_CLOSE(h) CloseHandle(h)
#endif
#ifndef ACK457_FATAL
__declspec(noreturn) static void ack457_fatal(const char *where,DWORD error)
{
    fprintf(stderr,"[ACK457] STOP where=%s error=%lu backing-not-retired=1\n",where,(unsigned long)error);
    fflush(stderr);TerminateProcess(GetCurrentProcess(),57);abort();
}
#define ACK457_FATAL(where,error) ack457_fatal(where,error)
#endif
static SRWLOCK ack457_lock=SRWLOCK_INIT;
static HANDLE ack457_event;
/*0 off/refused,1 live,2 stopping,3 stopped. Handle protected by lock for notifiers;
  worker may use it without lock because successful join precedes handle closure. */
static volatile LONG ack457_phase;
static volatile LONG64 ack457_notifications;
static uint64_t ack457_wakes,ack457_timeouts;
static int ack457_enabled(void){return ack457_phase==1;}
static void ack457_init(int eligible)
{
    int saved_errno=errno;DWORD saved_error=GetLastError();
    const char *v=getenv("DRIVING_ACK457");
    if(!v||strcmp(v,"1")){errno=saved_errno;SetLastError(saved_error);return;}
    if(!eligible){
        fprintf(stderr,"[ACK457] REFUSED requires-PB_SYNC-trapped-APU-no-mirrors legacy-polling=1\n");
        errno=saved_errno;SetLastError(saved_error);return;
    }
    AcquireSRWLockExclusive(&ack457_lock);
    if(ack457_event||ack457_phase==1||ack457_phase==2){
        ReleaseSRWLockExclusive(&ack457_lock);ACK457_FATAL("duplicate-start",ERROR_INVALID_STATE);
    }
    ack457_event=ACK457_CREATE();
    if(!ack457_event){DWORD e=GetLastError();ReleaseSRWLockExclusive(&ack457_lock);ACK457_FATAL("CreateEvent",e);}
    ack457_notifications=0;ack457_wakes=ack457_timeouts=0;
    InterlockedExchange(&ack457_phase,1);ReleaseSRWLockExclusive(&ack457_lock);
    fprintf(stderr,"[ACK457] enabled=1 wait-timeout-ms=1 full-worker-sweep=1 no-GPU-completion-change=1\n");
    errno=saved_errno;SetLastError(saved_error);
}
/* Called by both original PFB writers AFTER their original store. */
void driving_ack457_wake(void)
{
    int saved_errno,failed=0;DWORD saved_error,error=0;fenv_t fp;unsigned csr;
    if(ack457_phase!=1)return;
    saved_errno=errno;saved_error=GetLastError();csr=_mm_getcsr();fegetenv(&fp);
    AcquireSRWLockShared(&ack457_lock);
    if(ack457_phase==1){
        if(!ack457_event){failed=1;error=ERROR_INVALID_HANDLE;}
        else if(!ACK457_SET(ack457_event)){failed=1;error=GetLastError();}
        else InterlockedIncrement64(&ack457_notifications);
    }
    ReleaseSRWLockShared(&ack457_lock);
    fesetenv(&fp);_mm_setcsr(csr);errno=saved_errno;SetLastError(saved_error);
    if(failed)ACK457_FATAL("SetEvent-notify",error);
}
static void ack457_registration_gate(void)
{
    if(ack457_phase==1||ack457_phase==2)
        ACK457_FATAL("mirror-registration-outside-validated-mode",ERROR_NOT_SUPPORTED);
}
static void ack457_report(void)
{
    FILETIME c,e,k,u;uint64_t ticks=0;
    if(GetThreadTimes(GetCurrentThread(),&c,&e,&k,&u))
        ticks=((uint64_t)k.dwHighDateTime<<32|k.dwLowDateTime)+((uint64_t)u.dwHighDateTime<<32|u.dwLowDateTime);
    fprintf(stderr,"[ACK457] wakes=%llu timeouts=%llu notifications=%lld worker_cpu_100ns=%llu\n",
        (unsigned long long)ack457_wakes,(unsigned long long)ack457_timeouts,
        (long long)InterlockedCompareExchange64(&ack457_notifications,0,0),(unsigned long long)ticks);
}
static void ack457_wait(void)
{
    DWORD result=ACK457_WAIT(ack457_event,1);
    if(result==WAIT_OBJECT_0)++ack457_wakes;
    else if(result==WAIT_TIMEOUT)++ack457_timeouts;
    else ACK457_FATAL("worker-wait",result==WAIT_FAILED?GetLastError():result);
    if(((ack457_wakes+ack457_timeouts)&8191)==0)ack457_report();
}
/* Owner calls before retiring permissions/kernel/apertures. No lock held during
   join. After phase2 no notifier can touch the event. Any failure stops without
   closing handles or allowing the caller to retire memory. */
static void ack457_shutdown(HANDLE thread,volatile LONG *stop)
{
    HANDLE event;DWORD error=0,result;int failed=0;
    AcquireSRWLockExclusive(&ack457_lock);
    if(ack457_phase!=1||!ack457_event||!thread){
        ReleaseSRWLockExclusive(&ack457_lock);ACK457_FATAL("shutdown-state",ERROR_INVALID_STATE);
    }
    InterlockedExchange(&ack457_phase,2);InterlockedExchange(stop,1);
    if(!ACK457_SET(ack457_event)){failed=1;error=GetLastError();}
    ReleaseSRWLockExclusive(&ack457_lock);
    if(failed)ACK457_FATAL("SetEvent-shutdown",error);
    result=ACK457_WAIT(thread,1000);
    if(result!=WAIT_OBJECT_0)ACK457_FATAL("worker-join",result==WAIT_FAILED?GetLastError():result);
    AcquireSRWLockExclusive(&ack457_lock);
    event=ack457_event;ack457_event=NULL;InterlockedExchange(&ack457_phase,3);
    if(!ACK457_CLOSE(event)){failed=1;error=GetLastError();}
    ReleaseSRWLockExclusive(&ack457_lock);
    if(failed)ACK457_FATAL("CloseHandle-event",error);
    if(!ACK457_CLOSE(thread))ACK457_FATAL("CloseHandle-thread",GetLastError());
    fprintf(stderr,"[ACK457] joined-before-retirement=1 event-closed=1\n");
}
#endif
