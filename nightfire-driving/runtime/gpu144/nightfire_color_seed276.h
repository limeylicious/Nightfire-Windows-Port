/*276: exact owned color bytes only. Never guest residency or pointer equality
 * alone. Policy is CPU-testable; runtime scope belongs to private248. */
#ifndef NIGHTFIRE_COLOR_SEED276_H
#define NIGHTFIRE_COLOR_SEED276_H
#include "../driving_lanes227.h"
enum {NF_COLOR_BYTES276=640*480*4};
typedef struct NFColorRecord276 {
 uint64_t token,epoch,allocation;uint8_t *storage;
 const void *resource[2];NFHardwareColorKey276 key;unsigned equal,valid;
} NFColorRecord276;
typedef struct NFColorPolicy276 {
 uint64_t epoch,serial;unsigned dead;
 NFColorRecord276 certificate,prepared,completed;
} NFColorPolicy276;
static void nf_color_revoke276(NFColorPolicy276 *p){
 p->certificate.valid=p->prepared.valid=p->completed.valid=0;
 if(!++p->epoch)p->dead=1;
}
/* Always freshly split, including first use, mismatch and exhausted serials.
 * Only this function computes the equality bits accepted by the backend. */
static uint64_t nf_color_split276(NFColorPolicy276 *p,const NFHardwareColorKey276 *key,
 uint64_t allocation,const uint8_t *source,uint8_t *storage){
 NFColorRecord276 old=p->certificate;
 unsigned eligible=!p->dead&&old.valid&&old.epoch==p->epoch&&old.storage==storage&&
  old.allocation==allocation&&allocation&&!memcmp(&old.key,key,sizeof *key)?3u:0u;
 p->certificate.valid=p->prepared.valid=p->completed.valid=0;
 unsigned equal=driving_split_equal276(source,storage,storage+NF_COLOR_BYTES276,640*480,eligible);
 if(p->dead||!allocation||!++p->serial){p->dead=1;return 0;}
 NFColorRecord276 fresh={0};fresh.token=p->serial;fresh.epoch=p->epoch;fresh.allocation=allocation;
 fresh.storage=storage;fresh.key=*key;fresh.equal=equal;fresh.valid=1;
 fresh.resource[0]=old.resource[0];fresh.resource[1]=old.resource[1];p->prepared=fresh;
 return fresh.token;
}
/* Consume before any admission or GPU action. Even refusal revokes old proof. */
static NFColorRecord276 nf_color_take276(NFColorPolicy276 *p,uint64_t token){
 NFColorRecord276 lease={0};
 if(!p->dead&&token&&p->prepared.valid&&p->prepared.token==token&&p->prepared.epoch==p->epoch)lease=p->prepared;
 nf_color_revoke276(p);lease.epoch=p->epoch;if(p->dead)lease.valid=0;return lease;
}
static unsigned nf_color_allow276(const NFColorPolicy276 *p,const NFColorRecord276 *lease,
 const void *resource0,const void *resource1,const void *output0,const void *output1){
 if(p->dead||!lease->valid||lease->epoch!=p->epoch||lease->storage!=output0||
  lease->storage+NF_COLOR_BYTES276!=output1)return 0;
 return lease->equal&((lease->resource[0]==resource0?1u:0u)|(lease->resource[1]==resource1?2u:0u));
}
static void nf_color_complete276(NFColorPolicy276 *p,const NFColorRecord276 *lease,int ok,
 const void *resource0,const void *resource1,const void *output0,const void *output1){
 p->completed.valid=0;
 if(p->dead||!ok||!lease->valid||lease->epoch!=p->epoch||lease->storage!=output0||lease->storage+NF_COLOR_BYTES276!=output1)return;
 p->completed=*lease;p->completed.epoch=p->epoch;p->completed.resource[0]=resource0;p->completed.resource[1]=resource1;
}
static int nf_color_publish276(NFColorPolicy276 *p,uint64_t token){
 if(p->dead||!token||!p->completed.valid||p->completed.token!=token||p->completed.epoch!=p->epoch)return 0;
 p->certificate=p->completed;p->completed.valid=0;return 1;
}
#ifndef NF_COLOR276_POLICY_ONLY
static NFColorPolicy276 color276_policy;
static int color276_setting=-1;
int nf_hw_color_seed276_enabled(void){
 if(color276_setting<0){DWORD saved=GetLastError();const char*v=getenv("DRIVING_COLOR_SEED276");color276_setting=v&&!strcmp(v,"1");SetLastError(saved);}
 return color276_setting;
}
void nf_hw_color_seed276_revoke(void){nf_color_revoke276(&color276_policy);}
typedef struct NFColorUpload276 {const void *resource[2];const uint8_t *output[2];unsigned allow,used;uint64_t epoch;} NFColorUpload276;
static NFColorUpload276 *color276_current;
static struct {uint64_t split,eligible_lanes,equal_lanes,batches,uploads,skips,published,failed;} color276_counts;
static unsigned color276_bits(unsigned n){return(n&1)+((n>>1)&1);}
static int color276_skip(const NFHardwareState *s){
 NFColorUpload276 *p=color276_current;if(!p||p->epoch!=color276_policy.epoch||!s||!s->material221||s->color_layout||
  s->width!=640||s->height!=480||s->pitch!=2560||width!=640||height!=480)return 0;
 for(unsigned lane=0;lane<2;lane++)if(color==p->resource[lane]&&s->color==p->output[lane]){
  unsigned bit=1u<<lane;int skip=(p->allow&bit)&&!(p->used&bit);p->used|=bit;
  if(skip)color276_counts.skips++;else color276_counts.uploads++;return skip;
 }return 0;
}
static void color276_report(void){
 if(!nf_hw_color_seed276_enabled())return;
 if(++color276_counts.batches!=1&&color276_counts.batches%512)return;
 DWORD saved=GetLastError();fprintf(stderr,"[COLOR-SEED276] batches=%llu splits=%llu eligible_lanes=%llu equal_lanes=%llu upload_calls=%llu skipped_uploads=%llu skipped_bytes=%llu published_certificates=%llu failed=%llu exact_fresh_split=1 readback_unchanged=1 upload_counts_include_failed_attempts=1\n",
 (unsigned long long)color276_counts.batches,(unsigned long long)color276_counts.split,
 (unsigned long long)color276_counts.eligible_lanes,(unsigned long long)color276_counts.equal_lanes,
 (unsigned long long)color276_counts.uploads,(unsigned long long)color276_counts.skips,
 (unsigned long long)(color276_counts.skips*NF_COLOR_BYTES276),(unsigned long long)color276_counts.published,(unsigned long long)color276_counts.failed);SetLastError(saved);
}
#endif
#endif
