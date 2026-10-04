/*401 DIAGNOSTIC ONLY. Copies already-published output for a later exact
 * comparison. Never suppresses a GPU command, holds a guest pointer for later
 * dereference, or grants authoritative GPU ownership. Serialized caller. */
#ifndef NIGHTFIRE_PAIR_COLOR401_H
#define NIGHTFIRE_PAIR_COLOR401_H
enum {COLOR401_BYTES=640*480*4};
static struct {
 unsigned enabled_known,enabled,dead,valid,ready,had_reference,equal,profile,pending;
 uint64_t epoch,certificate_epoch,pending_epoch,attempts,failed,published,eligible,equal_lanes,reported;
 uint64_t compare_ticks,snapshot_ticks,clock_failures;
 uint64_t comparisons,observed_reference,observed_equal,refused,fatal,excluded;
 const void *resource[2],*pending_resource[2],*pending_source[2];
 unsigned pending_profile,pending_reference,pending_equal;
 unsigned char snapshot[2][COLOR401_BYTES];
} color401;
static int color401_enabled(void){
 if(!color401.enabled_known){
  PCGuard399 g;pc_save399(&g);const char*v=getenv("DRIVING_PAIR_COLOR_CENSUS401");
  color401.enabled=v&&!strcmp(v,"1");color401.enabled_known=1;pc_restore399(&g);
 }
 return color401.enabled&&!color401.dead;
}
static void color401_revoke(void){
 if(!color401_enabled())return;
 color401.valid=color401.pending=0;
 if(!++color401.epoch)color401.dead=1;
}
#ifndef COLOR401_QPC
#define COLOR401_QPC QueryPerformanceCounter
#endif
#ifndef COLOR401_FREQUENCY
#define COLOR401_FREQUENCY QueryPerformanceFrequency
#endif
static uint64_t color401_clock(void){
 LARGE_INTEGER n;if(!COLOR401_QPC(&n)||n.QuadPart<=0)return 0;
 return (uint64_t)n.QuadPart;
}
static void color401_elapsed(uint64_t start,uint64_t *sum){
 uint64_t end=color401_clock();if(start&&end>=start)*sum+=end-start;else color401.clock_failures++;
}
static void color401_enter(unsigned profile){
 if(!color401_enabled())return;
 color401.pending=color401.ready=color401.had_reference=color401.equal=0;
 color401.profile=(profile==25||profile==26)?profile:0;
 if(color401.profile)color401.attempts++;else color401_revoke();
}
/* Call only AFTER original preflight and actual resource creation succeeded. */
static void color401_compare(const NFHardwareState states[2],const void *r0,const void *r1){
 if(!color401_enabled()||!color401.profile)return;
 PCGuard399 g;pc_save399(&g);uint64_t start=color401_clock();
 color401.ready=1;
 if(color401.valid&&color401.certificate_epoch==color401.epoch&&
    color401.resource[0]==r0&&color401.resource[1]==r1){
  color401.had_reference=1;
  for(unsigned lane=0;lane<2;lane++)
   if(!memcmp(states[lane].color,color401.snapshot[lane],COLOR401_BYTES))color401.equal|=1u<<lane;
 }
 color401.comparisons++;color401.observed_reference+=2*color401.had_reference;
 color401.observed_equal+=(color401.equal&1)+((color401.equal>>1)&1);
 color401_revoke();color401_elapsed(start,&color401.compare_ticks);pc_restore399(&g);
}
/* Called after original pair cleanup/selection restoration, before guest join.
 * Nothing is certified until the existing caller has published both outputs. */
static void color401_leave(const NFHardwareState states[2],int result,const void *r0,const void *r1){
 if(!color401_enabled())return;
 if(color401.profile){color401.refused+=result==0;color401.fatal+=result<0;}
 if(!color401.profile||!color401.ready||result!=1||color==r0||color==r1){
  color401.excluded+=color401.profile&&result==1;
  color401.failed+=color401.profile&&result!=1;color401_revoke();return;
 }
 color401.pending=1;color401.pending_epoch=color401.epoch;
 color401.pending_profile=color401.profile;color401.pending_reference=color401.had_reference;color401.pending_equal=color401.equal;
 color401.pending_resource[0]=r0;color401.pending_resource[1]=r1;
 color401.pending_source[0]=states[0].color;color401.pending_source[1]=states[1].color;
}
void nf_hw_pair_publish401(unsigned profile,const NFHardwareState states[2],int result){
 if(!color401_enabled())return;
 if(!color401.pending||result!=1||profile!=color401.pending_profile||color401.epoch!=color401.pending_epoch){color401_revoke();return;}
 if(states[0].color!=color401.pending_source[0]||states[1].color!=color401.pending_source[1]){color401_revoke();return;}
 PCGuard399 g;pc_save399(&g);uint64_t start=color401_clock();
 for(unsigned lane=0;lane<2;lane++){
  /* Fresh arguments, still owned and stable under the original caller contract. */
  memcpy(color401.snapshot[lane],states[lane].color,COLOR401_BYTES);
  color401.resource[lane]=color401.pending_resource[lane];
 }
 color401.certificate_epoch=color401.epoch;color401.valid=1;color401.pending=0;
 color401.published++;color401.eligible+=2*color401.pending_reference;
 color401.equal_lanes+=(color401.pending_equal&1)+((color401.pending_equal>>1)&1);
 color401_elapsed(start,&color401.snapshot_ticks);pc_restore399(&g);
}
void nf_hw_pair_report401(void){
 if(!color401_enabled()||color401.published-color401.reported<128)return;
 PCGuard399 g;pc_save399(&g);LARGE_INTEGER hz={0};int frequency_valid=COLOR401_FREQUENCY(&hz)&&hz.QuadPart>0;if(!frequency_valid)hz.QuadPart=0;
 fprintf(stderr,"[PAIRCOLOR401] attempts=%llu failed=%llu published=%llu reference_lanes=%llu equal_lanes=%llu compare_ticks=%llu snapshot_ticks=%llu hz=%lld clock_failures=%llu comparisons=%llu observed_reference=%llu observed_equal=%llu refused=%llu fatal=%llu excluded=%llu frequency_valid=%d reference_counts_successful_publications_only=1 diagnostic_only=1 uploads_skipped=0\n",
  (unsigned long long)color401.attempts,(unsigned long long)color401.failed,(unsigned long long)color401.published,
  (unsigned long long)color401.eligible,(unsigned long long)color401.equal_lanes,
  (unsigned long long)color401.compare_ticks,(unsigned long long)color401.snapshot_ticks,hz.QuadPart,(unsigned long long)color401.clock_failures,
  (unsigned long long)color401.comparisons,(unsigned long long)color401.observed_reference,(unsigned long long)color401.observed_equal,
  (unsigned long long)color401.refused,(unsigned long long)color401.fatal,(unsigned long long)color401.excluded,frequency_valid);
 color401.reported=color401.published;pc_restore399(&g);
}
#endif
