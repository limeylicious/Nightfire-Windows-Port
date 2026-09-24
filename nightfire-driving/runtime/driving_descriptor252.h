/* Optional fresh12-byte same-process read, retaining scalar fallback/barriers. */
#ifndef DRIVING_DESCRIPTOR252_H
#define DRIVING_DESCRIPTOR252_H
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "driving_descriptor252_core.h"
extern ptrdiff_t xbox_GetMemoryOffset(void);
#ifndef DRIVING_DESCRIPTOR252_BEFORE_READ
#error Descriptor252 requires the existing full-span residency read barrier
#endif
static uint64_t descriptor_bulk252,descriptor_fallback252;
static int descriptor_enabled252(void){static int enabled=-1;if(enabled<0){const char*v=getenv("DRIVING_DESCRIPTOR252");enabled=v&&!strcmp(v,"1");}return enabled;}
static int descriptor_tuple252(void*ctx,uint32_t instance,uint32_t owned[3]){
 (void)ctx;
 if(instance<0xfd700000u||instance>0xfd7ffff0u||(instance&15))return 0;
 uintptr_t offset=(uintptr_t)xbox_GetMemoryOffset(),address=offset+instance;
 if(address<offset||sizeof(uint32_t)*3>UINTPTR_MAX-address)return 0;
 DRIVING_DESCRIPTOR252_BEFORE_READ(instance,12);
 SIZE_T got=0;
 int ok=ReadProcessMemory(GetCurrentProcess(),(const void*)address,owned,12,&got)&&got==12;
 if(ok)descriptor_bulk252++;else descriptor_fallback252++;
 uint64_t n=descriptor_bulk252+descriptor_fallback252;
 if(n==1||!(n%65536))fprintf(stderr,"[DESCRIPTOR252] bulk=%llu scalar_fallback=%llu successful_calls_saved=%llu\n",(unsigned long long)descriptor_bulk252,(unsigned long long)descriptor_fallback252,(unsigned long long)(2*descriptor_bulk252));
 return ok;
}
static int descriptor_read252(void*ctx,ReadWord252 read,uint32_t instance,uint32_t result[3]){
 return driving_descriptor252(ctx,read,descriptor_enabled252()?descriptor_tuple252:NULL,instance,result);
}
#endif
