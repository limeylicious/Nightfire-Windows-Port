/* Diagnostic-only single interval. All GPU completion stays unchanged.
 * Metadata/handler live for process lifetime. No locks, allocations, stdio,
 * guest-memory loads, symbol lookup or GPU calls inside the VEH. */
#include <windows.h>
#include <intrin.h>
#include <stdint.h>
#include <string.h>
#include "nightfire_surface_probe83.h"
#ifndef NIGHTFIRE_SURFACE_PROBE_DIAGNOSTIC
#error Build this file only in the private surface-observer diagnostic.
#endif

typedef struct NF83Range {uintptr_t base,allocation;size_t bytes;} NF83Range;
static NF83Range ranges[4];
static unsigned range_count;
static uintptr_t contig_base,tiled_base,color_offset,depth_offset;
static size_t arena_size,color_size,depth_size;
static const void *original_color,*original_depth;
static volatile LONG status,attempted,cancelled,arm_finished,plan_ready;
static volatile LONG protected_mask,handlers,reason,error_code,restore_calls;
static volatile LONG hit_claim,hit_ready,late_retries;
static volatile LONG end_started,end_finished,external_calls;
static unsigned arm_thread,hit_thread,hit_access,hit_arming,hit_class;
static uintptr_t hit_rip,hit_address;
static PVOID handler_registration;
static __declspec(thread) int inside_handler;
static __declspec(thread) uintptr_t last_late_rip,last_late_address;
static __declspec(thread) unsigned same_late_retries;
static LONG read_long(volatile LONG *value){return InterlockedCompareExchange(value,0,0);}
static void wait_zero(volatile LONG *value,int want_zero);

static void fail_probe(unsigned error){
 InterlockedExchange(&error_code,(LONG)error);
 InterlockedExchange(&status,NF83_FAILED);
 /* Never let the private diagnostic run with a stranded NOACCESS page. */
 TerminateProcess(GetCurrentProcess(),0xe0830000u|(error&0xffffu));
 __fastfail(7);
}
static int add_ok(uintptr_t p,size_t n){return n<=UINTPTR_MAX-p;}
static int in_range(uintptr_t p,uintptr_t base,size_t n){return p>=base && p-base<n;}
static int query_range(const NF83Range *r,DWORD required){
 uintptr_t at=r->base,end=r->base+r->bytes;
 while(at<end){
  MEMORY_BASIC_INFORMATION mbi;
  if(VirtualQuery((const void*)at,&mbi,sizeof mbi)!=sizeof mbi ||
     mbi.State!=MEM_COMMIT || mbi.Type!=MEM_MAPPED ||
     mbi.Protect!=required || (uintptr_t)mbi.AllocationBase!=r->allocation ||
     (uintptr_t)mbi.BaseAddress>at || !mbi.RegionSize ||
     !add_ok((uintptr_t)mbi.BaseAddress,mbi.RegionSize))return 0;
  uintptr_t next=(uintptr_t)mbi.BaseAddress+mbi.RegionSize;
  if(next<=at)return 0;
  at=next<end?next:end;
 }
 return 1;
}
static void restore_all(void){
 if(!read_long(&plan_ready) || !read_long(&protected_mask))return;
 for(unsigned i=0;i<range_count;i++){
  DWORD previous;
  if(!VirtualProtect((void*)ranges[i].base,ranges[i].bytes,PAGE_READWRITE,&previous))
   fail_probe(10);
  InterlockedIncrement(&restore_calls);
 }
}
static unsigned classify(uintptr_t address){
 uintptr_t offset;unsigned result=0;
 if(in_range(address,contig_base,arena_size))offset=address-contig_base;
 else if(in_range(address,tiled_base,arena_size)){offset=address-tiled_base;result=4;}
 else return 0;
 if(in_range(offset,color_offset,color_size))result|=1;
 if(in_range(offset,depth_offset,depth_size))result|=2;
 return result;
}
static LONG CALLBACK surface_handler(EXCEPTION_POINTERS *ep){
 if(!ep || !ep->ExceptionRecord || !ep->ContextRecord)return EXCEPTION_CONTINUE_SEARCH;
 EXCEPTION_RECORD *record=ep->ExceptionRecord;
 if(record->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION ||
    (record->ExceptionFlags&EXCEPTION_NONCONTINUABLE) || record->NumberParameters<2 ||
    record->ExceptionInformation[0]>1 || !read_long(&plan_ready))return EXCEPTION_CONTINUE_SEARCH;
 uintptr_t address=(uintptr_t)record->ExceptionInformation[1];
 unsigned index=range_count;
 for(unsigned i=0;i<range_count;i++)
  if(in_range(address,ranges[i].base,ranges[i].bytes)){index=i;break;}
 if(index==range_count || !(read_long(&protected_mask)&(1L<<index)))return EXCEPTION_CONTINUE_SEARCH;
 if(inside_handler)fail_probe(11);
 inside_handler=1;InterlockedIncrement(&handlers);
 LONG current=read_long(&status);
 LONG result=EXCEPTION_CONTINUE_SEARCH;
 MEMORY_BASIC_INFORMATION mbi;
 int mapped=VirtualQuery((const void*)address,&mbi,sizeof mbi)==sizeof mbi &&
  mbi.State==MEM_COMMIT && mbi.Type==MEM_MAPPED &&
  (uintptr_t)mbi.AllocationBase==ranges[index].allocation;
 if((current==NF83_ARMING || current==NF83_ARMED) && mapped && mbi.Protect==PAGE_NOACCESS){
  if(current==NF83_ARMING)InterlockedExchange(&cancelled,1);
  if(InterlockedCompareExchange(&hit_claim,1,0)==0){
   hit_thread=GetCurrentThreadId();hit_access=(unsigned)record->ExceptionInformation[0];
   hit_arming=current==NF83_ARMING;hit_rip=(uintptr_t)ep->ContextRecord->Rip;
   hit_address=address;hit_class=classify(address);
   InterlockedExchange(&hit_ready,1);
  }
  /* The armer observes cancellation before/after each protection call and
   * rolls back again after its last possible call. Ordinary end waits for
   * active handlers before permitting guest protection/lifetime changes. */
  restore_all();
  result=EXCEPTION_CONTINUE_EXECUTION;
 }else if(mapped && mbi.Protect==PAGE_READWRITE &&
          (read_long(&hit_claim) || read_long(&cancelled) || current==NF83_ENDED)){
  /* Fault delivery can lag behind restoration. Never re-protect or restore in
   * this branch: end may be followed by a legitimate guest lifetime change.
   * Retry only the known old range still mapped with its original RW access.
   * Fixed TLS bound prevents endlessly swallowing an unrelated repeated AV. */
  uintptr_t rip=(uintptr_t)ep->ContextRecord->Rip;
  if(rip==last_late_rip && address==last_late_address)same_late_retries++;
  else {last_late_rip=rip;last_late_address=address;same_late_retries=1;}
  if(same_late_retries<=4){InterlockedIncrement(&late_retries);result=EXCEPTION_CONTINUE_EXECUTION;}
 }
 InterlockedDecrement(&handlers);inside_handler=0;
 return result;
}
#ifdef NF_SURFACE_PROBE83_TEST
extern void nf_surface_probe_test_after_protect(unsigned index);
extern void nf_surface_probe_test_before_end_restore(void);
LONG nf_surface_probe_test_exception(EXCEPTION_POINTERS *ep){return surface_handler(ep);}
#endif

static int reject_probe(unsigned error){
 InterlockedExchange(&error_code,(LONG)error);
 InterlockedExchange(&status,NF83_REJECTED);
 InterlockedExchange(&arm_finished,1);
 return 0;
}
int nf_surface_probe_arm(const void *color,size_t cb,const void *depth,size_t db,
 uintptr_t contiguous_base,uintptr_t shared_tiled_base,size_t arena_bytes){
 if(InterlockedCompareExchange(&status,NF83_VALIDATING,NF83_UNUSED)!=NF83_UNUSED)return 0;
 InterlockedExchange(&attempted,1);arm_thread=GetCurrentThreadId();
 if(read_long(&external_calls))return reject_probe(6);
 SYSTEM_INFO si;GetSystemInfo(&si);size_t page=si.dwPageSize;
 original_color=color;original_depth=depth;color_size=cb;depth_size=db;
 contig_base=contiguous_base;tiled_base=shared_tiled_base;arena_size=arena_bytes;
 if(!page || !color || !depth || !cb || !db || !arena_bytes ||
    contiguous_base%page || shared_tiled_base%page || cb%page || db%page ||
    !add_ok(contiguous_base,arena_bytes) || !add_ok(shared_tiled_base,arena_bytes) ||
    !(contiguous_base+arena_bytes<=shared_tiled_base || shared_tiled_base+arena_bytes<=contiguous_base))
  return reject_probe(1);
 uintptr_t addresses[2]={(uintptr_t)color,(uintptr_t)depth},offsets[2];size_t sizes[2]={cb,db};
 for(unsigned i=0;i<2;i++){
  uintptr_t a=addresses[i];
  if(a%page)return reject_probe(1);
  if(in_range(a,contiguous_base,arena_bytes))offsets[i]=a-contiguous_base;
  else if(in_range(a,shared_tiled_base,arena_bytes))offsets[i]=a-shared_tiled_base;
  else return reject_probe(1);
  if(sizes[i]>arena_bytes-offsets[i])return reject_probe(1);
 }
 color_offset=offsets[0];depth_offset=offsets[1];
 if(offsets[1]<offsets[0]){uintptr_t o=offsets[0];offsets[0]=offsets[1];offsets[1]=o;size_t n=sizes[0];sizes[0]=sizes[1];sizes[1]=n;}
 unsigned physical_count=2;
 if(offsets[1]<=offsets[0]+sizes[0]){
  uintptr_t end0=offsets[0]+sizes[0],end1=offsets[1]+sizes[1];
  sizes[0]=(end0>end1?end0:end1)-offsets[0];physical_count=1;
 }
 range_count=physical_count*2;
 for(unsigned view=0;view<2;view++)for(unsigned i=0;i<physical_count;i++){
  NF83Range *r=&ranges[view*physical_count+i];
  r->allocation=view?shared_tiled_base:contiguous_base;
  r->base=r->allocation+offsets[i];r->bytes=sizes[i];
  if(!query_range(r,PAGE_READWRITE))return reject_probe(2);
 }
 if(read_long(&cancelled)){InterlockedExchange(&status,NF83_ENDED);InterlockedExchange(&arm_finished,1);return 0;}
 handler_registration=AddVectoredExceptionHandler(1,surface_handler);
 if(!handler_registration)return reject_probe(3);
 InterlockedExchange(&plan_ready,1);InterlockedExchange(&status,NF83_ARMING);
 for(unsigned i=0;i<range_count;i++){
  if(read_long(&cancelled))break;
  DWORD previous;
  /* Publish may-have-changed ownership before the kernel can make it fault. */
  InterlockedOr(&protected_mask,1L<<i);
  if(!VirtualProtect((void*)ranges[i].base,ranges[i].bytes,PAGE_NOACCESS,&previous)){
   InterlockedExchange(&error_code,4);InterlockedExchange(&cancelled,1);break;
  }
  if(previous!=PAGE_READWRITE){InterlockedExchange(&error_code,5);InterlockedExchange(&cancelled,1);break;}
#ifdef NF_SURFACE_PROBE83_TEST
  nf_surface_probe_test_after_protect(i);
#endif
  if(read_long(&cancelled))break;
 }
 if(read_long(&cancelled)){
  restore_all();InterlockedExchange(&status,read_long(&error_code)?NF83_REJECTED:NF83_ENDED);
  wait_zero(&handlers,1);
  InterlockedExchange(&arm_finished,1);return 0;
 }
 InterlockedExchange(&status,NF83_ARMED);
 if(read_long(&cancelled)){
  restore_all();InterlockedExchange(&status,NF83_ENDED);wait_zero(&handlers,1);
  InterlockedExchange(&arm_finished,1);return 0;
 }
 InterlockedExchange(&arm_finished,1);
 return 1;
}
static void wait_zero(volatile LONG *value,int want_zero){
 ULONGLONG start=GetTickCount64();
 while(want_zero?read_long(value)!=0:read_long(value)==0){
  if(GetTickCount64()-start>5000)fail_probe(12);
  SwitchToThread();
 }
}
void nf_surface_probe_end(unsigned end_reason){
 if(read_long(&status)==NF83_UNUSED)return;
 if(InterlockedCompareExchange(&end_started,1,0)!=0){wait_zero(&end_finished,0);return;}
 InterlockedCompareExchange(&reason,(LONG)end_reason,0);
 InterlockedExchange(&cancelled,1);
 /* Never call synchronously from an arming test callback or a VEH. */
 wait_zero(&arm_finished,0);
 LONG old=read_long(&status);
#ifdef NF_SURFACE_PROBE83_TEST
 nf_surface_probe_test_before_end_restore();
#endif
 /* Keep first-fault ownership visible until every NOACCESS span has been
  * restored. Publishing ENDED first would send a racing owned fault to the
  * normal crash handler during the restoration gap. Arming has finished, so
  * nothing can protect a page again; in-flight handlers only restore RW. */
 restore_all();
 if(old!=NF83_REJECTED && old!=NF83_FAILED)InterlockedExchange(&status,NF83_ENDED);
 wait_zero(&handlers,1);
 /* Concurrent ARMING/handler restoration only writes RW. This final pass
  * completes before callers can legitimately protect/unmap/reuse the range. */
 restore_all();
 InterlockedExchange(&end_finished,1);
}
void nf_surface_probe_external_enter(unsigned end_reason){
 DWORD saved=GetLastError();InterlockedIncrement(&external_calls);
 nf_surface_probe_end(end_reason);SetLastError(saved);
}
void nf_surface_probe_external_leave(void){
 DWORD saved=GetLastError();
 if(InterlockedDecrement(&external_calls)<0)fail_probe(13);
 SetLastError(saved);
}
void nf_surface_probe_snapshot(NF83SurfaceSnapshot *out){
 if(!out)return;memset(out,0,sizeof *out);
 out->attempted=(unsigned)read_long(&attempted);out->status=(unsigned)read_long(&status);
 out->reason=(unsigned)read_long(&reason);out->arm_finished=(unsigned)read_long(&arm_finished);
 out->active_handlers=(unsigned)read_long(&handlers);out->error=(unsigned)read_long(&error_code);
 out->external_calls=(unsigned)read_long(&external_calls);
 out->late_retries=(unsigned)read_long(&late_retries);out->restore_calls=(unsigned)read_long(&restore_calls);
 if(!out->arm_finished || out->active_handlers)return;
 out->arm_thread=arm_thread;out->range_count=range_count;
 out->protected_mask=(unsigned)read_long(&protected_mask);
 out->color=(uintptr_t)original_color;out->depth=(uintptr_t)original_depth;
 out->color_bytes=color_size;out->depth_bytes=depth_size;
 out->contiguous_base=contig_base;out->tiled_base=tiled_base;out->arena_bytes=arena_size;
 if(read_long(&hit_ready)){
  out->first_hit=1;out->hit_thread=hit_thread;out->access=hit_access;
  out->hit_during_arming=hit_arming;out->rip=hit_rip;out->address=hit_address;
  out->classification=hit_class;
 }
}
void nf_surface_probe_report(FILE *stream){
 if(!stream)return;NF83SurfaceSnapshot s;nf_surface_probe_snapshot(&s);
 fprintf(stream,"[SURFACE83] attempted=%u status=%u reason=%u error=%u ranges=%u protected=%x arm_finished=%u handlers=%u first_hit=%u thread=%u access=%u arming=%u class=%u rip=%llx address=%llx late=%u restores=%u\n",
 s.attempted,s.status,s.reason,s.error,s.range_count,s.protected_mask,s.arm_finished,s.active_handlers,
 s.first_hit,s.hit_thread,s.access,s.hit_during_arming,s.classification,
 (unsigned long long)s.rip,(unsigned long long)s.address,s.late_retries,s.restore_calls);
 fprintf(stream,"[SURFACE83-RANGES] color=%llx bytes=%llu depth=%llx bytes=%llu contig=%llx tiled=%llx arena=%llu arm_thread=%u\n",
 (unsigned long long)s.color,(unsigned long long)s.color_bytes,(unsigned long long)s.depth,
 (unsigned long long)s.depth_bytes,(unsigned long long)s.contiguous_base,
 (unsigned long long)s.tiled_base,(unsigned long long)s.arena_bytes,s.arm_thread);
}
