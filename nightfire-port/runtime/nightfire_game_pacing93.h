/* Optional PAL overspeed prevention. No guest writes, extra updates or render
 * skipping. This does not repair slowdown below 50 or establish correct60Hz. */
#ifndef NIGHTFIRE_GAME_PACING93_H
#define NIGHTFIRE_GAME_PACING93_H
#include <windows.h>
#include "nightfire_pacer93.h"
static NFPacer93 nf_game93_pacer;
static HANDLE nf_game93_timer;
static uint64_t nf_game93_frequency,nf_game93_start,nf_game93_waited;
static unsigned nf_game93_depth,nf_game93_updates,nf_game93_renders,nf_game93_stack;
static unsigned nf_game93_eligible,nf_game93_calls,nf_game93_waits,nf_game93_verified;
static uint64_t nf_game93_clock(void){LARGE_INTEGER t;QueryPerformanceCounter(&t);return (uint64_t)t.QuadPart;}
static void nf_game93_close(void){if(nf_game93_timer){CloseHandle(nf_game93_timer);nf_game93_timer=NULL;}}
static int nf_game93_enabled(void){
    static int on=-1;
    if(on<0){
        const char *v=getenv("NIGHTFIRE_GAME_PACING93");on=v && !strcmp(v,"1");
        if(on){
            LARGE_INTEGER f;if(!QueryPerformanceFrequency(&f) || f.QuadPart<=0){on=0;return 0;}
            nf_game93_frequency=(uint64_t)f.QuadPart;
            /* HIGH_RESOLUTION=2, documented on Windows10 1803+. Fall back
             * without requesting a system-wide timer-resolution change. */
            nf_game93_timer=CreateWaitableTimerExW(NULL,NULL,2,TIMER_MODIFY_STATE|SYNCHRONIZE);
            if(!nf_game93_timer)nf_game93_timer=CreateWaitableTimerW(NULL,FALSE,NULL);
            atexit(nf_game93_close);
            fprintf(stderr,"[GAME-PACE93] enabled PAL50 gameplay only; no catch-up, guest writes or movie pacing\n");
        }
    }
    return on;
}
static int nf_game93_gate(void){
    unsigned depth=MEM16(0x17bfe8);
    return depth>0 && depth<32 && MEM32(0x17bfec+depth*4)==2 &&
        !MEM8(0x2c5760) && MEM32(0x17c0f0)==50 && MEM32(0x17c0f4)==50 &&
        MEM32(0x17c104)==0x3ca3d70a && !MEM16(0x1fec64) &&
        !MEM8(0x1f6564) && !MEM8(0x1f65c0) && !MEM32(0x2ae288) &&
        !(MEM8(0x1fec48) && MEM8(0x25d79c));
}
static void nightfire_game_pacing93(unsigned after){
    if(after>1 || !nf_game93_enabled())return;
    if(!after){
        if(nf_game93_depth++){nf_game93_pacer.valid=0;nf_game93_eligible=0;return;}
        nf_game93_eligible=nf_game93_gate();
        unsigned stack=MEM16(0x17bfe8);
        if(stack!=nf_game93_stack)nf_game93_pacer.valid=0;
        nf_game93_stack=stack;
        uint64_t now=nf_game93_clock(),begin=now;
        uint64_t period=(nf_game93_frequency+49)/50;
        uint64_t delay=nf_pacer93_delay(&nf_game93_pacer,now,period,nf_game93_eligible);
        if(delay)nf_game93_waits++;
        while(delay){
            LARGE_INTEGER due;
            due.QuadPart=-(LONGLONG)(delay*10000000/nf_game93_frequency+1);
            if(nf_game93_timer && SetWaitableTimer(nf_game93_timer,&due,0,NULL,NULL,FALSE))
                WaitForSingleObject(nf_game93_timer,50);
            else Sleep(1);
            now=nf_game93_clock();
            delay=nf_pacer93_delay(&nf_game93_pacer,now,period,nf_game93_eligible);
        }
        if(now>=begin)nf_game93_waited+=now-begin;
        nf_game93_start=now;
        nf_game93_updates=MEM32(0x1f65b4);nf_game93_renders=MEM32(0x1f65b0);
        return;
    }
    if(!nf_game93_depth || --nf_game93_depth)return;
    int verified=nf_game93_eligible && nf_game93_gate() &&
        MEM16(0x17bfe8)==nf_game93_stack &&
        (uint32_t)(MEM32(0x1f65b4)-nf_game93_updates)==1 &&
        (uint32_t)(MEM32(0x1f65b0)-nf_game93_renders)==1;
    nf_pacer93_commit(&nf_game93_pacer,nf_game93_start,(nf_game93_frequency+49)/50,verified);
    nf_game93_verified+=verified;
    if(++nf_game93_calls%250==0)
        fprintf(stderr,"[GAME-PACE93] outer=%u verified=%u waits=%u waited_ms=%.3f\n",
            nf_game93_calls,nf_game93_verified,nf_game93_waits,1000.0*nf_game93_waited/nf_game93_frequency);
}
#endif
