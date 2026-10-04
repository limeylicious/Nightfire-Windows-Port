#ifndef DRIVING_COMMAND_READ265_H
#define DRIVING_COMMAND_READ265_H
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driving_permissions264.h"
#ifdef DRIVING_COMMAND_PUBLISH325
#include "driving_publication324.h"
#endif
/* Fresh scalar contents only. Metadata is consumer-thread-local in usage,
 * scoped to a serialized outer drain and permission generation. */
static struct {unsigned depth,valid;uintptr_t begin,end;uint64_t generation;} command_map265;
static struct {uint64_t direct,queries,fallback,exceptions,generation_changes;} command_counts265;
static int command_setting265=-1;
static int command_enabled265(void){
 DWORD error=GetLastError();
 if(command_setting265<0){const char*v=getenv("DRIVING_COMMAND_READ265");command_setting265=v&&!strcmp(v,"1");}
 int result=command_setting265&&driving_permissions264_enabled();SetLastError(error);return result;
}
static void command_enter265(void){command_map265.valid=0;command_map265.depth++;}
static void command_leave265(void){command_map265.valid=0;if(command_map265.depth)command_map265.depth--;}
/* The production hook checks this BEFORE evaluating the allocator highwater
 * argument. Disabled/MMIO/nested paths retain the original RPM cost. */
static int command_eligible265(uint32_t va){
 return command_enabled265()&&command_map265.depth==1&&!(va&3)&&va>=0x80000000u&&va<=0x83fffffcu;
}
/*1 means copied successfully;0 means the caller must execute its unchanged RPM.
 * Caller performs residency_read236 first, outside the permission lock. */
static int command_read265(uint32_t va,uint32_t *value,uintptr_t offset,uint32_t allocated){
 DWORD error=GetLastError();uint32_t result=0;int copied=0;
 if(!command_enabled265()){SetLastError(error);return 0;}
 if(command_map265.depth!=1||!value||(va&3)||va<0x80000000u||va>0x83fffffcu||
    allocated<4||va-0x80000000u>allocated-4||offset>UINTPTR_MAX-va||offset+va>UINTPTR_MAX-4){
  command_counts265.fallback++;SetLastError(error);return 0;
 }
 uintptr_t address=offset+va;uint64_t generation=driving_permissions264_read_begin();
 if(command_map265.generation!=generation){
  if(command_map265.valid)command_counts265.generation_changes++;
  command_map265.valid=0;command_map265.generation=generation;
 }
 if(generation){
  if(!command_map265.valid||address<command_map265.begin||address+4>command_map265.end){
   MEMORY_BASIC_INFORMATION info;command_map265.valid=0;command_counts265.queries++;
   if(VirtualQuery((const void*)address,&info,sizeof info)==sizeof info){
    uintptr_t begin=(uintptr_t)info.BaseAddress;
    if(begin<=address&&info.RegionSize<=UINTPTR_MAX-begin&&address+4<=begin+info.RegionSize&&
       info.State==MEM_COMMIT&&!(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&
       (info.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE))){
     command_map265.begin=begin;command_map265.end=begin+info.RegionSize;command_map265.valid=1;
    }
   }
  }
  if(command_map265.valid){
   /* Best effort only: a process VEH runs before this local SEH handler.
    * Correctness relies on serialized tracked mutations, not exception recovery
    * from arbitrary external protection changes. Never directly read guards. */
#ifdef DRIVING_COMMAND_PUBLISH325
   int shared325=xbox_CommandPublication325Begin();
   __try {
#endif
   __try {result=*(volatile const uint32_t*)address;copied=1;}
   __except((GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION||GetExceptionCode()==EXCEPTION_IN_PAGE_ERROR)
              ?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){
    command_map265.valid=0;command_counts265.exceptions++;
   }
#ifdef DRIVING_COMMAND_PUBLISH325
   } __finally {xbox_CommandPublication325End(shared325);}
#endif
  }
 }
 driving_permissions264_read_end();
 if(copied){*value=result;command_counts265.direct++;}else command_counts265.fallback++;
 SetLastError(error);return copied;
}
static void command_report265(void){
 DWORD error=GetLastError();
 if(command_enabled265())fprintf(stderr,"[COMMAND-READ265] direct=%llu queries=%llu fallback=%llu exceptions=%llu generation_changes=%llu fresh_scalar=1 permission_cache_only=1\n",
  (unsigned long long)command_counts265.direct,(unsigned long long)command_counts265.queries,
  (unsigned long long)command_counts265.fallback,(unsigned long long)command_counts265.exceptions,
  (unsigned long long)command_counts265.generation_changes);
 SetLastError(error);
}
#endif
