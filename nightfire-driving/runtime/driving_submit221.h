#include "driving_native247.h"
/* Owned material submission. No guest target writes until both sample lanes
 * finish. The two-lane sample model remains experimental, not NV2A AA proof. */
#include "driving_plan221.h"
#include "driving_vertex218.h"
#include "driving_fog217.h"
#include "driving_lanes227.h"
#include "driving_timing227.h"
#include "driving_storage228.h"
#include "driving_road242.h"
#include "driving_palette287.h"
#include "driving_depth291.h"
#include "driving_depth302.h"
#include "driving_mapping230.h"
static void *mapped_submit230(DrivingMap230 *ctx,uint32_t va,size_t bytes,int write)
{
#ifdef DRIVING_NATIVE_MAPPING230
 return driving_map230(ctx,va,bytes,write,(uintptr_t)xbox_GetMemoryOffset(),xbox_ContiguousAllocatedBytes());
#else
 /* Owned-resource fixtures supply their own checked mapping callback. */
 (void)ctx;return mapped201(va,bytes,write);
#endif
}
static int overlap221(uint32_t a,size_t an,uint32_t b,size_t bn)
{return (uint64_t)a+an>b && (uint64_t)b+bn>a;}
static void pixel_state221(NFPixel221 *p,const uint32_t *s)
{
 memset(p,0,sizeof *p);p->control=s[0x1e60/4];p->program=s[0x1e70/4];
 p->final0=s[0x288/4];p->final1=s[0x28c/4];
 p->final_constant0=s[0x1e20/4];p->final_constant1=s[0x1e24/4];
 for(unsigned i=0;i<8;i++){
  p->rgb[i]=s[0xac0/4+i];p->alpha[i]=s[0x260/4+i];
  p->rgb_out[i]=s[0x1e40/4+i];p->alpha_out[i]=s[0xaa0/4+i];
  p->constant0[i]=s[0xa60/4+i];p->constant1[i]=s[0xa80/4+i];
 }
}
static unsigned texture_bytes221(unsigned fmt)
{
 unsigned kind=fmt>>8&255,w=1u<<(fmt>>20&15),h=1u<<(fmt>>24&15),levels=fmt>>16&15;
 if((kind!=5&&kind!=6&&kind!=12&&kind!=14)||!levels||w>4096||h>4096)return 0;
 uint64_t bytes=0;
 for(unsigned i=0;i<levels;i++){
  bytes+=kind==12||kind==14?(uint64_t)((w+3)/4)*((h+3)/4)*(kind==12?8:16):(uint64_t)w*h*(kind==5?2:4);
  if(i+1<levels&&w==1&&h==1)return 0;
  w=w>1?w/2:1;h=h>1?h/2:1;
 }
 return bytes<=0x08000000?(unsigned)bytes:0;
}
static int submit221(const uint32_t *state,const unsigned char *known,NFVertexProgram *program,
                     unsigned primitive,const uint32_t *indices,unsigned count)
{
 enum {LANE221=640*480*4,PAIR221=2*LANE221};unsigned profile;
 if(!indices||count<3||count>8192||!driving_plan221(state,known,program,primitive,&profile))return 0;
 unsigned dense=primitive==5?count-count%3:(count-2)*3;
 if(dense>16384)return 0;
 uint64_t times227[6];times227[0]=driving_clock227();
 NFHardwareMaterial221 material={0};pixel_state221(&material.pixel,state);material.integer_depth291=driving_depth291(state,known);material.integer_depth302=driving_depth302(state,known);
 if(!nf_pixel221_valid(&material.pixel))return 0;
 uint32_t ramht,allocated=xbox_ContiguousAllocatedBytes();DrivingSpan183 spans[6],stream;
 uint8_t *mapped[6]={0};unsigned lengths[6]={PAIR221,PAIR221,0,0,0,0};size_t total=4*LANE221;
 DrivingSpan183 palette_span287;uint8_t *palette_mapped287=NULL;int indexed287=0;
 if(!read143(NULL,0xfd002210,&ramht))return 0;
 if(!nf_hw_sync())fail143("material prior completion221",primitive,count);
 DrivingMap230 mapping230;driving_map230_begin(&mapping230);
 for(unsigned i=0;i<6;i++){
  if(i>=2){
   unsigned slot=i-2,mode=material.pixel.program>>(slot*5)&31;
   if(mode!=1)continue;
   NFHardwareMaterialTexture221 *t=&material.textures[slot];
   t->format=state[(0x1b04+slot*64)/4];t->filter=state[(0x1b14+slot*64)/4];t->address=state[(0x1b08+slot*64)/4];
   t->anisotropy=1u<<(state[(0x1b0c+slot*64)/4]>>4&3);t->bias215=t->filter==0x02063f01;
   if((t->format>>8&255)==0x0b&&driving_palette287_state(state,known,slot)){
    indexed287=1;lengths[i]=DP287_INDEX_BYTES;total+=DP287_EXTRA_BYTES;
   }else lengths[i]=texture_bytes221(t->format);if(!lengths[i])return 0;total+=lengths[i];
  }
  if(!driving_span183(NULL,read143,ramht,state,known,i<2?(i?DRIVING_DEPTH183:DRIVING_COLOR183):DRIVING_TEXTURE183,
       i<2?0:i-2,lengths[i],allocated,&spans[i]))return 0;
  /* Read-only texture aliases are legal, but cannot alias either writable target. */
  for(unsigned j=0;j<i&&j<2;j++)if(overlap221(spans[i].address,lengths[i],spans[j].address,lengths[j]))return 0;
  mapped[i]=mapped_submit230(&mapping230,spans[i].address,lengths[i],i<2);if(!mapped[i])return 0;
 }
 if(indexed287){
  /* Both original sources receive DMA, permission and writable-target alias
   * validation before either is copied. Never read a palette at GPU replay. */
  
  if(!driving_span183(NULL,read143,ramht,state,known,DRIVING_PALETTE183,3,DP287_PALETTE_BYTES,allocated,&palette_span287))return 0;
  for(unsigned j=0;j<2;j++)if(overlap221(palette_span287.address,DP287_PALETTE_BYTES,spans[j].address,lengths[j]))return 0;
  
  palette_mapped287=mapped_submit230(&mapping230,palette_span287.address,DP287_PALETTE_BYTES,0);if(!palette_mapped287)return 0;
 }
 unsigned maximum=0;for(unsigned i=0;i<count;i++)if(indices[i]>maximum)maximum=indices[i];
 const uint8_t *source[16]={0};uint32_t lengths_v[16]={0};
 for(unsigned k=0;k<16;k++){
  uint32_t fmt=state[(0x1760+4*k)/4],type=fmt&15,n=fmt>>4&15;if(!n)continue;
  unsigned bytes=type==6&&n==1?4:type==0&&n==4?4:type==2&&n<=4?n*4:type==5&&n<=4?n*2:0;
  uint64_t need=(uint64_t)maximum*(fmt>>8)+bytes;
  if(!bytes||!need||need>0x08000000||!driving_span183(NULL,read143,ramht,state,known,DRIVING_VERTEX183,k,(uint32_t)need,allocated,&stream))return 0;
  source[k]=mapped_submit230(&mapping230,stream.address,(size_t)need,0);if(!source[k])return 0;lengths_v[k]=(uint32_t)need;
 }
 driving_map230_end(&mapping230);
 static DrivingStorage228 input_storage,vertex_storage,target_storage,shader_storage234;
 NFHardwareMaterialVertex221 *input=driving_storage228(&input_storage,(size_t)count*sizeof *input);
 NFHardwareMaterialVertex221 *vertices=driving_storage228(&vertex_storage,(size_t)dense*2*sizeof *vertices);
 uint8_t *owned=driving_storage228(&target_storage,total);int accepted=0;
 /* Local to this original draw; compare every freshly decoded attribute before
  * reusing a successful transform. Never reuse solely by guest address/index. */
 typedef struct {float attributes[16][4];uint32_t index,previous;} ShaderMemo234;
 ShaderMemo234 *memo234=driving_storage228(&shader_storage234,256*sizeof *memo234);
 if(memo234)for(unsigned i=0;i<256;i++)memo234[i].previous=0;
 if(!input||!vertices||!owned)goto done221;
 /* Own the current complete texture spans before any compatibility decision. */
 size_t at=4*LANE221;
 for(unsigned k=0;k<4;k++)if(lengths[k+2]){memcpy(owned+at,mapped[k+2],lengths[k+2]);material.textures[k].data=owned+at;material.textures[k].identity228=mapped[k+2];material.textures[k].available=lengths[k+2];at+=lengths[k+2];}
 if(indexed287){
  /* Retain raw4096 index +1024 palette and derived16384 bytes in this same
   * owned allocation; byte accounting above includes all three. */
  memcpy(owned+at,palette_mapped287,DP287_PALETTE_BYTES);
  NFHardwareMaterialTexture221 *t=&material.textures[3];
  if(!driving_palette287_expand(owned+at+DP287_PALETTE_BYTES,DP287_BGRA_BYTES,
      t->data,t->available,owned+at,DP287_PALETTE_BYTES))goto done221;
  t->data=owned+at+DP287_PALETTE_BYTES;t->available=DP287_BGRA_BYTES;
  t->format=0x06610629u;at+=DP287_EXTRA_BYTES;
 }
 driving_road_specialize242(profile,program,&material);
 DrivingVertex247 vertex247=driving_bind247(program,profile);
 times227[1]=driving_clock227();
 float bias,slope;memcpy(&bias,&state[0x9c0/4],4);memcpy(&slope,&state[0x9c4/4],4);
 for(unsigned i=0;i<count;i++){
  float in[16][4]={{0}},out[16][4],fog=1;
  for(unsigned k=0;k<16;k++)if(source[k]&&!driving_vertex218(source[k],lengths_v[k],state[(0x1760+4*k)/4],0,indices[i],in[k]))goto done221;
  ShaderMemo234 *slot234=memo234?&memo234[indices[i]&255]:NULL;
  if(slot234 && slot234->previous && slot234->index==indices[i] &&
     !memcmp(slot234->attributes,in,sizeof in)){
   input[i]=input[slot234->previous-1];continue;
  }
  if(!vertex247(program,in,out))goto done221;
  for(unsigned k=0;k<64;k++)if(!isfinite(((float*)out)[k]))goto done221;
  if(!out[0][3])goto done221;
  if(state[0x2a4/4]&&!driving_fog217(state[0x29c/4],bias,slope,out[5][0],&fog))goto done221;
  NFHardwareMaterialVertex221 *v=&input[i];memcpy(v->position,out[0],16);memcpy(v->uv,out[9],64);v->fog=fog;
  for(unsigned k=0;k<4;k++){v->color[k]=fminf(1,fmaxf(0,out[3][k]));v->specular[k]=fminf(1,fmaxf(0,out[4][k]));}
  for(unsigned stage=0;stage<4;stage++){
   unsigned mode=material.pixel.program>>(stage*5)&31;float *uv=v->uv[stage];
   if(material.pixel.white_stage2_242&&stage==2){if(!nf_pixel_zero_uv242(&material.pixel,stage,uv))goto done221;}
   else if(mode==1&&(!uv[3]||!isfinite(uv[0]/uv[3])||!isfinite(uv[1]/uv[3])))goto done221;
   if(mode==4)for(unsigned k=0;k<4;k++)if(uv[k]<0||uv[k]>1)goto done221;
  }
  if(slot234){memcpy(slot234->attributes,in,sizeof in);slot234->index=indices[i];slot234->previous=i+1;}
 }
 for(unsigned i=0;i<dense;i++){
  unsigned tri=i/3,k=i%3,src=primitive==5?i:tri+(k==2?2:((tri&1)?1-k:k));
  for(unsigned lane=0;lane<2;lane++){
   NFHardwareMaterialVertex221 *v=&vertices[lane*dense+i];*v=input[src];v->position[0]+=lane*.5f;v->position[1]+=lane*.5f;
   float w=v->position[3];if(!isfinite((v->position[0]*2/640-1)*w)||!isfinite((1-v->position[1]*2/480)*w)||!isfinite(v->position[2]/16777215*w))goto done221;
   for(unsigned stage=0;stage<4;stage++)if((material.pixel.program>>(stage*5)&31)==1&&
      (v->uv[stage][3]<0)!=(vertices[lane*dense+i-k].uv[stage][3]<0))goto done221;
  }
 }
 times227[2]=driving_clock227();
 driving_split227(mapped[0],owned,owned+LANE221,640*480);
 driving_split227(mapped[1],owned+2*LANE221,owned+3*LANE221,640*480);

 times227[3]=driving_clock227();
 NFHardwareState pair234[2];
 const NFHardwareMaterialVertex221 *pv234[2]={vertices,vertices+dense};
 for(unsigned lane=0;lane<2;lane++){
  NFHardwareState s={0};s.material221=&material;s.color=owned+lane*LANE221;s.depth=owned+(2+lane)*LANE221;
  s.width=s.right=640;s.height=s.bottom=480;s.pitch=s.depth_pitch=2560;
  s.depth_enable=state[0x30c/4];s.depth_write=state[0x35c/4];s.depth_func=state[0x354/4];
  s.alpha_enable=state[0x300/4];s.alpha_func=state[0x33c/4];s.alpha_ref=state[0x340/4];
  s.blend_enable=state[0x304/4];s.blend_src=state[0x344/4];s.blend_dst=state[0x348/4];
  s.cull_enable=state[0x308/4];s.cull_face=state[0x39c/4];s.front_face=state[0x3a0/4];
  s.fog_color=state[0x2a8/4];s.filtered=1;s.combiner=1;s.scale=1;
  unsigned cm=state[0x358/4];s.color_write_mask212=0x10|((cm>>16&1)?1:0)|((cm>>8&1)?2:0)|((cm&1)?4:0)|((cm>>24&1)?8:0);
  pair234[lane]=s;
 }
 int result234=nf_hw_material_pair234(pair234,pv234,dense);
 if(result234<0)fail143("material pair234 fatal",profile,0);
 if(!result234)for(unsigned lane=0;lane<2;lane++)
  if(!nf_hw_begin(&pair234[lane])||!nf_hw_draw_material221(pv234[lane],dense)||!nf_hw_sync())fail143("material pair234 fallback",profile,lane);
 times227[4]=driving_clock227();
 driving_join227(mapped[0],owned,owned+LANE221,640*480);
 driving_join227(mapped[1],owned+2*LANE221,owned+3*LANE221,640*480);
 times227[5]=driving_clock227();driving_timing227(times227);
#ifdef DRIVING_PRODUCER235
 if(profile==7&&count==512)driving_producer235_request();
#endif
 {static unsigned completed221;unsigned n=++completed221;const char *dir=getenv("DRIVING_CAPTURE_DIR");
  if(dir&&(n==1||n==16||n==64||n==256))for(unsigned lane=0;lane<2;lane++){
   char path[2048];int length=snprintf(path,sizeof path,"%s/gpu221-draw%u-color-lane%u.bin",dir,n,lane);
   if(length>0&&(size_t)length<sizeof path){FILE *f=fopen(path,"wb");int ok=0;
    if(f){ok=fwrite(owned+lane*LANE221,1,LANE221,f)==LANE221;if(fclose(f))ok=0;}
    fprintf(stderr,"[GPU221] completed-surface draw=%u profile=%u lane=%u capture=%d not-presented-frame=1\n",n,profile,lane,ok);
   }
  }
 }
 accepted=1;
done221:
 driving_storage_release228(&input_storage,input);driving_storage_release228(&vertex_storage,vertices);
 driving_storage_release228(&target_storage,owned);driving_storage_release228(&shader_storage234,memo234);return accepted;
}

