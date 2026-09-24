#ifndef DRIVING_COMMAND245_H
#define DRIVING_COMMAND245_H
#include "driving_collect214.h"
/* Owned payload snapshots, never a guest pointer or a permission cache.
 * Include after driving_gpu143_core.h. Only an already-active main collector
 * can prefetch index words. Headers, END, flow control and GET stay scalar. */
static unsigned driving_command245_words(const DrivingPB143 *p,uint32_t put,
 const DrivingCollector214 *c,unsigned family,unsigned held,unsigned object_class,
 unsigned capture_or_unknown,unsigned cap)
{
 if(!p||!c||!cap||capture_or_unknown||held||family!=4||object_class!=0x97||
    p->subchannel||p->increment||!p->remaining||c->phase!=DC214_ACTIVE||
    (c->kind&&c->kind!=1)||(p->method!=0x1800&&p->method!=0x1808)||
    (p->cursor&3)||(put&3)||p->cursor>=0x04000000u||put>=0x08000000u||
    put<=p->cursor||c->method_count>=DRIVING_COLLECT214_METHODS||
    c->index_count>DRIVING_COLLECT214_INDICES)return 0;
 unsigned n=p->remaining,indices=p->method==0x1800?2:1;
#define DC245_MIN(v) do{unsigned lim=(v);if(n>lim)n=lim;}while(0)
 DC245_MIN(cap);DC245_MIN(256);DC245_MIN((put-p->cursor)/4);
 DC245_MIN((0x04000000u-p->cursor)/4);
 DC245_MIN((4096-(p->cursor&4095))/4);
 DC245_MIN(DRIVING_COLLECT214_METHODS-c->method_count);
 DC245_MIN((DRIVING_COLLECT214_INDICES-c->index_count)/indices);
#undef DC245_MIN
 return n>1?n:0;
}
#endif
