#ifdef DRIVING_CPU_ACCESS320
#include "driving_cpu320.h"
#endif
#include "driving_timing250.h"
#include "driving_queue251.h"
#include "driving_census255.h"
#include "driving_native247.h"
#include "driving_finite328.h"
#include "driving_proof466.h"
#ifdef DRIVING_LEASE322
#include "driving_lease322.h"
#endif
#ifdef DRIVING_PUBLISH324
#include "driving_publication324.h"
#endif
#include "driving_pc450.h" /*450 PC-mode state (default OFF)*/
/* Experimental236 packets. Include after current driving_submit221.h.
 * Lifetime/visibility is bounded by residency236 drain and read/command guards. */
#ifndef DRIVING_PACKETS234_H
#define DRIVING_PACKETS234_H
#include "gpu144/nightfire_batch_limit260.h"
#define DRIVING_BATCH_COUNT234 NF_BATCH_MAX260
#define DRIVING_BATCH_BYTES234 (32u*1024*1024)
/*253: retain capacity only; no guest data survives as valid across batches. */
enum {DS253_INPUT,DS253_MEMO,DS253_TARGETS,DS253_COUNT};
static DrivingStorage228 driving_owned253[DS253_COUNT];
/* Token is outside batch234: no change to packet/capacity byte accounting. */
static uint64_t batch_seed276,batch_seed278,allocation276=1;
static void driving_allocation276_changed(void){nf_hw_color_seed276_revoke();nf_hw_depth_seed278_revoke();if(allocation276&& !++allocation276)allocation276=0;}

static uint64_t owned_requests253[DS253_COUNT],owned_reuses253[DS253_COUNT],owned_failures253;
#ifndef DRIVING_OWNED253_ENABLED
static int driving_owned253_enabled(void){static int on=-1;if(on<0){const char*v=getenv("DRIVING_OWNED253");on=v&&!strcmp(v,"1");}return on;}
#define DRIVING_OWNED253_ENABLED() driving_owned253_enabled()
#endif
#ifndef DRIVING_OWNED253_STORAGE
#define DRIVING_OWNED253_STORAGE(s,n) driving_storage228(s,n)
#endif
static void *driving_acquire253(unsigned slot,size_t bytes){
 if(!DRIVING_OWNED253_ENABLED()){if(slot==DS253_TARGETS)driving_allocation276_changed();return slot==DS253_MEMO?calloc(1,bytes):malloc(bytes);}
 void *old276=driving_owned253[slot].bytes;size_t capacity276=driving_owned253[slot].capacity;
 if(slot==DS253_TARGETS&&bytes>capacity276)driving_allocation276_changed();
 int reuse=bytes<=driving_owned253[slot].capacity;owned_requests253[slot]++;
 void*p=DRIVING_OWNED253_STORAGE(&driving_owned253[slot],bytes);
 if(slot==DS253_TARGETS&&(driving_owned253[slot].bytes!=old276||driving_owned253[slot].capacity!=capacity276))driving_allocation276_changed();
 if(!p)owned_failures253++;else if(reuse)owned_reuses253[slot]++;
 if(slot==DS253_TARGETS&&(owned_requests253[slot]==1||!(owned_requests253[slot]%512))){
  size_t capacity=0;for(unsigned i=0;i<DS253_COUNT;i++)capacity+=driving_owned253[i].capacity;
  fprintf(stderr,"[OWNED253] target_requests=%llu target_reuses=%llu input_reuses=%llu memo_reuses=%llu retained_bytes=%zu failures=%llu fresh_target_seed=1 fresh_memo_reset=1\n",(unsigned long long)owned_requests253[DS253_TARGETS],(unsigned long long)owned_reuses253[DS253_TARGETS],(unsigned long long)owned_reuses253[DS253_INPUT],(unsigned long long)owned_reuses253[DS253_MEMO],capacity,(unsigned long long)owned_failures253);
 }
 return p;
}
static void driving_release253(unsigned slot,void*p){
 if(slot==DS253_TARGETS&&(!DRIVING_OWNED253_ENABLED()||p!=driving_owned253[slot].bytes))driving_allocation276_changed();
 if(DRIVING_OWNED253_ENABLED())driving_storage_release228(&driving_owned253[slot],p);else free(p);
}
/* End253 owned capacity helpers. */

enum {DRIVING_LANE234=640*480*4,DRIVING_PAIR234=2*DRIVING_LANE234};
typedef struct DrivingPacket234 {
 NFHardwareMaterial221 material;NFHardwareState state;
 NFHardwareMaterialVertex221 *vertices;uint8_t *textures;unsigned dense;
} DrivingPacket234;
static uint64_t batch_epoch236;
#include "driving_batchmap264.h"
static struct {uint64_t queued,fallback,flushes,draws,queue_ticks,fallback_ticks,flush_ticks,backend_ticks,publish_ticks;unsigned maximum;} batch_time236;
static void driving_batch236_report(void){
 static uint64_t reported;
 if(!batch_time236.flushes||(reported&&batch_time236.flushes-reported<120))return;
 reported=batch_time236.flushes;LARGE_INTEGER hz;QueryPerformanceFrequency(&hz);double ms=1000.0/hz.QuadPart;
 driving_census255_report(ms);
 fprintf(stderr,"[BATCH236-TIME] queued=%llu fallback=%llu flushes=%llu draws=%llu max_batch=%u queue_cpu_ms=%.3f flush_backend_ms=%.3f publish_ms=%.3f flush_total_ms=%.3f fallback_ms=%.3f wall-clock-not-GPU-duration=1\n",(unsigned long long)batch_time236.queued,(unsigned long long)batch_time236.fallback,(unsigned long long)batch_time236.flushes,(unsigned long long)batch_time236.draws,batch_time236.maximum,batch_time236.queue_ticks*ms,batch_time236.backend_ticks*ms,batch_time236.publish_ticks*ms,batch_time236.flush_ticks*ms,batch_time236.fallback_ticks*ms);
}
static struct {
 DrivingPacket234 packets[DRIVING_BATCH_COUNT234];unsigned count;size_t bytes;
 DrivingSpan183 spans[2];uint8_t *mapped[2],*targets;uint32_t target_fields[7];
#ifdef DRIVING_LEASE322
 DL322Key keys322[2];
#endif
} batch234;
static int driving_batch234_read_overlap(uint32_t va,size_t bytes)
{
 /*450: while a PC-mode session is open the pair is still unpublished. */
 const DrivingSpan183 *spans=batch234.count?batch234.spans:pc450.spans;
 if((!batch234.count&&!pc450.dirty)||!bytes)return 0;
 if(bytes>UINT32_MAX||bytes>(uint64_t)UINT32_MAX+1-va)return 1;
 for(unsigned i=0;i<2;i++)if((uint64_t)va+bytes>spans[i].address&&
   (uint64_t)spans[i].address+DRIVING_PAIR234>va)return 1;
 return 0;
}
#define DRIVING_PC450_IMPL
#include "driving_pc450.h"
#undef DRIVING_PC450_IMPL
static int driving_batch234_flush_body295(const char *reason,unsigned capacity255,size_t incoming255)
{
 if(pc450_flush_hook(reason))return 1; /*450 default OFF*/
 if(!batch234.count)return 1;
#ifdef DRIVING_LEASE322
 /* Only the pre-existing synchronous flush is pinned. No guest handoff,
  * clear/read barrier or publication is deferred by this lifetime guard. */
 DL322Lease lease322={0};
 if(!xbox_ContiguousLease322Begin(batch234.keys322,&lease322,DL322_PUBLISH_EXISTING))
  fail143("lease322 target lifetime changed before publication",batch234.count,0);
#endif
 driving_batchmap264_reset();batch_epoch236++;
 uint64_t flush_start236=driving_clock227();
 unsigned flushed_count236=batch234.count;size_t flushed_bytes255=batch234.bytes;
#ifdef NIGHTFIRE_BATCH_TIMING244
 nf_hw_batch244_mark(0,flushed_count236);
#endif
 static uint64_t flushes236,draws236;flushes236++;draws236+=flushed_count236;
 if(flushes236<=24||!(flushes236%120))fprintf(stderr,"[BATCH236] flush=%llu draws=%u total=%llu reason=%s saved_intervals=%u\n",(unsigned long long)flushes236,flushed_count236,(unsigned long long)draws236,reason,2*(flushed_count236-1));
 /* No partial guest publication on backend failure. Preserve packet storage
  * until fail143 stops; never replay a partially rendered accepted batch. */
 int handled248=0;
#ifdef NIGHTFIRE_PAIR_BATCH248
 NFHardwareBatchDraw248 sequence248[DRIVING_BATCH_COUNT234];
 _Static_assert(sizeof sequence248<=32768,"260 sequence stack bound");
 for(unsigned i=0;i<batch234.count;i++){
  DrivingPacket234 *p=&batch234.packets[i];sequence248[i].count=p->dense;
  for(unsigned lane=0;lane<2;lane++){
   sequence248[i].states[lane]=p->state;
   sequence248[i].states[lane].color=batch234.targets+lane*DRIVING_LANE234;
   sequence248[i].states[lane].depth=batch234.targets+(2+lane)*DRIVING_LANE234;
   sequence248[i].vertices[lane]=p->vertices+lane*p->dense;
  }
 }
 handled248=(batch_seed276||batch_seed278)?nf_hw_material_batch_seeds278(sequence248,batch234.count,batch_seed276,batch_seed278):nf_hw_material_retained315(sequence248,batch234.count);
 if(handled248<0)fail143("batch248 fatal after begin",batch234.count,0);
#endif
 if(!handled248)
 for(unsigned lane=0;lane<2;lane++){
  for(unsigned i=0;i<batch234.count;i++){
   DrivingPacket234 *p=&batch234.packets[i];NFHardwareState state=p->state;
   state.color=batch234.targets+lane*DRIVING_LANE234;state.depth=batch234.targets+(2+lane)*DRIVING_LANE234;
   if((i&&!nf_hw_batch234_boundary(&state))||!nf_hw_begin(&state)||
      !nf_hw_draw_material221(p->vertices+lane*p->dense,p->dense))fail143("batch234 draw/boundary",i,lane);
  }
  if(!nf_hw_sync())fail143("batch234 sync",batch234.count,lane);
 }
 uint64_t backend_done236=driving_clock227();
 batch_time236.backend_ticks+=backend_done236-flush_start236;
#ifdef NIGHTFIRE_BATCH_TIMING244
 nf_hw_batch244_mark(1,0);
#endif
#ifdef DRIVING_PUBLISH324
 if(!xbox_Publication324Begin(&lease322))fail143("publication324 invalid allocation lease",batch234.count,0);
#endif
 driving_join227(batch234.mapped[0],batch234.targets,batch234.targets+DRIVING_LANE234,640*480);
 driving_join227(batch234.mapped[1],batch234.targets+2*DRIVING_LANE234,batch234.targets+3*DRIVING_LANE234,640*480);
#ifdef DRIVING_PUBLISH324
 if(!xbox_Publication324End(&lease322))fail143("publication324 invalid release",batch234.count,0);
 {static unsigned reports324;if(!(reports324++%120))xbox_Publication324Report();}
#endif
#ifdef DRIVING_LEASE322
 if(!xbox_ContiguousLease322End(&lease322))fail143("lease322 publication release failed",batch234.count,0);
 {static uint64_t completed322;completed322++;
  if(completed322==1||!(completed322%120))fprintf(stderr,"[LEASE322] completed=%llu scope=existing_flush lifetime_pinned=1 exclusive_content=0 retained_handoffs=0\n",(unsigned long long)completed322);}
#endif
#ifdef DRIVING_CPU_ACCESS320
 if(driving_cpu320_enabled){
  uint32_t address320[2]={batch234.spans[0].address,batch234.spans[1].address};
  uint32_t bytes320[2]={2*DRIVING_LANE234,2*DRIVING_LANE234};
  driving_cpu320_publish(address320,bytes320);
#ifdef DRIVING_ACCESS321
  driving_access321_publish(address320,bytes320,(void *const *)batch234.mapped);
#endif
 }
#endif
 if(handled248==1&&batch_seed276)nf_hw_color_seed276_publish(batch_seed276);else nf_hw_color_seed276_revoke();
 if(handled248==1&&batch_seed278)nf_hw_depth_seed278_publish(batch_seed278);else nf_hw_depth_seed278_revoke();
 batch_seed276=batch_seed278=0;

#ifdef NIGHTFIRE_BATCH_TIMING244
 nf_hw_batch244_mark(2,0);
#endif
 uint64_t publish_ticks255=driving_clock227()-backend_done236;batch_time236.publish_ticks+=publish_ticks255;
 batch_time236.flushes++;batch_time236.draws+=flushed_count236;
 if(flushed_count236>batch_time236.maximum)batch_time236.maximum=flushed_count236;
 for(unsigned i=0;i<batch234.count;i++){free(batch234.packets[i].vertices);free(batch234.packets[i].textures);}
 driving_release253(DS253_TARGETS,batch234.targets);memset(&batch234,0,sizeof batch234);uint64_t elapsed250=driving_clock227()-flush_start236;batch_time236.flush_ticks+=elapsed250;dt250_flush(elapsed250);
 driving_census255_commit(reason,flushed_count236,flushed_bytes255,capacity255,incoming255,backend_done236-flush_start236,publish_ticks255,elapsed250);return 1;
}
/*295 opt-in host FP isolation on BOTH synchronous and worker batch paths.
 * Legacy host sticky side effects are deliberately not guest CPU semantics. */
static int driving_batch234_flush_impl255(const char *reason,unsigned capacity,size_t incoming){
#ifdef DRIVING_ASYNC295
 if(driving_async295_enabled()){
  DWORD error295=GetLastError();DrivingFP295 fp;driving_fp295_save(&fp);
  int result=driving_batch234_flush_body295(reason,capacity,incoming);
  driving_async295_fp_observed(&fp);driving_fp295_restore(&fp);SetLastError(error295);return result;
 }
#endif
 return driving_batch234_flush_body295(reason,capacity,incoming);
}
static int driving_batch234_flush(const char *reason){return driving_batch234_flush_impl255(reason,0,0);}
static int read_packet234(void *ctx,uint32_t va,uint32_t *out)
{if(driving_batch234_read_overlap(va,4))driving_batch234_flush("descriptor-read");return read143(ctx,va,out);}
static int same_target234(const uint32_t *s,const DrivingSpan183 *spans,uint8_t *const *mapped)
{
 if(memcmp(batch234.target_fields,s+0x200/4,sizeof batch234.target_fields))return 0;
 for(unsigned i=0;i<2;i++)if(batch234.spans[i].address!=spans[i].address||
  batch234.spans[i].available!=spans[i].available||batch234.spans[i].handle!=spans[i].handle||
  batch234.spans[i].instance!=spans[i].instance||batch234.spans[i].offset!=spans[i].offset||batch234.mapped[i]!=mapped[i])return 0;
 return 1;
}
static void *mapped_packet236(DrivingMap230 *mapping,uint64_t *epoch,DrivingProof264 *proof,uint32_t va,size_t bytes,int write){
 if(*epoch!=batch_epoch236){driving_map230_end(mapping);driving_map230_begin(mapping);*epoch=batch_epoch236;}
#ifdef DRIVING_NATIVE_MAPPING230
 if(proof->enabled)return driving_batchmap264(mapping,proof,batch_epoch236,batch234.count,va,bytes,write,(uintptr_t)xbox_GetMemoryOffset(),xbox_ContiguousAllocatedBytes());
#else
 (void)proof;
#endif
 return mapped_submit230(mapping,va,bytes,write);
}
static int prepare_packet234(const uint32_t *state,const unsigned char *known,NFVertexProgram *program,
                     unsigned primitive,const uint32_t *indices,unsigned count)
{
 DrivingQueue251 q251;driving_queue251_begin(&q251,batch_time236.flush_ticks);
 enum {LANE221=640*480*4,PAIR221=2*LANE221};unsigned profile;
 if(!indices||count<3||count>8192||!driving_plan221(state,known,program,primitive,&profile))return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);
 unsigned dense=primitive==5?count-count%3:(count-2)*3;
 if(dense>16384)return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);

 NFHardwareMaterial221 material={0};pixel_state221(&material.pixel,state);material.integer_depth291=driving_depth291(state,known);material.integer_depth302=driving_depth302(state,known);
 if(!nf_pixel221_valid(&material.pixel))return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);
 driving_queue251_mark(&q251,DQ251_RESOURCES,batch_time236.flush_ticks);
 uint32_t ramht,allocated=xbox_ContiguousAllocatedBytes();DrivingSpan183 spans[6],stream;
 uint8_t *mapped[6]={0};unsigned lengths[6]={PAIR221,PAIR221,0,0,0,0};size_t total=4*LANE221;
 DrivingSpan183 palette_span287;uint8_t *palette_mapped287=NULL;int indexed287=0;
 if(!read_packet234(NULL,0xfd002210,&ramht))return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);

 DrivingMap230 mapping230;driving_map230_begin(&mapping230);uint64_t mapping_epoch236=batch_epoch236;
 DrivingProof264 proof264;driving_proof264_begin(&proof264,batch_epoch236);
 for(unsigned i=0;i<6;i++){
  if(i>=2){
   unsigned slot=i-2,mode=material.pixel.program>>(slot*5)&31;
   if(mode!=1)continue;
   NFHardwareMaterialTexture221 *t=&material.textures[slot];
   t->format=state[(0x1b04+slot*64)/4];t->filter=state[(0x1b14+slot*64)/4];t->address=state[(0x1b08+slot*64)/4];
   t->anisotropy=1u<<(state[(0x1b0c+slot*64)/4]>>4&3);t->bias215=t->filter==0x02063f01;
   if((t->format>>8&255)==0x0b&&driving_palette287_state(state,known,slot)){
    indexed287=1;lengths[i]=DP287_INDEX_BYTES;total+=DP287_EXTRA_BYTES;
   }else lengths[i]=texture_bytes221(t->format);if(!lengths[i])return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);total+=lengths[i];
  }
  if(q251.enabled)q251.spans++;
  if(!driving_span183(NULL,read_packet234,ramht,state,known,i<2?(i?DRIVING_DEPTH183:DRIVING_COLOR183):DRIVING_TEXTURE183,
       i<2?0:i-2,lengths[i],allocated,&spans[i]))return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);
  /* Read-only texture aliases are legal, but cannot alias either writable target. */
  for(unsigned j=0;j<i&&j<2;j++)if(overlap221(spans[i].address,lengths[i],spans[j].address,lengths[j]))return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);
  if(i>=2&&driving_batch234_read_overlap(spans[i].address,lengths[i]))driving_batch234_flush("texture-read");
  mapped[i]=mapped_packet236(&mapping230,&mapping_epoch236,&proof264,spans[i].address,lengths[i],i<2);if(!mapped[i])return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);
  if(i==1&&(spans[0].address!=0x82cbc000u||spans[1].address!=0x8316c000u))return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);
  if(i==1&&batch234.count&&!same_target234(state,spans,mapped))driving_batch234_flush("target-change");
 }
 if(indexed287){
  /* Both original sources receive DMA, permission and writable-target alias
   * validation before either is copied. Never read a palette at GPU replay. */
  if(q251.enabled)q251.spans++;
  if(!driving_span183(NULL,read_packet234,ramht,state,known,DRIVING_PALETTE183,3,DP287_PALETTE_BYTES,allocated,&palette_span287))return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);
  for(unsigned j=0;j<2;j++)if(overlap221(palette_span287.address,DP287_PALETTE_BYTES,spans[j].address,lengths[j]))return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);
  if(driving_batch234_read_overlap(palette_span287.address,DP287_PALETTE_BYTES))driving_batch234_flush("texture-read");
  palette_mapped287=mapped_packet236(&mapping230,&mapping_epoch236,&proof264,palette_span287.address,DP287_PALETTE_BYTES,0);if(!palette_mapped287)return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);
 }
 unsigned maximum=0;for(unsigned i=0;i<count;i++)if(indices[i]>maximum)maximum=indices[i];
 const uint8_t *source[16]={0};uint32_t lengths_v[16]={0};
 for(unsigned k=0;k<16;k++){
  uint32_t fmt=state[(0x1760+4*k)/4],type=fmt&15,n=fmt>>4&15;if(!n)continue;
  unsigned bytes=type==6&&n==1?4:type==0&&n==4?4:type==2&&n<=4?n*4:type==5&&n<=4?n*2:0;
  uint64_t need=(uint64_t)maximum*(fmt>>8)+bytes;
  if(!bytes||!need||need>0x08000000)return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);
  if(q251.enabled)q251.spans++;
  if(!driving_span183(NULL,read_packet234,ramht,state,known,DRIVING_VERTEX183,k,(uint32_t)need,allocated,&stream))return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);
  if(driving_batch234_read_overlap(stream.address,(size_t)need))driving_batch234_flush("vertex-read");
  source[k]=mapped_packet236(&mapping230,&mapping_epoch236,&proof264,stream.address,(size_t)need,0);if(!source[k])return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);lengths_v[k]=(uint32_t)need;
 }
 driving_map230_end(&mapping230);
 driving_queue251_mark(&q251,DQ251_ALLOCATIONS,batch_time236.flush_ticks);
 size_t payload=(size_t)dense*2*sizeof(NFHardwareMaterialVertex221)+total-4*LANE221;
 if(payload+4*LANE221+nf_batch_metadata260(sizeof batch234,sizeof(DrivingPacket234))>DRIVING_BATCH_BYTES234)return driving_queue251_finish(&q251,0,batch_time236.flush_ticks);
 unsigned capacity260=nf_batch_capacity260(batch234.count,batch234.bytes,payload+sizeof(DrivingPacket234),DRIVING_BATCH_BYTES234);
 if(capacity260)driving_batch234_flush_impl255("capacity",capacity260,payload);
 if(q251.enabled)q251.allocations+=4;
 NFHardwareMaterialVertex221 *input=driving_acquire253(DS253_INPUT,(size_t)count*sizeof *input);
 NFHardwareMaterialVertex221 *vertices=malloc((size_t)dense*2*sizeof *vertices);
 uint8_t *owned=malloc(total>4*LANE221?total-4*LANE221:1);int accepted=0;
 typedef struct {float attributes[16][4];uint32_t index,previous;} ShaderMemo234;
 ShaderMemo234 *memo234=driving_acquire253(DS253_MEMO,256*sizeof *memo234);
 if(memo234&&DRIVING_OWNED253_ENABLED())for(unsigned i=0;i<256;i++)memo234[i].previous=0;
 if(!input||!vertices||!owned)goto done234;
 driving_queue251_mark(&q251,DQ251_TEXTURES,batch_time236.flush_ticks);
 /* Own the current complete texture spans before any compatibility decision. */
 size_t at=0;if(q251.enabled)q251.texture_bytes=total-4*LANE221;
 for(unsigned k=0;k<4;k++)if(lengths[k+2]){memcpy(owned+at,mapped[k+2],lengths[k+2]);material.textures[k].data=owned+at;material.textures[k].identity228=mapped[k+2];material.textures[k].available=lengths[k+2];at+=lengths[k+2];}
 if(indexed287){
  /* Retain raw4096 index +1024 palette and derived16384 bytes in this same
   * owned allocation; byte accounting above includes all three. */
  memcpy(owned+at,palette_mapped287,DP287_PALETTE_BYTES);
  NFHardwareMaterialTexture221 *t=&material.textures[3];
  if(!driving_palette287_expand(owned+at+DP287_PALETTE_BYTES,DP287_BGRA_BYTES,
      t->data,t->available,owned+at,DP287_PALETTE_BYTES))goto done234;
  t->data=owned+at+DP287_PALETTE_BYTES;t->available=DP287_BGRA_BYTES;
  t->format=0x06610629u;at+=DP287_EXTRA_BYTES;
 }
 driving_road_specialize242(profile,program,&material);
 driving_queue251_mark(&q251,DQ251_VERTICES,batch_time236.flush_ticks);
 DrivingVertex247 vertex247=driving_bind247(program,profile);
 const int finite328=driving_finite328_enabled();
 const int proof466=finite328&&proof466_enabled();
 if(proof466)vertex247=driving_proof466_select(vertex247);
 float bias,slope;memcpy(&bias,&state[0x9c0/4],4);memcpy(&slope,&state[0x9c4/4],4);
 for(unsigned i=0;i<count;i++){
  if(q251.enabled)q251.inputs++;
  float in[16][4]={{0}},out[16][4],fog=1;
  for(unsigned k=0;k<16;k++)if(source[k]&&!driving_vertex218(source[k],lengths_v[k],state[(0x1760+4*k)/4],0,indices[i],in[k]))goto done234;
  ShaderMemo234 *slot234=memo234?&memo234[indices[i]&255]:NULL;
  if(slot234 && slot234->previous && slot234->index==indices[i] &&
     !memcmp(slot234->attributes,in,sizeof in)){
   if(q251.enabled)q251.memo_hits++;
   input[i]=input[slot234->previous-1];continue;
  }
  if(q251.enabled)q251.transforms++;
  int result466=vertex247(program,in,out);if(!result466)goto done234;
  if(finite328){if(!(proof466&&result466==2)&&!driving_finite328(out))goto done234;}
  else for(unsigned k=0;k<64;k++)if(!isfinite(((float*)out)[k]))goto done234;
  if(!out[0][3])goto done234;
  if(state[0x2a4/4]&&!driving_fog217(state[0x29c/4],bias,slope,out[5][0],&fog))goto done234;
  NFHardwareMaterialVertex221 *v=&input[i];memcpy(v->position,out[0],16);memcpy(v->uv,out[9],64);v->fog=fog;
  for(unsigned k=0;k<4;k++){v->color[k]=fminf(1,fmaxf(0,out[3][k]));v->specular[k]=fminf(1,fmaxf(0,out[4][k]));}
  for(unsigned stage=0;stage<4;stage++){
   unsigned mode=material.pixel.program>>(stage*5)&31;float *uv=v->uv[stage];
   if(material.pixel.white_stage2_242&&stage==2){if(!nf_pixel_zero_uv242(&material.pixel,stage,uv))goto done234;}
   else if(mode==1&&(!uv[3]||!isfinite(uv[0]/uv[3])||!isfinite(uv[1]/uv[3])))goto done234;
   if(mode==4)for(unsigned k=0;k<4;k++)if(uv[k]<0||uv[k]>1)goto done234;
  }
  if(slot234){memcpy(slot234->attributes,in,sizeof in);slot234->index=indices[i];slot234->previous=i+1;}
 }
 driving_queue251_mark(&q251,DQ251_EXPAND,batch_time236.flush_ticks);
 for(unsigned i=0;i<dense;i++){
  unsigned tri=i/3,k=i%3,src=primitive==5?i:tri+(k==2?2:((tri&1)?1-k:k));
  for(unsigned lane=0;lane<2;lane++){
   NFHardwareMaterialVertex221 *v=&vertices[lane*dense+i];*v=input[src];v->position[0]+=lane*.5f;v->position[1]+=lane*.5f;
   float w=v->position[3];if(!isfinite((v->position[0]*2/640-1)*w)||!isfinite((1-v->position[1]*2/480)*w)||!isfinite(v->position[2]/16777215*w))goto done234;
   for(unsigned stage=0;stage<4;stage++)if((material.pixel.program>>(stage*5)&31)==1&&
      (v->uv[stage][3]<0)!=(vertices[lane*dense+i-k].uv[stage][3]<0))goto done234;
  }
 }

 if(q251.enabled)q251.expanded=2u*dense;
 driving_queue251_mark(&q251,DQ251_STAGE,batch_time236.flush_ticks);
 if(!batch234.count){
  if(q251.enabled)q251.allocations++;
  batch234.targets=driving_acquire253(DS253_TARGETS,4*LANE221);if(!batch234.targets)goto done234;
  memcpy(batch234.spans,spans,2*sizeof *spans);memcpy(batch234.mapped,mapped,2*sizeof *mapped);
  memcpy(batch234.target_fields,state+0x200/4,sizeof batch234.target_fields);
#ifdef DRIVING_LEASE322
  for(unsigned i322=0;i322<2;i322++)if(!xbox_ContiguousKey322(spans[i322].address,2*LANE221,mapped[i322],&batch234.keys322[i322]))
   fail143("lease322 target snapshot rejected",spans[i322].address,2*LANE221);
#endif
  batch_seed276=batch_seed278=0;int split276=0,split278=0;
  if(DRIVING_OWNED253_ENABLED()&&allocation276&&batch234.targets==driving_owned253[DS253_TARGETS].bytes&&(nf_hw_color_seed276_enabled()||nf_hw_depth_seed278_enabled())){
   NFHardwareColorKey276 key276={0};memcpy(key276.surface,batch234.target_fields,sizeof key276.surface);
   for(unsigned k276=0;k276<2;k276++){
    key276.span[k276][0]=spans[k276].address;key276.span[k276][1]=spans[k276].available;
    key276.span[k276][2]=spans[k276].handle;key276.span[k276][3]=spans[k276].instance;key276.span[k276][4]=spans[k276].offset;key276.mapped[k276]=(uintptr_t)mapped[k276];
   }
   if(nf_hw_color_seed276_enabled())split276=nf_hw_color_seed276_split(&key276,allocation276,mapped[0],batch234.targets,&batch_seed276);
   if(nf_hw_depth_seed278_enabled())split278=nf_hw_depth_seed278_split(&key276,allocation276,mapped[1],batch234.targets+2*LANE221,&batch_seed278);
  }
  /*450: an open PC-mode session owns the current pair on the GPU; a new batch
   * for the same target needs no import, so its lane split is skipped. */
  int skip450=pc450.dirty&&batch234.targets==pc450.targets&&pc450_same_target(spans,mapped,batch234.target_fields);
  if(pc450.dirty&&!skip450)pc450_publish("target-change");
  if(skip450)pc450.split_skipped++;
  if(!split276&&!skip450)driving_split227(mapped[0],batch234.targets,batch234.targets+LANE221,640*480);
  if(!split278&&!skip450)driving_split227(mapped[1],batch234.targets+2*LANE221,batch234.targets+3*LANE221,640*480);
  if(q251.enabled)q251.seed_bytes=4*LANE221;
  batch234.bytes=4*LANE221+nf_batch_metadata260(sizeof batch234,sizeof(DrivingPacket234));
 }

 DrivingPacket234 *packet=&batch234.packets[batch234.count];memset(packet,0,sizeof *packet);
 packet->material=material;packet->vertices=vertices;packet->textures=owned;packet->dense=dense;
 NFHardwareState *out=&packet->state;out->material221=&packet->material;
 out->width=out->right=640;out->height=out->bottom=480;out->pitch=out->depth_pitch=2560;
 out->depth_enable=state[0x30c/4];out->depth_write=state[0x35c/4];out->depth_func=state[0x354/4];
 out->alpha_enable=state[0x300/4];out->alpha_func=state[0x33c/4];out->alpha_ref=state[0x340/4];
 out->blend_enable=state[0x304/4];out->blend_src=state[0x344/4];out->blend_dst=state[0x348/4];
 out->cull_enable=state[0x308/4];out->cull_face=state[0x39c/4];out->front_face=state[0x3a0/4];
 out->fog_color=state[0x2a8/4];out->filtered=1;out->combiner=1;out->scale=1;
 unsigned cm=state[0x358/4];out->color_write_mask212=0x10|((cm>>16&1)?1:0)|((cm>>8&1)?2:0)|((cm&1)?4:0)|((cm>>24&1)?8:0);
 batch234.bytes+=payload+sizeof *packet;batch234.count++;accepted=1;driving_proof264_publish(&proof264,batch_epoch236);
 done234:driving_release253(DS253_MEMO,memo234);driving_release253(DS253_INPUT,input);if(!accepted){free(vertices);free(owned);}return driving_queue251_finish(&q251,accepted,batch_time236.flush_ticks);
}
static int submit234_impl(const uint32_t *state,const unsigned char *known,NFVertexProgram *program,
                     unsigned primitive,const uint32_t *indices,unsigned count)
{
 if(!batch234.count&&!pc450.dirty&&!nf_hw_sync())fail143("batch234 prior completion",primitive,count);
 if(!batch234.count&&!pc450.dirty&&!nf_hw_batch234_prepare(640,480)){
  driving_batch234_flush("backend-unsupported");return submit221(state,known,program,primitive,indices,count);
 }
 if(prepare_packet234(state,known,program,primitive,indices,count))return 1;
 driving_batch234_flush("current-refused");return submit221(state,known,program,primitive,indices,count);
}
static int submit234(const uint32_t *state,const unsigned char *known,NFVertexProgram *program,unsigned primitive,const uint32_t *indices,unsigned count){
 uint64_t start=driving_clock227(),prior_flush=batch_time236.flush_ticks;
 int result=submit234_impl(state,known,program,primitive,indices,count);
 uint64_t elapsed=driving_clock227()-start-(batch_time236.flush_ticks-prior_flush);
 if(result&&batch234.count){batch_time236.queued++;batch_time236.queue_ticks+=elapsed;}
 else{batch_time236.fallback++;batch_time236.fallback_ticks+=elapsed;}
 return result;
}
#endif

