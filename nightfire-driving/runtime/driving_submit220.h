/* Bounded owned two-texture48 submission. Original sample model remains experimental. */
#include "driving_plan220.h"
#include "driving_vertex212.h"
#include "driving_fog217.h"
static int overlap220(uint32_t a,size_t an,uint32_t b,size_t bn)
{return (uint64_t)a+an>b && (uint64_t)b+bn>a;}
static int submit220(const uint32_t *state,const unsigned char *known,NFVertexProgram *program,
                     unsigned primitive,const uint32_t *indices,unsigned count)
{
 enum {LANE220=640*480*4,PAIR220=LANE220*2,TEX0220=87360,TEX1220=65536};
 if(!indices||count<3||count>8192||count%3||!driving_plan220(state,known,program,primitive))return 0;
 uint32_t ramht,allocated=xbox_ContiguousAllocatedBytes();DrivingSpan183 spans[4],stream;
 if(!read143(NULL,0xfd002210,&ramht))return 0;
 if(!nf_hw_sync())fail143("dual prior GPU completion220",primitive,count);
 const unsigned kind[4]={DRIVING_COLOR183,DRIVING_DEPTH183,DRIVING_TEXTURE183,DRIVING_TEXTURE183};
 const unsigned slot[4]={0,0,0,1},lengths[4]={PAIR220,PAIR220,TEX0220,TEX1220};
 uint8_t *mapped[4];
 for(unsigned i=0;i<4;i++){
  if(!driving_span183(NULL,read143,ramht,state,known,kind[i],slot[i],lengths[i],allocated,&spans[i]))return 0;
  for(unsigned j=0;j<i;j++)if(overlap220(spans[i].address,lengths[i],spans[j].address,lengths[j]))return 0;
  mapped[i]=mapped201(spans[i].address,lengths[i],i<2);if(!mapped[i])return 0;
 }
 unsigned maximum=0;for(unsigned i=0;i<count;i++)if(indices[i]>maximum)maximum=indices[i];
 const uint8_t *source[3];uint32_t lengths_v[3];
 for(unsigned k=0;k<3;k++){
  uint32_t fmt=state[(0x1760+4*k)/4],type=fmt&15,n=(fmt>>4)&15;
  uint64_t need=(uint64_t)maximum*(fmt>>8)+n*(type==2?4:type==5?2:1);
  if(!need||need>0x08000000||!driving_span183(NULL,read143,ramht,state,known,DRIVING_VERTEX183,k,(uint32_t)need,allocated,&stream))return 0;
  source[k]=mapped201(stream.address,(size_t)need,0);if(!source[k])return 0;lengths_v[k]=(uint32_t)need;
 }
 NFHardwareDual220 *vertices=malloc((size_t)count*2*sizeof *vertices);
 uint8_t *owned=malloc(LANE220*4+TEX0220+TEX1220);if(!vertices||!owned){free(vertices);free(owned);return 0;}
 int accepted=0;float bias,slope;memcpy(&bias,&state[0x9c0/4],4);memcpy(&slope,&state[0x9c4/4],4);
 for(unsigned i=0;i<count;i++){
  float in[16][4]={{0}},out[16][4],fog;
  for(unsigned k=0;k<3;k++)if(!driving_vertex212(source[k],lengths_v[k],state[(0x1760+4*k)/4],0,indices[i],in[k]))goto done220;
  if(!nf_vp_run(program,in,out)||out[0][3]==0||out[9][3]!=1||out[10][3]!=1||
     !driving_fog217(state[0x29c/4],bias,slope,out[5][0],&fog))goto done220;
  for(unsigned k=0;k<4;k++)if(!isfinite(out[0][k])||!isfinite(out[9][k])||!isfinite(out[10][k])||!isfinite(out[3][k])||out[3][k]<0||out[3][k]>1)goto done220;
  for(unsigned lane=0;lane<2;lane++){
   NFHardwareDual220 *v=&vertices[lane*count+i];memcpy(v->position,out[0],16);v->position[0]+=lane*.5f;v->position[1]+=lane*.5f;
   float w=v->position[3];if(!isfinite((v->position[0]*2/640-1)*w)||!isfinite((1-v->position[1]*2/480)*w)||!isfinite(v->position[2]/16777215*w))goto done220;
   memcpy(v->uv0,out[9],16);memcpy(v->uv1,out[10],16);memcpy(v->color,out[3],16);v->fog=fog;
  }
 }
 for(unsigned lane=0;lane<2;lane++)for(unsigned i=0;i<640*480;i++){
  memcpy(owned+lane*LANE220+i*4,mapped[0]+i*8+lane*4,4);
  memcpy(owned+(2+lane)*LANE220+i*4,mapped[1]+i*8+lane*4,4);
 }
 memcpy(owned+4*LANE220,mapped[2],TEX0220);memcpy(owned+4*LANE220+TEX0220,mapped[3],TEX1220);
 for(unsigned lane=0;lane<2;lane++){
  NFHardwareState s={0};s.color=owned+lane*LANE220;s.depth=owned+(2+lane)*LANE220;s.width=s.right=640;s.height=s.bottom=480;s.pitch=s.depth_pitch=2560;
  s.depth_enable=s.depth_write=1;s.depth_func=0x203;s.alpha_func=0x204;s.combiner=1;s.scale=2;s.fog_enable=1;s.fog_color=state[0x2a8/4];s.color_write_mask212=0x1f;s.dual220=1;
  s.texture=owned+4*LANE220;s.texture_available=TEX0220;s.texture_format=0x08860e29;s.texture_filter=0x02063f01;s.texture_address=0x10101;s.filtered=1;
  s.texture1_220=owned+4*LANE220+TEX0220;s.texture1_available220=TEX1220;s.texture1_format220=0x07710629;s.texture1_filter220=0x02063f01;s.texture1_address220=0x10101;
  if(!nf_hw_begin(&s)||!nf_hw_draw_dual220(vertices+lane*count,count)||!nf_hw_sync())fail143("dual GPU begin/draw/sync220",lane,count);
 }
 /* Publish colour AND depth only after both original sample lanes complete. */
 for(unsigned lane=0;lane<2;lane++)for(unsigned i=0;i<640*480;i++){
  memcpy(mapped[0]+i*8+lane*4,owned+lane*LANE220+i*4,4);
  memcpy(mapped[1]+i*8+lane*4,owned+(2+lane)*LANE220+i*4,4);
 }
 {static int saved220;const char *dir=getenv("DRIVING_CAPTURE_DIR");if(!saved220&&dir){saved220=1;
  for(unsigned part=0;part<4;part++){
   char path[2048];int n=snprintf(path,sizeof path,"%s/gpu220-%s-lane%u.bin",dir,part<2?"color":"depth",part%2);
   if(n>0&&(size_t)n<sizeof path){FILE*f=fopen(path,"wb");int ok=0;if(f){ok=fwrite(owned+part*LANE220,1,LANE220,f)==LANE220;if(fclose(f))ok=0;}
    fprintf(stderr,"[GPU220] actual-draw part=%u capture=%d not-presented-scene=1\n",part,ok);}
  }
 }}
 accepted=1;
done220:
 free(vertices);free(owned);return accepted;
}
