/*465 default-OFF mapper-only query-origin experiment.
 * A previous cache entry supplies only an ADDRESS to a NEW real Windows query.
 * Old permissions/data are never reused across a generation. A newly observed
 * complete region must cover the requested page, fit today's whitelist, and
 * retain the same nonzero generation before the unchanged445 cache may use it.
 * Otherwise the original exact-address query path runs, with original state.
 * This entry is private to map230's valid local stack MBI, not a public API. */
#ifndef DRIVING_HINT465_H
#define DRIVING_HINT465_H
#include <fenv.h>
#include <errno.h>
#include <intrin.h>
static volatile LONG hint465_setting=-1;
static __declspec(thread) struct {uint64_t calls,attempts,covered,refused,no_hint,already_cached,zero_generation;} hint465;
static int hint465_enabled(void){
 LONG setting=InterlockedCompareExchange(&hint465_setting,-1,-1);
 if(setting<0){DWORD e=GetLastError();int c=errno;fenv_t env;fegetenv(&env);unsigned csr=_mm_getcsr();
  const char*v=getenv("DRIVING_HINT465");LONG want=v&&!strcmp(v,"1");LONG old=InterlockedCompareExchange(&hint465_setting,want,-1);setting=old<0?want:old;
  fesetenv(&env);_mm_setcsr(csr);errno=c;SetLastError(e);
 }return setting!=0;
}
static void hint465_report(void){
 fprintf(stderr,"[HINT465] calls=%llu fresh_queries=%llu covered=%llu refused=%llu no_hint=%llu already_cached=%llu no_generation=%llu permissions_from_fresh_os_query=1\n",hint465.calls,hint465.attempts,hint465.covered,hint465.refused,hint465.no_hint,hint465.already_cached,hint465.zero_generation);
}
SIZE_T driving_region465_map_query(LPCVOID address,PMEMORY_BASIC_INFORMATION info,SIZE_T length){
 if(!hint465_enabled()||!info||length!=sizeof *info||region445_mode()!=1)return driving_region445_query(address,info,length);
 DWORD error=GetLastError();int crt=errno;fenv_t env;fegetenv(&env);unsigned csr=_mm_getcsr();
 uint64_t generation=driving_permissions264_peek445();uintptr_t a=(uintptr_t)address,page=dr445_page(a);DR445Info unused;
 hint465.calls++;
 if(!generation)hint465.zero_generation++;
 else if(dr445_lookup(&region_cache445,generation,a,&unused))hint465.already_cached++;
 else {
  uintptr_t lo=0,hi=0,hint=page;int inside=0;
  for(int window=0;window<2;window++)if(xbox_Region445Window(window,&lo,&hi)&&a>=lo&&a<hi){inside=1;break;}
  if(inside)for(unsigned i=0;i<DR445_ENTRIES;i++){
   const DR445Entry*e=&region_cache445.entries[i];
   /* Stale range is a performance hint only. The OS must prove it afresh. */
   if(e->generation&&e->begin>=lo&&e->end<=hi&&e->begin< hint&&page<e->end)hint=e->begin;
  }
  if(!inside||hint==page)hint465.no_hint++;
  else {
   MEMORY_BASIC_INFORMATION observed={0};hint465.attempts++;
   SIZE_T n=VirtualQuery((LPCVOID)hint,&observed,sizeof observed);
   if(n==sizeof observed&&observed.State==MEM_COMMIT&&(uintptr_t)observed.BaseAddress==hint&&
      observed.RegionSize<=UINTPTR_MAX-hint&&page<hint+observed.RegionSize&&
      driving_permissions264_peek445()==generation){
    DR445Info fresh;region445_convert(&observed,&fresh);
    if(dr445_store(&region_cache445,generation,hint,&fresh,lo,hi,&observed,sizeof observed))hint465.covered++;
    else hint465.refused++;
   }else hint465.refused++;
  }
 }
 if(hint465.calls==1||!(hint465.calls%8192))hint465_report();
 fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(error);
 /* Fresh permission generation is re-read by445 here. A concurrent change
  * defeats the seeded entry and takes the original real-query path. */
 return driving_region445_query(address,info,length);
}
#endif
