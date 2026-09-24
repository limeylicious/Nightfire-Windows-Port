/* Experimental synchronous GPU adapter. Semaphores acknowledge the software
 * executor's work, not complete NV2A fidelity. Unsupported rendering remains
 * counted by the pinned executor. No action-engine fence-value mirroring. */
#include <windows.h>
#define DRIVING_ASYNC295 1
#include "driving_async295.h"
#include "driving_writeguard270.h"
#include "driving_semaphore281.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef DRIVING_ACCESS321
#define DRIVING_ACCESS321_KIND DA321_RENDER_READ
#define DRIVING_ACCESS321_RENDER 1
#include "driving_access321_hooks.h"
#endif
#include "driving_timing250.h"
static void residency_flush236(const char *reason);
static void residency_read236(uint32_t va,size_t bytes);
#include "gpu144/nightfire_vertex_program.h"
static int submit_residency236(const uint32_t *,const unsigned char *,NFVertexProgram *,unsigned,const uint32_t *,unsigned);
#define DRIVING_OBJECT227_BEFORE_READ(va,bytes) residency_read236(va,bytes)
#include "driving_object227.h"
#ifdef DRIVING_DESCRIPTOR252
#define DRIVING_DESCRIPTOR252_BEFORE_READ(va,bytes) residency_read236(va,bytes)
#include "driving_descriptor252.h"
#define DRIVING_DMA183_DESCRIPTOR_READ(ctx,read,instance,result) descriptor_read252(ctx,read,instance,result)
#endif
#define DRIVING_OBJECT143_BULK_READ(ctx,va,dst,bytes) driving_object227_read(ctx,va,dst,bytes)
#define DRIVING_NATIVE_MAPPING230 1
#define DRIVING_PRODUCER235 1
extern void driving_producer235_request(void);
#if defined(DRIVING_COMMAND245) || defined(DRIVING_COMMAND262)
struct DrivingPB143;
static unsigned snapshot245(void *,const struct DrivingPB143 *,uint32_t,uint32_t *,unsigned);
static void command245_fetch(int);
#define DRIVING_COMMAND245_SNAPSHOT snapshot245
#define DRIVING_COMMAND245_FETCH command245_fetch
#endif
#include "driving_gpu143_core.h"
#include "driving_dma183.h"
#include "driving_blit143.h"
#include "driving_gpu_trace143.h"
#include "driving_surface145.h"
#include "driving_depth202.h"
#include "driving_scanout152.h"
#include "texture_select161.h"
extern ptrdiff_t xbox_GetMemoryOffset(void);
extern void nv2a_pb_exec_method(uint32_t,uint32_t,uint32_t);
#ifdef DRIVING_TIMING250
static __inline void driving_exec250(uint32_t channel,uint32_t method,uint32_t value){
 if(!dt250_enabled()){nv2a_pb_exec_method(channel,method,value);return;}
 unsigned kind=method==0x17fc&&!value?0:method==0x1d94?1:2;
 uint64_t started250=dt250_software_begin(kind);
 nv2a_pb_exec_method(channel,method,value);
 dt250_software_end(kind,started250);
}
/* Covers direct forwarding and both201/214 replay helpers in this unit. */
#define nv2a_pb_exec_method driving_exec250
#endif

extern void nv2a_pb_exec_report(void);
extern void driving_gpu_probe143(uint32_t);
extern uint32_t xbox_ContiguousAllocatedBytes(void);
extern int driving_flip204_method(unsigned,uint32_t);
static DrivingPB143 pb;
typedef struct GPUObject143 {
    uint32_t instance,handle,cls,semaphore_handle,semaphore_offset,state[0x2000/4];
    unsigned char written167[0x2000/4];
} GPUObject143;
static GPUObject143 objects[64];
static unsigned object_count,bound[8];
static unsigned releases,segments;
static volatile LONG movie_requested180,movie_marked180;
void driving_gpu_movie_phase180(void)
{
    const char *v=getenv("DRIVING_MOVIE_TRACE180");
    if(v&&v[0]=='1'&&!InterlockedCompareExchange(&movie_marked180,1,0))
        InterlockedExchange(&movie_requested180,1);
}
static void movie_phase180(void)
{
    if(!InterlockedCompareExchange(&movie_requested180,0,0)||pb.remaining||pb.return_address)return;
    for(unsigned i=0;i<object_count;i++)if(objects[i].cls==0x97&&objects[i].state[0x17fc/4])return;
    const char *dir=getenv("DRIVING_CAPTURE_DIR");if(!dir)return;
    InterlockedExchange(&movie_requested180,0);
    driving_trace143_start_movie180();
    fprintf(stderr,"[GPU180] movie phase starts at completed boundary cursor=%08X; raw method state is not a full VP/memory replay\n",pb.cursor);
    for(unsigned i=0;i<object_count;i++){
        char path[2048];int n=snprintf(path,sizeof path,"%s/gpu180-object-%02u-state.bin",dir,i);
        if(n<0||(size_t)n>=sizeof path)continue;
        FILE *f=fopen(path,"wb");if(!f)continue;
        int ok=fwrite(objects[i].state,1,sizeof(objects[i].state),f)==sizeof(objects[i].state);if(fclose(f))ok=0;
        fprintf(stderr,"[GPU180] object=%u class=%X handle=%08X instance=%08X state_saved=%d\n",i,objects[i].cls,objects[i].handle,objects[i].instance,ok);
    }
    for(unsigned i=0;i<8;i++)if(bound[i])fprintf(stderr,"[GPU180] binding sub=%u object=%u\n",i,bound[i]-1);
}

#ifdef DRIVING_COMMAND_READ265
#include "driving_command_read265.h"
#endif
static int enabled=-1;
#ifdef DRIVING_PUBLISH324
#include "driving_publication324.h"
#endif
static int read143_impl326(void *ctx,uint32_t va,uint32_t *value)
{
    SIZE_T got=0;(void)ctx;
    if(va<0x08000000u)va+=0x80000000u;
    residency_read236(va,4);
#ifdef DRIVING_COMMAND_READ265
    if(command_eligible265(va)&&command_read265(va,value,(uintptr_t)xbox_GetMemoryOffset(),xbox_ContiguousAllocatedBytes()))return 1;
#endif
#ifdef DRIVING_PUBLISH324
    return xbox_PublicationRead324(GetCurrentProcess(),(void *)((uintptr_t)xbox_GetMemoryOffset()+va),value,4,&got)&&got==4;
#else
    return ReadProcessMemory(GetCurrentProcess(),(void *)((uintptr_t)xbox_GetMemoryOffset()+va),value,4,&got)&&got==4;
#endif
}
/* Optional one-in-256 direct-command read sample. The normal build's installed
 * binary remains unchanged; this diagnostic never changes the guest result,
 * protection scope or original residency_read236 handoff. */
static uint64_t read_sample326_count,read_sample326_ticks,read_sample326_calls,read_sample326_probe_ticks;
static int read_sample326_enabled=-1;
static int read143(void *ctx,uint32_t va,uint32_t *value){
 if(read_sample326_enabled<0){DWORD error=GetLastError();int crt=errno;
  const char*v=getenv("DRIVING_READ_SAMPLE326");read_sample326_enabled=v&&!strcmp(v,"1");
  if(read_sample326_enabled)for(unsigned i=0;i<4096;i++){LARGE_INTEGER a,b;QueryPerformanceCounter(&a);QueryPerformanceCounter(&b);
   if(b.QuadPart>=a.QuadPart)read_sample326_probe_ticks+=(uint64_t)(b.QuadPart-a.QuadPart);}
  errno=crt;SetLastError(error);}
 if(!read_sample326_enabled)return read143_impl326(ctx,va,value);
 ++read_sample326_calls;
 if((read_sample326_calls&255)!=0)return read143_impl326(ctx,va,value);
 DWORD error=GetLastError();int crt=errno;LARGE_INTEGER start,end;
 QueryPerformanceCounter(&start);errno=crt;SetLastError(error);
 int result=read143_impl326(ctx,va,value);
 error=GetLastError();crt=errno;QueryPerformanceCounter(&end);
 if(end.QuadPart>=start.QuadPart){read_sample326_ticks+=(uint64_t)(end.QuadPart-start.QuadPart);read_sample326_count++;}
 errno=crt;SetLastError(error);return result;
}
static void fail143(const char *reason,uint32_t a,uint32_t b)
{
    driving_trace143(4,a,b,pb.cursor);
    fprintf(stderr,"[GPU143] STOP %s %08X %08X cursor=%08X\n",reason,a,b,pb.cursor);
    driving_gpu_probe143(0xf0000000u|b);
    fflush(stderr);abort();
}
static void *span143(uint32_t va,size_t size,int writable)
{
    residency_flush236("direct-span");
    uintptr_t address=(uintptr_t)xbox_GetMemoryOffset()+va,at=address;
    if(va<0x80000000u || !size || (uint64_t)(va-0x80000000u)+size>xbox_ContiguousAllocatedBytes())
        fail143("unallocated DMA span",va,(uint32_t)size);
    while(at<address+size){
        MEMORY_BASIC_INFORMATION m;
        if(!VirtualQuery((void *)at,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))
           ||(writable?!(m.Protect&(PAGE_READWRITE|PAGE_EXECUTE_READWRITE)):!(m.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE))))
            fail143("inaccessible DMA span",va,(uint32_t)size);
        at=(uintptr_t)m.BaseAddress+m.RegionSize;
    }
    return (void *)address;
}
static void blit143(GPUObject143 *blit)
{
    uint32_t ramht,instance,entry,source_va,destination_va;
    if(!read143(NULL,0xfd002210,&ramht)||!driving_object143(NULL,read143,ramht,blit->state[0x19c/4],&instance,&entry))
        fail143("blit surface object",blit->state[0x19c/4],0);
    unsigned i;
    for(i=0;i<object_count;i++)if(objects[i].instance==instance)break;
    if(i==object_count||objects[i].cls!=0x62)fail143("uninitialized blit surface",instance,0);
    GPUObject143 *surface=&objects[i];
    DrivingBlit143Rect r={blit->state[0x2fc/4],surface->state[0x300/4],
        surface->state[0x304/4]&0xffffu,surface->state[0x304/4]>>16,
        blit->state[0x300/4]&0xffffu,blit->state[0x300/4]>>16,
        blit->state[0x304/4]&0xffffu,blit->state[0x304/4]>>16,
        blit->state[0x308/4]&0xffffu,blit->state[0x308/4]>>16};
    if(r.operation!=3||r.format!=0xa)fail143("unsupported blit operation/format",r.operation,r.format);
    if(!r.width||!r.height)return;
    size_t source_first,source_end,destination_first,destination_end;
    if(!driving_blit143_range(0x08000000,r.source_pitch,r.source_x,r.source_y,r.width,r.height,&source_first,&source_end)
       ||!driving_blit143_range(0x08000000,r.destination_pitch,r.destination_x,r.destination_y,r.width,r.height,&destination_first,&destination_end)
       ||!driving_dma143(NULL,read143,ramht,surface->state[0x184/4],surface->state[0x308/4],(uint32_t)source_end,0,&source_va)
       ||!driving_dma143(NULL,read143,ramht,surface->state[0x188/4],surface->state[0x30c/4],(uint32_t)destination_end,1,&destination_va))
        fail143("invalid blit DMA/rectangle",surface->state[0x308/4],surface->state[0x30c/4]);
    if((uint64_t)source_va+source_first<(uint64_t)destination_va+destination_end
       && (uint64_t)destination_va+destination_first<(uint64_t)source_va+source_end)
        fail143("unverified overlapping blit",source_va,destination_va);
    /* Current software executor is synchronous and has no texture cache.
     * Hardware reuse must complete here and invalidate destination caches. */
    void *src=span143(source_va,source_end,0),*dst=span143(destination_va,destination_end,1);
    if(driving_blit143_copy(src,source_end,dst,destination_end,&r)!=DRIVING_BLIT143_OK)
        fail143("blit copy failed",source_va,destination_va);
    static unsigned copies;
    if(++copies<=12)fprintf(stderr,"[GPU143] SRCCOPY %ux%u %08X -> %08X\n",r.width,r.height,source_va,destination_va);
}
/* Optional bounded source-texture observation, not a rendered frame. The
 * synchronous consumer owns method state here; copies own their bytes before
 * file I/O. Future hardware integration requires alias completion before reads. */
static void texture_capture167(GPUObject143 *object)
{
    static int enabled167=-1;
    static unsigned counts[3],serial,rejections,attempts;
    static uint32_t keys[9][3];
    const char *folder;
    if(enabled167<0){const char *v=getenv("DRIVING_TEXTURE_CAPTURE167");enabled167=v&&v[0]=='1';}
    if(!enabled167||serial>=9||attempts>=9)return;
    folder=getenv("DRIVING_CAPTURE_DIR");if(!folder)return;
    NFTextureState161 s={0};NFTextureSelection161 selected;
    const unsigned methods[9]={0x1e70,0x1e60,0xac0,0x260,0x1e40,0xaa0,0x288,0x28c,0x1e78};
    uint32_t values[9];
    for(unsigned i=0;i<9;i++){values[i]=object->state[methods[i]/4];if(object->written167[methods[i]/4])s.known|=1u<<i;}
    s.program=values[0];s.control=values[1];s.rgb=values[2];s.alpha=values[3];
    s.rgb_out=values[4];s.alpha_out=values[5];s.final0=values[6];s.final1=values[7];s.other=values[8];
    for(unsigned i=0;i<4;i++){
        unsigned b=0x1b00+i*0x40;
        s.format[i]=object->state[(b+4)/4];s.texture_control[i]=object->state[(b+12)/4];
        if(object->written167[(b+4)/4])s.format_known|=1u<<i;
        if(object->written167[(b+12)/4])s.texture_control_known|=1u<<i;
    }
    if(nf_texture_select161(&s,&selected)!=NF161_OK||selected.stage!=3)return;
    uint32_t fmt=s.format[3],off=object->state[0x1bc0/4],pal=object->state[0x1be0/4];
    unsigned kind=fmt==0x06610b29?0:fmt==0x05610b29?1:2;
    if(counts[kind]>=3||!object->written167[0x1bc0/4])return;
    if(selected.palette_required&&(!object->written167[0x1be0/4]||(pal&63)))return;
    for(unsigned i=0;i<serial;i++)if(keys[i][0]==fmt&&keys[i][1]==off&&keys[i][2]==pal)return;
    /* Exact161 formats: one 2D mip, A texture context, palette context A/256.
     * Bounds include the selected DMA object's limit, then allocated host RAM. */
    attempts++;
    unsigned width=1u<<((fmt>>20)&15),height=1u<<((fmt>>24)&15);
    unsigned bytes=width*height*(selected.palette_required?1:4);
    unsigned palette_bytes=selected.palette_required?1024:0;
    uint32_t ramht,va,pva=0;unsigned char pixels[16384],palette[1024];SIZE_T got;
    if(bytes>sizeof(pixels)||!object->written167[0x184/4]
       ||!read143(NULL,0xfd002210,&ramht)
       ||!driving_dma143(NULL,read143,ramht,object->state[0x184/4],off,bytes,0,&va)
       ||(uint64_t)(va-0x80000000u)+bytes>xbox_ContiguousAllocatedBytes())goto rejected;
    if(palette_bytes&&(!driving_dma143(NULL,read143,ramht,object->state[0x184/4],pal&0xffffffc0u,palette_bytes,0,&pva)
       ||(uint64_t)(pva-0x80000000u)+palette_bytes>xbox_ContiguousAllocatedBytes()))goto rejected;
    residency_read236(va,bytes);if(palette_bytes)residency_read236(pva,palette_bytes);
    if(!ReadProcessMemory(GetCurrentProcess(),(void*)((uintptr_t)xbox_GetMemoryOffset()+va),pixels,bytes,&got)||got!=bytes)goto rejected;
    if(palette_bytes&&(!ReadProcessMemory(GetCurrentProcess(),(void*)((uintptr_t)xbox_GetMemoryOffset()+pva),palette,palette_bytes,&got)||got!=palette_bytes))goto rejected;
    char path[2048];FILE *f;int n;unsigned number=serial+1;int ok=1;
    n=snprintf(path,sizeof(path),"%s/texture167-%02u.bin",folder,number);if(n<0||(size_t)n>=sizeof(path))goto rejected;
    f=fopen(path,"wb");if(!f)goto rejected;ok=fwrite(pixels,1,bytes,f)==bytes;if(fclose(f))ok=0;
    if(palette_bytes){
        n=snprintf(path,sizeof(path),"%s/texture167-%02u-palette.bin",folder,number);if(n<0||(size_t)n>=sizeof(path))goto rejected;
        f=fopen(path,"wb");if(!f)goto rejected;ok&=fwrite(palette,1,palette_bytes,f)==palette_bytes;if(fclose(f))ok=0;
    }
    n=snprintf(path,sizeof(path),"%s/texture167-%02u-state.bin",folder,number);if(n<0||(size_t)n>=sizeof(path))goto rejected;
    f=fopen(path,"wb");if(!f)goto rejected;ok&=fwrite(object->state,1,sizeof(object->state),f)==sizeof(object->state);if(fclose(f))ok=0;
    fprintf(stderr,"[TEXTURE167] snapshot=%u kind=%u format=%08X offset=%08X palette=%08X source=%08X palette_source=%08X width=%u height=%u bytes=%u palette_bytes=%u context=%08X handle=%08X ok=%d observation=source-texture-not-frame\n",
        number,kind,fmt,off,pal,va,pva,width,height,bytes,palette_bytes,object->instance,object->state[0x184/4],ok);
    keys[serial][0]=fmt;keys[serial][1]=off;keys[serial][2]=pal;serial++;counts[kind]++;return;
rejected:
    if(++rejections<=3)fprintf(stderr,"[TEXTURE167] capture refused format=%08X offset=%08X palette=%08X\n",fmt,off,pal);
}
#include "driving_movie198.h"
#include "driving_live201.h"
#include "driving_world210.h"
#include "driving_capture223.h"
#include "driving_live214.h"
#include "driving_residency236.h"
#if defined(DRIVING_COMMAND245) || defined(DRIVING_COMMAND262)
#include "driving_command245.h"
#include "driving_command262.h"
static int setting245=-1,setting262=-1;
static unsigned owned_kind262;
static uint64_t bulk_calls262,bulk_success262,loaded_words262,owned_words262;
static int command262_enabled(void){
#ifdef DRIVING_COMMAND262
 if(setting262<0){const char *v=getenv("DRIVING_COMMAND262");setting262=v&&!strcmp(v,"1");}
 return setting262;
#else
 return 0;
#endif
}
static uint64_t bulk_calls245,bulk_success245,loaded_words245,owned_words245,scalar_words245;
static int command245_enabled(void){
#ifdef DRIVING_COMMAND245
 if(setting245<0){const char *v=getenv("DRIVING_COMMAND245");setting245=v&&!strcmp(v,"1");}
 return setting245;
#else
 return 0;
#endif
}
static void command245_fetch(int owned){
 if(owned&&owned_kind262){owned_words262++;return;}
 if(command245_enabled()){if(owned)owned_words245++;else scalar_words245++;}
}
static void command245_report(void){
 if(command262_enabled())fprintf(stderr,"[COMMAND262] bulk_calls=%llu successful=%llu loaded_words=%llu owned_fetches=%llu saved_read_calls=%lld upload-only=1 counters-only-no-speed-claim=1\n",
  (unsigned long long)bulk_calls262,(unsigned long long)bulk_success262,
  (unsigned long long)loaded_words262,(unsigned long long)owned_words262,
  (long long)owned_words262-(long long)bulk_calls262);
 if(command245_enabled())fprintf(stderr,"[COMMAND245] bulk_calls=%llu successful=%llu loaded_words=%llu owned_fetches=%llu scalar_fallthrough=%llu saved_read_calls=%lld counters-only-no-speed-claim=1\n",
  (unsigned long long)bulk_calls245,(unsigned long long)bulk_success245,(unsigned long long)loaded_words245,
  (unsigned long long)owned_words245,(unsigned long long)scalar_words245,
  (long long)owned_words245-(long long)bulk_calls245);
}
static unsigned snapshot245(void *ctx,const DrivingPB143 *p,uint32_t put,uint32_t *out,unsigned cap)
{
 SIZE_T got=0;(void)ctx;
 owned_kind262=0;
 if(!command245_enabled()&&!command262_enabled())return 0;
 unsigned cls=p->subchannel<8&&bound[p->subchannel]?objects[bound[p->subchannel]-1].cls:0;
 unsigned capture=enabled210!=0||capture_enabled223!=0||commands210!=NULL||commands223!=NULL;
 unsigned n=command245_enabled()?driving_command245_words(p,put,&collector214,family214,held201,cls,capture,cap):0;
 if(!n&&command262_enabled()){
  GPUObject143 *object=p->subchannel<8&&bound[p->subchannel]?&objects[bound[p->subchannel]-1]:NULL;
  n=driving_command262_words(p,put,&collector214,held201,live201.active,cls,
   object?object->written167[0x17fc/4]:0,object?object->state[0x17fc/4]:1,
   capture||movie_enabled198!=0,cap);
  if(n)owned_kind262=1;
 }
 if(!n)return 0;
 uint32_t va=p->cursor+0x80000000u;size_t bytes=(size_t)n*4;
 uintptr_t base=(uintptr_t)xbox_GetMemoryOffset();
 if(base>UINTPTR_MAX-va||base+va>UINTPTR_MAX-bytes)return 0;
 /* Alias completion precedes the read. Discard partial RPM and retry scalar.
  * This owns bytes, not a locked mapping or other guest resources. */
 DWORD saved=GetLastError();residency_read236(va,bytes);
 if(owned_kind262)bulk_calls262++;else bulk_calls245++;
 int okay=ReadProcessMemory(GetCurrentProcess(),(void *)(base+va),out,bytes,&got)&&got==bytes;
 if(okay){if(owned_kind262){bulk_success262++;loaded_words262+=n;}else{bulk_success245++;loaded_words245+=n;}}
 SetLastError(saved);return okay?n:0;
}
#endif

static void postmovie207(GPUObject143 *object,unsigned sub,unsigned method,uint32_t value)
{
    static int saved;
    if(saved||!draws201[0]||method!=0x208||value!=0x08080228u||object->state[0x17fc/4]||held201)return;
    const char *v=getenv("DRIVING_POSTMOVIE_TRACE207"),*dir=getenv("DRIVING_CAPTURE_DIR");
    if(!v||strcmp(v,"1")||!dir)return;
    saved=1;driving_trace143_start_postmovie207();
    /* Own command state only: not texture contents or a standalone replay. */
    char path[2048];int n=snprintf(path,sizeof path,"%s/gpu207-kelvin-state.bin",dir);
    int ok=0;FILE *f=NULL;
    if(n>0&&(size_t)n<sizeof path&&(f=fopen(path,"wb"))){
        ok=fwrite(object->state,1,sizeof object->state,f)==sizeof object->state;
        if(fclose(f))ok=0;
    }
    driving_trace143(5,object->handle,object->instance,object->cls);
    driving_trace143(1,sub,method,value);
    fprintf(stderr,"[GPU207] post-movie command window cursor=%08X class=%X handle=%08X instance=%08X sub=%u state_saved=%d state_bytes=%zu command-state-only=1\n",
        pb.cursor,object->cls,object->handle,object->instance,sub,ok,sizeof object->state);
}
#include "driving_method_timing327.h"
static int intercept201_probe327(unsigned method,uint32_t value){
#ifdef DRIVING_TIMING250
    if(method==0x17fc&&!value&&method327_enabled()){
        MethodScope327 s=method327_begin();int result=intercept201(method,value);
        method327_end(5,s);return result;
    }
#endif
    return intercept201(method,value);
}
static int execute143_impl327(void *ctx,uint32_t sub,uint32_t method,uint32_t value)
{
    uint32_t ramht,instance,entry;
    driving_trace143(1,sub,method,value);
    if(method==0){
        residency_flush236("object-bind");
        barrier214();
        barrier201();
        uint32_t cls;
        if(!read143(ctx,0xfd002210,&ramht)||!driving_object143(ctx,read143,ramht,value,&instance,&entry)
           ||(entry&0x30000u)!=0x10000u||!read143(ctx,instance,&cls))return 0;
        unsigned i;
        for(i=0;i<object_count;i++)if(objects[i].instance==instance)break;
        if(i==object_count){
            if(object_count==64)fail143("object capacity",instance,value);
            if((cls&0xfff)==0x97)for(unsigned j=0;j<object_count;j++)
                if(objects[j].cls==0x97)fail143("unimplemented second Kelvin context",instance,value);
            objects[i].instance=instance;objects[i].cls=cls&0xfff;object_count++;
        }
        if(objects[i].cls!=(cls&0xfff))fail143("object instance class changed",instance,cls);
        objects[i].handle=value;bound[sub]=i+1;
        driving_trace143(5,value,instance,objects[i].cls);
        return 1;
    }
    if(!bound[sub])fail143("unbound subchannel",sub,method);
    GPUObject143 *object=&objects[bound[sub]-1];
    residency_command236(object,method,value);
    if(object->cls!=0x97){
        barrier214();
        barrier201();
        if(method<0x400)object->state[method/4]=value;
        if(object->cls==0x9f&&method==0x308){blit143(object);return 1;}
        /* These captured non-3D methods only bind context/operation state.
         * Stop at an actual unsupported blit rather than acknowledging it. */
        if((object->cls==0x39 && method==0x180)
           ||(object->cls==0x62 && (method==0x184||method==0x188||method==0x300||method==0x304||method==0x308||method==0x30c))
           ||(object->cls==0x9f && ((method>=0x184&&method<=0x19c)||method==0x2fc||method==0x300||method==0x304)))return 1;
        fail143("unimplemented object method",object->cls,method);
    }
    if(method<0x2000){object->state[method/4]=value;object->written167[method/4]=1;}
    if(method==0x208){DWORD saved207=GetLastError();postmovie207(object,sub,method,value);SetLastError(saved207);}
    {DWORD saved198=GetLastError();movie_observe198(method,value);SetLastError(saved198);}
    if(method==0x17fc&&value){DWORD saved167=GetLastError();texture_capture167(object);SetLastError(saved167);}
    if(method==0x17fc)driving_scanout152_begin(value);
    {DWORD saved223=GetLastError();capture223(object,method,value);SetLastError(saved223);}
    {DWORD saved201=GetLastError();int handled201=intercept201_probe327(method,value);SetLastError(saved201);if(handled201)return 1;}
    {DWORD saved210=GetLastError();world210(object,method,value);SetLastError(saved210);}
    {DWORD saved214=GetLastError();int handled214=intercept214(object,method,value);SetLastError(saved214);if(handled214)return 1;}
    if(driving_flip204_method(method,value))return 1;
    if(enabled201>0&&method==0x1d94&&(value&3)){
        DrivingSurface145 z;uint32_t address;
        if(!object->written167[0x290/4]||(object->state[0x290/4]&0x1000)||
           !object->written167[0x1d8c/4]||!object->written167[0x198/4]||!object->written167[0x214/4]||
           !driving_surface145(&z,object->state[0x208/4],object->state[0x200/4],object->state[0x204/4],object->state[0x20c/4]>>16)||
           !read143(ctx,0xfd002210,&ramht)||!driving_dma143(ctx,read143,ramht,object->state[0x198/4],object->state[0x214/4],(uint32_t)z.span,1,&address))
            fail143("unsupported depth clear202",object->state[0x208/4],value);
        if(!nf_hw_sync()||!driving_depth202(span143(address,z.span,1),z.span,&z,object->state[0x1d98/4],object->state[0x1d9c/4],value,object->state[0x1d8c/4]))
            fail143("depth clear202 failed",address,value);
        static unsigned clears202,clears208;if(++clears202<=4||(z.format==0x09090113u&&clears208++<4))fprintf(stderr,"[GPU202] original depth clear format=%08X target=%08X value=%08X lanes=%u flags=%X\n",z.format,address,object->state[0x1d8c/4],z.lanes,value);
        value&=~3u;
        if(!(value&0xf0))return 1;
    }
    if(method==0x1d94 && (value&0xf0)){
        DrivingSurface145 surface;uint32_t address;
        if(!driving_surface145(&surface,object->state[0x208/4],object->state[0x200/4],
             object->state[0x204/4],object->state[0x20c/4]&0xffff))
            fail143("unsupported color-clear storage",object->state[0x208/4],value);
        if(!read143(ctx,0xfd002210,&ramht) || !driving_dma143(ctx,read143,ramht,
             object->state[0x194/4],object->state[0x210/4],(uint32_t)surface.span,1,&address))
            fail143("invalid color-clear DMA",object->state[0x194/4],object->state[0x210/4]);
        if(!driving_color_clear145(span143(address,surface.span,1),surface.span,&surface,
             object->state[0x1d98/4],object->state[0x1d9c/4],value,object->state[0x1d90/4]))
            fail143("unsupported color-clear rectangle",object->state[0x1d98/4],object->state[0x1d9c/4]);
        static unsigned clears;
        if(++clears<=6)fprintf(stderr,"[GPU145] original clear %08X storage=%ux%u lanes=%u color=%08X\n",
             address,surface.storage_width,surface.height,surface.lanes,object->state[0x1d90/4]);
        /* Suppress the generic color clear: it guesses bpp from pitch and also
         * selects the render target as scanout. Original display owns scanout. */
        nv2a_pb_exec_method(0,method,value&~0xf0u);return 1;
    }
    if(method==0x1a4){object->semaphore_handle=value;return 1;}
    if(method==0x1d6c){object->semaphore_offset=value;return 1;}
    if(method==0x1d70){
        uint32_t destination;
        if(!read143(ctx,0xfd002210,&ramht)
           ||!driving_semaphore143(ctx,read143,ramht,object->semaphore_handle,object->semaphore_offset,&destination))
            fail143("unresolved semaphore",object->semaphore_handle,object->semaphore_offset);
        /* Pinned software executor executes supported draws/clears synchronously.
         * If replaced by D3D11, this boundary needs actual backend completion. */
        MemoryBarrier();
        SIZE_T wrote=0;
        BOOL released281=semaphore281_enabled()?semaphore281_write(destination,value,(uintptr_t)xbox_GetMemoryOffset(),xbox_ContiguousAllocatedBytes(),&wrote):
            driving_write_process270(GetCurrentProcess(),(void *)((uintptr_t)xbox_GetMemoryOffset()+destination),&value,4,&wrote);
        if(!released281||wrote!=4)
            fail143("semaphore write",destination,value);
        if(++releases<=16)fprintf(stderr,"[GPU143] software release %u -> %08X\n",value,destination);
        return 1;
    }
    residency_generic237(method,value);
    nv2a_pb_exec_method(0,method,value);return 1;
}
static int execute143(void *ctx,uint32_t sub,uint32_t method,uint32_t value){
#ifdef DRIVING_TIMING250
    if(method327_enabled()){
        unsigned group=method==0x17fc&&!value?0:method==0x1d94?1:method==0x100?2:method==0x130?3:4;
        MethodScope327 s=method327_begin();int result=execute143_impl327(ctx,sub,method,value);
        method327_end(group,s);return result;
    }
#endif
    return execute143_impl327(ctx,sub,method,value);
}
/* Disable duplicate execution by the toolkit's polling observer. */
void nv2a_pb_scan(uint32_t a,uint32_t b){(void)a;(void)b;}
void nv2a_pb_scan_report(void){}
/*295: only a fully owned tail is handed off. All parser state remains here. */
static struct {uint32_t put,get;unsigned submitted;} tail295;
static void driving_gpu_completed295(uint32_t put,uint32_t get){
    driving_trace143(3,put,get,pb.remaining);
    MemoryBarrier();
    *(volatile uint32_t *)((uintptr_t)xbox_GetMemoryOffset()+0xfd800044u)=get;
    if(++segments<=8 || !(segments%512)){
#if defined(DRIVING_COMMAND245) || defined(DRIVING_COMMAND262)
        command245_report();
#endif
        fprintf(stderr,"[GPU143] drains=%u words=%llu methods=%llu releases=%u GET=%08X pending=%u\n",segments,(unsigned long long)pb.words,(unsigned long long)pb.methods,releases,pb.cursor,pb.remaining);
#ifdef DRIVING_COMMAND_READ265
        command_report265();
#endif
        if(read_sample326_enabled>0){DWORD error326=GetLastError();int crt326=errno;LARGE_INTEGER hz326;
            if(QueryPerformanceFrequency(&hz326))fprintf(stderr,"[READ-SAMPLE326] calls=%llu samples=%llu sampled_ticks=%llu qpc_probe_4096_ticks=%llu qpc_hz=%lld one_in_256=1\n",
                (unsigned long long)read_sample326_calls,(unsigned long long)read_sample326_count,
                (unsigned long long)read_sample326_ticks,(unsigned long long)read_sample326_probe_ticks,(long long)hz326.QuadPart);
            errno=crt326;SetLastError(error326);}
        nv2a_pb_exec_report();semaphore281_report();
    }
}
static int driving_gpu_tail295(void *context){
    (void)context;
    /* Producer has closed every proof scope and released its own scanout lock.
     * No guest callbacks or command reads occur in this native owned flush. */
    driving_scanout152_enter();
    if(residency_depth236||!batch234.count)fail143("async295 lost tail ownership",residency_depth236,batch234.count);
    residency_flush236("drain-before-GET");
    driving_gpu_completed295(tail295.put,tail295.get);
    driving_batch236_report();
    driving_scanout155_leave(1,tail295.submitted);
    driving_scanout152_report();
    return 1;
}
static unsigned driving_tail295_refusal(void){
    if(!batch234.count)return 1;
    if(residency_depth236!=1)return 2;
    if(!pb.initialized||pb.remaining||pb.return_address)return 3;
    if(collector214.phase!=DC214_IDLE||collector214.method_count||collector214.index_count)return 4;
    if(held201||words201||live201.active||movie198.active)return 5;
    if(commands210||commands223||InterlockedCompareExchange(&movie_requested180,0,0))return 6;
    for(unsigned i=0;i<object_count;i++)if(objects[i].cls==0x97&&objects[i].state[0x17fc/4])return 7;
    if(batch234.count>DRIVING_BATCH_COUNT234||batch234.bytes>DRIVING_BATCH_BYTES234||
       !batch234.targets||!batch234.mapped[0]||!batch234.mapped[1]||
       batch234.spans[0].address!=0x82cbc000u||batch234.spans[1].address!=0x8316c000u)return 8;
    if((_mm_getcsr()&_MM_ROUND_MASK)!=_MM_ROUND_NEAREST)return 10;
    return 0;
}
static int driving_gpu_drain143_inner152(void)
{
    if(enabled<0){enabled=getenv("DRIVING_PB_SYNC")!=NULL;if(enabled)fprintf(stderr,"[GPU143] synchronous command executor; optional295 owned-tail worker; incomplete rendering\n");}
    if(!enabled)return 0;
    movie_phase180();
    uint32_t put=0;
    if(!read143(NULL,0xfd800040,&put))fail143("PUT read",0,put);
    driving_trace143(2,put,pb.cursor,pb.remaining);
    if(!driving_pb143_consume(&pb,put,NULL,read143,execute143))fail143("command/object decode",pb.method,put);
    if(driving_async295_enabled()){
        unsigned refusal=driving_tail295_refusal();
        if(!refusal){
            tail295.put=put;tail295.get=pb.cursor;tail295.submitted=1;
            if(driving_async295_arm(driving_gpu_tail295,NULL,batch234.count,batch234.bytes))return 1;
            refusal=9;
        }
        driving_async295_reject(refusal);
    }
    residency_flush236("drain-before-GET");
    driving_gpu_completed295(put,pb.cursor);return 0;
}

void driving_gpu_drain143(void)
{
    driving_async295_enter(); /* join before taking the scanout lock */
    driving_scanout152_enter();
    driving_drainmap274_enter();
#ifdef DRIVING_COMMAND_READ265
    command_enter265();
#endif
    DrivingDrain250 drain250=dt250_drain_begin(batch_time236.queue_ticks,batch_time236.fallback_ticks,batch_time236.flush_ticks);
    residency_enter236();
    uint64_t before155=pb.words;
    int deferred295=driving_gpu_drain143_inner152();
    if(deferred295){ /* exclusive retained batch; never flush on producer leave */
        driving_batch236_report();residency_depth236--;
    }else residency_leave236();
    driving_drainmap274_leave();
    dt250_drain_end(drain250,batch_time236.queue_ticks,batch_time236.fallback_ticks,batch_time236.flush_ticks);
#ifdef DRIVING_TIMING250
    method327_report();
#endif
#ifdef DRIVING_COMMAND_READ265
    command_leave265();
#endif
    driving_scanout155_leave(!deferred295 && enabled>0 && pb.initialized && !pb.remaining && !pb.return_address,pb.words!=before155);
    driving_scanout152_report();
    if(deferred295)driving_async295_kick();
    driving_async295_leave();
}
