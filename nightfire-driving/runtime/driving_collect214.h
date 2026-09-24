/* Candidate only; no live hook. Caller owns state/DMA/shader admission.
 * Single-threaded, non-reentrant protocol. No allocation and no drawing.
 * A rejected current command is NEVER buffered or emitted here. Replay the
 * exact prefix, then forward that current command through the normal path.
 * Callback must synchronously forward baseline commands and must not reenter.
 * GPU replacement must have no visible side effects on failure before replay.
 */
#ifndef DRIVING_COLLECT214_H
#define DRIVING_COLLECT214_H
#include <stdint.h>
#include <string.h>
enum {DRIVING_COLLECT214_METHODS=4096,DRIVING_COLLECT214_INDICES=8192};
enum {DC214_IDLE,DC214_ACTIVE,DC214_COMPLETE,DC214_FLUSH};
enum {DC214_PASS,DC214_HELD,DC214_READY,DC214_FLUSH_BEFORE_CURRENT};
enum {DC214_NO_REASON,DC214_STATE_CHANGE,DC214_MIXED,DC214_METHOD_LIMIT,
      DC214_INDEX_LIMIT,DC214_ARRAY_RANGE,DC214_BARRIER,DC214_PENDING_RESULT};
typedef struct {uint32_t method,value;} DrivingCommand214;
typedef struct {
    unsigned phase,reason,primitive,kind,method_count,index_count;
    DrivingCommand214 methods[DRIVING_COLLECT214_METHODS];
    uint32_t indices[DRIVING_COLLECT214_INDICES];
} DrivingCollector214;
typedef void (*DrivingEmit214)(void *,uint32_t,uint32_t);
static void driving_collect214_reset(DrivingCollector214 *c)
{
    /* Data need not be erased; counts/phase control all subsequent access. */
    c->phase=DC214_IDLE;c->reason=DC214_NO_REASON;c->primitive=c->kind=0;
    c->method_count=c->index_count=0;
}
static int driving_collect214_begin(DrivingCollector214 *c,unsigned primitive)
{
    if(c->phase!=DC214_IDLE || (primitive!=5 && primitive!=6))return 0;
    c->phase=DC214_ACTIVE;c->primitive=primitive;c->reason=DC214_NO_REASON;
    c->kind=c->index_count=0;c->method_count=1;
    c->methods[0].method=0x17fc;c->methods[0].value=primitive;
    return 1;
}
static unsigned driving_collect214_refuse(DrivingCollector214 *c,unsigned reason)
{
    c->phase=DC214_FLUSH;c->reason=reason;return DC214_FLUSH_BEFORE_CURRENT;
}
static unsigned driving_collect214_feed(DrivingCollector214 *c,uint32_t method,uint32_t value)
{
    unsigned kind=0,count=0;uint32_t first=0;
    if(c->phase==DC214_IDLE)return DC214_PASS;
    if(c->phase==DC214_FLUSH)return DC214_FLUSH_BEFORE_CURRENT;
    if(c->phase==DC214_COMPLETE)return DC214_FLUSH_BEFORE_CURRENT;
    if(method==0x17fc && !value){
        if(c->method_count==DRIVING_COLLECT214_METHODS)
            return driving_collect214_refuse(c,DC214_METHOD_LIMIT);
        c->methods[c->method_count++]=(DrivingCommand214){method,value};
        c->phase=DC214_COMPLETE;return DC214_READY;
    }
    if(method==0x1800){kind=1;count=2;}
    else if(method==0x1808){kind=1;count=1;}
    else if(method==0x1810){
        kind=2;first=value&0xffffff;count=(value>>24)+1;
        if((uint64_t)first+count>0x1000000ULL)
            return driving_collect214_refuse(c,DC214_ARRAY_RANGE);
    }else return driving_collect214_refuse(c,DC214_STATE_CHANGE);
    if(c->kind && c->kind!=kind)return driving_collect214_refuse(c,DC214_MIXED);
    if(c->method_count==DRIVING_COLLECT214_METHODS)
        return driving_collect214_refuse(c,DC214_METHOD_LIMIT);
    if(count>DRIVING_COLLECT214_INDICES-c->index_count)
        return driving_collect214_refuse(c,DC214_INDEX_LIMIT);
    c->methods[c->method_count++]=(DrivingCommand214){method,value};c->kind=kind;
    if(method==0x1800){c->indices[c->index_count++]=value&65535;c->indices[c->index_count++]=value>>16;}
    else if(method==0x1808)c->indices[c->index_count++]=value;
    else for(unsigned i=0;i<count;i++)c->indices[c->index_count++]=first+i;
    return DC214_HELD;
}
static int driving_collect214_barrier(DrivingCollector214 *c)
{
    if(c->phase==DC214_IDLE)return 0;
    c->phase=DC214_FLUSH;c->reason=DC214_BARRIER;return 1;
}
static const uint32_t *driving_collect214_result(const DrivingCollector214 *c,unsigned *count)
{
    if(count)*count=c->phase==DC214_COMPLETE?c->index_count:0;
    return c->phase==DC214_COMPLETE?c->indices:NULL;
}
static int driving_collect214_commit(DrivingCollector214 *c)
{
    if(c->phase!=DC214_COMPLETE)return 0;
    driving_collect214_reset(c);return 1;
}
static int driving_collect214_replay(DrivingCollector214 *c,DrivingEmit214 emit,void *ctx)
{
    if(c->phase==DC214_IDLE)return 1;
    if(!emit)return 0;
    for(unsigned i=0;i<c->method_count;i++)emit(ctx,c->methods[i].method,c->methods[i].value);
    driving_collect214_reset(c);return 1;
}
#endif
