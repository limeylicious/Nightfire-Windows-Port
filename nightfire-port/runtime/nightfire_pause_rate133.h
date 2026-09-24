/* Host-only pacing for the observed PAL single-player pause loop. Requires
 * GAME_RATE94 infrastructure, but never changes its gameplay admission or any
 * guest byte. Unknown states, movies and loading do not enter this scope. */
#ifndef NIGHTFIRE_PAUSE_RATE133_H
#define NIGHTFIRE_PAUSE_RATE133_H
static struct {
    NFPacer93 pacer;
    unsigned depth,eligible,updates,renders,iterations,calls,verified,waits;
    uint64_t start,waited;
} nf_pause133;
static int nf_pause133_enabled(void){
    static int enabled=-1;
    if(enabled<0){const char *v=getenv("NIGHTFIRE_PAUSE_RATE133");enabled=v && !strcmp(v,"1");
        if(enabled)fprintf(stderr,"[PAUSE-RATE133] enabled observed PAL50 pause pacing; host-only, no guest timing changes\n");}
    return enabled;
}
static int nf_pause133_scope(void){
    return MEM16(0x17bfe8)==2 && MEM32(0x17bff0)==1 && MEM32(0x17bff4)==2 &&
        MEM8(0x1f65d0)==1 && !MEM16(0x1fec64) && !MEM32(0x260018) &&
        !MEM8(0x2c5760) && !MEM8(0x1f6564) && !MEM8(0x1f65c0) &&
        !MEM32(0x2ae288) && !MEM8(0x1fec48) && !MEM8(0x25d79c) &&
        MEM32(0x17c0f0)==50 && MEM32(0x17c0f4)==50 && MEM32(0x17c104)==0x3ca3d70a;
}
static int nf_pause133_verified(void){
    return nf_pause133.eligible && nf_pause133_scope() &&
        (uint32_t)(MEM32(0x1f65b4)-nf_pause133.updates)==0 &&
        (uint32_t)(MEM32(0x1f65b0)-nf_pause133.renders)==1 &&
        (uint32_t)(MEM32(0x1f65bc)-nf_pause133.iterations)==1;
}
static void nightfire_pause_pacing133(uint32_t va,unsigned after){
    if(va!=0xdd1d0 || after>1 || !nf_pause133_enabled())return;
    uint64_t period=(nf_game94_frequency+49)/50;
    if(!after){
        if(nf_pause133.depth++){nf_pause133.pacer.valid=0;nf_pause133.eligible=0;return;}
        nf_pause133.eligible=nf_pause133_scope();
        uint64_t now=nf_game94_clock(),begin=now;
        uint64_t delay=nf_pacer93_delay(&nf_pause133.pacer,now,period,nf_pause133.eligible);
        if(delay)nf_pause133.waits++;
        while(delay){
            LARGE_INTEGER due;due.QuadPart=-(LONGLONG)(delay*10000000/nf_game94_frequency+1);
            if(nf_game94_timer && SetWaitableTimer(nf_game94_timer,&due,0,NULL,NULL,FALSE))
                WaitForSingleObject(nf_game94_timer,50);
            else Sleep(1);
            now=nf_game94_clock();
            delay=nf_pacer93_delay(&nf_pause133.pacer,now,period,nf_pause133.eligible);
        }
        if(now>=begin)nf_pause133.waited+=now-begin;
        nf_pause133.start=now;
        nf_pause133.updates=MEM32(0x1f65b4);nf_pause133.renders=MEM32(0x1f65b0);
        nf_pause133.iterations=MEM32(0x1f65bc);
        return;
    }
    if(!nf_pause133.depth || --nf_pause133.depth)return;
    int verified=nf_pause133_verified();
    nf_pacer93_commit(&nf_pause133.pacer,nf_pause133.start,period,verified);
    nf_pause133.verified+=verified;
    if(++nf_pause133.calls%250==0)
        fprintf(stderr,"[PAUSE-RATE133] outer=%u verified=%u waits=%u waited_ms=%.3f\n",
            nf_pause133.calls,nf_pause133.verified,nf_pause133.waits,
            1000.0*nf_pause133.waited/nf_game94_frequency);
}
#endif
