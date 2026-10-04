#ifndef DRIVING_COMMAND262_H
#define DRIVING_COMMAND262_H
#include "driving_collect214.h"
/* Host-only program/constant payloads while all draw interception is idle.
 * Caller preserves every execute callback and owns no bytes across drains. */
static unsigned driving_command262_words(const DrivingPB143 *p,uint32_t put,
 const DrivingCollector214 *c,unsigned held,unsigned active,unsigned object_class,
 unsigned begin_known,unsigned begin,unsigned capture_or_unknown,unsigned cap)
{
 if(!p||!c||!cap||capture_or_unknown||held||active||object_class!=0x97||
    !begin_known||begin||c->phase!=DC214_IDLE||p->subchannel||
    (p->increment!=0&&p->increment!=4)||!p->remaining||(p->method&3)||
    p->method<0xb00||p->method>=0xc00||
    (p->cursor&3)||(put&3)||p->cursor>=0x04000000u||
    put>=0x08000000u||put<=p->cursor)return 0;
 unsigned n=p->remaining;
#define DC262_MIN(v) do{unsigned lim=(v);if(n>lim)n=lim;}while(0)
 DC262_MIN(cap);DC262_MIN(256);DC262_MIN((put-p->cursor)/4);
 DC262_MIN((0x04000000u-p->cursor)/4);
 DC262_MIN((4096-(p->cursor&4095))/4);
 if(p->increment)DC262_MIN((0x80-(p->method&0x7f))/4);
#undef DC262_MIN
 return n>1?n:0;
}
#endif
