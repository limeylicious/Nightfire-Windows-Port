/* Experimental ordinary-gameplay60Hz. The sole guest writes replace both50
 * arguments of DD1D0's rate setter with60. The original setter derives all
 * floats. PAL mode/region/languages/movie selection remain unchanged.
 * No extra updates or render skipping; slowdown below60 remains unresolved. */
#ifndef NIGHTFIRE_GAME_RATE94_H
#define NIGHTFIRE_GAME_RATE94_H
#include <windows.h>
#include "nightfire_pacer93.h"
static NFPacer93 nf_game94_pacer;
static HANDLE nf_game94_timer;
static uint64_t nf_game94_frequency,nf_game94_start,nf_game94_waited;
static unsigned nf_game94_depth,nf_game94_updates,nf_game94_renders,nf_game94_stack;
static unsigned nf_game94_eligible,nf_game94_calls,nf_game94_waits,nf_game94_verified;
static uint64_t nf_game94_clock(void){LARGE_INTEGER t;QueryPerformanceCounter(&t);return (uint64_t)t.QuadPart;}
static void nf_game94_close(void){if(nf_game94_timer){CloseHandle(nf_game94_timer);nf_game94_timer=NULL;}}
static int nf_game94_enabled(void){
    static int on=-1;
    if(on<0){
        const char *v=getenv("NIGHTFIRE_GAME_RATE94");on=v && !strcmp(v,"1");
        if(on){
            LARGE_INTEGER f;if(!QueryPerformanceFrequency(&f) || f.QuadPart<=0){on=0;return 0;}
            nf_game94_frequency=(uint64_t)f.QuadPart;
            /* HIGH_RESOLUTION=2, documented on Windows10 1803+. Fall back
             * without requesting a system-wide timer-resolution change. */
            nf_game94_timer=CreateWaitableTimerExW(NULL,NULL,2,TIMER_MODIFY_STATE|SYNCHRONIZE);
            if(!nf_game94_timer)nf_game94_timer=CreateWaitableTimerW(NULL,FALSE,NULL);
            atexit(nf_game94_close);
            fprintf(stderr,"[GAME-RATE94] enabled gameplay60Hz experiment; scoped rate arguments only, no catch-up or PAL movie-mode change\n");
        }
    }
    return on;
}
static int nf_game94_scope(void){
    /* Checkpoint231: explicitly opt multiplayer into the same verified
     * one-update/one-render 60 Hz boundary. Unknown mode values stay excluded. */
    static int multiplayer=-1;
    if(multiplayer<0){const char *v=getenv("NIGHTFIRE_MP_RATE231");multiplayer=v && !strcmp(v,"1");}
    unsigned depth=MEM16(0x17bfe8);
    return depth>0 && depth<32 && MEM32(0x17bfec+depth*4)==2 &&
        !MEM8(0x2c5760) && !MEM16(0x1fec64) && !MEM8(0x1f65d0) &&
        (!MEM32(0x260018) || (MEM32(0x260018)==1 && multiplayer)) &&
        !MEM8(0x1f6564) && !MEM8(0x1f65c0) && !MEM32(0x2ae288) &&
        !(MEM8(0x1fec48) && MEM8(0x25d79c));
}
static int nf_game94_gate(void){
    unsigned rate=MEM32(0x17c0f4),step=MEM32(0x17c104);
    return nf_game94_scope() && MEM32(0x17c0f0)==rate &&
        ((rate==50 && step==0x3ca3d70a) || (rate==60 && step==0x3c888889));
}
/* DD1D0 pushes two equal50 arguments, then return address DD1E6. Never
 * change another caller, a paused/movie path, non-PAL mode or unknown inputs. */
static void nf_game94_rate_arguments(void){
    if(nf_game94_depth==1 && nf_game94_eligible && nf_game94_scope() &&
       g_esp>=0x1000 && g_esp<=0x03fffff4 && MEM32(g_esp)==0xdd1e6 &&
       MEM32(g_esp+4)==50 && MEM32(g_esp+8)==50){
        MEM32(g_esp+4)=60;MEM32(g_esp+8)=60;
    }
}
#include "nightfire_pause_rate133.h"
static void nightfire_game_pacing94(uint32_t va,unsigned after){
    if(after>1 || !nf_game94_enabled())return;
    nightfire_pause_pacing133(va,after);
    if(va==0x6b040){if(!after)nf_game94_rate_arguments();return;}
    if(va!=0xdd1d0)return;
    if(!after){
        if(nf_game94_depth++){nf_game94_pacer.valid=0;nf_game94_eligible=0;return;}
        nf_game94_eligible=nf_game94_gate();
        unsigned stack=MEM16(0x17bfe8);
        if(stack!=nf_game94_stack)nf_game94_pacer.valid=0;
        nf_game94_stack=stack;
        uint64_t now=nf_game94_clock(),begin=now;
        uint64_t period=(nf_game94_frequency+59)/60;
        uint64_t delay=nf_pacer93_delay(&nf_game94_pacer,now,period,nf_game94_eligible);
        if(delay)nf_game94_waits++;
        while(delay){
            LARGE_INTEGER due;
            due.QuadPart=-(LONGLONG)(delay*10000000/nf_game94_frequency+1);
            if(nf_game94_timer && SetWaitableTimer(nf_game94_timer,&due,0,NULL,NULL,FALSE))
                WaitForSingleObject(nf_game94_timer,50);
            else Sleep(1);
            now=nf_game94_clock();
            delay=nf_pacer93_delay(&nf_game94_pacer,now,period,nf_game94_eligible);
        }
        if(now>=begin)nf_game94_waited+=now-begin;
        nf_game94_start=now;
        nf_game94_updates=MEM32(0x1f65b4);nf_game94_renders=MEM32(0x1f65b0);
        return;
    }
    if(!nf_game94_depth || --nf_game94_depth)return;
    int verified=nf_game94_eligible && nf_game94_gate() && MEM32(0x17c0f4)==60 &&
        MEM16(0x17bfe8)==nf_game94_stack &&
        (uint32_t)(MEM32(0x1f65b4)-nf_game94_updates)==1 &&
        (uint32_t)(MEM32(0x1f65b0)-nf_game94_renders)==1;
    nf_pacer93_commit(&nf_game94_pacer,nf_game94_start,(nf_game94_frequency+59)/60,verified);
    nf_game94_verified+=verified;
    if(++nf_game94_calls%250==0)
        fprintf(stderr,"[GAME-RATE94] outer=%u verified=%u waits=%u waited_ms=%.3f mode=%u rate=%u step=%08X update_delta=%u render_delta=%u\n",
            nf_game94_calls,nf_game94_verified,nf_game94_waits,1000.0*nf_game94_waited/nf_game94_frequency,
            MEM32(0x260018),MEM32(0x17c0f4),MEM32(0x17c104),
            MEM32(0x1f65b4)-nf_game94_updates,MEM32(0x1f65b0)-nf_game94_renders);
}
#endif
