/*410 Default-OFF permission-only reuse for one synchronous geometry350 draw.
 * No cached contents or pointer lifetime extension. All original joins remain.
 * Stack-local proof: refusal/fatal unwinding cannot leave an active global scope.
 * Included after mapping230 and mapped_submit230;264 has an include guard. */
#ifndef DRIVING_DIRECTMAP410_H
#define DRIVING_DIRECTMAP410_H
#include "driving_batchmap264.h"
typedef struct DrivingDirectMap410 {
 DrivingMap230 mapping;
 DrivingProof264 proof;
 uint64_t epoch;
 unsigned enabled;
} DrivingDirectMap410;
static int directmap410_setting=-1;
static uint64_t directmap410_draws,directmap410_requests,directmap410_completed;
static int directmap410_enabled(void){
 if(directmap410_setting<0){DWORD e=GetLastError();int c=errno;
  const char*v=getenv("DRIVING_DIRECT_MAP410");directmap410_setting=v&&!strcmp(v,"1");
  errno=c;SetLastError(e);}
 return directmap410_setting;
}
static void driving_directmap410_begin(DrivingDirectMap410*c){
 driving_map230_begin(&c->mapping);c->enabled=0;c->epoch=0;
#ifdef DRIVING_NATIVE_MAPPING230
 if(directmap410_enabled()&&driving_drainmap274_active()){
  c->epoch=driving_scope274.epoch;driving_proof264_begin(&c->proof,c->epoch);
  c->enabled=c->proof.enabled;if(c->enabled)directmap410_draws++;
 }
#endif
}
static void*mapped_direct410(DrivingDirectMap410*c,uint32_t va,size_t bytes,int write){
#ifdef DRIVING_NATIVE_MAPPING230
 if(c->enabled){directmap410_requests++;
  return driving_batchmap264(&c->mapping,&c->proof,c->epoch,0,va,bytes,write,
    (uintptr_t)xbox_GetMemoryOffset(),xbox_ContiguousAllocatedBytes());
 }
#endif
 return mapped_submit230(&c->mapping,va,bytes,write);
}
static void driving_directmap410_end(DrivingDirectMap410*c){driving_map230_end(&c->mapping);}
static void driving_directmap410_publish(DrivingDirectMap410*c){
 if(!c->enabled)return;
 DWORD saved_error=GetLastError();int saved_crt=errno;
 /* Existing publish rechecks generation/scope under its short shared lock.
  * Caller reaches here only after original complete RAM publication. */
 driving_proof264_publish(&c->proof,c->epoch);c->enabled=0;
 if(++directmap410_completed==1){DWORD e=GetLastError();int crt=errno;
  fprintf(stderr,"[DIRECTMAP410] active=1 geometry350=1 permission_only=1 original_joins=1 proof_rechecked=1\n");
  errno=crt;SetLastError(e);
 }
 errno=saved_crt;SetLastError(saved_error);
}
#endif
