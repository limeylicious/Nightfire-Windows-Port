#include "driving_extramap419.h"
#include "driving_timing250.h"
/* Exact owned single-sample256 material adapter. Included after submit221 helpers.
 * Guest targets are Morton-swizzled; backend sees owned linear planes.
 * No guest writes until synchronous success; no replay after begin. */
#include "driving_plan243.h"
#include "gpu144/nightfire_swizzled_shadow131.h"
static int submit243_impl250(const uint32_t *state,const unsigned char *known,NFVertexProgram *program,
                     unsigned primitive,const uint32_t *indices,unsigned count)
{
 enum {LANE221=256*256*4,PAIR221=LANE221};unsigned profile;
 if(!indices||count<3||count>8192||!driving_plan243(state,known,program,primitive,&profile))return 0;
 unsigned dense=primitive==5?count-count%3:(count-2)*3;
 if(dense>16384)return 0;
 uint64_t times227[6];times227[0]=dt250_clock();uint64_t child250=dt250_flush_ticks();
 NFHardwareMaterial221 material={0};pixel_state221(&material.pixel,state);
 if(!nf_pixel221_valid(&material.pixel))return 0;
 uint32_t ramht,allocated=xbox_ContiguousAllocatedBytes();DrivingSpan183 spans[6],stream;
 uint8_t *mapped[6]={0};unsigned lengths[6]={PAIR221,PAIR221,0,0,0,0};size_t total=2*LANE221;
 if(!read143(NULL,0xfd002210,&ramht))return 0;
 residency_flush236("offscreen243-before-read");
 if(!nf_hw_sync())fail143("offscreen prior completion243",primitive,count);
 DrivingExtraMap419 mapping419;driving_extramap419_begin(&mapping419);
 for(unsigned i=0;i<6;i++){
  if(i>=2){
   unsigned slot=i-2,mode=material.pixel.program>>(slot*5)&31;
   if(mode!=1)continue;
   NFHardwareMaterialTexture221 *t=&material.textures[slot];
   t->format=state[(0x1b04+slot*64)/4];t->filter=state[(0x1b14+slot*64)/4];t->address=state[(0x1b08+slot*64)/4];
   t->anisotropy=1u<<(state[(0x1b0c+slot*64)/4]>>4&3);t->bias215=t->filter==0x02063f01;
   lengths[i]=texture_bytes221(t->format);if(!lengths[i]){driving_extramap419_end(&mapping419);return 0;}total+=lengths[i];
  }
  if(!driving_span183(NULL,read143,ramht,state,known,i<2?(i?DRIVING_DEPTH183:DRIVING_COLOR183):DRIVING_TEXTURE183,
       i<2?0:i-2,lengths[i],allocated,&spans[i])){driving_extramap419_end(&mapping419);return 0;}
  /* Read-only texture aliases are legal, but cannot alias either writable target. */
  for(unsigned j=0;j<i&&j<2;j++)if(overlap221(spans[i].address,lengths[i],spans[j].address,lengths[j])){driving_extramap419_end(&mapping419);return 0;}
  mapped[i]=mapped_offscreen419(&mapping419,spans[i].address,lengths[i],i<2);if(!mapped[i]){driving_extramap419_end(&mapping419);return 0;}
 }
 unsigned maximum=0;for(unsigned i=0;i<count;i++)if(indices[i]>maximum)maximum=indices[i];
 const uint8_t *source[16]={0};uint32_t lengths_v[16]={0};
 for(unsigned k=0;k<16;k++){
  uint32_t fmt=state[(0x1760+4*k)/4],type=fmt&15,n=fmt>>4&15;if(!n)continue;
  unsigned bytes=type==6&&n==1?4:type==0&&n==4?4:type==2&&n<=4?n*4:type==5&&n<=4?n*2:0;
  uint64_t need=(uint64_t)maximum*(fmt>>8)+bytes;
  if(!bytes||!need||need>0x08000000||!driving_span183(NULL,read143,ramht,state,known,DRIVING_VERTEX183,k,(uint32_t)need,allocated,&stream)){driving_extramap419_end(&mapping419);return 0;}
  source[k]=mapped_offscreen419(&mapping419,stream.address,(size_t)need,0);if(!source[k]){driving_extramap419_end(&mapping419);return 0;}lengths_v[k]=(uint32_t)need;
 }
 driving_extramap419_end(&mapping419);
 static DrivingStorage228 input_storage,vertex_storage,target_storage,shader_storage234;
 NFHardwareMaterialVertex221 *input=driving_storage228(&input_storage,(size_t)count*sizeof *input);
 NFHardwareMaterialVertex221 *vertices=driving_storage228(&vertex_storage,(size_t)dense*sizeof *vertices);
 uint8_t *owned=driving_storage228(&target_storage,total);int accepted=0;
 /* Local to this original draw; compare every freshly decoded attribute before
  * reusing a successful transform. Never reuse solely by guest address/index. */
 typedef struct {float attributes[16][4];uint32_t index,previous;} ShaderMemo234;
 ShaderMemo234 *memo234=driving_storage228(&shader_storage234,256*sizeof *memo234);
 if(memo234)for(unsigned i=0;i<256;i++)memo234[i].previous=0;
 if(!input||!vertices||!owned)goto done221;
 /* Own the current complete texture spans before any compatibility decision. */
 size_t at=2*LANE221;
 for(unsigned k=0;k<4;k++)if(lengths[k+2]){memcpy(owned+at,mapped[k+2],lengths[k+2]);material.textures[k].data=owned+at;material.textures[k].identity228=mapped[k+2];material.textures[k].available=lengths[k+2];at+=lengths[k+2];}
 times227[1]=dt250_clock();
 float bias,slope;memcpy(&bias,&state[0x9c0/4],4);memcpy(&slope,&state[0x9c4/4],4);
 for(unsigned i=0;i<count;i++){
  float in[16][4]={{0}},out[16][4],fog=1;
  for(unsigned k=0;k<16;k++)if(source[k]&&!driving_vertex218(source[k],lengths_v[k],state[(0x1760+4*k)/4],0,indices[i],in[k]))goto done221;
  ShaderMemo234 *slot234=memo234?&memo234[indices[i]&255]:NULL;
  if(slot234 && slot234->previous && slot234->index==indices[i] &&
     !memcmp(slot234->attributes,in,sizeof in)){
   input[i]=input[slot234->previous-1];continue;
  }
  if(!nf_vp_run(program,in,out))goto done221;
  for(unsigned k=0;k<64;k++)if(!isfinite(((float*)out)[k]))goto done221;
  if(!out[0][3])goto done221;
  if(state[0x2a4/4]&&!driving_fog217(state[0x29c/4],bias,slope,out[5][0],&fog))goto done221;
  NFHardwareMaterialVertex221 *v=&input[i];memcpy(v->position,out[0],16);memcpy(v->uv,out[9],64);v->fog=fog;
  for(unsigned k=0;k<4;k++){v->color[k]=fminf(1,fmaxf(0,out[3][k]));v->specular[k]=fminf(1,fmaxf(0,out[4][k]));}
  for(unsigned stage=0;stage<4;stage++){
   unsigned mode=material.pixel.program>>(stage*5)&31;float *uv=v->uv[stage];
   if(mode==1&&(!uv[3]||!isfinite(uv[0]/uv[3])||!isfinite(uv[1]/uv[3])))goto done221;
   if(mode==4)for(unsigned k=0;k<4;k++)if(uv[k]<0||uv[k]>1)goto done221;
  }
  if(slot234){memcpy(slot234->attributes,in,sizeof in);slot234->index=indices[i];slot234->previous=i+1;}
 }
 for(unsigned i=0;i<dense;i++){
  unsigned tri=i/3,k=i%3,src=primitive==5?i:tri+(k==2?2:((tri&1)?1-k:k));
  for(unsigned lane=0;lane<1;lane++){
   NFHardwareMaterialVertex221 *v=&vertices[lane*dense+i];*v=input[src];/* Single sample: no lane shift. */
   float w=v->position[3];if(!isfinite((v->position[0]*2/256-1)*w)||!isfinite((1-v->position[1]*2/256)*w)||!isfinite(v->position[2]/16777215*w))goto done221;
   for(unsigned stage=0;stage<4;stage++)if((material.pixel.program>>(stage*5)&31)==1&&
      (v->uv[stage][3]<0)!=(vertices[lane*dense+i-k].uv[stage][3]<0))goto done221;
  }
 }
 times227[2]=dt250_clock();
 nf_swizzled131_import_size(owned,1024,mapped[0],256);
 nf_swizzled131_import_size(owned+LANE221,1024,mapped[1],256);

 times227[3]=dt250_clock();
 {
  NFHardwareState s={0};s.material221=&material;s.color=owned;s.depth=owned+LANE221;
  s.width=s.right=256;s.height=s.bottom=256;s.pitch=s.depth_pitch=1024;
  s.depth_enable=state[0x30c/4];s.depth_write=state[0x35c/4];s.depth_func=state[0x354/4];
  s.alpha_enable=state[0x300/4];s.alpha_func=state[0x33c/4];s.alpha_ref=state[0x340/4];
  s.blend_enable=state[0x304/4];s.blend_src=state[0x344/4];s.blend_dst=state[0x348/4];
  s.cull_enable=state[0x308/4];s.cull_face=state[0x39c/4];s.front_face=state[0x3a0/4];
  s.fog_color=state[0x2a8/4];s.filtered=1;s.combiner=1;s.scale=1;
  unsigned cm=state[0x358/4];s.color_write_mask212=0x10|((cm>>16&1)?1:0)|((cm>>8&1)?2:0)|((cm&1)?4:0)|((cm>>24&1)?8:0);
  /* Begin can retain staging even on a later failure: no replay/free past here. */
  text_material510(P510_GEOMETRY,0,spans[0].address,profile,0,0,&s,&material,vertices,dense,0,0);
  if(!nf_hw_begin(&s)||!nf_hw_draw_material221(vertices,dense)||!nf_hw_sync())
   fail143("offscreen material243 completion",profile,count);
 }
 times227[4]=dt250_clock();
 nf_swizzled131_export_size(mapped[0],owned,1024,256);
 /* Exact contract disables stencil; preserve read-only Z24S8 bytes. */
 if(state[0x30c/4]&&state[0x35c/4])nf_swizzled131_export_size(mapped[1],owned+LANE221,1024,256);
 driving_extramap419_publish(&mapping419,1);
 times227[5]=dt250_clock();
 dt250_phases(profile,times227,child250);
 {static unsigned completed243;unsigned n=++completed243;
  if(n<=8||n%120==0)fprintf(stderr,"[GPU243] profile=%u accepted=%u indices=%u color=%08X depth=%08X single-sample256=1 synchronous=1\n",profile,n,count,state[0x210/4],state[0x214/4]);
 }
 accepted=1;
done221:
 driving_storage_release228(&input_storage,input);driving_storage_release228(&vertex_storage,vertices);
 driving_storage_release228(&target_storage,owned);driving_storage_release228(&shader_storage234,memo234);return accepted;
}


static int submit243(const uint32_t *state,const unsigned char *known,NFVertexProgram *program,
 unsigned primitive,const uint32_t *indices,unsigned count){
 DrivingAttempt250 timing250=dt250_attempt();
 int accepted=submit243_impl250(state,known,program,primitive,indices,count);
 dt250_attempt_end(timing250,accepted);return accepted;
}
