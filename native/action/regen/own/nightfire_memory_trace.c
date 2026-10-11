/* Preserve the latest native copy call even when the host CRT faults. */
#define RECOMP_GENERATED_CODE
#define NIGHTFIRE_MEMORY_TRACE_IMPL
#include "recomp_types.h"
#include <stdio.h>
static RECOMP_TLS volatile struct {
    const char *function, *operation;
    uintptr_t destination, source;
    size_t length;
    unsigned line, active;
} last_copy;
static void begin(const char *op, void *d, const void *s, size_t n, const char *fn, unsigned line)
{
    last_copy.operation=op; last_copy.destination=(uintptr_t)d;
    last_copy.source=(uintptr_t)s; last_copy.length=n;
    last_copy.function=fn; last_copy.line=line; last_copy.active=1;
}
void *nightfire_trace_memcpy(void *d,const void *s,size_t n,const char *fn,unsigned line)
{
    begin("memcpy",d,s,n,fn,line);
    void *result=memcpy(d,s,n); last_copy.active=0; return result;
}
void *nightfire_trace_memset(void *d,int v,size_t n,const char *fn,unsigned line)
{
    begin("memset",d,NULL,n,fn,line);
    void *result=memset(d,v,n); last_copy.active=0; return result;
}
void nightfire_memory_trace_dump(void)
{
    if (!last_copy.function) return;
    fprintf(stderr,"[COPY-TRACE] %s %s at %s:%u dst-host=%p src-host=%p length=%llu dst-guest=%08X src-guest=%08X\n",
        last_copy.active ? "active" : "last completed",last_copy.operation,
        last_copy.function,last_copy.line,(void *)last_copy.destination,
        (void *)last_copy.source,(unsigned long long)last_copy.length,
        (unsigned)(last_copy.destination-g_xbox_mem_offset),
        last_copy.source ? (unsigned)(last_copy.source-g_xbox_mem_offset) : 0);
}
