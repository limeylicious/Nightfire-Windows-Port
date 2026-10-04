#ifndef DRIVING_LEASE322_CORE_H
#define DRIVING_LEASE322_CORE_H
#include "driving_lease322.h"
#include "driving_contig_reuse237.h"
#include <errno.h>
#include "driving_pc508_guards.h"
typedef struct { DC237 *allocator; SRWLOCK *lock; uintptr_t offset; } DL322Context;
/* Called with this allocator's shared or exclusive lock held. Validate the
 * actual native private backing; never fold low/F/800 addresses together. */
static int dl322_key_locked(const DL322Context *c,uint32_t va,uint32_t bytes,
                           const void *native,DL322Key *out){
 DL322Key k={0};MEMORY_BASIC_INFORMATION m;
 if(!out||!bytes||c->offset>UINTPTR_MAX-va)return 0;
 k.va=va;k.bytes=bytes;k.native=c->offset+va;
 if(k.native!=(uintptr_t)native||bytes>UINTPTR_MAX-k.native||
    !dc237_snapshot311(c->allocator,va,bytes,&k.base,&k.size,&k.generation)||
    VirtualQuery(native,&m,sizeof m)!=sizeof m||m.State!=MEM_COMMIT||m.Type!=MEM_PRIVATE||
    (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))||
    (m.Protect&255u)!=PAGE_READWRITE||k.native<(uintptr_t)m.BaseAddress||
    bytes>m.RegionSize-(k.native-(uintptr_t)m.BaseAddress))return 0;
 k.backing=(uintptr_t)m.AllocationBase;*out=k;return 1;
}
static int dl322_snapshot(const DL322Context *c,DL322Lease *owner,uint32_t va,
                         uint32_t bytes,const void *native,DL322Key *out){
 DWORD error=GetLastError();int crt=errno;
 int held=owner&&owner->active&&owner->thread==GetCurrentThreadId()&&owner->lock==c->lock&&owner->allocator==c->allocator;
 if(owner&&!held){errno=crt;SetLastError(error);return 0;}
 if(!held){AcquireSRWLockShared(c->lock);PC508_DEPTH_ENTER(1);}
 int ok=dl322_key_locked(c,va,bytes,native,out);
 if(!held){ReleaseSRWLockShared(c->lock);PC508_DEPTH_LEAVE(1);}
 errno=crt;SetLastError(error);return ok;
}
static int dl322_same(const DL322Key *a,const DL322Key *b){
 return a->va==b->va&&a->bytes==b->bytes&&a->base==b->base&&a->size==b->size&&
 a->generation==b->generation&&a->native==b->native&&a->backing==b->backing;
}
/* owner is caller TLS. Refuse nested, copied-token, unknown-purpose and stale
 * generation requests before granting a pin. Both target keys are checked
 * under one lock, which remains held until publication completes. */
static int dl322_begin(const DL322Context *c,DL322Lease **owner,
                       const DL322Key keys[2],DL322Lease *lease,unsigned purpose){
 DWORD error=GetLastError();int crt=errno,ok=0;
 if(!owner||*owner||!keys||!lease||lease->active||purpose!=DL322_PUBLISH_EXISTING)goto done;
 AcquireSRWLockShared(c->lock);PC508_DEPTH_ENTER(1);
 DL322Key now[2];
 for(unsigned i=0;i<2;i++)if(!dl322_key_locked(c,keys[i].va,keys[i].bytes,(void*)keys[i].native,&now[i])||!dl322_same(&keys[i],&now[i]))goto unlock;
 if(keys[0].native<keys[1].native+keys[1].bytes&&keys[1].native<keys[0].native+keys[0].bytes)goto unlock;
 lease->lock=c->lock;lease->allocator=c->allocator;lease->offset=c->offset;
 lease->thread=GetCurrentThreadId();lease->active=1;*owner=lease;ok=1;goto done;
unlock:ReleaseSRWLockShared(c->lock);PC508_DEPTH_LEAVE(1);
done:errno=crt;SetLastError(error);return ok;
}
static int dl322_end(const DL322Context *c,DL322Lease **owner,DL322Lease *lease){
 DWORD error=GetLastError();int crt=errno,ok=0;
 if(!owner||!lease||*owner!=lease||!lease->active||lease->thread!=GetCurrentThreadId()||
    lease->lock!=c->lock||lease->allocator!=c->allocator||lease->offset!=c->offset)goto done;
 lease->active=0;*owner=NULL;ReleaseSRWLockShared(c->lock);PC508_DEPTH_LEAVE(1);ok=1;
done:errno=crt;SetLastError(error);return ok;
}
#endif
