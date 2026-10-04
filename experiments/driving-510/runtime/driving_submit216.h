#include "driving_text510_material.h"
/* Opt-in original main indexed draw, experimental center/corner lanes.
 * Owned-resource replay and command-order checks are in analysis/main216. */
#include "driving_plan216.h"
#include "driving_vertex212.h"
static int overlap216(uint32_t a,size_t an,uint32_t b,size_t bn)
{return (uint64_t)a+an>b && (uint64_t)b+bn>a;}
static int submit216(const uint32_t *state,const unsigned char *known,NFVertexProgram *program,
                     unsigned primitive,const uint32_t *indices,unsigned count)
{
 enum {LANE216=640*480*4,PAIRED216=LANE216*2,TEXTURE216=256*256*4};
 if(!indices||count<3||count>8192||!driving_plan216(state,known,program,primitive))return 0;
 uint32_t ramht,allocated=xbox_ContiguousAllocatedBytes();DrivingSpan183 c,z,t,stream;
 if(!read143(NULL,0xfd002210,&ramht))return 0;
 if(!nf_hw_sync())fail143("main prior GPU completion216",primitive,count);
 if(!driving_span183(NULL,read143,ramht,state,known,DRIVING_COLOR183,0,PAIRED216,allocated,&c)||
    !driving_span183(NULL,read143,ramht,state,known,DRIVING_DEPTH183,0,PAIRED216,allocated,&z)||
    !driving_span183(NULL,read143,ramht,state,known,DRIVING_TEXTURE183,0,TEXTURE216,allocated,&t))return 0;
 if(overlap216(c.address,PAIRED216,z.address,PAIRED216)||overlap216(c.address,PAIRED216,t.address,TEXTURE216)||overlap216(z.address,PAIRED216,t.address,TEXTURE216))return 0;
 uint8_t *color=mapped201(c.address,PAIRED216,1),*depth=mapped201(z.address,PAIRED216,0),*texture=mapped201(t.address,TEXTURE216,0);
 if(!color||!depth||!texture)return 0;
 unsigned maximum=0;for(unsigned i=0;i<count;i++)if(indices[i]>maximum)maximum=indices[i];
 const uint8_t *source[3];uint32_t length[3];
 for(unsigned slot=0;slot<3;slot++){
  uint32_t fmt=state[(0x1760+4*slot)/4],kind=fmt&15,n=(fmt>>4)&15;
  uint64_t need=(uint64_t)maximum*(fmt>>8)+n*(kind==2?4:kind==5?2:1);
  if(!need||need>0x08000000||!driving_span183(NULL,read143,ramht,state,known,DRIVING_VERTEX183,slot,(uint32_t)need,allocated,&stream))return 0;
  source[slot]=mapped201(stream.address,(size_t)need,0);if(!source[slot])return 0;length[slot]=(uint32_t)need;
 }
 NFHardwareVertex *vertices=malloc((size_t)count*2*sizeof *vertices);
 uint8_t *owned=malloc(LANE216*4+TEXTURE216);if(!vertices||!owned){free(vertices);free(owned);return 0;}
 int accepted=0;
 for(unsigned i=0;i<count;i++){
  float in[16][4]={{0}},out[16][4];
  for(unsigned slot=0;slot<3;slot++)if(!driving_vertex212(source[slot],length[slot],state[(0x1760+4*slot)/4],0,indices[i],in[slot]))goto done216;
  if(!nf_vp_run(program,in,out)||out[0][3]==0||out[9][3]!=1)goto done216;
  for(unsigned k=0;k<4;k++)if(!isfinite(out[0][k])||!isfinite(out[9][k])||!isfinite(out[3][k])||out[3][k]<0||out[3][k]>1)goto done216;
  unsigned rgba[4];for(unsigned k=0;k<4;k++){
   rgba[k]=(unsigned)floorf(out[3][k]*255+.5f);if((float)rgba[k]/255!=out[3][k])goto done216;
  }
  for(unsigned lane=0;lane<2;lane++){
   NFHardwareVertex *v=&vertices[lane*count+i];memcpy(v->position,out[0],16);
   v->position[0]+=lane*.5f;v->position[1]+=lane*.5f;
   float w=v->position[3];
   if(!isfinite((v->position[0]*2/640-1)*w)||!isfinite((1-v->position[1]*2/480)*w)||!isfinite(v->position[2]/16777215*w))goto done216;
   memcpy(v->uv,out[9],8);v->color=(rgba[3]<<24)|(rgba[0]<<16)|(rgba[1]<<8)|rgba[2];v->fog=1;
  }
 }
 /* Own both sample lanes and the texture before either GPU submission. */
 for(unsigned lane=0;lane<2;lane++)for(unsigned i=0;i<640*480;i++){
  memcpy(owned+lane*LANE216+i*4,color+i*8+lane*4,4);
  memcpy(owned+(2+lane)*LANE216+i*4,depth+i*8+lane*4,4);
 }
 memcpy(owned+4*LANE216,texture,TEXTURE216);
 for(unsigned lane=0;lane<2;lane++){
  NFHardwareState s={0};s.color=owned+lane*LANE216;s.depth=owned+(2+lane)*LANE216;
  s.width=s.right=640;s.height=s.bottom=480;s.pitch=s.depth_pitch=2560;
  s.depth_enable=1;s.depth_func=0x203;s.alpha_func=0x204;s.combiner=1;s.scale=2;s.color_write_mask212=0x1f;
  s.texture=owned+4*LANE216;s.texture_available=TEXTURE216;s.texture_format=0x08810629;s.texture_filter=0x02063f01;s.texture_address=0x10101;
  s.filtered=1;s.sampler_anisotropy213=8;s.sampler_bias215=1;
  text_basic510(lane,c.address,216,&s,vertices+lane*count,count,sizeof *vertices,offsetof(NFHardwareVertex,color),1);
  if(!nf_hw_begin(&s)||!nf_hw_draw(vertices+lane*count,count)||!nf_hw_sync())fail143("main GPU begin/draw/sync216",lane,count);
 }
 /* Publish only after both lanes complete. Original depth/stencil unchanged. */
 for(unsigned lane=0;lane<2;lane++)for(unsigned i=0;i<640*480;i++)memcpy(color+i*8+lane*4,owned+lane*LANE216+i*4,4);
 /* First actual main draw only, before later commands overwrite its target.
  * Captured draw result is not a presented/completed scene. */
 {static int saved216;const char *dir=getenv("DRIVING_CAPTURE_DIR");
  if(!saved216&&dir){saved216=1;
   for(unsigned lane=0;lane<2;lane++){
    char path[2048];int n=snprintf(path,sizeof path,"%s/gpu216-main-lane%u.bin",dir,lane);
    if(n>0&&(size_t)n<sizeof path){FILE *f=fopen(path,"wb");int ok=0;
     if(f){ok=fwrite(owned+lane*LANE216,1,LANE216,f)==LANE216;if(fclose(f))ok=0;}
     fprintf(stderr,"[GPU216] actual-draw lane=%u bytes=%u capture=%d not-presented-scene=1\n",lane,LANE216,ok);
    }
   }
  }
 }
 accepted=1;
done216:
 free(vertices);free(owned);return accepted;
}
