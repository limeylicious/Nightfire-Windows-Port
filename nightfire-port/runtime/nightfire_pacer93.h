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
static void nf_pacer93_commit(NFPacer93 *p,uint64_t start,uint64_t period,int verified){
    p->last=start;p->period=period;p->valid=verified!=0;
}
#endif
