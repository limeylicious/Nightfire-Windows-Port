#include "driving_text510_material.h"
/*352 UNTESTED common input/material submission, derived from350/221.
 * No resident batching, handoff extension or altered target backing.
 * Kept separate so default-OFF retains the original submit221 function. */
#include "driving_geometry350.h"
#include "driving_input352.h"
#include "driving_plan352.h"
#include "driving_transform352.h"
#ifndef GEOMETRY352_INJECT
#define GEOMETRY352_INJECT(site) 0
#endif
static int geometry_submit352_impl(const uint32_t *state,const unsigned char *known,NFVertexProgram *program,
                     const InputPacket352 *packet)
{
 if(!packet)return 0;
 unsigned primitive=packet->primitive,count=packet->count;
 const uint32_t *indices=packet->indices;
 enum {LANE221=640*480*4,PAIR221=2*LANE221};unsigned profile;
 DrivingTopology350 topology;
 if(!input_result352(packet,&topology)||
    !renderer_plan352(state,known,program,primitive,&profile))return 0;
 unsigned dense=topology.dense;
 uint64_t times227[6];times227[0]=driving_clock227();
 NFHardwareMaterial221 material={0};pixel_state221(&material.pixel,state);material.integer_depth291=driving_depth291(state,known);material.integer_depth302=driving_depth302(state,known);
 if(!nf_pixel221_valid(&material.pixel))return 0;
 uint32_t ramht,allocated=xbox_ContiguousAllocatedBytes();DrivingSpan183 spans[6],stream;
 uint8_t *mapped[6]={0};unsigned lengths[6]={PAIR221,PAIR221,0,0,0,0};size_t total=4*LANE221;
 DrivingSpan183 palette_span287;uint8_t *palette_mapped287=NULL;int indexed287=0;
 if(!read143(NULL,0xfd002210,&ramht))return 0;
 if(GEOMETRY352_INJECT(1)||!nf_hw_sync())fail143("material prior completion221",primitive,count);
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
   }else lengths[i]=texture_bytes221(t->format);if(!lengths[i]){driving_map230_end(&mapping230);return 0;}total+=lengths[i];
  }
  if(!driving_span183(NULL,read143,ramht,state,known,i<2?(i?DRIVING_DEPTH183:DRIVING_COLOR183):DRIVING_TEXTURE183,
       i<2?0:i-2,lengths[i],allocated,&spans[i])){driving_map230_end(&mapping230);return 0;}
  /* Read-only texture aliases are legal, but cannot alias either writable target. */
  for(unsigned j=0;j<i&&j<2;j++)if(overlap221(spans[i].address,lengths[i],spans[j].address,lengths[j])){driving_map230_end(&mapping230);return 0;}
  mapped[i]=mapped_submit230(&mapping230,spans[i].address,lengths[i],i<2);if(!mapped[i]){driving_map230_end(&mapping230);return 0;}
 }
 if(indexed287){
  /* Both original sources receive DMA, permission and writable-target alias
   * validation before either is copied. Never read a palette at GPU replay. */
  
  if(!driving_span183(NULL,read143,ramht,state,known,DRIVING_PALETTE183,3,DP287_PALETTE_BYTES,allocated,&palette_span287)){driving_map230_end(&mapping230);return 0;}
  for(unsigned j=0;j<2;j++)if(overlap221(palette_span287.address,DP287_PALETTE_BYTES,spans[j].address,lengths[j])){driving_map230_end(&mapping230);return 0;}
  
  palette_mapped287=mapped_submit230(&mapping230,palette_span287.address,DP287_PALETTE_BYTES,0);if(!palette_mapped287){driving_map230_end(&mapping230);return 0;}
 }
 unsigned maximum=0;for(unsigned i=0;i<count;i++)if(indices[i]>maximum)maximum=indices[i];
 const uint8_t *source[16]={0};uint32_t lengths_v[16]={0};
 if(packet->kind!=INPUT352_INLINE)for(unsigned k=0;k<16;k++){
  uint32_t fmt=state[(0x1760+4*k)/4],type=fmt&15,n=fmt>>4&15;if(!n)continue;
  unsigned bytes=type==6&&n==1?4:type==0&&n==4?4:type==2&&n<=4?n*4:type==5&&n<=4?n*2:0;
  uint64_t need=(uint64_t)maximum*(fmt>>8)+bytes;
  if(!bytes||!need||need>0x08000000||!driving_span183(NULL,read143,ramht,state,known,DRIVING_VERTEX183,k,(uint32_t)need,allocated,&stream)){driving_map230_end(&mapping230);return 0;}
  source[k]=mapped_submit230(&mapping230,stream.address,(size_t)need,0);if(!source[k]){driving_map230_end(&mapping230);return 0;}lengths_v[k]=(uint32_t)need;
 }
 driving_map230_end(&mapping230);
 static DrivingStorage228 input_storage,vertex_storage,target_storage,shader_storage234;
 static DrivingStorage228 raw_storage352,reference_storage352;
 const int compare352=renderer_compute352();
 NFHardwareInputVertex *raw352=compare352?driving_storage228(&raw_storage352,(size_t)count*sizeof *raw352):NULL;
 NFHardwareVertexResult352 *reference352=compare352?driving_storage228(&reference_storage352,(size_t)count*sizeof *reference352):NULL;
 NFHardwareMaterialVertex221 *input=GEOMETRY352_INJECT(2)?NULL:driving_storage228(&input_storage,(size_t)count*sizeof *input);
 NFHardwareMaterialVertex221 *vertices=GEOMETRY352_INJECT(3)?NULL:driving_storage228(&vertex_storage,(size_t)dense*2*sizeof *vertices);
 uint8_t *owned=GEOMETRY352_INJECT(4)?NULL:driving_storage228(&target_storage,total);int accepted=0;
 /* Local to this original draw; compare every freshly decoded attribute before
  * reusing a successful transform. Never reuse solely by guest address/index. */
 typedef struct {float attributes[16][4];uint32_t index,previous;} ShaderMemo234;
 ShaderMemo234 *memo234=driving_storage228(&shader_storage234,256*sizeof *memo234);
 if(memo234)for(unsigned i=0;i<256;i++)memo234[i].previous=0;
 if(!input||!vertices||!owned||(compare352&&(!raw352||!reference352)))goto done352;
 /* Own the current complete texture spans before any compatibility decision. */
 size_t at=4*LANE221;
 for(unsigned k=0;k<4;k++)if(lengths[k+2]){memcpy(owned+at,mapped[k+2],lengths[k+2]);material.textures[k].data=owned+at;material.textures[k].identity228=mapped[k+2];material.textures[k].available=lengths[k+2];at+=lengths[k+2];}
 if(indexed287){
  /* Retain raw4096 index +1024 palette and derived16384 bytes in this same
   * owned allocation; byte accounting above includes all three. */
  memcpy(owned+at,palette_mapped287,DP287_PALETTE_BYTES);
  NFHardwareMaterialTexture221 *t=&material.textures[3];
  if(!driving_palette287_expand(owned+at+DP287_PALETTE_BYTES,DP287_BGRA_BYTES,
      t->data,t->available,owned+at,DP287_PALETTE_BYTES))goto done352;
  t->data=owned+at+DP287_PALETTE_BYTES;t->available=DP287_BGRA_BYTES;
  t->format=0x06610629u;at+=DP287_EXTRA_BYTES;
 }
 driving_road_specialize242(profile==GEOMETRY_GLINT350?12:profile,program,&material);
 DrivingVertex247 vertex247=driving_bind247(program,profile==GEOMETRY_GLINT350?12:profile);
 times227[1]=driving_clock227();
 float bias,slope;memcpy(&bias,&state[0x9c0/4],4);memcpy(&slope,&state[0x9c4/4],4);
 for(unsigned visit352=0;visit352<count;visit352++){
  unsigned i=driving_topology_first350(&topology,visit352);
  float in[16][4]={{0}},out[16][4],fog=1;
  if(packet->kind==INPUT352_INLINE){if(!input_inline352(packet,i,in))goto done352;}
  else for(unsigned k=0;k<16;k++)if(source[k]&&!driving_vertex218(source[k],lengths_v[k],state[(0x1760+4*k)/4],0,indices[i],in[k]))goto done352;
  if(compare352)memcpy(raw352[i].attributes,in,sizeof in);
  ShaderMemo234 *slot234=memo234?&memo234[indices[i]&255]:NULL;
  if(slot234 && slot234->previous && slot234->index==indices[i] &&
     !memcmp(slot234->attributes,in,sizeof in)){
   input[i]=input[slot234->previous-1];if(compare352)reference352[i]=reference352[slot234->previous-1];continue;
  }
  if(!vertex247(program,in,out))goto done352;
  if(compare352){memcpy(reference352[i].output,out,sizeof out);reference352[i].valid=1;
   reference352[i].pc=program->error_pc;reference352[i].constant=program->error_constant;reference352[i].reserved=0;}
  for(unsigned k=0;k<64;k++)if(!isfinite(((float*)out)[k]))goto done352;
  if(!out[0][3])goto done352;
  if(state[0x2a4/4]&&!driving_fog217(state[0x29c/4],bias,slope,out[5][0],&fog))goto done352;
  NFHardwareMaterialVertex221 *v=&input[i];memcpy(v->position,out[0],16);memcpy(v->uv,out[9],64);v->fog=fog;
  for(unsigned k=0;k<4;k++){v->color[k]=fminf(1,fmaxf(0,out[3][k]));v->specular[k]=fminf(1,fmaxf(0,out[4][k]));}
  for(unsigned stage=0;stage<4;stage++){
   unsigned mode=material.pixel.program>>(stage*5)&31;float *uv=v->uv[stage];
   if(material.pixel.white_stage2_242&&stage==2){if(!nf_pixel_zero_uv242(&material.pixel,stage,uv))goto done352;}
   else if(mode==1&&(!uv[3]||!isfinite(uv[0]/uv[3])||!isfinite(uv[1]/uv[3])))goto done352;
   if(mode==4)for(unsigned k=0;k<4;k++)if(uv[k]<0||uv[k]>1)goto done352;
  }
  if(slot234){memcpy(slot234->attributes,in,sizeof in);slot234->index=indices[i];slot234->previous=i+1;}
 }
 for(unsigned i=0;i<dense;i++){
  unsigned k=i%3,src=input_corner352(&topology,i);
  for(unsigned lane=0;lane<2;lane++){
   NFHardwareMaterialVertex221 *v=&vertices[lane*dense+i];*v=input[src];v->position[0]+=lane*.5f;v->position[1]+=lane*.5f;
   float w=v->position[3];if(!isfinite((v->position[0]*2/640-1)*w)||!isfinite((1-v->position[1]*2/480)*w)||!isfinite(v->position[2]/16777215*w))goto done352;
   for(unsigned stage=0;stage<4;stage++)if((material.pixel.program>>(stage*5)&31)==1&&
      (v->uv[stage][3]<0)!=(vertices[lane*dense+i-k].uv[stage][3]<0))goto done352;
  }
 }
 if(compare352&&!renderer_compare352(program,raw352,reference352,count))goto done352;
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
 for(unsigned lane510=0;lane510<2;lane510++)text_material510(profile>=25?P510_SPRITE:P510_GEOMETRY,lane510,spans[0].address,profile,profile==25&&lengths[5]?state[0x1bc4/4]:0,profile==25&&lengths[5]?spans[5].address:0,&pair234[lane510],&material,pv234[lane510],dense,0,profile>=25);
 int result234=GEOMETRY352_INJECT(5)?-1:nf_hw_material_pair234(pair234,pv234,dense);
 if(result234<0)fail143("material pair234 fatal",profile,0);
 if(!result234)for(unsigned lane=0;lane<2;lane++)
  if(GEOMETRY352_INJECT(10+lane*3)||!nf_hw_begin(&pair234[lane])||
     GEOMETRY352_INJECT(11+lane*3)||!nf_hw_draw_material221(pv234[lane],dense)||
     GEOMETRY352_INJECT(12+lane*3)||!nf_hw_sync())fail143("material pair234 fallback",profile,lane);
 times227[4]=driving_clock227();
 driving_join227(mapped[0],owned,owned+LANE221,640*480);
 /* Existing5/6 admission retains221's unconditional depth roundtrip. New
  *7/8/9 draws keep350's read-only-depth rule; they never had a221 route. */
 if(primitive<=6||(state[0x30c/4]&&state[0x35c/4]))
  driving_join227(mapped[1],owned+2*LANE221,owned+3*LANE221,640*480);
 times227[5]=driving_clock227();driving_timing227(times227);
#ifdef DRIVING_PRODUCER235
 /* Same original indexed/array profile7 count512 producer request as221.
  * Inline was never proved eligible for that producer-side mechanism. */
 if(profile==7&&count==512&&packet->kind!=INPUT352_INLINE)driving_producer235_request();
#endif
 {static unsigned completed221;unsigned n=++completed221;const char *dir=getenv("DRIVING_CAPTURE_DIR");
  if(dir&&(n==1||n==16||n==64||n==256))for(unsigned lane=0;lane<2;lane++){
   char path[2048];int length=snprintf(path,sizeof path,"%s/geometry352-draw%u-color-lane%u.bin",dir,n,lane);
   if(length>0&&(size_t)length<sizeof path){FILE *f=fopen(path,"wb");int ok=0;
    if(f){ok=fwrite(owned+lane*LANE221,1,LANE221,f)==LANE221;if(fclose(f))ok=0;}
    fprintf(stderr,"[GEOMETRY352] completed-surface draw=%u profile=%u lane=%u capture=%d not-presented-frame=1\n",n,profile,lane,ok);
   }
  }
 }
 accepted=1;
done352:
 driving_storage_release228(&input_storage,input);driving_storage_release228(&vertex_storage,vertices);
 driving_storage_release228(&target_storage,owned);driving_storage_release228(&shader_storage234,memo234);
 driving_storage_release228(&raw_storage352,raw352);driving_storage_release228(&reference_storage352,reference352);return accepted;
}

