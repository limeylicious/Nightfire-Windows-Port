/* Generation-protected permission observations from successfully queued packets.
 * Data/descriptors stay fresh. Current-draw mapper230 remains the fallback.
 * Normal GPU/raw-pointer lifetime rules remain required after read_end. */
#ifndef DRIVING_BATCHMAP264_H
#define DRIVING_BATCHMAP264_H
#include "driving_permissions264.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#ifndef DRIVING_NATIVE_MAPPING230
/* Existing owned-resource fixtures retain their mapper and need no new link. */
typedef struct DrivingProof264 {unsigned enabled;} DrivingProof264;
static void driving_batchmap264_reset(void){}
static void driving_drainmap274_enter(void){}
static void driving_drainmap274_leave(void){}
static void driving_proof264_begin(DrivingProof264*p,uint64_t epoch){(void)epoch;p->enabled=0;}
static void driving_proof264_publish(DrivingProof264*p,uint64_t epoch){(void)p;(void)epoch;}
#else
/* Region metadata may cover other suballocations, but never provides data or
 * allocation ownership. Serialized mutation generation governs all backing
 * permissions; map230 repeats current aperture/highwater/state/access checks. */
typedef struct DrivingProof264 {
 unsigned enabled,invalid,generation_seen;uint64_t epoch,generation,scope274;
 unsigned seen363,valid363;uintptr_t base363;
 DrivingMap230 observed;
} DrivingProof264;
/*363 copies only observed permissions, never backing ownership or bytes.
 * Seed/publish run under264 shared lock and only inside active274 scopes. */
static struct {unsigned count;uintptr_t base;uint64_t generation;DrivingMapRegion230 regions[16];} donor363;
static struct {uint64_t seeded,published,generation_miss,base_miss,nested_clear;} counts363;
static int aperture363_enabled(void){
 static int setting=-1;if(setting<0){DWORD error=GetLastError();int crt=errno;
  const char*v=getenv("DRIVING_APERTURE_CACHE363");setting=v&&!strcmp(v,"1");
  errno=crt;SetLastError(error);}return setting;
}
static void aperture363_record(DrivingProof264*p,uint32_t va,size_t bytes,uintptr_t offset){
 if(!aperture363_enabled())return;
 if(va<0x80000000u||va>=0x84000000u||!bytes||bytes>0x84000000ull-va||offset>UINTPTR_MAX-0x80000000u){p->valid363=0;return;}
 uintptr_t base=offset+0x80000000u;
 if(p->seen363&&p->base363!=base)p->valid363=0;
 p->seen363=1;p->base363=base;
}
static void aperture363_seed(DrivingMap230*c,DrivingProof264*p,uint64_t generation,int active){
 if(!aperture363_enabled()||!active||!p->valid363||!p->seen363)return;
 if(!generation||donor363.generation!=generation){counts363.generation_miss+=donor363.count!=0;donor363.count=0;return;}
 if(donor363.count&&donor363.base!=p->base363){counts363.base_miss++;donor363.count=0;return;}
 if(!c->count&&donor363.count){c->count=donor363.count;memcpy(c->regions,donor363.regions,c->count*sizeof*c->regions);counts363.seeded++;}
}
static void aperture363_publish(const DrivingProof264*p,uint64_t generation,int active){
 if(!aperture363_enabled()||!active||!generation||!p->seen363||!p->valid363||!p->observed.count)return;
 donor363.base=p->base363;donor363.generation=generation;donor363.count=p->observed.count;
 memcpy(donor363.regions,p->observed.regions,donor363.count*sizeof*donor363.regions);counts363.published++;
}
static struct {unsigned count;uint64_t epoch,generation,published_batch274;DrivingMapRegion230 regions[16];} driving_published264;
static struct {uint64_t requests,seeded_contexts,seeded_regions,fallback,discarded,publications,published_regions,flush_resets,generation_resets;unsigned peak;} driving_map264_counts;
/* Only the serialized outer GPU drain owns this state. Nested drains invalidate
 * all current proofs and disable cross-batch reuse until the outer exit. */
static struct {unsigned depth,usable,retired;uint64_t epoch,drains,nested,retained_flushes,cross_batch_seeds,scope_resets;} driving_scope274;
static int driving_drainmap274_enabled(void){
#ifdef DRIVING_DRAIN_MAP274
 static int cached=-1;if(cached<0){DWORD e=GetLastError();const char*v=getenv("DRIVING_DRAIN_MAP274");cached=v&&!strcmp(v,"1");SetLastError(e);}
 return cached&&driving_permissions264_enabled();
#else
 return 0;
#endif
}
static int driving_drainmap274_active(void){return driving_drainmap274_enabled()&&driving_scope274.depth==1&&driving_scope274.usable&&!driving_scope274.retired;}
static void driving_drainmap274_clear(void){
 if(driving_published264.count)driving_scope274.scope_resets++;
 driving_published264.count=0;driving_published264.generation=0;
}
static void driving_drainmap274_advance(void){if(++driving_scope274.epoch==0)driving_scope274.retired=1;}
static void driving_drainmap274_enter(void){
 if(!driving_drainmap274_enabled())return;
 driving_drainmap274_clear();driving_drainmap274_advance();
 if(driving_scope274.depth){driving_scope274.nested++;driving_scope274.usable=0;
  if(aperture363_enabled()){counts363.nested_clear+=donor363.count!=0;donor363.count=0;}}
 else{driving_scope274.drains++;driving_scope274.usable=!driving_scope274.retired;}
 if(driving_scope274.depth==~0u){driving_scope274.retired=1;driving_scope274.usable=0;}else driving_scope274.depth++;
}
static void driving_drainmap274_leave(void){
 if(!driving_drainmap274_enabled())return;
 driving_drainmap274_clear();driving_drainmap274_advance();driving_scope274.usable=0;
 if(driving_scope274.depth)driving_scope274.depth--;else driving_scope274.retired=1;
 if(!driving_scope274.depth&&(driving_scope274.drains==1||!(driving_scope274.drains%128))){
  DWORD e=GetLastError();fprintf(stderr,"[DRAIN-MAP274] drains=%llu nested=%llu retained_flushes=%llu cross_batch_seeds=%llu scope_resets=%llu depth=%u fresh_data=1 permission_only=1\n",
   (unsigned long long)driving_scope274.drains,(unsigned long long)driving_scope274.nested,(unsigned long long)driving_scope274.retained_flushes,(unsigned long long)driving_scope274.cross_batch_seeds,(unsigned long long)driving_scope274.scope_resets,driving_scope274.depth);
  if(aperture363_enabled()){int crt363=errno;fprintf(stderr,"[APERTURE363] seeded=%llu published=%llu generation_miss=%llu base_miss=%llu nested_clear=%llu metadata_only=1\n",
   (unsigned long long)counts363.seeded,(unsigned long long)counts363.published,(unsigned long long)counts363.generation_miss,(unsigned long long)counts363.base_miss,(unsigned long long)counts363.nested_clear);errno=crt363;}
  SetLastError(e);
 }
}
static void driving_batchmap264_reset(void){
 if(driving_drainmap274_active()){if(driving_published264.count)driving_scope274.retained_flushes++;return;}
 if(driving_published264.count)driving_map264_counts.flush_resets++;
 driving_published264.count=0;driving_published264.generation=0;
}
static void driving_proof264_begin(DrivingProof264*p,uint64_t epoch){
 p->enabled=driving_permissions264_enabled();if(!p->enabled)return;
 p->invalid=p->generation_seen=0;p->epoch=epoch;p->generation=0;p->observed.count=0;
 p->seen363=0;p->valid363=1;p->base363=0;
 p->scope274=driving_drainmap274_enabled()?driving_scope274.epoch:0;
}
static void driving_proof264_discard(DrivingProof264*p){if(p->enabled){p->invalid=1;p->observed.count=0;}}
static void *driving_batchmap264(DrivingMap230*ctx,DrivingProof264*proof,uint64_t epoch,unsigned queued,
 uint32_t va,size_t bytes,int write,uintptr_t offset,uint32_t allocated){
 if(!proof->enabled)return driving_map230(ctx,va,bytes,write,offset,allocated);
 driving_map264_counts.requests++;
 if(!ctx||!ctx->active||ctx->count>16){driving_proof264_discard(proof);return driving_map230(ctx,va,bytes,write,offset,allocated);}
 uint64_t generation=driving_permissions264_read_begin();
 int scoped274=driving_drainmap274_enabled(),retain274=driving_drainmap274_active();
 uint64_t cache_epoch274=retain274?driving_scope274.epoch:epoch;
 if(scoped274&&proof->scope274!=driving_scope274.epoch){driving_map230_end(ctx);driving_map230_begin(ctx);driving_proof264_discard(proof);proof->scope274=driving_scope274.epoch;}
 if(proof->generation_seen&&(proof->generation!=generation||proof->epoch!=epoch)){
  /* Both the local current-draw cache and pending publication are stale. */
  driving_map230_end(ctx);driving_map230_begin(ctx);driving_proof264_discard(proof);
 }
 proof->generation_seen=1;proof->generation=generation;
 if(proof->epoch!=epoch){driving_proof264_discard(proof);proof->epoch=epoch;}
 if(driving_published264.epoch!=cache_epoch274||driving_published264.generation!=generation||(!retain274&&!queued)||(scoped274&&!retain274)||!generation){
  if(driving_published264.count&&driving_published264.generation!=generation)driving_map264_counts.generation_resets++;
  driving_published264.count=0;driving_published264.epoch=cache_epoch274;driving_published264.generation=generation;
 }
 if(ctx&&ctx->active&&!ctx->count&&generation&&(retain274||(!scoped274&&queued))&&driving_published264.count){
  if(retain274&&driving_published264.published_batch274!=epoch)driving_scope274.cross_batch_seeds++;
  ctx->count=driving_published264.count;
  memcpy(ctx->regions,driving_published264.regions,ctx->count*sizeof ctx->regions[0]);
  driving_map264_counts.seeded_contexts++;driving_map264_counts.seeded_regions+=ctx->count;
 }else driving_map264_counts.fallback++;
 aperture363_record(proof,va,bytes,offset);
 aperture363_seed(ctx,proof,generation,retain274&&!proof->invalid);
 /* The original mapper validates every bound and every region's current
  * generation-protected state/access. Saturation still queries uncached. */
 void *result=driving_map230(ctx,va,bytes,write,offset,allocated);
 if(!result)driving_proof264_discard(proof);
 else if(!proof->invalid&&generation)proof->observed=*ctx;
 driving_permissions264_read_end();return result;
}
static void driving_proof264_publish(DrivingProof264*p,uint64_t epoch){
 if(!p->enabled)return;
 uint64_t generation=driving_permissions264_read_begin();
 int scoped274=driving_drainmap274_enabled(),retain274=driving_drainmap274_active();
 if((scoped274&&(!retain274||p->scope274!=driving_scope274.epoch))||p->invalid||!generation||!p->generation_seen||p->generation!=generation||p->epoch!=epoch||!p->observed.active||p->observed.count>16){
  driving_map264_counts.discarded++;driving_permissions264_read_end();return;
 }
 driving_published264.epoch=retain274?driving_scope274.epoch:epoch;driving_published264.generation=generation;
 driving_published264.published_batch274=epoch;
 driving_published264.count=p->observed.count;
 memcpy(driving_published264.regions,p->observed.regions,p->observed.count*sizeof p->observed.regions[0]);
 aperture363_publish(p,generation,retain274);
 driving_map264_counts.publications++;driving_map264_counts.published_regions+=p->observed.count;
 if(driving_published264.count>driving_map264_counts.peak)driving_map264_counts.peak=driving_published264.count;
 driving_permissions264_read_end();
 if(driving_map264_counts.publications==1||!(driving_map264_counts.publications%4096)){
  DWORD error=GetLastError();
  fprintf(stderr,"[BATCH-MAP264] queued_packets=%llu requests=%llu seeded_contexts=%llu seeded_regions=%llu unseeded_requests=%llu discarded_proofs=%llu published_regions=%llu flush_resets=%llu generation_resets=%llu peak=%u fresh_data=1 region_permissions=1\n",
   (unsigned long long)driving_map264_counts.publications,(unsigned long long)driving_map264_counts.requests,(unsigned long long)driving_map264_counts.seeded_contexts,(unsigned long long)driving_map264_counts.seeded_regions,(unsigned long long)driving_map264_counts.fallback,
   (unsigned long long)driving_map264_counts.discarded,(unsigned long long)driving_map264_counts.published_regions,(unsigned long long)driving_map264_counts.flush_resets,(unsigned long long)driving_map264_counts.generation_resets,driving_map264_counts.peak);SetLastError(error);
 }
}
#endif
#endif
