/*278: private owned depth only. Reuses276 exact split/token policy and272's
 * integer canonicalization. No guest validity or naked pointer proof. */
#ifndef NIGHTFIRE_DEPTH_SEED278_H
#define NIGHTFIRE_DEPTH_SEED278_H
#ifdef NF_DEPTH_PING272_AVAILABLE
static NFColorPolicy276 depth278_policy;
static NFDepthSlot272 depth278_published[2],depth278_prepared[2],depth278_completed[2];
static int depth278_setting=-1;
int nf_hw_depth_seed278_enabled(void){
 if(depth278_setting<0){DWORD e=GetLastError();const char*v=getenv("DRIVING_DEPTH_SEED278");depth278_setting=v&&!strcmp(v,"1");SetLastError(e);}
 return depth278_setting&&depth272_enabled()&&!depth268_enabled()&&!depth269_enabled();
}
void nf_hw_depth_seed278_revoke(void){
 nf_color_revoke276(&depth278_policy);memset(depth278_published,0,sizeof depth278_published);
 memset(depth278_prepared,0,sizeof depth278_prepared);memset(depth278_completed,0,sizeof depth278_completed);
}
typedef struct NFDepthSeedScope278 {
 NFColorRecord276 lease;NFDepthSlot272 previous[2],final[2];
 const uint8_t *output[2];unsigned lane,allow,used;uint64_t epoch;
} NFDepthSeedScope278;
static NFDepthSeedScope278 *depth278_current;
#ifdef NF_PAIR234_TEST
static void (*depth278_test_import_hook)(void);
#endif
static struct{uint64_t splits,eligible,equal,batches,canonical,ordinary,max_clear,fp_miss,resource_miss,published,failed;} depth278_counts;
static int depth278_slot_equal(NFDepthSlot272 a,NFDepthSlot272 b){return a.texture&&a.dsv&&a.srv&&a.texture==b.texture&&a.dsv==b.dsv&&a.srv==b.srv;}
/* Only opaque triplets owned by this lane are selectable. No dereference of a
 * retained pointer is needed before current resource/epoch identity matches. */
static void depth278_select(NFDepthSeedScope278 *p,unsigned lane,NFDepthScope272 *scope){
 p->lane=lane;
 if(!p->lease.valid||p->epoch!=depth278_policy.epoch||p->lease.epoch!=p->epoch||
  !(p->lease.equal&(1u<<lane))||!scope->slots[0].texture||!scope->slots[1].texture||
  scope->slots[0].texture==scope->slots[1].texture)return;
 for(unsigned i=0;i<2;i++)if(depth278_slot_equal(p->previous[lane],scope->slots[i])&&p->lease.resource[lane]==scope->slots[i].texture){
  p->allow|=1u<<lane;scope->at=i;depth=scope->slots[i].texture;dsv=scope->slots[i].dsv;memset(&bound,0,sizeof bound);return;
 }
 depth278_counts.resource_miss++;
}
static int depth278_first(const NFHardwareState *s){
 NFDepthSeedScope278 *p=depth278_current;if(!p||p->lane>1||!s||!s->material221||s->color_only||s->color_layout||
  s->width!=640||s->height!=480||s->depth_pitch!=2560||width!=640||height!=480||s->depth!=p->output[p->lane])return 0;
 unsigned bit=1u<<p->lane;if(p->used&bit)return 0;p->used|=bit;return 1;
}
static void depth278_max(const NFHardwareState *s){if(depth278_first(s))depth278_counts.max_clear++;}
/* -1 only means scoped operational failure; caller must take existing fatal
 * cleanup rather than replay after any rendering has begun. */
static int depth278_apply(const NFHardwareState *s){
 if(!depth278_first(s))return 0;
 NFDepthSeedScope278 *p=depth278_current;unsigned bit=1u<<p->lane;
 if(p->epoch!=depth278_policy.epoch||!p->lease.valid||!(p->allow&bit))goto ordinary;
#ifdef NF_PAIR234_TEST
 if(depth278_test_import_hook)depth278_test_import_hook();
#endif
 if(!depth268_mode()){depth278_counts.fp_miss++;goto ordinary;}
 if(!depth272_current||depth272_current->at>1||
  !depth278_slot_equal(p->previous[p->lane],depth272_current->slots[depth272_current->at])||
  depth!=p->previous[p->lane].texture||dsv!=p->previous[p->lane].dsv){depth278_counts.resource_miss++;goto ordinary;}
 {int ok=depth272_apply();if(ok!=1)return -1;
  /* Keep the existing272 counter scoped to between-draw boundaries. */
  depth272_conversions--;depth268_target=NULL;depth278_counts.canonical++;return 1;}
ordinary:depth278_counts.ordinary++;return 0;
}
static void depth278_report(void){
 if(!nf_hw_depth_seed278_enabled())return;
 if(++depth278_counts.batches!=1&&depth278_counts.batches%512)return;
 DWORD e=GetLastError();fprintf(stderr,"[DEPTH-SEED278] batches=%llu splits=%llu eligible_lanes=%llu equal_lanes=%llu canonical_imports=%llu ordinary_imports=%llu max_clears=%llu fp_miss=%llu resource_miss=%llu published=%llu failed=%llu first-import-attempts=1 full32bit-fresh-split=1 final-readback-unchanged=1\n",
 (unsigned long long)depth278_counts.batches,(unsigned long long)depth278_counts.splits,(unsigned long long)depth278_counts.eligible,(unsigned long long)depth278_counts.equal,
 (unsigned long long)depth278_counts.canonical,(unsigned long long)depth278_counts.ordinary,(unsigned long long)depth278_counts.max_clear,
 (unsigned long long)depth278_counts.fp_miss,(unsigned long long)depth278_counts.resource_miss,(unsigned long long)depth278_counts.published,(unsigned long long)depth278_counts.failed);SetLastError(e);
}
#else
int nf_hw_depth_seed278_enabled(void){return 0;}
void nf_hw_depth_seed278_revoke(void){}
static int depth278_apply(const NFHardwareState*s){(void)s;return 0;}
static void depth278_max(const NFHardwareState*s){(void)s;}
#endif
#endif
