/*452: opt-in permission-cache invalidation for PC451 page transitions.
 * The OS page state is authoritative. All transitions, including rollback and
 * failed calls, use the established264 write interval. No GPU work, callbacks
 * or publication may run while that interval is held. Default OFF.
 */
#ifndef DRIVING_PROTECTION452_H
#define DRIVING_PROTECTION452_H
#include <windows.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "driving_permissions264.h"
static int pc452_enabled(void){
 static int mode=-1;
 if(mode<0){DWORD e=GetLastError();int crt=errno;const char*v=getenv("DRIVING_PC452");
  mode=v&&!strcmp(v,"1");errno=crt;SetLastError(e);}
 return mode;
}
static BOOL pc452_protect(void *base,SIZE_T bytes,DWORD protect,DWORD *old){
 if(!pc452_enabled())return VirtualProtect(base,bytes,protect,old);
 unsigned token=driving_permissions264_write_begin();BOOL result=FALSE;
 __try {result=VirtualProtect(base,bytes,protect,old);}
 __finally {driving_permissions264_write_end(token);}
 return result;
}
/* Bypass the445 textual redirect only for independent diagnostic reads.
 * Never use a cached protection value as proof of its own correctness. */
#pragma push_macro("VirtualQuery")
#undef VirtualQuery
static SIZE_T pc452_real_query(const void *address,MEMORY_BASIC_INFORMATION *info){
 return VirtualQuery(address,info,sizeof *info);
}
#pragma pop_macro("VirtualQuery")
#endif
