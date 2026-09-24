/* Synchronous, opt-in movie bridge. Only captured contracts replace baseline
 * draws. Every GPU draw completes to guest RAM before another command executes. */
#include "gpu144/nightfire_hardware.h"
#include "driving_plan201.h"
#include "driving_scene240.h"
static DrivingImmediate195 live201;
static unsigned held201,words201,buffer201[96][2],draws201[2],declined201;
static int enabled201=-1;
extern void driving_scanout201_frame(uint32_t,const uint8_t *);
static void replay201(void){
 residency_flush236("immediate-replay");
 for(unsigned i=0;i<words201;i++)nv2a_pb_exec_method(0,buffer201[i][0],buffer201[i][1]);
 words201=held201=0;
}
/* Called before rebinding or processing another object's commands, so buffered
 * baseline work cannot accidentally move past a clear/blit/semaphore boundary. */
static void barrier201(void){if(held201)replay201();}
#ifndef NF_LIVE201_TEST
static void *mapped201(uint32_t va,size_t bytes,int write){
 if(!write)residency_read236(va,bytes);
 if(va<0x80000000u||!bytes||(uint64_t)(va-0x80000000u)+bytes>xbox_ContiguousAllocatedBytes())return NULL;
 uintptr_t begin=(uintptr_t)xbox_GetMemoryOffset()+va,at=begin,end=begin+bytes;if(end<begin)return NULL;
 while(at<end){MEMORY_BASIC_INFORMATION m;
  if(!VirtualQuery((void*)at,&m,sizeof m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))||
     (write?!(m.Protect&(PAGE_READWRITE|PAGE_EXECUTE_READWRITE)):!(m.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE))))return NULL;
  uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=at)return NULL;at=next;
 }return(void*)begin;
}
static int submit201(const DrivingDraw195 *d,unsigned family){
 residency_flush236("movie-resolve");
 const uint32_t *r=d->state;uint32_t ramht;DrivingSpan183 color,depth,texture;
 unsigned cp=r[0x20c/4]&0xffff,dp=r[0x20c/4]>>16,tp=r[0x1b10/4]>>16;
 unsigned tw=r[0x1b1c/4]>>16,th=r[0x1b1c/4]&0xffff,allocated=xbox_ContiguousAllocatedBytes();
 if(!read143(NULL,0xfd002210,&ramht)||!driving_span183(NULL,read143,ramht,r,d->known,DRIVING_COLOR183,0,cp*480,allocated,&color)||
    !driving_span183(NULL,read143,ramht,r,d->known,DRIVING_TEXTURE183,0,tp*th,allocated,&texture))return 0;
 NFHardwareState s={0};s.width=family?640:1280;s.height=s.bottom=480;s.right=s.width;s.pitch=cp;
 s.color=mapped201(color.address,cp*480,1);s.texture=mapped201(texture.address,tp*th,0);s.texture_available=tp*th;
 if(!s.color||!s.texture)return 0;
 if(!family){
  if(!driving_span183(NULL,read143,ramht,r,d->known,DRIVING_DEPTH183,0,dp*480,allocated,&depth))return 0;
  s.depth=mapped201(depth.address,dp*480,1);s.depth_pitch=dp;if(!s.depth)return 0;
 }
 s.depth_enable=r[0x30c/4];s.depth_write=r[0x35c/4];s.depth_func=r[0x354/4];
 s.alpha_enable=r[0x300/4];s.alpha_func=r[0x33c/4];s.alpha_ref=r[0x340/4];
 s.blend_enable=r[0x304/4];s.blend_src=r[0x344/4];s.blend_dst=r[0x348/4];
 s.cull_enable=r[0x308/4];s.cull_face=r[0x39c/4];s.front_face=r[0x3a0/4];
 s.texture_width=tw;s.texture_height=th;s.texture_pitch=tp;s.texture_format=r[0x1b04/4];s.texture_filter=r[0x1b14/4];s.texture_address=r[0x1b08/4];
 uintptr_t ca=(uintptr_t)s.color,ta=(uintptr_t)s.texture,za=(uintptr_t)s.depth;
 if((ca<ta+(size_t)tp*th&&ta<ca+(size_t)cp*480)||(!family&&((ca<za+(size_t)dp*480&&za<ca+(size_t)cp*480)||(ta<za+(size_t)dp*480&&za<ta+(size_t)tp*th))))return 0;
 s.filtered=1;s.scale=1;s.combiner=family?0:1;s.color_only=family;s.selected_coordinates182=1;s.cpu_vertex182=!family;s.program=&d->program;
 if(family)s.experimental_resolve193=enabled201;else s.experimental_movie200=enabled201;
 NFHardwareInputVertex v[6];unsigned n=family?3:6;
 static const unsigned strip[6]={0,1,2,2,1,3};
 for(unsigned i=0;i<n;i++)memcpy(&v[i],&d->vertices[family?i:strip[i]],sizeof v[i]);
 if(!nf_hw_begin(&s))return 0;
 /* Backend failures after begin are explicit stops, never partial GPU writes
  * followed by a second baseline rendering attempt. */
 if(!nf_hw_draw_input(v,n)||!nf_hw_sync())fail143("movie GPU draw/sync201",family,draws201[family]);
 unsigned count=++draws201[family];if(count<=3||!(count%120))fprintf(stderr,"[GPU201] %s=%u target=%08X source=%08X completed=1 profile=%d\n",family?"resolve":"movie",count,color.address,texture.address,enabled201);
 if(scene_resolve240(family,draws201[0],r[0x1b14/4]))driving_scanout201_frame(color.address-0x80000000u,s.color);
 return 1;
}
#else
static int submit201(const DrivingDraw195 *,unsigned);
#endif
#ifndef NF_LIVE201_TEST
#include "driving_resolve_capture222.h"
#else
static void capture_resolve222(const DrivingDraw195 *d){(void)d;}
#endif
static int intercept201(unsigned method,uint32_t value){
 if(enabled201<0){const char *v=getenv("DRIVING_MOVIE_GPU201");enabled201=v&&(!strcmp(v,"1")||!strcmp(v,"2"))?atoi(v):0;}
 if(!enabled201)return 0;
 int result=driving_immediate195(&live201,method,value);unsigned family;
 if(result==1)capture_resolve222(&live201.draw);
 if(!held201){
  if(method==0x17fc&&value&&driving_plan201(&live201.draw,&family)&&live201.selected){held201=1;words201=0;}
  else return 0;
 }else if(method==0x17fc&&!value){
  if(result==1&&driving_vertices201(&live201.draw)&&driving_plan201(&live201.draw,&family)&&submit201(&live201.draw,family)){held201=words201=0;return 1;}
  if(++declined201<=5)fprintf(stderr,"[GPU201] candidate declined at END; exact baseline replay\n");
  replay201();return 0;
 }else if(!((method>=0x1880&&method<0x1900)||(method>=0x1940&&method<0x1980)||(method>=0x1518&&method<=0x1524))){replay201();return 0;}
 if(words201==96){replay201();return 0;}
 buffer201[words201][0]=method;buffer201[words201++][1]=value;return 1;
}
