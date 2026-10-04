#include "driving_diag510.h" /*510 private default-OFF diagnostics*/
/* Driving PAL SDK dispatcher-object boundary. Nt* HANDLE APIs remain separate.
 * The core is byte-identical to analysis/wait143/dispatcher_candidate.h (73 checks).
 * All guest pointers are validated before the core uses them. Object lifetime
 * while blocked remains the caller's contract, as for the original kernel API.
 * No guest scheduler/APC/wait-list or hardware completion emulation is claimed. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "driving_dispatcher143.h"
#include "driving_dispatcher143_core.h"
extern ptrdiff_t g_xbox_mem_offset;
extern size_t g_xbox_map_size,g_xbox_total_ram;

DRIVING_WAIT143_NORETURN void driving_wait143_stop(const char *operation,uint32_t va) {
    fprintf(stderr,"[DRIVING WAIT143 STOP] thread=%lu operation=%s guest=%08X error=%lu\n",
            GetCurrentThreadId(),operation,va,GetLastError());
    fflush(stderr);
    driving_diag510_terminal("dispatcher-stop");ExitProcess(2);
}
/* Only current low RAM and the separate contiguous RAM window are admitted.
 * Generic hardware apertures, thunk VAs, and unverified mirror aliases are not
 * dispatcher storage. VirtualQuery also rejects unmapped/protected subranges. */
static void *wait143_guest(uint32_t va,size_t bytes,int write,const char *operation) {
    uint64_t end=(uint64_t)va+bytes;
    size_t low_bytes=g_xbox_map_size?g_xbox_map_size:g_xbox_total_ram;
    uintptr_t first=(uintptr_t)va+(uintptr_t)g_xbox_mem_offset,cur=first,last;
    if(!va||!bytes||end>0x100000000ull||
       !((va>=0x10000u&&end<=low_bytes)||(va>=0x80000000u&&end<=0x84000000ull))||
       first>UINTPTR_MAX-bytes) driving_wait143_stop(operation,va);
    last=first+bytes;
    while(cur<last) {
        MEMORY_BASIC_INFORMATION m;DWORD p;uintptr_t next;
        if(!VirtualQuery((void*)cur,&m,sizeof(m))||m.State!=MEM_COMMIT||
           (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) driving_wait143_stop(operation,va);
        p=m.Protect&0xFF;
        if(write?!(p==PAGE_READWRITE||p==PAGE_EXECUTE_READWRITE):
                 !(p==PAGE_READONLY||p==PAGE_READWRITE||p==PAGE_EXECUTE_READ||p==PAGE_EXECUTE_READWRITE))
            driving_wait143_stop(operation,va);
        next=(uintptr_t)m.BaseAddress+m.RegionSize;
        if(next<=cur) driving_wait143_stop(operation,va);
        cur=next<last?next:last;
    }
    return (void*)first;
}
static Wait143Header *wait143_object(uint32_t va,const char *operation) {
    Wait143Header*h=wait143_guest(va,sizeof(*h),1,operation);
    if(va&3) driving_wait143_stop("unaligned dispatcher object",va);
    return h;
}
static uint32_t wait143_objects(unsigned count,const uint32_t *vas,unsigned type,
                               unsigned alertable,uint32_t timeout_va) {
    Wait143Header*objects[64];LARGE_INTEGER timeout,*p=NULL;uint32_t status;unsigned i;
    if(!count||count>64||type>1||alertable)
        driving_wait143_stop("unsupported wait count/type/alertable",count?vas[0]:0);
    for(i=0;i<count;i++) objects[i]=wait143_object(vas[i],"invalid wait object");
    if(timeout_va) {memcpy(&timeout,wait143_guest(timeout_va,8,0,"invalid timeout"),8);p=&timeout;}
    driving_diag510_wait(count,vas,type,p!=NULL,p?p->QuadPart:0,1);
    status=wait143_wait(count,objects,type,0,p);
    driving_diag510_wait_phase(0);
    if((int32_t)status<0) driving_wait143_stop("unsupported/failed dispatcher wait",vas[0]);
    return status;
}
uint32_t driving_wait143_single(uint32_t object,unsigned alertable,uint32_t timeout) {
    return wait143_objects(1,&object,1,alertable,timeout);
}
uint32_t driving_wait143_multiple(unsigned count,uint32_t objects,unsigned type,
                                 unsigned alertable,uint32_t timeout) {
    uint32_t vas[64];
    if(!count||count>64) driving_wait143_stop("invalid multiple count",objects);
    memcpy(vas,wait143_guest(objects,count*4,0,"invalid object array"),count*4);
    return wait143_objects(count,vas,type,alertable,timeout);
}
uint32_t driving_wait143_set_event(uint32_t event,unsigned wait_next) {
    Wait143Header*h=wait143_object(event,"invalid event");LONG previous=0;
    if(wait_next||h->type>1) driving_wait143_stop("unsupported SetEvent type/WaitNext",event);
    if(wait143_set(h,1,&previous)) driving_wait143_stop("failed SetEvent",event);
    return (uint32_t)previous;
}
void driving_wait143_initialize_timer(uint32_t timer,unsigned type) {
    Wait143Header*h=wait143_guest(timer,40,1,"invalid timer initialization");
    if((timer&3)||type>1) driving_wait143_stop("invalid timer type/alignment",timer);
    AcquireSRWLockExclusive(&wait143_lock);
    /* Preserve the retained bridge's 40-byte guest layout exactly. */
    memset(h,0,40);h->type=(uint8_t)(8+type);
    ReleaseSRWLockExclusive(&wait143_lock);
}
void driving_wait143_validate_timer(uint32_t timer) {
    Wait143Header*h=wait143_object(timer,"invalid timer");
    if(h->type!=8&&h->type!=9) driving_wait143_stop("unsupported timer object",timer);
}
void driving_wait143_reset_timer(uint32_t timer) {
    driving_wait143_validate_timer(timer);
    if(wait143_set(wait143_object(timer,"timer reset"),0,NULL)) driving_wait143_stop("timer reset failed",timer);
}
void driving_wait143_signal_timer(uint32_t timer) {
    driving_wait143_validate_timer(timer);
    if(wait143_set(wait143_object(timer,"timer expiry"),1,NULL)) driving_wait143_stop("timer expiry failed",timer);
}
