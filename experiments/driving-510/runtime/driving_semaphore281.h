/*281: one current-process aligned GPU release, after original completion.
 * The shared264 guard protects the exact query/store interval, not guest data
 * ownership. Every non-RW or unsupported span retains unchanged WPM270. */
#ifndef DRIVING_SEMAPHORE281_H
#define DRIVING_SEMAPHORE281_H
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driving_writeguard270.h"
static int semaphore281_setting=-1;
static struct {uint64_t attempts,direct,queries,bounds,retired,protection,exceptions,fallback_ok,fallback_bad;} semaphore281_counts;
static int semaphore281_enabled(void){
 DWORD error=GetLastError();if(semaphore281_setting<0){const char*v=getenv("DRIVING_SEMAPHORE281");semaphore281_setting=v&&!strcmp(v,"1");}
 int result=semaphore281_setting&&driving_permissions264_enabled();SetLastError(error);return result;
}
static LONG semaphore281_fault(EXCEPTION_POINTERS *e,uintptr_t address){
 EXCEPTION_RECORD*r=e->ExceptionRecord;
 return (r->ExceptionCode==EXCEPTION_ACCESS_VIOLATION||r->ExceptionCode==EXCEPTION_IN_PAGE_ERROR)&&r->NumberParameters>=2&&r->ExceptionInformation[0]==1&&
  r->ExceptionInformation[1]>=address&&r->ExceptionInformation[1]-address<4?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH;
}
#ifndef DRIVING_SEMAPHORE281_EXCHANGE
#define DRIVING_SEMAPHORE281_EXCHANGE(p,v) InterlockedExchange((p),(v))
#endif
static BOOL semaphore281_write(uint32_t destination,uint32_t value,uintptr_t offset,uint32_t allocated,SIZE_T *written){
 DWORD entry_error=GetLastError();uintptr_t address=offset+destination;BOOL stored=FALSE;
 PC508_GUARD_GUEST(destination,4,"semaphore281.destination");
 if(written)PC508_GUARD_NATIVE(written,sizeof *written,"semaphore281.written");
 if(!semaphore281_enabled())goto fallback;
 semaphore281_counts.attempts++;
 if((destination&3)||destination<0x80000000u||destination>0x83fffffcu||allocated<4||allocated>0x04000000u||
  destination-0x80000000u>allocated-4||offset>UINTPTR_MAX-destination||address>UINTPTR_MAX-4||(address&3)){
  semaphore281_counts.bounds++;goto fallback;
 }
 {
  uint64_t generation=driving_permissions264_read_begin();
  __try {
   if(!generation){semaphore281_counts.retired++;}
   else{
    MEMORY_BASIC_INFORMATION m;semaphore281_counts.queries++;
    if(VirtualQuery((const void*)address,&m,sizeof m)==sizeof m&&m.State==MEM_COMMIT&&m.Type==MEM_PRIVATE&&m.Protect==PAGE_READWRITE&&
     (uintptr_t)m.AllocationBase==offset+0x80000000u&&(uintptr_t)m.BaseAddress<=address&&m.RegionSize<=UINTPTR_MAX-(uintptr_t)m.BaseAddress&&address+4<=(uintptr_t)m.BaseAddress+m.RegionSize){
     LONG bits;memcpy(&bits,&value,sizeof bits);
     __try {DRIVING_SEMAPHORE281_EXCHANGE((volatile LONG*)address,bits);stored=TRUE;}
     __except(semaphore281_fault(GetExceptionInformation(),address)){semaphore281_counts.exceptions++;}
    }else semaphore281_counts.protection++;
   }
  } __finally {driving_permissions264_read_end();}
 }
 if(stored){if(written)*written=4;semaphore281_counts.direct++;SetLastError(entry_error);return TRUE;}
fallback:
 /* Never upgrade the nonrecursive SRW lock: WPM270 takes exclusive ownership
  * only after our shared interval has ended, preserving its API semantics. */
 SetLastError(entry_error);
 {BOOL result=driving_write_process270(GetCurrentProcess(),(void*)address,&value,4,written);
  DWORD error=GetLastError();if(result&&(!written||*written==4))semaphore281_counts.fallback_ok++;else semaphore281_counts.fallback_bad++;
  SetLastError(error);return result;}
}
static void semaphore281_report(void){
 DWORD e=GetLastError();if(semaphore281_enabled())fprintf(stderr,"[SEMAPHORE281] attempts=%llu direct=%llu queries=%llu bounds=%llu retired=%llu protection=%llu exceptions=%llu fallback_ok=%llu fallback_bad=%llu completion-barriers-unchanged=1 direct-content-only-no-generation-bump=1\n",
 (unsigned long long)semaphore281_counts.attempts,(unsigned long long)semaphore281_counts.direct,(unsigned long long)semaphore281_counts.queries,
 (unsigned long long)semaphore281_counts.bounds,(unsigned long long)semaphore281_counts.retired,(unsigned long long)semaphore281_counts.protection,(unsigned long long)semaphore281_counts.exceptions,
 (unsigned long long)semaphore281_counts.fallback_ok,(unsigned long long)semaphore281_counts.fallback_bad);SetLastError(e);
}
#endif
