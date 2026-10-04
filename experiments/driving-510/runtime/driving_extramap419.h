/*419 Permission metadata only for font348 and offscreen243. No data/pointer
 * lifetime cache. Each caller ends this local map before allocations/GPU work
 * and can donate permissions only after its original complete RAM export. */
#ifndef DRIVING_EXTRAMAP419_H
#define DRIVING_EXTRAMAP419_H
#include "driving_mapping230.h"
#include "driving_batchmap264.h"
#include <fenv.h>
#include <xmmintrin.h>
typedef struct DrivingExtraMap419 {
 DrivingMap230 mapping;DrivingProof264 proof;uint64_t epoch;unsigned enabled;
} DrivingExtraMap419;
static int extramap419_setting=-1;
static uint64_t extramap419_completed[2];
static int extramap419_enabled(void){
 if(extramap419_setting<0){DWORD e=GetLastError();int c=errno;
  const char*v=getenv("DRIVING_EXTRA_MAP419");extramap419_setting=v&&!strcmp(v,"1");
  errno=c;SetLastError(e);}
 return extramap419_setting;
}
static void driving_extramap419_begin_kind(DrivingExtraMap419*c,unsigned font){
 c->mapping.active=0;c->enabled=0;c->epoch=0;
#ifdef DRIVING_NATIVE_MAPPING230
 if(extramap419_enabled()&&driving_drainmap274_active()){
  c->epoch=driving_scope274.epoch;driving_proof264_begin(&c->proof,c->epoch);
  c->enabled=c->proof.enabled;
 }
#endif
 /* The original font fallback has no mapper230 context or diagnostic counter. */
 if(!font||c->enabled)driving_map230_begin(&c->mapping);
}
static void driving_extramap419_begin(DrivingExtraMap419*c){driving_extramap419_begin_kind(c,0);}
static void driving_fontmap419_begin(DrivingExtraMap419*c){driving_extramap419_begin_kind(c,1);}
static void*mapped_font419(DrivingExtraMap419*c,uint32_t va,size_t bytes,int write){
#ifdef DRIVING_NATIVE_MAPPING230
 if(c->enabled){
  /* Preserve mapped201's read barrier BEFORE checking permissions. It may
   * flush packets or invalidate the scope;264 rechecks after the callback. */
  if(!write)residency_read236(va,bytes);
  return driving_batchmap264(&c->mapping,&c->proof,c->epoch,0,va,bytes,write,
   (uintptr_t)xbox_GetMemoryOffset(),xbox_ContiguousAllocatedBytes());
 }
#else
 (void)c;
#endif
 return mapped201(va,bytes,write);
}
static void*mapped_offscreen419(DrivingExtraMap419*c,uint32_t va,size_t bytes,int write){
#ifdef DRIVING_NATIVE_MAPPING230
 if(c->enabled)return driving_batchmap264(&c->mapping,&c->proof,c->epoch,0,
  va,bytes,write,(uintptr_t)xbox_GetMemoryOffset(),xbox_ContiguousAllocatedBytes());
 return driving_map230(&c->mapping,va,bytes,write,
  (uintptr_t)xbox_GetMemoryOffset(),xbox_ContiguousAllocatedBytes());
#else
 (void)c;return mapped201(va,bytes,write);
#endif
}
static void driving_extramap419_end(DrivingExtraMap419*c){if(c->mapping.active)driving_map230_end(&c->mapping);}
static void driving_extramap419_publish(DrivingExtraMap419*c,unsigned family){
 if(!c->enabled)return;
 DWORD e=GetLastError();int crt=errno;
 fenv_t fp;fegetenv(&fp);unsigned csr=_mm_getcsr();
 /* The existing function rechecks generation/scope under the short264 lock.
  * No lock spans GPU calls or the original guest publication. */
 driving_proof264_publish(&c->proof,c->epoch);c->enabled=0;
 if(family<2&&++extramap419_completed[family]==1)
  fprintf(stderr,"[EXTRAMAP419] family=%u completed=1 permission_only=1 full_publication=1 proof_rechecked=1\n",family);
 fesetenv(&fp);_mm_setcsr(csr);errno=crt;SetLastError(e);
}
#endif
