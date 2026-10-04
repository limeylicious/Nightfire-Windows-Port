#ifndef DRIVING_SUBMIT456_H
#define DRIVING_SUBMIT456_H
/* Diagnostic only. No guest reads/writes, acknowledgements, or wait changes.
   Ghidra PAL KickOff 16C940..16CA22 identifies the phase boundaries. */
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fenv.h>
#include <xmmintrin.h>
typedef struct NFSubmit456 {
    uint64_t last,ticks[6],busy_calls;
    unsigned on,phase,wait_requested;
} NFSubmit456;
typedef struct NFSubmit456Total {
    uint64_t calls,ticks[6],busy_calls,wait_requested,alternate;
} NFSubmit456Total;
static NFSubmit456Total nf_submit456_total;
static int nf_submit456_mode=-1;
#ifndef NF456_CLOCK
static uint64_t nf_submit456_clock(void) { LARGE_INTEGER v; QueryPerformanceCounter(&v);return (uint64_t)v.QuadPart; }
#define NF456_CLOCK() nf_submit456_clock()
#endif
#ifndef NF456_FREQ
static uint64_t nf_submit456_frequency(void) { LARGE_INTEGER v; QueryPerformanceFrequency(&v);return (uint64_t)v.QuadPart; }
#define NF456_FREQ() nf_submit456_frequency()
#endif
static NFSubmit456 nf_submit456_begin(void)
{
    NFSubmit456 s={0};int saved_errno;DWORD saved_error;fenv_t saved_fenv;unsigned saved_mxcsr;
    if(nf_submit456_mode==0)return s;
    saved_errno=errno;saved_error=GetLastError();saved_mxcsr=_mm_getcsr();fegetenv(&saved_fenv);
    if(nf_submit456_mode<0){const char *v=getenv("DRIVING_SUBMIT456");nf_submit456_mode=v&&v[0]=='1'&&v[1]==0;}
    s.on=(unsigned)nf_submit456_mode;
    if(s.on)s.last=NF456_CLOCK();
    fesetenv(&saved_fenv);_mm_setcsr(saved_mxcsr);errno=saved_errno;SetLastError(saved_error);return s;
}
static void nf_submit456_phase(NFSubmit456 *s,unsigned phase)
{
    if(s->on){
        int saved_errno=errno;DWORD saved_error=GetLastError();uint64_t now;
        fenv_t saved_fenv;unsigned saved_mxcsr=_mm_getcsr();fegetenv(&saved_fenv);now=NF456_CLOCK();
        s->ticks[s->phase]+=now-s->last;s->last=now;s->phase=phase;
        fesetenv(&saved_fenv);_mm_setcsr(saved_mxcsr);errno=saved_errno;SetLastError(saved_error);
    }
}
static void nf_submit456_end(NFSubmit456 *s)
{
    if(s->on){
        int saved_errno=errno;DWORD saved_error=GetLastError();unsigned i;
        nf_submit456_phase(s,s->phase);
        ++nf_submit456_total.calls;
        for(i=0;i<6;++i)nf_submit456_total.ticks[i]+=s->ticks[i];
        nf_submit456_total.busy_calls+=s->busy_calls;
        nf_submit456_total.wait_requested+=s->wait_requested;
        nf_submit456_total.alternate+=s->phase==5;
        if((nf_submit456_total.calls&255)==0){
            fenv_t saved_fenv;unsigned saved_mxcsr=_mm_getcsr();fegetenv(&saved_fenv);
            fprintf(stderr,"[SUBMIT456] calls=%llu setup=%llu pfb_wait=%llu publish_setup=%llu host_drain=%llu flag_and_optional_busy=%llu alternate=%llu wait_requested=%llu busy_calls=%llu alternate_calls=%llu qpc_frequency=%llu diagnostic_only=1\n",
                (unsigned long long)nf_submit456_total.calls,
                (unsigned long long)nf_submit456_total.ticks[0],(unsigned long long)nf_submit456_total.ticks[1],
                (unsigned long long)nf_submit456_total.ticks[2],(unsigned long long)nf_submit456_total.ticks[3],
                (unsigned long long)nf_submit456_total.ticks[4],(unsigned long long)nf_submit456_total.ticks[5],
                (unsigned long long)nf_submit456_total.wait_requested,(unsigned long long)nf_submit456_total.busy_calls,
                (unsigned long long)nf_submit456_total.alternate,(unsigned long long)NF456_FREQ());
            fesetenv(&saved_fenv);_mm_setcsr(saved_mxcsr);
        }
        errno=saved_errno;SetLastError(saved_error);
    }
}
#endif
