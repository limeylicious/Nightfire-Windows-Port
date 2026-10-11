/* Host-only minimum start interval. Late work never creates catch-up debt. */
#ifndef NIGHTFIRE_PACER93_H
#define NIGHTFIRE_PACER93_H
#include <stdint.h>
typedef struct { uint64_t last,period; unsigned valid; } NFPacer93;
static uint64_t nf_pacer93_delay(NFPacer93 *p,uint64_t now,uint64_t period,int eligible){
    if(!eligible || !period || !p->valid || p->period!=period || now<p->last){p->valid=0;return 0;}
    uint64_t elapsed=now-p->last;
    return elapsed<period?period-elapsed:0;
}
/* 2026-10-08 (the owner approved for all Action builds): a frame that started late only because
 * the wait overshot (by less than one period) keeps the ideal timeline, so the average stays
 * at the target rate instead of drifting ~3% slow (each timer overshoot used to be added to
 * every frame: ~58 updates/s at a 60 Hz target). A frame later than one period still
 * restarts the timeline: no catch-up debt. NIGHTFIRE_PACER_TIMELINE=0 restores the old rule.
 * Measured in the native copy (native-driving/action-native/README.md): 59.6-60.0 updates/s. */
#include <stdlib.h>
static int nf_pacer93_timeline(void){
    static int on=-1;
    if(on<0){const char *v=getenv("NIGHTFIRE_PACER_TIMELINE");on=!(v && v[0]=='0');}
    return on;
}
static void nf_pacer93_commit(NFPacer93 *p,uint64_t start,uint64_t period,int verified){
    uint64_t ideal=p->last+p->period;
    if(nf_pacer93_timeline() && p->valid && p->period==period && start>=ideal && start-ideal<period)start=ideal;
    p->last=start;p->period=period;p->valid=verified!=0;
}
#endif
