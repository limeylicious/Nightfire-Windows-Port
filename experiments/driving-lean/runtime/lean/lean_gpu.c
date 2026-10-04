/* Lean Driving renderer: synchronous pushbuffer consumer feeding the
 * GPU-resident D3D11 backend. Replaces driving_gpu143.c + the generic
 * executor in the lean build only. Object/DMA decoding reuses the existing
 * driving_gpu143_core.h walker; flips reuse driving_flip204.c. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../driving_gpu143_core.h"
#include "../driving_blit143.h"
#include "../driving_scanout152.h"
#include "../gpu144/nightfire_vertex_program.h"
#include "lean_d3d.h"

extern ptrdiff_t xbox_GetMemoryOffset(void);
extern uint32_t xbox_ContiguousAllocatedBytes(void);
extern int driving_flip204_method(unsigned,uint32_t);
extern int driving_flip204_enabled(void);
extern void driving_present201(const uint8_t *,uint32_t);
extern int driving_present201_capture_due(void);
extern int lean_d3d_present(uint32_t va,const uint32_t *pixels);
unsigned lean_d3d_vp_mask(const void *vp);

static uint64_t qpc(void){LARGE_INTEGER t;QueryPerformanceCounter(&t);return (uint64_t)t.QuadPart;}
static struct {uint64_t drain,draw,readback,flipwait,frames,last,outside,start,req,stall,nreq,nstall;} tm;
static void stop(const char *why,uint32_t a,uint32_t b){fprintf(stderr,"[LEAN] STOP %s %08X %08X\n",why,a,b);fflush(stderr);abort();}

uint8_t *lean_guest(uint32_t va,uint32_t bytes){
    uint32_t allocated=xbox_ContiguousAllocatedBytes();
    if(va>=0x80000000u){uint64_t off=(uint64_t)va-0x80000000u;
        if(off+bytes>allocated){
            /* Past the tracked high-water mark: accept committed readable pages. */
            if(off+bytes>0x04000000u)return NULL;
            uintptr_t at=(uintptr_t)xbox_GetMemoryOffset()+va,end=at+bytes;
            while(at<end){MEMORY_BASIC_INFORMATION m;
                if(!VirtualQuery((void*)at,&m,sizeof m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return NULL;
                at=(uintptr_t)m.BaseAddress+m.RegionSize;}
        }}
    else if((uint64_t)va+bytes>0x04000000u)return NULL;
    return (uint8_t*)((uintptr_t)xbox_GetMemoryOffset()+va);
}
static int rd(void *ctx,uint32_t va,uint32_t *out){
    (void)ctx;if(va<0x08000000u)va+=0x80000000u;
    *out=*(volatile uint32_t*)((uintptr_t)xbox_GetMemoryOffset()+va);return 1;
}

/* ----------------------------------------------------------- objects/DMA */
typedef struct Obj {uint32_t instance,handle,cls,state[0x400/4];} Obj;
static Obj objs[64];static unsigned nobjs,bound[8];
static DrivingPB143 pb;
static uint32_t K[0x2000/4];
static NFVertexProgram vp;
static uint32_t ramht;
typedef struct {uint32_t handle,base,limit;int ok;} DmaE;
static DmaE dmac[32];static unsigned ndma;
static int dma(uint32_t handle,uint32_t offset,uint32_t bytes,uint32_t *va){
    DmaE *e=NULL;
    for(unsigned i=0;i<ndma;i++)if(dmac[i].handle==handle){e=&dmac[i];break;}
    if(!e){
        if(ndma==32)ndma=0;e=&dmac[ndma++];e->handle=handle;e->ok=0;
        uint32_t instance,entry,flags,limit,frame;
        if(driving_object143(NULL,rd,ramht,handle,&instance,&entry)&&!(entry&0x30000u)&&rd(NULL,instance,&flags)&&rd(NULL,instance+4,&limit)&&rd(NULL,instance+8,&frame)){
            e->base=0x80000000u+(frame&0xfffff000u)+(flags>>20);e->limit=limit;e->ok=1;
        }
    }
    if(!e->ok)return 0;
    if((uint64_t)offset+bytes>(uint64_t)e->limit+1){
        static unsigned warned;if(warned++<8)fprintf(stderr,"[LEAN] DMA %08X offset %08X+%u beyond limit %08X (allowed)\n",handle,offset,bytes,e->limit);
    }
    *va=e->base+offset;return 1;
}

/* ------------------------------------------------------------ vertices */
static float cur[16][4];
typedef struct {unsigned prim,active,mode;uint32_t *idx;unsigned nidx,cidx;uint32_t *inl;unsigned ninl,cinl;float *imm;unsigned nimm,cimm;} Draw;
static Draw dr;
static float *vbuf;static size_t vcap;static uint32_t *ibuf;static size_t icap;
static void *grow(void *p,size_t *cap,size_t need,size_t elem){if(need<=*cap)return p;size_t n=*cap?*cap:4096;while(n<need)n*=2;void *q=realloc(p,n*elem);if(!q)stop("out of memory",0,0);*cap=n;return q;}
static void push_idx(uint32_t i){if(dr.nidx==dr.cidx){size_t c=dr.cidx;dr.idx=grow(dr.idx,&c,dr.nidx+1,4);dr.cidx=(unsigned)c;}dr.idx[dr.nidx++]=i;}
static void push_inl(uint32_t w){if(dr.ninl==dr.cinl){size_t c=dr.cinl;dr.inl=grow(dr.inl,&c,dr.ninl+1,4);dr.cinl=(unsigned)c;}dr.inl[dr.ninl++]=w;}
static void push_imm(void){
    size_t need=(size_t)(dr.nimm+1)*64;
    if(need>dr.cimm){size_t c=dr.cimm;dr.imm=grow(dr.imm,&c,need,4);dr.cimm=(unsigned)c;}
    memcpy(dr.imm+(size_t)dr.nimm*64,cur,sizeof cur);dr.nimm++;
}
static float s16(int16_t v){return (float)v;}
static void decode_attr(float *o,const uint8_t *p,uint32_t fmt){
    unsigned type=fmt&15,size=(fmt>>4)&15;o[0]=o[1]=o[2]=0;o[3]=1;
    switch(type){
    case 0: /* UB_D3D: B,G,R,A in memory */
        if(size>=4||size==3){o[0]=p[2]/255.0f;o[1]=p[1]/255.0f;o[2]=p[0]/255.0f;if(size==4)o[3]=p[3]/255.0f;}
        else for(unsigned i=0;i<size;i++)o[i]=p[i]/255.0f;
        break;
    case 4: for(unsigned i=0;i<size&&i<4;i++)o[i]=p[i]/255.0f;break;
    case 2: memcpy(o,p,4*(size>4?4:size));break;
    case 1: for(unsigned i=0;i<size&&i<4;i++){int16_t v;memcpy(&v,p+2*i,2);o[i]=v<0?v/32768.0f:v/32767.0f;}break;
    case 5: for(unsigned i=0;i<size&&i<4;i++){int16_t v;memcpy(&v,p+2*i,2);o[i]=s16(v);}break;
    case 6:{uint32_t v;memcpy(&v,p,4);int x=(int)(v<<21)>>21,y=(int)((v>>11)<<21)>>21,z=(int)(v)>>22;o[0]=x/1023.0f;o[1]=y/1023.0f;o[2]=z/511.0f;}break;
    default:{static unsigned w;if(w++<8)fprintf(stderr,"[LEAN] vertex attribute type %u unsupported\n",type);}
    }
}
static unsigned attr_bytes(uint32_t fmt){
    unsigned type=fmt&15,size=(fmt>>4)&15;
    switch(type){case 0:case 4:return size?4:0;case 2:return size*4;case 1:case 5:return size*2;case 6:return size?4:0;}
    return 0;
}

/* ---------------------------------------------------------- surface/draw */
static int target(LeanTarget *t){
    memset(t,0,sizeof *t);
    uint32_t fmt=K[0x208/4],pitch=K[0x20c/4];
    t->width=K[0x200/4]>>16;t->height=K[0x204/4]>>16;
    unsigned aa=(fmt>>12)&15;t->aa_x=aa?2:1;t->aa_y=aa==2?2:1;
    t->color_fmt=fmt&15;t->zeta_fmt=(fmt>>4)&15;t->swizzled=((fmt>>8)&15)==2;
    if(t->swizzled){t->width=1u<<((fmt>>16)&255);t->height=1u<<(fmt>>24);}
    t->color_pitch=pitch&0xffff;t->zeta_pitch=pitch>>16;
    if(!t->width||!t->height)return 0;
    if(t->swizzled){t->color_pitch=t->width*4;t->zeta_pitch=t->width*4;}
    unsigned bytes=t->color_pitch*t->height*t->aa_y;
    if(t->color_fmt&&!dma(K[0x194/4],K[0x210/4],bytes?bytes:4,&t->color))t->color=0;
    if(t->zeta_fmt&&K[0x214/4]&&!dma(K[0x198/4],K[0x214/4],t->zeta_pitch*t->height*t->aa_y?t->zeta_pitch*t->height*t->aa_y:4,&t->zeta))t->zeta=0;
    if(!K[0x214/4]&&!(K[0x198/4]))t->zeta=0;
    return 1;
}
static uint64_t ndraws,nrefused;
static void end_draw(void){
    unsigned prim=dr.prim;dr.active=0;
    if((vp.mode&3)!=2){static unsigned w;if(w++<8)fprintf(stderr,"[LEAN] non-program transform mode %X skipped\n",vp.mode);nrefused++;goto reset;}
    unsigned mask=lean_d3d_vp_mask(&vp);if(!mask){nrefused++;goto reset;}
    unsigned slots[16],nslot=0;for(unsigned k=0;k<16;k++)if(mask&(1u<<k))slots[nslot++]=k;
    unsigned nv=0;const uint32_t *srcidx=NULL;unsigned nsrc=0;
    /* Build the vertex list. */
    if(dr.mode==1){
        if(!dr.nidx)goto reset;
        uint32_t lo=0xffffffffu,hi=0;for(unsigned i=0;i<dr.nidx;i++){if(dr.idx[i]<lo)lo=dr.idx[i];if(dr.idx[i]>hi)hi=dr.idx[i];}
        nv=hi-lo+1;if(nv>1u<<20){fprintf(stderr,"[LEAN] index span %u too large\n",nv);nrefused++;goto reset;}
        vbuf=grow(vbuf,&vcap,(size_t)nv*nslot*4,4);
        for(unsigned s=0;s<nslot;s++){
            unsigned k=slots[s];uint32_t fmt=K[(0x1760+4*k)/4],off=K[(0x1720+4*k)/4];unsigned size=(fmt>>4)&15,stride=fmt>>8,eb=attr_bytes(fmt);
            if(!size||!eb){for(unsigned v=0;v<nv;v++)memcpy(&vbuf[((size_t)v*nslot+s)*4],cur[k],16);continue;}
            uint32_t va;uint32_t span=(nv-1)*stride+eb;
            if(!dma(off&0x80000000u?K[0x1a0/4]:K[0x19c/4],(off&0x7fffffffu)+lo*stride,span,&va)){nrefused++;goto reset;}
            const uint8_t *g=lean_guest(va,span);if(!g){fprintf(stderr,"[LEAN] vertex stream %08X+%u outside RAM\n",va,span);nrefused++;goto reset;}
            for(unsigned v=0;v<nv;v++)decode_attr(&vbuf[((size_t)v*nslot+s)*4],g+(size_t)v*stride,fmt);
        }
        for(unsigned i=0;i<dr.nidx;i++)dr.idx[i]-=lo;
        srcidx=dr.idx;nsrc=dr.nidx;
    }else if(dr.mode==2){
        unsigned stride=0,off[16];
        for(unsigned k=0;k<16;k++){uint32_t fmt=K[(0x1760+4*k)/4];off[k]=stride;stride+=(attr_bytes(fmt)+3)&~3u;}
        if(!stride||!dr.ninl)goto reset;
        nv=dr.ninl*4/stride;vbuf=grow(vbuf,&vcap,(size_t)nv*nslot*4,4);
        const uint8_t *g=(const uint8_t*)dr.inl;
        for(unsigned v=0;v<nv;v++)for(unsigned s=0;s<nslot;s++){
            unsigned k=slots[s];uint32_t fmt=K[(0x1760+4*k)/4];
            if(!attr_bytes(fmt))memcpy(&vbuf[((size_t)v*nslot+s)*4],cur[k],16);
            else decode_attr(&vbuf[((size_t)v*nslot+s)*4],g+(size_t)v*stride+off[k],fmt);
        }
    }else if(dr.mode==3){
        nv=dr.nimm;vbuf=grow(vbuf,&vcap,(size_t)nv*nslot*4,4);
        for(unsigned v=0;v<nv;v++)for(unsigned s=0;s<nslot;s++)memcpy(&vbuf[((size_t)v*nslot+s)*4],dr.imm+(size_t)v*64+slots[s]*4,16);
    }else goto reset;
    if(!nv)goto reset;
    /* Topology -> list indices. */
    #define SRC(i) (srcidx?srcidx[i]:(uint32_t)(i))
    unsigned count=srcidx?nsrc:nv,topo=0;size_t n=0;
    ibuf=grow(ibuf,&icap,(size_t)count*6+16,4);
    switch(prim){
    case 1:topo=2;for(unsigned i=0;i<count;i++)ibuf[n++]=SRC(i);break;
    case 2:topo=1;for(unsigned i=0;i+1<count;i+=2){ibuf[n++]=SRC(i);ibuf[n++]=SRC(i+1);}break;
    case 3:case 4:topo=1;for(unsigned i=0;i+1<count;i++){ibuf[n++]=SRC(i);ibuf[n++]=SRC(i+1);}if(prim==3&&count>2){ibuf[n++]=SRC(count-1);ibuf[n++]=SRC(0);}break;
    case 5:for(unsigned i=0;i+2<count;i+=3){ibuf[n++]=SRC(i);ibuf[n++]=SRC(i+1);ibuf[n++]=SRC(i+2);}break;
    case 6:for(unsigned i=0;i+2<count;i++){if(i&1){ibuf[n++]=SRC(i+1);ibuf[n++]=SRC(i);ibuf[n++]=SRC(i+2);}else{ibuf[n++]=SRC(i);ibuf[n++]=SRC(i+1);ibuf[n++]=SRC(i+2);}}break;
    case 7:case 10:for(unsigned i=1;i+1<count;i++){ibuf[n++]=SRC(0);ibuf[n++]=SRC(i);ibuf[n++]=SRC(i+1);}break;
    case 8:for(unsigned i=0;i+3<count;i+=4){ibuf[n++]=SRC(i);ibuf[n++]=SRC(i+1);ibuf[n++]=SRC(i+2);ibuf[n++]=SRC(i);ibuf[n++]=SRC(i+2);ibuf[n++]=SRC(i+3);}break;
    case 9:for(unsigned i=0;i+3<count;i+=2){ibuf[n++]=SRC(i);ibuf[n++]=SRC(i+1);ibuf[n++]=SRC(i+3);ibuf[n++]=SRC(i);ibuf[n++]=SRC(i+3);ibuf[n++]=SRC(i+2);}break;
    default:{static unsigned w;if(w++<8)fprintf(stderr,"[LEAN] primitive %u unsupported\n",prim);}goto reset;
    }
    #undef SRC
    if(!n)goto reset;
    LeanDraw d;memset(&d,0,sizeof d);
    if(!target(&d.t)){nrefused++;goto reset;}
    d.K=K;d.vp=&vp;d.topo=topo;d.verts=vbuf;d.nverts=nv;d.mask=mask;d.idx=ibuf;d.nidx=(unsigned)n;
    uint32_t prog=K[0x1e70/4];
    for(unsigned i=0;i<4;i++){
        unsigned mode=(prog>>(5*i))&31;if(!mode||mode==4||mode==5)continue;
        uint32_t fmt=K[(0x1b04+i*64)/4];uint32_t h=(fmt&3)==2?K[0x188/4]:K[0x184/4];
        if(!dma(h,K[(0x1b00+i*64)/4],4,&d.tex_addr[i]))d.tex_addr[i]=0;
        if(((fmt>>8)&255)==0x0b){uint32_t pf=K[(0x1b20+i*64)/4];uint32_t ph=(pf&1)?K[0x188/4]:K[0x184/4];if(!dma(ph,pf&~63u,4,&d.pal_addr[i]))d.pal_addr[i]=0;}
    }
    {static unsigned long long t0,sec;static unsigned cnt,nt;static uint32_t tg[8];if(!t0)t0=GetTickCount64();const char*e=getenv("LEAN_LOG_SW_AFTER_MS");
     if(e){unsigned long long now=GetTickCount64()-t0;if(now/1000!=sec){if(now>(unsigned long long)atoll(e)){fprintf(stderr,"[LEAN-SEC] %llu draws=%u frames=%llu targets",sec,cnt,(unsigned long long)tm.frames);for(unsigned i=0;i<nt;i++)fprintf(stderr," %08X",tg[i]);fputc(10,stderr);}sec=now/1000;cnt=0;nt=0;}
      cnt++;unsigned i;for(i=0;i<nt;i++)if(tg[i]==d.t.color)break;if(i==nt&&nt<8)tg[nt++]=d.t.color;}}
    {uint64_t t0=qpc();lean_d3d_draw(&d);tm.draw+=qpc()-t0;}ndraws++;
reset:
    dr.nidx=dr.ninl=dr.nimm=0;dr.mode=0;
}
static void set_mode(unsigned m){if(dr.mode&&dr.mode!=m){static unsigned w;if(w++<8)fprintf(stderr,"[LEAN] mixed vertex submission %u->%u\n",dr.mode,m);}dr.mode=m;}
static void imm_write(unsigned k,unsigned c,float v,int complete){
    cur[k][c]=v;
    if(k==0&&complete&&dr.active){set_mode(3);push_imm();}
}

/* ------------------------------------------------------------- methods */
static unsigned seen_unhandled[0x2000/4];
static void unhandled(uint32_t m,uint32_t v){if(m<0x2000&&!seen_unhandled[m/4]++)fprintf(stderr,"[LEAN] first unhandled Kelvin method %04X value %08X\n",m,v);}
static uint32_t sem_handle,sem_offset;
static int flip_mode=-1;
/* Direct presentation pacing: wait for the next 20 ms boundary (PAL 50 Hz). */
static void pace50(void){
    static HANDLE timer;static LARGE_INTEGER hz,next;
    LARGE_INTEGER now;QueryPerformanceCounter(&now);
    if(!timer){timer=CreateWaitableTimerExW(NULL,NULL,2,TIMER_ALL_ACCESS);if(!timer)timer=CreateWaitableTimerW(NULL,FALSE,NULL);QueryPerformanceFrequency(&hz);next=now;}
    next.QuadPart+=hz.QuadPart/50;
    if(now.QuadPart>=next.QuadPart){if(now.QuadPart-next.QuadPart>hz.QuadPart/10)next=now;return;}
    LARGE_INTEGER due;due.QuadPart=-(LONGLONG)((next.QuadPart-now.QuadPart)*10000000/hz.QuadPart);
    if(due.QuadPart<0&&SetWaitableTimer(timer,&due,0,NULL,NULL,FALSE))WaitForSingleObject(timer,100);
}
static void present_physical(uint32_t physical){
    static uint32_t *pix;if(!pix)pix=malloc(640*480*4);if(!pix)return;
    uint32_t va=0x80000000u+physical;
    /* Optional wall-clock captures for game-time checks: LEAN_CAPTURE_EVERY_MS. */
    static long long every=-1,next_at;static ULONGLONG t0;
    if(every<0){const char *v=getenv("LEAN_CAPTURE_EVERY_MS");every=v?atoll(v):0;t0=GetTickCount64();next_at=every;}
    const char *dir=getenv("DRIVING_CAPTURE_DIR");
    ULONGLONG wall_now=GetTickCount64()-t0;
    int wall_due=every>0&&dir&&(long long)wall_now>=next_at;
    /* Direct3D window: the frame goes GPU to GPU, and CPU pixels are read back
     * only when a capture wants them or the surface can't be copied directly. */
    int shown=lean_d3d_present(va,NULL),have=0;
    if(shown!=1||wall_due||driving_present201_capture_due()){
        uint64_t rb0=qpc();int resident=lean_d3d_readback(va,640,480,pix);tm.readback+=qpc()-rb0;
        if(!resident){const uint8_t *g=lean_guest(va,2560*480);if(g)memcpy(pix,g,2560*480);have=g!=NULL;}else have=1;
        if(shown==-1&&have)shown=lean_d3d_present(va,pix);
    }
    tm.frames++;
    if(!(tm.frames%150)){LARGE_INTEGER hz;QueryPerformanceFrequency(&hz);double k=1000.0/hz.QuadPart/150;uint64_t now=qpc();
        fprintf(stderr,"[LEAN-TIME] per-frame ms: wall=%.1f drain=%.1f (draw-submit=%.1f readback=%.1f flip-wait=%.1f [request=%.1f x%.2f stall=%.1f x%.2f]) outside-drains=%.1f\n",
            (now-tm.last)*k,tm.drain*k,tm.draw*k,tm.readback*k,tm.flipwait*k,tm.req*k,tm.nreq/150.0,tm.stall*k,tm.nstall/150.0,((now-tm.last)-tm.drain)*k);
        tm.drain=tm.draw=tm.readback=tm.flipwait=tm.req=tm.stall=tm.nreq=tm.nstall=0;tm.last=now;lean_d3d_report();}
    {extern void lean_d3d_guard_surfaces(void);lean_d3d_guard_surfaces();}
    if(!have&&shown!=1)return;
    driving_present201(have?(const uint8_t*)pix:NULL,physical);
    {static int extra=-1;if(extra<0){const char *v=getenv("LEAN_PRESENT_SLEEP_MS");extra=v?atoi(v):0;}if(extra>0)Sleep(extra);}
    if(wall_due&&have){
        ULONGLONG now=wall_now;
        {
            next_at=(long long)now+every;
            char path[MAX_PATH];snprintf(path,sizeof path,"%s/wall-%06llu.bmp",dir,(unsigned long long)now);
            FILE *f=fopen(path,"wb");
            if(f){unsigned char h[54]={0};uint32_t size=54+2560*480,off=54,dib=40,w=640;int32_t ht=-480;uint16_t planes=1,bits=32;
                memcpy(h,"BM",2);memcpy(h+2,&size,4);memcpy(h+10,&off,4);memcpy(h+14,&dib,4);memcpy(h+18,&w,4);memcpy(h+22,&ht,4);memcpy(h+26,&planes,2);memcpy(h+28,&bits,2);
                fwrite(h,1,54,f);fwrite(pix,1,2560*480,f);fclose(f);}
        }
    }
}
static void clear_surface(uint32_t v){
    LeanTarget t;if(!target(&t))return;
    uint32_t h=K[0x1d98/4],vv=K[0x1d9c/4];
    unsigned x0=h&0xffff,x1=h>>16,y0=vv&0xffff,y1=vv>>16;
    if(x1>=t.width)x1=t.width-1;if(y1>=t.height)y1=t.height-1;
    lean_d3d_clear(&t,v,x0,x1,y0,y1,K[0x1d90/4],K[0x1d8c/4]);
}
static void blit(Obj *b){
    uint32_t instance,entry;
    if(!driving_object143(NULL,rd,ramht,b->state[0x19c/4],&instance,&entry))stop("blit surface object",b->state[0x19c/4],0);
    Obj *s=NULL;for(unsigned i=0;i<nobjs;i++)if(objs[i].instance==instance){s=&objs[i];break;}
    if(!s||s->cls!=0x62)stop("blit surface class",instance,0);
    unsigned op=b->state[0x2fc/4],format=s->state[0x300/4];
    unsigned spitch=s->state[0x304/4]&0xffff,dpitch=s->state[0x304/4]>>16;
    unsigned sx=b->state[0x300/4]&0xffff,sy=b->state[0x300/4]>>16,dx=b->state[0x304/4]&0xffff,dy=b->state[0x304/4]>>16;
    unsigned w=b->state[0x308/4]&0xffff,h=b->state[0x308/4]>>16;
    if(op!=3||format!=0xa){fprintf(stderr,"[LEAN] blit op %u format %X unsupported\n",op,format);return;}
    if(!w||!h)return;
    uint32_t src,dst;
    if(!dma(s->state[0x184/4],s->state[0x308/4],1,&src)||!dma(s->state[0x188/4],s->state[0x30c/4],1,&dst))stop("blit DMA",0,0);
    if(lean_d3d_blit(src,spitch,dst,dpitch,sx,sy,dx,dy,w,h))return;
    /* CPU copy of guest bytes; a stale resident copy of the destination is dropped. */
    uint8_t *gs=lean_guest(src,spitch*(sy+h)),*gd=lean_guest(dst,dpitch*(dy+h));if(!gs||!gd)return;
    for(unsigned y=0;y<h;y++)memmove(gd+(size_t)(dy+y)*dpitch+dx*4,gs+(size_t)(sy+y)*spitch+sx*4,(size_t)w*4);
    lean_d3d_forget(dst,dpitch*(dy+h));
}
static void kelvin(uint32_t m,uint32_t v){
    if(m<0x2000)K[m/4]=v;
    if((m>=0xb00&&m<0xc00)||(m>=0xa20&&m<=0xa2c)||(m>=0xaf0&&m<=0xafc)||m==0x1e9c||m==0x1ea0||m==0x1ea4||m==0x1e94){nf_vp_method(&vp,m,v);lean_d3d_vp_dirty();return;}
    switch(m){
    case 0x17fc:
        if(v){if(dr.active)end_draw();dr.active=1;dr.prim=v;dr.mode=0;dr.nidx=dr.ninl=dr.nimm=0;}
        else if(dr.active)end_draw();
        return;
    case 0x1800:if(dr.active){set_mode(1);push_idx(v&0xffff);push_idx(v>>16);}return;
    case 0x1808:if(dr.active){set_mode(1);push_idx(v);}return;
    case 0x1810:if(dr.active){set_mode(1);unsigned start=v&0xffffff,count=(v>>24)+1;for(unsigned i=0;i<count;i++)push_idx(start+i);}return;
    case 0x1818:if(dr.active){set_mode(2);push_inl(v);}return;
    case 0x1d94:clear_surface(v);return;
    case 0x1a4:sem_handle=v;return;
    case 0x1d6c:sem_offset=v;return;
    case 0x1d70:{
        uint32_t dst;
        if(!driving_semaphore143(NULL,rd,ramht,sem_handle,sem_offset,&dst))stop("semaphore",sem_handle,sem_offset);
        uint8_t *g=lean_guest(dst,4);if(!g)stop("semaphore address",dst,v);
        InterlockedExchange((volatile LONG*)g,(LONG)v);return;}
    case 0x100:
        {static unsigned long long t0;static unsigned n;if(!t0)t0=GetTickCount64();const char*e=getenv("LEAN_LOG_SW_AFTER_MS");
         if(e&&GetTickCount64()-t0>(unsigned long long)atoll(e)&&n<200){n++;fprintf(stderr,"[LEAN-SW] %llu 0x100 value=%08X selector=%u"+0,GetTickCount64()-t0,v,v&31);fputc(10,stderr);}}
        if(flip_mode<0){const char *e=getenv("LEAN_FLIP_MODE");flip_mode=e?atoi(e):1;fprintf(stderr,"[LEAN] flip mode %d (0 original queue, 1 direct 50 Hz, 2 direct unpaced)\n",flip_mode);}
        if(flip_mode&&v&&(v&31)==1){present_physical((v>>5)&~15u);if(flip_mode==1)pace50();return;}
        if(flip_mode&&v&&(v&31)!=1)return;
        if(driving_flip204_enabled()&&v&&(v&31)==1)present_physical((v>>5)&~15u);
        {uint64_t t0=qpc();driving_flip204_method(m,v);uint64_t e=qpc()-t0;tm.flipwait+=e;if(v&&(v&31)==1){tm.req+=e;tm.nreq++;}}return;
    case 0x130:{static unsigned n;const char*e=getenv("LEAN_LOG_SW_AFTER_MS");if(e&&n<50&&tm.frames>0){n++;fprintf(stderr,"[LEAN-SW] 0x130 value=%08X",v);fputc(10,stderr);}}
        if(flip_mode>0)return;{uint64_t t0=qpc();driving_flip204_method(m,v);uint64_t e=qpc()-t0;tm.flipwait+=e;tm.stall+=e;tm.nstall++;}return;
    }
    /* Immediate vertex data. */
    if(m>=0x1880&&m<0x1900){unsigned k=(m-0x1880)/8,c=((m-0x1880)/4)&1;float f;memcpy(&f,&v,4);if(c==0){cur[k][2]=0;cur[k][3]=1;}imm_write(k,c,f,c==1);return;}
    if(m>=0x1900&&m<0x1940){unsigned k=(m-0x1900)/4;cur[k][2]=0;cur[k][3]=1;cur[k][0]=(float)(int16_t)(v&0xffff);imm_write(k,1,(float)(int16_t)(v>>16),1);return;}
    if(m>=0x1940&&m<0x1980){unsigned k=(m-0x1940)/4;cur[k][0]=(v&255)/255.0f;cur[k][1]=((v>>8)&255)/255.0f;cur[k][2]=((v>>16)&255)/255.0f;imm_write(k,3,(v>>24)/255.0f,1);return;}
    if(m>=0x1980&&m<0x1a00){unsigned k=(m-0x1980)/8,c=((m-0x1980)/4)&1;
        if(c==0){cur[k][0]=(float)(int16_t)(v&0xffff);cur[k][1]=(float)(int16_t)(v>>16);}else{cur[k][2]=(float)(int16_t)(v&0xffff);imm_write(k,3,(float)(int16_t)(v>>16),1);}return;}
    if(m>=0x1a00&&m<0x1b00){unsigned k=(m-0x1a00)/16,c=((m-0x1a00)/4)&3;float f;memcpy(&f,&v,4);imm_write(k,c,f,c==3);return;}
    if(m>=0x1500&&m<0x1508){float f;memcpy(&f,&v,4);unsigned c=(m-0x1500)/4;if(c==0)cur[0][3]=1;imm_write(0,c,f,c==2);return;}
    if(m>=0x1518&&m<0x1528){float f;memcpy(&f,&v,4);unsigned c=(m-0x1518)/4;imm_write(0,c,f,c==3);return;}
    if(m==0x156c){cur[3][0]=((v>>16)&255)/255.0f;cur[3][1]=((v>>8)&255)/255.0f;cur[3][2]=(v&255)/255.0f;cur[3][3]=(v>>24)/255.0f;return;}
    if(m==0x158c){cur[4][0]=((v>>16)&255)/255.0f;cur[4][1]=((v>>8)&255)/255.0f;cur[4][2]=(v&255)/255.0f;cur[4][3]=(v>>24)/255.0f;return;}
    if(m>=0x1550&&m<0x1560){float f;memcpy(&f,&v,4);cur[3][(m-0x1550)/4]=f;return;}
    if(m>=0x1590&&m<0x1598){float f;memcpy(&f,&v,4);unsigned c=(m-0x1590)/4;cur[9][c]=f;if(c==1){cur[9][2]=0;cur[9][3]=1;}return;}
    if(m>=0x15a0&&m<0x15b0){float f;memcpy(&f,&v,4);cur[9][(m-0x15a0)/4]=f;return;}
    if(m>=0x15b8&&m<0x15c0){float f;memcpy(&f,&v,4);unsigned c=(m-0x15b8)/4;cur[10][c]=f;if(c==1){cur[10][2]=0;cur[10][3]=1;}return;}
    if(m>=0x15c8&&m<0x15d8){float f;memcpy(&f,&v,4);cur[10][(m-0x15c8)/4]=f;return;}
    /* State only: the backend reads K[] at draw time. */
    if(m<0x1d00||(m>=0x1b00&&m<0x1c00)||(m>=0x1d78&&m<0x1d94)||(m>=0x1d98&&m<0x1da0)||(m>=0x1e00&&m<0x1f00)||(m>=0x1720&&m<0x17a0))return;
    if(m==0x17d0||m==0x17c8||m==0x17cc){unhandled(m,v);return;}
    unhandled(m,v);
}
static int exec(void *ctx,uint32_t sub,uint32_t m,uint32_t v){
    (void)ctx;
    if(m==0){
        if(dr.active)end_draw();
        uint32_t instance,entry,cls;
        if(!driving_object143(NULL,rd,ramht,v,&instance,&entry)||(entry&0x30000u)!=0x10000u||!rd(NULL,instance,&cls))return 0;
        unsigned i;for(i=0;i<nobjs;i++)if(objs[i].instance==instance)break;
        if(i==nobjs){if(nobjs==64)stop("object capacity",instance,v);objs[i].instance=instance;objs[i].cls=cls&0xfff;nobjs++;}
        objs[i].handle=v;bound[sub]=i+1;return 1;
    }
    if(!bound[sub])stop("unbound subchannel",sub,m);
    Obj *o=&objs[bound[sub]-1];
    if(o->cls==0x97){kelvin(m,v);return 1;}
    if(m<0x400)o->state[m/4]=v;
    if(o->cls==0x9f&&m==0x308)blit(o);
    return 1;
}

/* --------------------------------------------------------------- drain */
static int enabled=-1;
static uint64_t drains;
void driving_gpu_drain143(void){
    if(enabled<0){enabled=getenv("DRIVING_PB_SYNC")!=NULL;
        if(enabled){extern void lean_sampler_start(void);lean_sampler_start();fprintf(stderr,"[LEAN] lean GPU-resident renderer active (D3D11 surfaces stay on the GPU)\n");if(!lean_d3d_init())stop("D3D11 init",0,0);}}
    if(!enabled)return;
    uint64_t d0=qpc();if(!tm.last)tm.last=d0;
    driving_scanout152_enter();
    ndma=0;lean_d3d_epoch();rd(NULL,0xfd002210,&ramht);
    uint32_t put;rd(NULL,0xfd800040,&put);
    uint64_t before=pb.words;
    if(!driving_pb143_consume(&pb,put,NULL,rd,exec))stop("command decode",pb.method,put);
    MemoryBarrier();
    *(volatile uint32_t*)((uintptr_t)xbox_GetMemoryOffset()+0xfd800044u)=pb.cursor;
    if(!(++drains%2048)){fprintf(stderr,"[LEAN] drains=%llu words=%llu draws=%llu refused=%llu\n",(unsigned long long)drains,(unsigned long long)pb.words,(unsigned long long)ndraws,(unsigned long long)nrefused);lean_d3d_report();}
    tm.drain+=qpc()-d0;
    driving_scanout155_leave(pb.initialized&&!pb.remaining&&!pb.return_address,pb.words!=before);
    driving_scanout152_report();
}

/* Original PCRTC scanout writes (from the display ISR or an immediate flip). */
static volatile LONG scanout_pending;static volatile LONG scanout_physical;
void lean_scanout(uint32_t physical){InterlockedExchange(&scanout_physical,(LONG)physical);InterlockedExchange(&scanout_pending,1);}
/* Interfaces kept for the unchanged kernel bridge and diagnostics. */
void driving_gpu_movie_phase180(void){}
void driving_pc451_kernel_memory(const void *native,uint32_t bytes,unsigned kind){(void)native;(void)bytes;(void)kind;}
void driving_pc451_probe(const char *where,unsigned code){(void)where;(void)code;}
void nv2a_pb_scan(uint32_t a,uint32_t b){(void)a;(void)b;}
void nv2a_pb_scan_report(void){}
void nv2a_pb_exec_method(uint32_t c,uint32_t m,uint32_t v){(void)c;(void)m;(void)v;}
void nv2a_pb_exec_report(void){}
