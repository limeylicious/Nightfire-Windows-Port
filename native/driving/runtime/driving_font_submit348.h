#include "driving_extramap419.h"
#include "driving_cpu448.h"
/*348 Default-OFF original inline A8 glyph draw, owned two-lane submission.
 * Host RGBA atlas stores RGB=1,A=original, padded by repeating row107 to127.
 * At normalized coordinate(u/256,v/128), bilinear texel position is(u-.5,v-.5),
 * identical to the original rectangle. Duplicated edge rows preserve clamp for
 * every finite coordinate, including beyond108. No extra mip levels exist. */
#include "driving_font_collect348.h"
#include "driving_font_plan348.h"
static unsigned swizzle_inline223(unsigned x,unsigned y){unsigned n=0;for(unsigned b=0;b<7;b++){n|=((x>>b)&1)<<(2*b);n|=((y>>b)&1)<<(2*b+1);}return n|((x>>7)<<14);}
static int overlaps_inline223(uint32_t a,unsigned an,uint32_t b,unsigned bn){return (uint64_t)a+an>b&&(uint64_t)b+bn>a;}
#ifndef FONT348_INJECT
#define FONT348_INJECT(site) 0
#endif
static int font_submit348_impl(const uint32_t *state,const unsigned char *known,const NFVertexProgram *program,const Inline223 *draw)
{
 enum{LANE=640*480*4,PAIR=LANE*2,ATLAS=256*108};
 if(!font_plan348(state,known,program)||!inline223_rectangles(draw)||draw->count>1024)return 0;
 for(unsigned i=0;i<draw->count;i++)for(unsigned a=0;a<16;a++)if(a==0||a==3||a==9)for(unsigned c=0;c<4;c++)if(!isfinite(draw->vertices[i][a][c]))return 0;
 for(unsigned i=0;i<draw->count;i++)for(unsigned c=0;c<4;c++)if(draw->vertices[i][3][c]<0||draw->vertices[i][3][c]>1)return 0;
 uint32_t ramht,allocated=xbox_ContiguousAllocatedBytes();DrivingSpan183 spans[3];uint8_t *map[3];unsigned sizes[3]={PAIR,PAIR,ATLAS};
 if(!read143(NULL,0xfd002210,&ramht))return 0;
 if(FONT348_INJECT(1)||!nf_hw_sync())fail143("inline prior completion223",0,0);
 DrivingExtraMap419 mapping419;driving_fontmap419_begin(&mapping419);
 for(unsigned i=0;i<3;i++){
  if(!driving_span183(NULL,read143,ramht,state,known,i==0?DRIVING_COLOR183:i==1?DRIVING_DEPTH183:DRIVING_TEXTURE183,0,sizes[i],allocated,&spans[i])){driving_extramap419_end(&mapping419);return 0;}
  for(unsigned j=0;j<i;j++)if(overlaps_inline223(spans[i].address,sizes[i],spans[j].address,sizes[j])){driving_extramap419_end(&mapping419);return 0;}
  map[i]=mapped_font419(&mapping419,spans[i].address,sizes[i],i<2);if(!map[i]){driving_extramap419_end(&mapping419);return 0;}
 }
 /*395 Complete bounded read-only resource checks for the captured, unselected
  * palette texture. No new target backing, ownership or deferred publication.
  * A refusal leaves the original commands available for exact replay. */
 if(state[0x1bcc/4]){
  DrivingSpan183 extra[2];const unsigned bytes[2]={65536,1024};
  for(unsigned i=0;i<2;i++){
   if(!driving_span183(NULL,read143,ramht,state,known,i?DRIVING_PALETTE183:DRIVING_TEXTURE183,3,bytes[i],allocated,&extra[i])){driving_extramap419_end(&mapping419);return 0;}
   for(unsigned j=0;j<3;j++)if(overlaps_inline223(extra[i].address,bytes[i],spans[j].address,sizes[j])){driving_extramap419_end(&mapping419);return 0;}
   if(i&&overlaps_inline223(extra[0].address,bytes[0],extra[1].address,bytes[1])){driving_extramap419_end(&mapping419);return 0;}
   if(!mapped_font419(&mapping419,extra[i].address,bytes[i],0)){driving_extramap419_end(&mapping419);return 0;}
  }
 }
 driving_extramap419_end(&mapping419);
 /*448: with DRIVING_CPU448=1/2 the 4.8 MiB scratch is allocated once and reused;
  * every byte of it is rewritten below before use (lanes by the split, atlas by the
  * bijective swizzle loop), so reuse cannot leak a previous call's content. */
 static uint8_t *scratch448;const int mode448=cpu448_mode();
 if(mode448&&!scratch448&&!FONT348_INJECT(2))scratch448=malloc(4*LANE+256*128*4);
 uint8_t *owned=FONT348_INJECT(2)?NULL:mode448?scratch448:malloc(4*LANE+256*128*4);unsigned dense=draw->count/4*6;NFHardwareMaterialVertex221 *v=FONT348_INJECT(3)?NULL:calloc((size_t)dense*2,sizeof *v);int okay=0;
 if(!owned||!v)goto done_inline223;
 if(mode448==1){cpu448_split(owned,owned+LANE,map[0],640*480);cpu448_split(owned+2*LANE,owned+3*LANE,map[1],640*480);}
 else for(unsigned lane=0;lane<2;lane++)for(unsigned i=0;i<640*480;i++){
  memcpy(owned+lane*LANE+i*4,map[0]+i*8+lane*4,4);memcpy(owned+(lane+2)*LANE+i*4,map[1]+i*8+lane*4,4);
 }
 if(mode448==2){uint8_t *check=malloc(4*LANE);if(check){cpu448_split(check,check+LANE,map[0],640*480);cpu448_split(check+2*LANE,check+3*LANE,map[1],640*480);
  cpu448_counts.font_calls++;if(memcmp(check,owned,4*LANE)){cpu448_counts.font_mismatch++;fprintf(stderr,"[CPU448] FONT SPLIT MISMATCH\n");}free(check);cpu448_report();}}
 uint8_t *atlas=owned+4*LANE;
 for(unsigned y=0;y<128;y++)for(unsigned x=0;x<256;x++){
  unsigned at=swizzle_inline223(x,y)*4;atlas[at]=atlas[at+1]=atlas[at+2]=255;atlas[at+3]=map[2][(y<108?y:107)*256+x];
 }
 NFHardwareMaterial221 material={0};NFPixel221 *p=&material.pixel;p->control=state[0x1e60/4];p->program=state[0x1e70/4];p->final0=state[0x288/4];p->final1=state[0x28c/4];p->final_constant0=state[0x1e20/4];p->final_constant1=state[0x1e24/4];
 for(unsigned i=0;i<8;i++){p->rgb[i]=state[0xac0/4+i];p->alpha[i]=state[0x260/4+i];p->rgb_out[i]=state[0x1e40/4+i];p->alpha_out[i]=state[0xaa0/4+i];p->constant0[i]=state[0xa60/4+i];p->constant1[i]=state[0xa80/4+i];}
 if(!nf_pixel221_valid(p))goto done_inline223;
 /* Current material ABI includes identity228; never use the stale223 positional
  * initializer. NULL identity requests ordinary content validation for scratch. */
 material.textures[0].data=atlas;material.textures[0].available=256*128*4;
 material.textures[0].identity228=NULL;material.textures[0].format=0x07810629;
 material.textures[0].filter=state[0x1b14/4];material.textures[0].address=0x00030303;
 material.textures[0].anisotropy=1;material.textures[0].bias215=state[0x1b14/4]==0x02063f01;
 static const unsigned order[6]={0,1,2,0,2,3};
 for(unsigned lane=0;lane<2;lane++)for(unsigned i=0;i<dense;i++){
  const float (*in)[4]=draw->vertices[i/6*4+order[i%6]];NFHardwareMaterialVertex221 *dst=&v[lane*dense+i];
  memcpy(dst->position,in[0],16);dst->position[0]+=lane*.5f;dst->position[1]+=lane*.5f;memcpy(dst->color,in[3],16);dst->fog=1;
  dst->uv[0][0]=in[9][0]/256;dst->uv[0][1]=in[9][1]/128;dst->uv[0][3]=1;
  for(unsigned c=0;c<4;c++)if(!isfinite(dst->position[c]))goto done_inline223;
  if(!isfinite((dst->position[0]*2/640-1))||!isfinite(1-dst->position[1]*2/480)||!isfinite(dst->position[2]/16777215))goto done_inline223;
 }
 for(unsigned lane=0;lane<2;lane++){
  NFHardwareState s={0};s.material221=&material;s.color=owned+lane*LANE;s.depth=owned+(lane+2)*LANE;s.width=s.right=640;s.height=s.bottom=480;s.pitch=s.depth_pitch=2560;
  s.depth_enable=state[0x30c/4];s.depth_write=state[0x35c/4];s.depth_func=state[0x354/4];s.alpha_enable=state[0x300/4];s.alpha_func=state[0x33c/4];s.alpha_ref=state[0x340/4];
  s.blend_enable=state[0x304/4];s.blend_src=state[0x344/4];s.blend_dst=state[0x348/4];s.cull_enable=state[0x308/4];s.cull_face=state[0x39c/4];s.front_face=state[0x3a0/4];s.fog_color=state[0x2a8/4];s.filtered=s.combiner=s.scale=1;s.color_write_mask212=0x17u|((state[0x358/4]>>21)&8u);
  if(FONT348_INJECT(10+lane*3)||!nf_hw_begin(&s)||FONT348_INJECT(11+lane*3)||!nf_hw_draw_material221(v+lane*dense,dense)||FONT348_INJECT(12+lane*3)||!nf_hw_font_sync469(&s))fail143("inline begin/draw/sync223",lane,dense);
 }
 if(mode448==2){uint8_t *check=malloc(2*LANE);if(check){cpu448_join(check,owned,owned+LANE,640*480);
  for(unsigned lane=0;lane<2;lane++)for(unsigned i=0;i<640*480;i++)if(memcmp(check+i*8+lane*4,owned+lane*LANE+i*4,4)){cpu448_counts.font_mismatch++;fprintf(stderr,"[CPU448] FONT JOIN MISMATCH\n");lane=2;break;}
  free(check);}}
 if(mode448==1)cpu448_join(map[0],owned,owned+LANE,640*480);
 else for(unsigned lane=0;lane<2;lane++)for(unsigned i=0;i<640*480;i++){
  memcpy(map[0]+i*8+lane*4,owned+lane*LANE+i*4,4);/*348 original depth/stencil RAM unchanged: both writes are pinned OFF. */
 }
 driving_extramap419_publish(&mapping419,0);
 okay=1;
done_inline223:if(!mode448)free(owned);free(v);return okay;
}

/* Preserve host FP/control and error state introduced by the adapter. Replay is
 * outside this wrapper and retains the original executor's effects. */
#include <fenv.h>
#include <errno.h>
#include <xmmintrin.h>
static int font_submit348(const uint32_t*s,const unsigned char*k,const NFVertexProgram*p,const Inline223*d){
 fenv_t env;fegetenv(&env);unsigned csr=_mm_getcsr();DWORD error=GetLastError();int crt=errno;
 int result=font_submit348_impl(s,k,p,d);
 fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(error);return result;
}

