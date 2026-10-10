/* Lean Driving renderer: GPU-resident D3D11 backend.
 * Independently written for the lean experiment. Kelvin vertex programs and
 * register combiners are translated to HLSL; surfaces stay on the GPU. */
#define COBJMACROS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_5.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "lean_d3d.h"
#include "../gpu144/nightfire_vertex_program.h"
#include "d3d8_swizzle.h"

#define LOG(...) do{fprintf(stderr,__VA_ARGS__);}while(0)
static unsigned warn_budget=400;
#define WARN(...) do{if(warn_budget){warn_budget--;fprintf(stderr,"[LEAN] " __VA_ARGS__);}}while(0)

static ID3D11Device *dev;
static ID3D11DeviceContext *ctx;
static ID3D11DeviceContext1 *ctx1;
static int ready;

/* ------------------------------------------------------------------ hash */
static uint64_t hash_bytes(const void *p,size_t n,uint64_t h){
    const uint8_t *b=(const uint8_t*)p;h^=0x9E3779B97F4A7C15ull*(n+1);
    while(n>=8){uint64_t v;memcpy(&v,b,8);h=(h^v)*0x100000001B3ull;h^=h>>29;b+=8;n-=8;}
    while(n){h=(h^*b++)*0x100000001B3ull;n--;}
    h^=h>>31;h*=0xBF58476D1CE4E5B9ull;h^=h>>27;return h;
}
static uint64_t hash_words(const uint32_t *w,size_t n,uint64_t h){return hash_bytes(w,n*4,h);}

/* ------------------------------------------------------------------ stats */
static struct {uint64_t draws,prims,tex_uploads,tex_hits,tex_hash_bytes,vs_compiles,ps_compiles,clears,blits,readbacks,rt_creates,rt_uploads,refused;} st;

/* ------------------------------------------------------------- surfaces */
typedef struct RT {
    uint32_t addr;unsigned w,h,fmt,pitch,swz,depth;
    ID3D11Texture2D *tex;ID3D11RenderTargetView *rtv;ID3D11DepthStencilView *dsv;ID3D11ShaderResourceView *srv;
    uint64_t ram_hash;uint32_t ram_bytes;unsigned last;int gpu_written;   /* ram_bytes: span ram_hash covers */
    unsigned gen,synced,gen_frame;   /* GPU writes so far / gen last copied to guest RAM (lean_d3d_sync_guest) / frame of the last write */
    unsigned tw,th;float k;          /* stored size = w,h x k (internal resolution, NF_OVERLAY; k 1 = Xbox size) */
} RT;
#define MAX_RT 48
static RT rts[MAX_RT];
static unsigned rt_clock,epoch;

/* LEAN_INTERP: recorded frames keep raw pointers to textures, views and
 * surfaces, so while it is on their release waits 6 game frames (the
 * replay only uses the newest two) instead of taking a reference per draw. */
#define GRAVE_MAX 16384
static struct {IUnknown *u;unsigned frame;} grave[GRAVE_MAX];static unsigned grave_n,grave_frame;static int grave_on;
static void defer_release(void *obj){
    IUnknown *u=(IUnknown*)obj;if(!u)return;
    if(!grave_on||grave_n==GRAVE_MAX){u->lpVtbl->Release(u);return;}
    grave[grave_n].u=u;grave[grave_n].frame=grave_frame;grave_n++;
}
static void grave_tick(void){
    unsigned j=0;grave_frame++;
    for(unsigned i=0;i<grave_n;i++){if(grave_frame-grave[i].frame>=6)grave[i].u->lpVtbl->Release(grave[i].u);else grave[j++]=grave[i];}
    grave_n=j;
}
/* Internal resolution (NF_OVERLAY=1, Resolution in the F10 menu, lean_overlay.inc):
 * full-screen surfaces (linear, at least 640x448) are stored k times larger. The
 * guest still sees its own sizes (w,h): geometry is mapped through -1..1 and the
 * game samples its surfaces with normalised coordinates, so only the places that
 * speak in guest pixels scale (viewport, clears, copies, uploads, readbacks,
 * visibility counts, polygon offset slope). Without NF_OVERLAY, k is always 1. */
static int nfo_enabled(void);
static float rt_render_scale(void);
static unsigned rt_scaled(unsigned v,float k){return k==1.0f?v:(unsigned)floorf((float)v*k+0.5f);}
static float rt_scale_for(unsigned w,unsigned h,unsigned swz,float k){return (!swz&&w>=640&&h>=448)?k:1.0f;}
static void rt_stretch(ID3D11DeviceContext *c,int replay,ID3D11RenderTargetView *dst,D3D11_RECT r,ID3D11ShaderResourceView *src,float u0,float v0,float u1,float v1);
static ID3D11Texture2D *up_tex;static ID3D11ShaderResourceView *up_srv;static unsigned up_w,up_h;   /* guest-size upload staging for scaled surfaces */
static void rt_release(RT *r){
    defer_release(r->rtv);defer_release(r->dsv);defer_release(r->srv);defer_release(r->tex);
    memset(r,0,sizeof *r);
}
static uint64_t ram_sample_hash(uint32_t addr,uint32_t bytes){
    /* Sparse: 64 lines of 64 bytes spread over the span. */
    uint8_t *g=lean_guest(addr,bytes);if(!g)return 0;
    uint64_t h=0;uint32_t step=bytes/64;if(step<64)step=64;
    for(uint32_t o=0;o+64<=bytes;o+=step)h=hash_bytes(g+o,64,h);
    return h^bytes;
}
static void rt_upload_scaled(RT *r,const void *px,unsigned row){
    if(!up_tex||up_w!=r->w||up_h!=r->h){
        if(up_srv)ID3D11ShaderResourceView_Release(up_srv);if(up_tex)ID3D11Texture2D_Release(up_tex);up_srv=NULL;up_tex=NULL;
        D3D11_TEXTURE2D_DESC td={r->w,r->h,1,1,DXGI_FORMAT_B8G8R8A8_UNORM,{1,0},D3D11_USAGE_DEFAULT,D3D11_BIND_SHADER_RESOURCE,0,0};
        if(FAILED(ID3D11Device_CreateTexture2D(dev,&td,NULL,&up_tex)))return;
        if(FAILED(ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)up_tex,NULL,&up_srv))){ID3D11Texture2D_Release(up_tex);up_tex=NULL;return;}
        up_w=r->w;up_h=r->h;}
    ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)up_tex,0,NULL,px,row,0);
    D3D11_RECT all={0,0,(LONG)r->tw,(LONG)r->th};rt_stretch(ctx,0,r->rtv,all,up_srv,0,0,1,1);
}
static void rt_upload_color(RT *r){
    uint32_t bytes=r->pitch*r->h;uint8_t *g=lean_guest(r->addr,bytes);if(!g)return;
    if(r->tw!=r->w||r->th!=r->h){   /* scaled (never swizzled): the guest pixels stretched onto the larger surface */
        if(r->fmt==3){uint32_t *tmp=malloc((size_t)r->w*r->h*4);if(!tmp)return;
            for(unsigned y=0;y<r->h;y++){const uint16_t *s=(const uint16_t*)(g+(size_t)y*r->pitch);
                for(unsigned x=0;x<r->w;x++){uint32_t v=s[x];tmp[(size_t)y*r->w+x]=0xff000000u|(d3d8_expand_channel(v>>11,5)<<16)|(d3d8_expand_channel((v>>5)&63,6)<<8)|d3d8_expand_channel(v&31,5);}}
            rt_upload_scaled(r,tmp,r->w*4);free(tmp);}
        else if(r->pitch>=r->w*4)rt_upload_scaled(r,g,r->pitch);
        st.rt_uploads++;return;
    }
    if(r->swz){
        uint32_t *tmp=malloc((size_t)r->w*r->h*4);if(!tmp)return;
        xbox_unswizzle_rect(tmp,g,r->w,r->h,4);
        ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)r->tex,0,NULL,tmp,r->w*4,0);free(tmp);
    }else if(r->fmt==3){ /* R5G6B5 */
        uint32_t *tmp=malloc((size_t)r->w*r->h*4);if(!tmp)return;
        for(unsigned y=0;y<r->h;y++){const uint16_t *s=(const uint16_t*)(g+(size_t)y*r->pitch);
            for(unsigned x=0;x<r->w;x++){uint32_t v=s[x];tmp[(size_t)y*r->w+x]=0xff000000u|(d3d8_expand_channel(v>>11,5)<<16)|(d3d8_expand_channel((v>>5)&63,6)<<8)|d3d8_expand_channel(v&31,5);}}
        ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)r->tex,0,NULL,tmp,r->w*4,0);free(tmp);
    }else ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)r->tex,0,NULL,g,r->pitch,0);
    st.rt_uploads++;
}
/* LEAN_RT_TRACE=addr (hex, diagnostic): log why that surface is released
 * and how textures at its address are fed (first 40 of each). */
static int rt_trace(uint32_t addr,const char *what,unsigned a,unsigned b){
    static int init;static uint32_t want;static unsigned n[8];
    if(!init){const char *v=getenv("LEAN_RT_TRACE");init=1;want=v?(uint32_t)strtoul(v,NULL,16):0;}
    if(!want||addr!=want)return 0;
    unsigned k=(unsigned)(what[0]+what[1]+what[9])%8;if(n[k]++>=400)return 1;
    LOG("[LEAN-RT] %08X %s %u %u\n",addr,what,a,b);return 1;
}
static RT *rt_find(uint32_t addr,int depth){
    for(unsigned i=0;i<MAX_RT;i++)if(rts[i].tex&&rts[i].addr==addr&&rts[i].depth==(unsigned)depth)return &rts[i];
    return NULL;
}
static float rt_k_failed;   /* an internal resolution the GPU could not hold (drawn at Xbox size instead) */
static RT *rt_get(uint32_t addr,unsigned w,unsigned h,unsigned fmt,unsigned pitch,unsigned swz,int depth,float kwant){
    float k=rt_scale_for(w,h,swz,kwant);if(k==rt_k_failed)k=1;
    unsigned tw=rt_scaled(w,k),th=rt_scaled(h,k);if(tw>16384||th>16384){k=1;tw=w;th=h;}
    RT *r=rt_find(addr,depth);
    if(r&&(r->w!=w||r->h!=h||(!depth&&r->fmt!=fmt&&(r->fmt==3)!=(fmt==3)))){rt_trace(addr,"release:size/format",fmt,r->fmt);rt_release(r);r=NULL;}
    ID3D11ShaderResourceView *keep=NULL;   /* internal resolution changed: the picture so far, stretched onto the new surface */
    if(r&&(r->tw!=tw||r->th!=th)){
        if(!depth&&r->gpu_written&&r->srv){keep=r->srv;ID3D11ShaderResourceView_AddRef(keep);}
        rt_trace(addr,"release:scale",tw,r->tw);rt_release(r);r=NULL;}
    if(r){r->last=++rt_clock;if(!depth)r->fmt=fmt;r->pitch=pitch;return r;}
    /* A colour surface replacing older storage at an overlapping address. */
    unsigned slot=MAX_RT;
    for(unsigned i=0;i<MAX_RT;i++)if(!rts[i].tex){slot=i;break;}
    if(slot==MAX_RT){unsigned best=0;for(unsigned i=1;i<MAX_RT;i++)if(rts[i].last<rts[best].last)best=i;rt_trace(rts[best].addr,"release:evicted",0,0);rt_release(&rts[best]);slot=best;}
    r=&rts[slot];
    D3D11_TEXTURE2D_DESC td={0};td.Width=tw;td.Height=th;td.MipLevels=1;td.ArraySize=1;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;
    td.Format=depth?DXGI_FORMAT_R24G8_TYPELESS:DXGI_FORMAT_B8G8R8A8_UNORM;
    td.BindFlags=depth?D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE:D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr=ID3D11Device_CreateTexture2D(dev,&td,NULL,&r->tex);
    if(FAILED(hr)&&k!=1.0f){   /* no room at this resolution: Xbox size from now on (until another is chosen) */
        LOG("[LEAN] %s surface %ux%u at %.2fx failed %08lX; drawn at Xbox size\n",depth?"depth":"color",w,h,k,hr);
        rt_k_failed=k;k=1;tw=w;th=h;td.Width=w;td.Height=h;hr=ID3D11Device_CreateTexture2D(dev,&td,NULL,&r->tex);}
    if(FAILED(hr)){WARN("%s create %ux%u failed %08lX\n",depth?"depth":"color",tw,th,hr);memset(r,0,sizeof *r);if(keep)ID3D11ShaderResourceView_Release(keep);return NULL;}
    if(depth){
        D3D11_DEPTH_STENCIL_VIEW_DESC dv={0};dv.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;dv.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
        ID3D11Device_CreateDepthStencilView(dev,(ID3D11Resource*)r->tex,&dv,&r->dsv);
        D3D11_SHADER_RESOURCE_VIEW_DESC sv={0};sv.Format=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sv.Texture2D.MipLevels=1;
        ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)r->tex,&sv,&r->srv);
        ID3D11DeviceContext_ClearDepthStencilView(ctx,r->dsv,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1.0f,0);
    }else{
        ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)r->tex,NULL,&r->rtv);
        ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)r->tex,NULL,&r->srv);
    }
    r->addr=addr;r->w=w;r->h=h;r->fmt=fmt;r->pitch=pitch;r->swz=swz;r->depth=depth;r->last=++rt_clock;r->tw=tw;r->th=th;r->k=k;
    if(keep){D3D11_RECT all={0,0,(LONG)tw,(LONG)th};rt_stretch(ctx,0,r->rtv,all,keep,0,0,1,1);ID3D11ShaderResourceView_Release(keep);r->gpu_written=1;}
    else if(!depth){rt_upload_color(r);}
    r->ram_bytes=pitch*h;r->ram_hash=ram_sample_hash(addr,r->ram_bytes);
    st.rt_creates++;
    if(tw!=w||th!=h)LOG("[LEAN] surface %s %08X %ux%u fmt=%u pitch=%u swz=%u stored %ux%u\n",depth?"depth":"color",addr,w,h,fmt,pitch,swz,tw,th);
    else LOG("[LEAN] surface %s %08X %ux%u fmt=%u pitch=%u swz=%u\n",depth?"depth":"color",addr,w,h,fmt,pitch,swz);
    return r;
}
void lean_d3d_forget(uint32_t addr,uint32_t bytes){
    for(unsigned i=0;i<MAX_RT;i++)if(rts[i].tex&&rts[i].addr<addr+bytes&&addr<rts[i].addr+rts[i].pitch*rts[i].h){rt_trace(rts[i].addr,"release:forget(blit)",addr,bytes);rt_release(&rts[i]);}
}

/* ------------------------------------------------------------- textures */
enum {TK_NONE,TK_ARGB8,TK_XRGB8,TK_565,TK_1555,TK_X1555,TK_4444,TK_L8,TK_AL8,TK_A8,TK_A8L8,TK_I8,TK_DXT1,TK_DXT3,TK_DXT5,
      TK_YUY2,TK_UYVY,TK_ABGR8,TK_BGRA8_,TK_RGBA8,TK_G8B8,TK_R8B8,TK_D24,TK_D16};
typedef struct {uint8_t kind,bpp,swz;} Fmt;
static Fmt fmt_info(unsigned f){
    switch(f){
    case 0x00:return (Fmt){TK_L8,1,1};case 0x01:return (Fmt){TK_AL8,1,1};case 0x02:return (Fmt){TK_1555,2,1};
    case 0x03:return (Fmt){TK_X1555,2,1};case 0x04:return (Fmt){TK_4444,2,1};case 0x05:return (Fmt){TK_565,2,1};
    case 0x06:return (Fmt){TK_ARGB8,4,1};case 0x07:return (Fmt){TK_XRGB8,4,1};case 0x0B:return (Fmt){TK_I8,1,1};
    case 0x0C:return (Fmt){TK_DXT1,0,0};case 0x0E:return (Fmt){TK_DXT3,0,0};case 0x0F:return (Fmt){TK_DXT5,0,0};
    case 0x10:return (Fmt){TK_1555,2,0};case 0x11:return (Fmt){TK_565,2,0};case 0x12:return (Fmt){TK_ARGB8,4,0};
    case 0x13:return (Fmt){TK_L8,1,0};case 0x16:return (Fmt){TK_R8B8,2,0};case 0x17:return (Fmt){TK_G8B8,2,0};
    case 0x19:return (Fmt){TK_A8,1,1};case 0x1A:return (Fmt){TK_A8L8,2,1};case 0x1B:return (Fmt){TK_AL8,1,0};
    case 0x1C:return (Fmt){TK_X1555,2,0};case 0x1D:return (Fmt){TK_4444,2,0};case 0x1E:return (Fmt){TK_XRGB8,4,0};
    case 0x1F:return (Fmt){TK_A8,1,0};case 0x20:return (Fmt){TK_A8L8,2,0};case 0x24:return (Fmt){TK_YUY2,2,0};
    case 0x25:return (Fmt){TK_UYVY,2,0};case 0x28:return (Fmt){TK_G8B8,2,1};case 0x29:return (Fmt){TK_R8B8,2,1};
    case 0x2A:case 0x2B:return (Fmt){TK_D24,4,1};case 0x2C:case 0x2D:return (Fmt){TK_D16,2,1};
    case 0x2E:case 0x2F:return (Fmt){TK_D24,4,0};case 0x30:case 0x31:return (Fmt){TK_D16,2,0};
    case 0x3A:return (Fmt){TK_ABGR8,4,1};case 0x3B:return (Fmt){TK_BGRA8_,4,1};case 0x3C:return (Fmt){TK_RGBA8,4,1};
    case 0x3F:return (Fmt){TK_ABGR8,4,0};case 0x40:return (Fmt){TK_BGRA8_,4,0};case 0x41:return (Fmt){TK_RGBA8,4,0};
    }
    return (Fmt){TK_NONE,0,0};
}
static uint32_t x5(uint32_t v){return d3d8_expand_channel(v&31,5);}
static uint32_t x6(uint32_t v){return d3d8_expand_channel(v&63,6);}
static uint32_t x4(uint32_t v){return (v&15)*17;}
static uint32_t yuv_px(int y,int u,int v){
    int c=y-16,d=u-128,e=v-128;
    int r=(298*c+409*e+128)>>8,g=(298*c-100*d-208*e+128)>>8,b=(298*c+516*d+128)>>8;
    r=r<0?0:r>255?255:r;g=g<0?0:g>255?255:g;b=b<0?0:b>255?255:b;return 0xff000000u|(r<<16)|(g<<8)|b;
}
/* Convert one row of n texels to A8R8G8B8 (memory B,G,R,A). */
static void decode_row(uint32_t *o,const uint8_t *s,unsigned n,unsigned kind,const uint32_t *pal){
    switch(kind){
    case TK_ARGB8:memcpy(o,s,n*4);break;
    case TK_XRGB8:for(unsigned i=0;i<n;i++){uint32_t v;memcpy(&v,s+i*4,4);o[i]=v|0xff000000u;}break;
    case TK_565:for(unsigned i=0;i<n;i++){uint32_t v=s[i*2]|s[i*2+1]<<8;o[i]=0xff000000u|x5(v>>11)<<16|x6(v>>5)<<8|x5(v);}break;
    case TK_1555:for(unsigned i=0;i<n;i++){uint32_t v=s[i*2]|s[i*2+1]<<8;o[i]=(v&0x8000?0xff000000u:0)|x5(v>>10)<<16|x5(v>>5)<<8|x5(v);}break;
    case TK_X1555:for(unsigned i=0;i<n;i++){uint32_t v=s[i*2]|s[i*2+1]<<8;o[i]=0xff000000u|x5(v>>10)<<16|x5(v>>5)<<8|x5(v);}break;
    case TK_4444:for(unsigned i=0;i<n;i++){uint32_t v=s[i*2]|s[i*2+1]<<8;o[i]=x4(v>>12)<<24|x4(v>>8)<<16|x4(v>>4)<<8|x4(v);}break;
    case TK_L8:for(unsigned i=0;i<n;i++){uint32_t l=s[i];o[i]=0xff000000u|l<<16|l<<8|l;}break;
    case TK_AL8:for(unsigned i=0;i<n;i++){uint32_t l=s[i];o[i]=l<<24|l<<16|l<<8|l;}break;
    case TK_A8:for(unsigned i=0;i<n;i++){o[i]=(uint32_t)s[i]<<24|0x00ffffffu;}break;
    case TK_A8L8:for(unsigned i=0;i<n;i++){uint32_t l=s[i*2],a=s[i*2+1];o[i]=a<<24|l<<16|l<<8|l;}break;
    case TK_I8:for(unsigned i=0;i<n;i++)o[i]=pal?pal[s[i]]:0xffff00ffu;break;
    case TK_ABGR8:for(unsigned i=0;i<n;i++){const uint8_t *p=s+i*4;o[i]=(uint32_t)p[3]<<24|(uint32_t)p[0]<<16|(uint32_t)p[1]<<8|p[2];}break;
    case TK_BGRA8_:for(unsigned i=0;i<n;i++){const uint8_t *p=s+i*4;o[i]=(uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}break;
    case TK_RGBA8:for(unsigned i=0;i<n;i++){const uint8_t *p=s+i*4;o[i]=(uint32_t)p[0]<<24|(uint32_t)p[3]<<16|(uint32_t)p[2]<<8|p[1];}break;
    case TK_G8B8:for(unsigned i=0;i<n;i++){o[i]=0xff000000u|(uint32_t)s[i*2+1]<<8|s[i*2];}break;
    case TK_R8B8:for(unsigned i=0;i<n;i++){o[i]=0xff000000u|(uint32_t)s[i*2+1]<<16|s[i*2];}break;
    case TK_YUY2:for(unsigned i=0;i+1<n;i+=2){const uint8_t *p=s+i*2;o[i]=yuv_px(p[0],p[1],p[3]);o[i+1]=yuv_px(p[2],p[1],p[3]);}break;
    case TK_UYVY:for(unsigned i=0;i+1<n;i+=2){const uint8_t *p=s+i*2;o[i]=yuv_px(p[1],p[0],p[2]);o[i+1]=yuv_px(p[3],p[0],p[2]);}break;
    case TK_D24:for(unsigned i=0;i<n;i++){uint32_t v;memcpy(&v,s+i*4,4);uint32_t d=v>>24;o[i]=0xff000000u|d<<16|d<<8|d;}break;
    case TK_D16:for(unsigned i=0;i<n;i++){uint32_t d=s[i*2+1];o[i]=0xff000000u|d<<16|d<<8|d;}break;
    default:for(unsigned i=0;i<n;i++)o[i]=0xffff00ffu;break;
    }
}
typedef struct TexE {
    uint64_t key,hash;uint32_t addr;unsigned w,h,levels,cube,epoch,last;size_t bytes;
    ID3D11Texture2D *tex;ID3D11ShaderResourceView *srv;
} TexE;
#define TEX_SLOTS 4096
static TexE texs[TEX_SLOTS];
static unsigned tex_count,tex_clock;
static size_t tex_memory;
static void tex_release(TexE *t){
    defer_release(t->srv);defer_release(t->tex);
    if(t->bytes&&tex_memory>=t->bytes)tex_memory-=t->bytes;
    memset(t,0,sizeof *t);tex_count--;
}
static unsigned level_bytes(Fmt f,unsigned fmtcode,unsigned w,unsigned h){
    if(f.kind==TK_DXT1||f.kind==TK_DXT3||f.kind==TK_DXT5){unsigned bw=(w+3)/4,bh=(h+3)/4;return bw*bh*d3d8_format_dxt_block_bytes(fmtcode);}
    return w*h*f.bpp;
}
static uint32_t *scratch;static size_t scratch_size;
static uint32_t *scratch_get(size_t pixels){if(pixels>scratch_size){free(scratch);scratch=malloc(pixels*4);scratch_size=scratch?pixels:0;}return scratch;}
/* Decode the guest texture into a fresh D3D texture. */
static int tex_build(TexE *t,const uint8_t *g,Fmt f,unsigned fmtcode,unsigned w,unsigned h,unsigned levels,unsigned cube,unsigned pitch,const uint32_t *pal){
    int bc=(f.kind==TK_DXT1||f.kind==TK_DXT3||f.kind==TK_DXT5)&&!(w&3)&&!(h&3);
    unsigned faces=cube?6:1;
    D3D11_TEXTURE2D_DESC td={0};td.Width=w;td.Height=h;td.MipLevels=levels;td.ArraySize=faces;td.SampleDesc.Count=1;
    td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;td.MiscFlags=cube?D3D11_RESOURCE_MISC_TEXTURECUBE:0;
    td.Format=bc?(f.kind==TK_DXT1?DXGI_FORMAT_BC1_UNORM:f.kind==TK_DXT3?DXGI_FORMAT_BC2_UNORM:DXGI_FORMAT_BC3_UNORM):DXGI_FORMAT_B8G8R8A8_UNORM;
    D3D11_SUBRESOURCE_DATA init[6*16];
    uint32_t *buf=scratch_get((size_t)w*h*2*faces+64);if(!buf)return 0;
    size_t out=0;const uint8_t *src=g;
    for(unsigned face=0;face<faces;face++){
        const uint8_t *fs=src;
        for(unsigned l=0;l<levels;l++){
            unsigned lw=w>>l?w>>l:1,lh=h>>l?h>>l:1;unsigned bytes=level_bytes(f,fmtcode,lw,lh);
            D3D11_SUBRESOURCE_DATA *sd=&init[face*levels+l];
            if(bc){sd->pSysMem=fs;sd->SysMemPitch=((lw+3)/4)*d3d8_format_dxt_block_bytes(fmtcode);sd->SysMemSlicePitch=0;}
            else{
                uint32_t *o=buf+out;
                if(f.kind==TK_DXT1||f.kind==TK_DXT3||f.kind==TK_DXT5){
                    for(unsigned y=0;y<lh;y++)for(unsigned x=0;x<lw;x++){uint32_t c=0;d3d8_dxt_decode_texel(fs,fmtcode,x,y,lw,&c);o[y*lw+x]=c;}
                }else if(f.swz){
                    uint8_t *lin=malloc((size_t)lw*lh*f.bpp);if(!lin)return 0;
                    xbox_unswizzle_rect(lin,fs,lw,lh,f.bpp);
                    for(unsigned y=0;y<lh;y++)decode_row(o+(size_t)y*lw,lin+(size_t)y*lw*f.bpp,lw,f.kind,pal);
                    free(lin);
                }else{
                    for(unsigned y=0;y<lh;y++)decode_row(o+(size_t)y*lw,fs+(size_t)y*pitch,lw,f.kind,pal);
                }
                sd->pSysMem=o;sd->SysMemPitch=lw*4;sd->SysMemSlicePitch=0;out+=(size_t)lw*lh;
            }
            fs+=f.swz||bc||f.kind==TK_DXT1||f.kind==TK_DXT3||f.kind==TK_DXT5?bytes:pitch*lh;
        }
        size_t face_bytes=(size_t)(fs-src);src+=cube?((face_bytes+127)&~(size_t)127):face_bytes;
    }
    HRESULT hr=ID3D11Device_CreateTexture2D(dev,&td,init,&t->tex);
    if(FAILED(hr)){WARN("texture create %ux%u fmt %02X levels %u failed %08lX\n",w,h,fmtcode,levels,hr);return 0;}
    D3D11_SHADER_RESOURCE_VIEW_DESC sv={0};sv.Format=td.Format;
    if(cube){sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURECUBE;sv.TextureCube.MipLevels=levels;}
    else{sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sv.Texture2D.MipLevels=levels;}
    ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)t->tex,&sv,&t->srv);
    t->w=w;t->h=h;t->levels=levels;t->cube=cube;
    return 1;
}
typedef struct TexInfo {unsigned w,h,levels,cube,linear,depth,kind;ID3D11ShaderResourceView *srv;unsigned tw,th;} TexInfo;   /* tw,th: stored size of a scaled render target (0: as w,h) */
static ID3D11ShaderResourceView *magenta_srv;static ID3D11ShaderResourceView *white_srv;   /* LEAN_RT_TEX_WHITE */static unsigned lean_frame_no;   /* defined with LEAN_PICK below */
/* Resolve a stage texture to an SRV. Returns 0 if unusable. */
static int tex_lookup(const uint32_t *K,unsigned stage,uint32_t addr,uint32_t paladdr,TexInfo *ti,ID3D11RenderTargetView *bound_rtv){
    uint32_t format=K[(0x1b04+stage*64)/4],ctl1=K[(0x1b10+stage*64)/4],rect=K[(0x1b1c+stage*64)/4];
    unsigned fmtcode=(format>>8)&255,cube=(format>>2)&1,levels=(format>>16)&15;
    unsigned lu=(format>>20)&15,lv=(format>>24)&15;
    Fmt f=fmt_info(fmtcode);memset(ti,0,sizeof *ti);
    if(f.kind==TK_NONE){WARN("unsupported texture format %02X (stage %u)\n",fmtcode,stage);ti->srv=magenta_srv;ti->w=ti->h=1;return 1;}
    int linear=!f.swz&&f.kind!=TK_DXT1&&f.kind!=TK_DXT3&&f.kind!=TK_DXT5;
    unsigned w,h,pitch=0;
    if(linear){w=rect>>16;h=rect&0xffff;pitch=ctl1>>16;levels=1;cube=0;if(!w||!h||!pitch)return 0;}
    else{w=1u<<lu;h=1u<<lv;if(!levels)levels=1;
        unsigned maxl=1;{unsigned m=w>h?w:h;while(m>1){m>>=1;maxl++;}}if(levels>maxl)levels=maxl;}
    ti->w=w;ti->h=h;ti->levels=levels;ti->cube=cube;ti->linear=linear;ti->kind=f.kind;ti->depth=f.kind==TK_D24||f.kind==TK_D16;
    {static int init;static uint32_t tw,twl[16];static unsigned ntw;if(!init){const char *v=getenv("LEAN_TEX_WHITE");tw=v?(uint32_t)strtoul(v,NULL,16):0;while(v&&*v&&ntw<16){twl[ntw++]=(uint32_t)strtoul(v,(char**)&v,16);if(*v==',')v++;else break;}init=1;}
     for(unsigned q=0;q<ntw;q++)if(twl[q]==addr&&white_srv&&twl[q]>3){ti->srv=white_srv;ti->w=ti->h=1;return 1;}   /* diagnostic; LEAN_TEX_WHITE=1/2/3: every texture / stages 1-3 / stage 0 of AA scene draws */
     if(tw&&white_srv&&(addr==tw||(tw<=3&&((K[0x208/4]>>12)&15)&&(tw==1||(tw==2&&stage>0)||(tw==3&&stage==0))))){ti->srv=white_srv;ti->w=ti->h=1;return 1;}}
    /* Render-target alias: the title samples a surface it rendered. */
    if(!cube){
        RT *r=rt_find(addr,ti->depth);
        if(!r&&ti->depth)r=rt_find(addr,0);
        if(r)rt_trace(addr,r->gpu_written?"texture:rt-written":"texture:rt-not-written",fmtcode,linear);
        else rt_trace(addr,"texture:no-rt",fmtcode,linear);
        if(r&&r->gpu_written){
            /* Compare over the span the stored hash was taken on. The surface can be
             * re-bound with another pitch (e.g. the Paris drive light map), and hashing a
             * different span read as "the CPU replaced it": the GPU-rendered surface was
             * dropped and its untouched guest memory (black) sampled instead.
             * LEAN_RT_HASH_FIXED_RANGE=0 restores the old comparison. */
            static int fixed=-1;if(fixed<0){const char *v=getenv("LEAN_RT_HASH_FIXED_RANGE");fixed=!(v&&v[0]=='0');}
            uint64_t hh=ram_sample_hash(r->addr,fixed&&r->ram_bytes?r->ram_bytes:r->pitch*r->h);
            if(hh==r->ram_hash){
                if(r->rtv&&r->rtv==bound_rtv){WARN("texture samples the bound target %08X\n",addr);return 0;}
                {   /* LEAN_DUMP_RT=addr: save that rendered surface (as sampled) at its
                     * 100th, 1000th and 3000th use. LEAN_RT_TEX_WHITE=addr: sample white
                     * instead (diagnostic A/B for what the surface contributes). */
                    static int init;static uint32_t dwant,wwant;static unsigned uses,dfrom,dframe,dlast,dn;   /* LEAN_DUMP_RT_FRAME=n: 6 dumps, every 50 frames from frame n */   /* LEAN_DUMP_RT_FROM=n: also every 500 uses from n */
                    if(!init){const char *v0=getenv("LEAN_DUMP_RT_FRAME");dframe=v0?(unsigned)atoi(v0):0;const char *v=getenv("LEAN_DUMP_RT_FROM");dfrom=v?(unsigned)atoi(v):0;v=getenv("LEAN_DUMP_RT");dwant=v?(uint32_t)strtoul(v,NULL,16):0;v=getenv("LEAN_RT_TEX_WHITE");wwant=v?(uint32_t)strtoul(v,NULL,16):0;init=1;}
                    if(wwant&&addr==wwant&&white_srv){ti->srv=white_srv;return 1;}
                    if(dwant&&addr==dwant&&!r->depth&&(++uses==100||uses==1000||uses==3000||(dfrom&&uses>=dfrom&&uses<dfrom+3000&&!((uses-dfrom)%500))||(dframe&&dn<6&&lean_frame_no>=dframe&&lean_frame_no>=dlast+50&&(dlast=lean_frame_no,++dn)))){
                        const char *dir=getenv("DRIVING_CAPTURE_DIR");D3D11_TEXTURE2D_DESC sd;ID3D11Texture2D_GetDesc(r->tex,&sd);
                        sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;sd.MiscFlags=0;ID3D11Texture2D *stg=NULL;
                        if(dir&&SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&sd,NULL,&stg))){D3D11_MAPPED_SUBRESOURCE m;
                            ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)stg,(ID3D11Resource*)r->tex);
                            if(SUCCEEDED(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)stg,0,D3D11_MAP_READ,0,&m))){
                                char path[MAX_PATH];snprintf(path,sizeof path,"%s/rt-%08X-use%u.bmp",dir,addr,uses);FILE *fo=fopen(path,"wb");
                                if(fo){unsigned char hd[54]={0};uint32_t W=sd.Width,H=sd.Height,size=54+W*H*4,off=54,dib=40;int32_t ht=-(int32_t)H;uint16_t pl=1,bits=32;
                                    memcpy(hd,"BM",2);memcpy(hd+2,&size,4);memcpy(hd+10,&off,4);memcpy(hd+14,&dib,4);memcpy(hd+18,&W,4);memcpy(hd+22,&ht,4);memcpy(hd+26,&pl,2);memcpy(hd+28,&bits,2);
                                    fwrite(hd,1,54,fo);for(uint32_t y=0;y<H;y++)fwrite((uint8_t*)m.pData+y*m.RowPitch,4,W,fo);fclose(fo);}
                                ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)stg,0);LOG("[LEAN-RT] dumped %08X use %u\n",addr,uses);}
                            ID3D11Texture2D_Release(stg);}}
                }
                ti->srv=r->srv;if(r->tw!=r->w||r->th!=r->h){ti->tw=r->tw;ti->th=r->th;}
                /* Linear coordinates address the guest storage; our storage has the same texel grid
                 * (a scaled surface: the same picture on a finer grid, sampled through normalised coordinates). */
                return 1;
            }
            /* CPU replaced the memory: the surface no longer describes it. */
            {static unsigned nd;if(nd++<60)LOG("[LEAN-RT-DISCARD] %08X %ux%u fmt=%u span stored=%u now=%u pitch=%u (texture fmt %02X)\n",r->addr,r->w,r->h,r->fmt,r->ram_bytes,r->pitch*r->h,r->pitch,fmtcode);}   /* audit: CPU-overwrite discards */
            if(rt_trace(addr,"release:ram-hash-changed",fmtcode,linear)){   /* show what the sampled guest lines now hold */
                uint32_t bytes=r->pitch*r->h,step=bytes/64;if(step<64)step=64;uint8_t *g=lean_guest(r->addr,bytes);unsigned nz=0;
                for(uint32_t o=0;g&&o+64<=bytes;o+=step){unsigned any=0;for(unsigned k=0;k<64;k++)any|=g[o+k];
                    if(any&&nz++<4)LOG("[LEAN-RT]   line +%06X: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n",o,
                        g[o],g[o+1],g[o+2],g[o+3],g[o+4],g[o+5],g[o+6],g[o+7],g[o+8],g[o+9],g[o+10],g[o+11],g[o+12],g[o+13],g[o+14],g[o+15]);}
                LOG("[LEAN-RT]   %u of 64 sampled lines non-zero; frame %u\n",nz,lean_frame_no);}
            rt_release(r);
        }
    }
    unsigned faces=cube?6:1;size_t total=0;
    if(linear)total=(size_t)pitch*h;
    else{for(unsigned l=0;l<levels;l++){unsigned lw=w>>l?w>>l:1,lh=h>>l?h>>l:1;total+=level_bytes(f,fmtcode,lw,lh);}if(cube)total=((total+127)&~(size_t)127)*faces;}
    uint8_t *g=lean_guest(addr,(uint32_t)total);if(!g){WARN("texture %08X bytes %zu outside guest RAM\n",addr,total);return 0;}
    uint32_t pal[256];int haspal=f.kind==TK_I8;unsigned palcount=256;
    if(haspal){
        uint32_t pf=K[(0x1b20+stage*64)/4];palcount=256>>((pf>>2)&3);
        uint8_t *pg=lean_guest(paladdr,palcount*4);if(!pg){WARN("palette %08X outside guest RAM\n",paladdr);return 0;}
        memcpy(pal,pg,palcount*4);for(unsigned i=palcount;i<256;i++)pal[i]=0;
    }
    uint64_t key=hash_words((uint32_t[]){addr,format,ctl1,linear?rect:0,haspal?paladdr:0},5,0x1234);
    unsigned slot=(unsigned)(key%TEX_SLOTS),i;TexE *t=NULL,*empty=NULL;
    for(i=0;i<32;i++){TexE *c=&texs[(slot+i)%TEX_SLOTS];if(c->tex&&c->key==key){t=c;break;}if(!c->tex&&!empty)empty=c;}
    if(t&&t->epoch==epoch){t->last=++tex_clock;st.tex_hits++;ti->srv=t->srv;return 1;}
    uint64_t hh=hash_bytes(g,total,haspal?hash_bytes(pal,palcount*4,0):0);st.tex_hash_bytes+=total;
    if(t&&t->hash==hh){t->epoch=epoch;t->last=++tex_clock;st.tex_hits++;ti->srv=t->srv;return 1;}
    if(t){tex_release(t);empty=t;}
    if(!empty){/* evict the stalest in the probe window */
        TexE *old=&texs[slot];for(i=1;i<32;i++){TexE *c=&texs[(slot+i)%TEX_SLOTS];if(c->last<old->last)old=c;}tex_release(old);empty=old;}
    t=empty;memset(t,0,sizeof *t);
    if(!tex_build(t,g,f,fmtcode,w,h,levels,cube,pitch,haspal?pal:NULL)){memset(t,0,sizeof *t);return 0;}
    {static unsigned seen[256];if(!seen[fmtcode]++||(getenv("LEAN_LOG_TEXTURES")&&seen[fmtcode]<40))
        LOG("[LEAN] texture fmt=%02X %ux%u levels=%u cube=%u linear=%d stage=%u addr=%08X format=%08X filter=%08X address=%08X\n",fmtcode,w,h,levels,cube,linear,stage,addr,format,K[(0x1b14+stage*64)/4],K[(0x1b08+stage*64)/4]);}
    {   /* LEAN_DUMP_TEX=addr[,addr...] (hex): save mip 0 of those textures as
         * BMP into DRIVING_CAPTURE_DIR once each (BGRA textures only). */
        static int parsed;static uint32_t want[8];static unsigned nwant,done[8];
        if(!parsed){parsed=1;const char *v=getenv("LEAN_DUMP_TEX");while(v&&*v&&nwant<8){want[nwant++]=(uint32_t)strtoul(v,(char**)&v,16);if(*v==',')v++;}}
        for(unsigned k=0;k<nwant;k++)if(want[k]==addr&&!done[k]){
            done[k]=1;const char *dir=getenv("DRIVING_CAPTURE_DIR");
            D3D11_TEXTURE2D_DESC sd;ID3D11Texture2D_GetDesc(t->tex,&sd);sd.MipLevels=1;sd.ArraySize=1;sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;sd.MiscFlags=0;
            ID3D11Texture2D *stg=NULL;D3D11_MAPPED_SUBRESOURCE m;
            if(dir&&sd.Format!=DXGI_FORMAT_B8G8R8A8_UNORM){   /* compressed: raw guest bytes */
                char path[MAX_PATH];snprintf(path,sizeof path,"%s/tex-%08X-%ux%u-fmt%02X.raw",dir,addr,w,h,fmtcode);
                FILE *fo=fopen(path,"wb");if(fo){fwrite(g,1,total,fo);fclose(fo);LOG("[LEAN] dumped raw texture %08X fmt %02X to %s\n",addr,fmtcode,path);}
            }
            if(dir&&sd.Format==DXGI_FORMAT_B8G8R8A8_UNORM&&SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&sd,NULL,&stg))){
                ID3D11DeviceContext_CopySubresourceRegion(ctx,(ID3D11Resource*)stg,0,0,0,0,(ID3D11Resource*)t->tex,0,NULL);
                if(SUCCEEDED(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)stg,0,D3D11_MAP_READ,0,&m))){
                    char path[MAX_PATH];snprintf(path,sizeof path,"%s/tex-%08X-%ux%u.bmp",dir,addr,sd.Width,sd.Height);
                    FILE *fo=fopen(path,"wb");
                    if(fo){uint32_t W=sd.Width,H=sd.Height,img=W*H*4;uint8_t hd[54]={'B','M'};
                        *(uint32_t*)(hd+2)=54+img;*(uint32_t*)(hd+10)=54;*(uint32_t*)(hd+14)=40;*(int32_t*)(hd+18)=(int32_t)W;*(int32_t*)(hd+22)=-(int32_t)H;
                        *(uint16_t*)(hd+26)=1;*(uint16_t*)(hd+28)=32;*(uint32_t*)(hd+34)=img;fwrite(hd,1,54,fo);
                        for(uint32_t y=0;y<H;y++)fwrite((uint8_t*)m.pData+(size_t)y*m.RowPitch,1,W*4,fo);fclose(fo);}
                    ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)stg,0);LOG("[LEAN] dumped texture %08X to %s\n",addr,path);
                }
                ID3D11Texture2D_Release(stg);
            }
        }
    }
    t->key=key;t->hash=hh;t->addr=addr;t->epoch=epoch;t->last=++tex_clock;t->bytes=total;tex_count++;tex_memory+=total;
    st.tex_uploads++;ti->srv=t->srv;return 1;
}

/* -------------------------------------------------------- state caches */
typedef struct {uint64_t key;IUnknown *obj;} Cached;
typedef struct {Cached e[512];unsigned n;} Cache;
static IUnknown *cache_get(Cache *c,uint64_t key){for(unsigned i=0;i<c->n;i++)if(c->e[i].key==key)return c->e[i].obj;return NULL;}
static void cache_put(Cache *c,uint64_t key,IUnknown *o){if(c->n<512){c->e[c->n].key=key;c->e[c->n].obj=o;c->n++;}}
static Cache blend_cache,depth_cache,raster_cache,sampler_cache;

static D3D11_BLEND blend_factor(uint32_t v,int alpha){
    switch(v){
    case 0:return D3D11_BLEND_ZERO;case 1:return D3D11_BLEND_ONE;
    case 0x300:return alpha?D3D11_BLEND_SRC_ALPHA:D3D11_BLEND_SRC_COLOR;case 0x301:return alpha?D3D11_BLEND_INV_SRC_ALPHA:D3D11_BLEND_INV_SRC_COLOR;
    case 0x302:return D3D11_BLEND_SRC_ALPHA;case 0x303:return D3D11_BLEND_INV_SRC_ALPHA;
    case 0x304:return D3D11_BLEND_DEST_ALPHA;case 0x305:return D3D11_BLEND_INV_DEST_ALPHA;
    case 0x306:return alpha?D3D11_BLEND_DEST_ALPHA:D3D11_BLEND_DEST_COLOR;case 0x307:return alpha?D3D11_BLEND_INV_DEST_ALPHA:D3D11_BLEND_INV_DEST_COLOR;
    case 0x308:return alpha?D3D11_BLEND_ONE:D3D11_BLEND_SRC_ALPHA_SAT;
    case 0x8001:case 0x8003:return D3D11_BLEND_BLEND_FACTOR;case 0x8002:case 0x8004:return D3D11_BLEND_INV_BLEND_FACTOR;
    }
    return D3D11_BLEND_ONE;
}
static D3D11_BLEND_OP blend_op(uint32_t v){
    switch(v){case 0x8007:return D3D11_BLEND_OP_MIN;case 0x8008:return D3D11_BLEND_OP_MAX;case 0x800a:return D3D11_BLEND_OP_SUBTRACT;case 0x800b:return D3D11_BLEND_OP_REV_SUBTRACT;}
    return D3D11_BLEND_OP_ADD;
}
static ID3D11BlendState *blend_state(int enable,uint32_t src,uint32_t dst,uint32_t eq,unsigned mask){
    uint64_t key=enable?((uint64_t)src<<40|(uint64_t)dst<<20|eq)^((uint64_t)mask<<60):((uint64_t)mask<<60)|1;
    ID3D11BlendState *b=(ID3D11BlendState*)cache_get(&blend_cache,key);if(b)return b;
    D3D11_BLEND_DESC d={0};d.RenderTarget[0].BlendEnable=enable;
    d.RenderTarget[0].SrcBlend=blend_factor(src,0);d.RenderTarget[0].DestBlend=blend_factor(dst,0);d.RenderTarget[0].BlendOp=blend_op(eq);
    d.RenderTarget[0].SrcBlendAlpha=blend_factor(src,1);d.RenderTarget[0].DestBlendAlpha=blend_factor(dst,1);d.RenderTarget[0].BlendOpAlpha=blend_op(eq);
    d.RenderTarget[0].RenderTargetWriteMask=(UINT8)mask;
    if(FAILED(ID3D11Device_CreateBlendState(dev,&d,&b)))return NULL;cache_put(&blend_cache,key,(IUnknown*)b);return b;
}
static D3D11_COMPARISON_FUNC cmp_func(uint32_t v){return (D3D11_COMPARISON_FUNC)((v&7)+1);}
static D3D11_STENCIL_OP stencil_op(uint32_t v){
    switch(v){case 0x1e00:return D3D11_STENCIL_OP_KEEP;case 0:return D3D11_STENCIL_OP_ZERO;case 0x1e01:return D3D11_STENCIL_OP_REPLACE;
    case 0x1e02:return D3D11_STENCIL_OP_INCR_SAT;case 0x1e03:return D3D11_STENCIL_OP_DECR_SAT;case 0x150a:return D3D11_STENCIL_OP_INVERT;
    case 0x8507:return D3D11_STENCIL_OP_INCR;case 0x8508:return D3D11_STENCIL_OP_DECR;}
    return D3D11_STENCIL_OP_KEEP;
}
static ID3D11DepthStencilState *depth_state(const uint32_t *K,int has_depth){
    int dt=has_depth&&K[0x30c/4],dw=has_depth&&K[0x35c/4]&&dt,se=has_depth&&K[0x32c/4];
    uint32_t words[10]={dt,dw,K[0x354/4],se,K[0x360/4]&255,K[0x364/4],K[0x36c/4]&255,K[0x370/4],K[0x374/4],K[0x378/4]};
    if(!se)words[4]=words[5]=words[6]=words[7]=words[8]=words[9]=0;
    if(!dt)words[2]=0;
    uint64_t key=hash_words(words,10,77);
    ID3D11DepthStencilState *s=(ID3D11DepthStencilState*)cache_get(&depth_cache,key);if(s)return s;
    D3D11_DEPTH_STENCIL_DESC d={0};d.DepthEnable=dt||dw;d.DepthWriteMask=dw?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
    d.DepthFunc=dt?cmp_func(words[2]):D3D11_COMPARISON_ALWAYS;
    d.StencilEnable=se;d.StencilWriteMask=(UINT8)words[4];d.StencilReadMask=(UINT8)words[6];
    d.FrontFace.StencilFunc=se?cmp_func(words[5]):D3D11_COMPARISON_ALWAYS;d.FrontFace.StencilFailOp=stencil_op(words[7]);
    d.FrontFace.StencilDepthFailOp=stencil_op(words[8]);d.FrontFace.StencilPassOp=stencil_op(words[9]);d.BackFace=d.FrontFace;
    if(FAILED(ID3D11Device_CreateDepthStencilState(dev,&d,&s)))return NULL;cache_put(&depth_cache,key,(IUnknown*)s);return s;
}
static int no_cull=-1,flip_winding=-1;
static int depth_bias_on=-1;
static struct {uint64_t by_topo[4],offset,point_params,point_smooth,additive;uint32_t point_size;} cen;
/* 1: fragments outside the depth range are culled (see raster_state). */
static int depth_clip_mode(const uint32_t *K,unsigned zunit){
    static int depth_clip_on=-1;if(depth_clip_on<0){const char*v=getenv("LEAN_DEPTH_CLIP");depth_clip_on=!(v&&*v=='0');}
    uint32_t zc=K[0x1d78/4];float clip_max;memcpy(&clip_max,&K[0x398/4],4);
    return depth_clip_on&&(zc&1)&&!(zc&0x10)&&clip_max<=(zunit==256?65535.5f:16777216.0f);
}
static float rs_slope_k=1;   /* internal resolution of the bound target (bind_target) */
static ID3D11RasterizerState *raster_state(const uint32_t *K,int scissor,int topo,unsigned zunit){
    if(no_cull<0){const char*v=getenv("LEAN_NO_CULL");no_cull=v&&*v=='1';v=getenv("LEAN_FLIP_WINDING");flip_winding=v&&*v=='1';}
    if(depth_bias_on<0){const char*v=getenv("LEAN_DEPTH_BIAS");depth_bias_on=!(v&&*v=='0');}
    D3D11_CULL_MODE cull=D3D11_CULL_NONE;
    if(!no_cull&&K[0x308/4]){uint32_t f=K[0x39c/4];cull=f==0x404?D3D11_CULL_FRONT:f==0x405?D3D11_CULL_BACK:D3D11_CULL_NONE;}
    int ccw=(K[0x3a0/4]==0x901)^flip_winding;
    int fill=K[0x38c/4]==0x1b01?1:0;
    /* Polygon offset (NV097 0x330/0x334/0x338 point/line/fill enable, 0x384
     * slope factor, 0x388 bias in Z-buffer units). Decals such as light pools
     * on the road rely on it to win the depth test against coplanar surfaces.
     * The bias unit is one step of the guest Z buffer, so a Z16 step is 256
     * D24 steps. LEAN_DEPTH_BIAS=0 ignores it as before. */
    int ofs=depth_bias_on&&K[(topo==2?0x330:topo==1?0x334:0x338)/4];
    float zfactor=0,zbias=0;if(ofs){memcpy(&zfactor,&K[0x384/4],4);memcpy(&zbias,&K[0x388/4],4);}
    INT dbias=ofs?(INT)lrintf(zbias*(float)zunit):0;
    /* Near/far handling (NV097 0x1D78 ZMIN_MAX_CONTROL = D3DRS_DEPTHCLIPCONTROL):
     * bit 0 culls fragments outside the depth range, bit 4 clamps them instead.
     * With cull, D3D11 depth clipping drops them as the chip does; clamping
     * them (the old behaviour) let geometry just behind the car's reflection
     * camera paint over a whole reflection view. D3D11 clips to the target's
     * full depth range, so a cull range reaching past it (0x398 clip max above
     * the Z buffer's maximum) keeps the clamp. LEAN_DEPTH_CLIP=0: always clamp. */
    uint32_t zc=K[0x1d78/4];
    int dclip=depth_clip_mode(K,zunit);
    {static uint32_t seen[16][3];static unsigned ns;uint32_t c0=K[0x394/4],c1=K[0x398/4];unsigned i;
     uint32_t zk=zc^(zunit<<16);for(i=0;i<ns&&!(seen[i][0]==zk&&seen[i][1]==c0&&seen[i][2]==c1);i++){}
     if(i==ns&&ns<16){seen[ns][0]=zk;seen[ns][1]=c0;seen[ns][2]=c1;ns++;float f0,f1;memcpy(&f0,&c0,4);memcpy(&f1,&c1,4);
         LOG("[LEAN] depth range control %08X clip %g..%g (zunit %u) -> %s\n",zc,f0,f1,zunit,dclip?"clip":"clamp");}}
    uint64_t key=(uint64_t)cull|(uint64_t)ccw<<4|(uint64_t)scissor<<5|(uint64_t)fill<<6|(uint64_t)dclip<<7|(uint64_t)(uint32_t)dbias<<8|(uint64_t)(ofs?K[0x384/4]:0)<<40;
    key^=(uint64_t)(ofs?K[0x384/4]:0)*0x9E3779B97F4A7C15ull;
    if(ofs&&rs_slope_k!=1.0f){uint32_t kb;memcpy(&kb,&rs_slope_k,4);key^=((uint64_t)kb+1)*0xC2B2AE3D27D4EB4Full;}
    ID3D11RasterizerState *r=(ID3D11RasterizerState*)cache_get(&raster_cache,key);if(r)return r;
    D3D11_RASTERIZER_DESC d={0};d.FillMode=fill?D3D11_FILL_WIREFRAME:D3D11_FILL_SOLID;d.CullMode=cull;d.FrontCounterClockwise=ccw;d.DepthClipEnable=dclip;d.ScissorEnable=scissor;
    d.DepthBias=dbias;d.SlopeScaledDepthBias=ofs?zfactor*rs_slope_k:0;d.DepthBiasClamp=0;
    if(ofs&&(zfactor!=0||zbias!=0)){static unsigned n;if(n++<16)LOG("[LEAN] polygon offset factor=%g bias=%g -> DepthBias=%d\n",zfactor,zbias,dbias);}
    if(FAILED(ID3D11Device_CreateRasterizerState(dev,&d,&r)))return NULL;cache_put(&raster_cache,key,(IUnknown*)r);return r;
}
static D3D11_TEXTURE_ADDRESS_MODE addr_mode(unsigned v){
    switch(v){case 1:return D3D11_TEXTURE_ADDRESS_WRAP;case 2:return D3D11_TEXTURE_ADDRESS_MIRROR;case 3:return D3D11_TEXTURE_ADDRESS_CLAMP;case 4:return D3D11_TEXTURE_ADDRESS_BORDER;case 5:return D3D11_TEXTURE_ADDRESS_CLAMP;}
    return D3D11_TEXTURE_ADDRESS_WRAP;
}
static ID3D11SamplerState *sampler_state(const uint32_t *K,unsigned stage,const TexInfo *ti){
    uint32_t filter=K[(0x1b14+stage*64)/4],address=K[(0x1b08+stage*64)/4],border=K[(0x1b24+stage*64)/4],ctl0=K[(0x1b0c+stage*64)/4];
    unsigned mn=(filter>>16)&255,mg=(filter>>24)&15,aniso=1u<<((ctl0>>4)&3);
    if(ti->linear){address=(address&~0xf0f0fu)|0x30303u;} /* rect textures clamp */
    /* NV097 TEXTURE_FILTER bits 0-12: MIPMAP_LOD_BIAS, signed, 1/256 units (D3D
     * MIPMAPLODBIAS plus the device's supersample bias; -1.0 in the Driving scene).
     * LEAN_LOD_BIAS=0 ignores it (old behaviour). */
    static int lodb=-1;if(lodb<0){const char *v=getenv("LEAN_LOD_BIAS");lodb=!(v&&*v=='0');}
    int bias=lodb?(int)((filter&0x1fffu)^0x1000u)-0x1000:0;
    uint64_t key=hash_words((uint32_t[]){mn,mg,address,border,aniso,ti->levels>1,(uint32_t)bias},7,99);
    ID3D11SamplerState *s=(ID3D11SamplerState*)cache_get(&sampler_cache,key);if(s)return s;
    D3D11_SAMPLER_DESC d={0};
    int minlin=mn==2||mn==4||mn==6||mn==7,maglin=mg==2||mg==4,miplin=mn==5||mn==6,mip=mn>=3&&mn<=6;
    if(aniso>1&&minlin)d.Filter=D3D11_FILTER_ANISOTROPIC;
    else d.Filter=(D3D11_FILTER)((minlin?0x10:0)|(maglin?0x4:0)|(miplin?0x1:0));
    d.AddressU=addr_mode(address&15);d.AddressV=addr_mode((address>>8)&15);d.AddressW=addr_mode((address>>16)&15);
    d.MaxAnisotropy=aniso;d.ComparisonFunc=D3D11_COMPARISON_NEVER;
    d.BorderColor[0]=((border>>16)&255)/255.0f;d.BorderColor[1]=((border>>8)&255)/255.0f;d.BorderColor[2]=(border&255)/255.0f;d.BorderColor[3]=(border>>24)/255.0f;
    d.MinLOD=0;d.MaxLOD=mip?D3D11_FLOAT32_MAX:0;d.MipLODBias=mip?bias/256.0f:0.0f;
    if(FAILED(ID3D11Device_CreateSamplerState(dev,&d,&s)))return NULL;cache_put(&sampler_cache,key,(IUnknown*)s);return s;
}

/* -------------------------------------------------------------- shaders */
typedef struct {char *t;size_t n,cap;int bad;} Text;
static void tx(Text *b,const char *fmt,...){
    if(b->bad)return;va_list a;
    for(;;){va_start(a,fmt);int n=vsnprintf(b->t+b->n,b->cap-b->n,fmt,a);va_end(a);
        if(n<0){b->bad=1;return;}
        if((size_t)n<b->cap-b->n){b->n+=n;return;}
        size_t nc=b->cap*2+n+1024;char *p=realloc(b->t,nc);if(!p){b->bad=1;return;}b->t=p;b->cap=nc;}
}
static void text_init(Text *b){b->cap=16384;b->n=0;b->bad=0;b->t=malloc(b->cap);if(!b->t)b->bad=1;else b->t[0]=0;}

/* cmask: constant rows 0..191 the program reads (all of them if it indexes with
 * a0); smooth mode blends only those. */
typedef struct VSE {uint64_t key;ID3D11VertexShader *vs;ID3D11InputLayout *il;unsigned mask;uint32_t cmask[6];int prows[4];} VSE;
typedef struct PSE {uint64_t key;ID3D11PixelShader *ps;} PSE;
#define SH_SLOTS 4096
static VSE vss[SH_SLOTS];static PSE pss[SH_SLOTS];

static ID3DBlob *compile(const char *src,size_t n,const char *profile,const char *name){
    ID3DBlob *code=NULL,*err=NULL;
    HRESULT hr=D3DCompile(src,n,name,NULL,NULL,"main",profile,D3DCOMPILE_OPTIMIZATION_LEVEL1,0,&code,&err);
    if(FAILED(hr)){
        WARN("%s compile failed: %s\n",name,err?(const char*)ID3D10Blob_GetBufferPointer(err):"?");
        static int dumped;if(dumped++<4){fprintf(stderr,"----- source -----\n%.*s\n------------------\n",(int)n,src);}
        if(err)ID3D10Blob_Release(err);return NULL;
    }
    if(err)ID3D10Blob_Release(err);return code;
}
static void vp_src(Text *b,unsigned packed,unsigned vi,unsigned ci,unsigned indexed){
    unsigned mux=packed&3,reg=(packed>>2)&15,swz=(packed>>6)&255;char sw[5];
    for(unsigned k=0;k<4;k++)sw[k]="xyzw"[(swz>>(6-2*k))&3];sw[4]=0;
    const char *neg=packed&0x4000?"-":"";
    if(mux==1)tx(b,"%sr[%u].%s",neg,reg>12?12:reg,sw);
    else if(mux==2)tx(b,"%sv.a%u.%s",neg,vi,sw);
    else if(mux==3){if(indexed)tx(b,"%skc[clamp(%u+a0,0,191)].%s",neg,ci,sw);else tx(b,"%skc[%u].%s",neg,ci<192?ci:191,sw);}
    else tx(b,"float4(0,0,0,0)");
}
static void vp_mask(Text *b,const char *dst,const char *src,unsigned mask){
    if(!mask)return;char sw[5];unsigned n=0;for(unsigned k=0;k<4;k++)if(mask&(8u>>k))sw[n++]="xyzw"[k];sw[n]=0;
    tx(b,"%s.%s=%s.%s;",dst,sw,src,sw);
}
static unsigned vp_input_mask(const NFVertexProgram *s){
    unsigned m=0;
    for(unsigned pc=s->start;pc<136;pc++){
        const uint32_t *w=s->code[pc];unsigned mac=nf_vp_field(w,85,4),ilu=nf_vp_field(w,89,3),vi=nf_vp_field(w,73,4);
        unsigned srcs[3]={nf_vp_field(w,58,15),nf_vp_field(w,43,15),nf_vp_field(w,28,15)};
        int use[3]={mac!=0,mac==2||mac==4||(mac>=5&&mac<=12),mac==3||mac==4||ilu!=0};
        for(unsigned i=0;i<3;i++)if(use[i]&&(srcs[i]&3)==2)m|=1u<<vi;
        if(w[3]&1)break;
    }
    return m|1u; /* position always present */
}
static void vp_const_mask(const NFVertexProgram *s,uint32_t cm[6]){
    memset(cm,0,6*4);
    for(unsigned pc=s->start;pc<136;pc++){
        const uint32_t *w=s->code[pc];
        unsigned mac=nf_vp_field(w,85,4),ilu=nf_vp_field(w,89,3),ci=nf_vp_field(w,77,8),indexed=nf_vp_field(w,1,1);
        unsigned srcs[3]={nf_vp_field(w,58,15),nf_vp_field(w,43,15),nf_vp_field(w,28,15)};
        int use[3]={mac!=0,mac==2||mac==4||(mac>=5&&mac<=12),mac==3||mac==4||ilu!=0};
        for(unsigned i=0;i<3;i++)if(use[i]&&(srcs[i]&3)==3){
            if(indexed){memset(cm,0xff,6*4);return;}
            unsigned r=ci<192?ci:191;cm[r>>5]|=1u<<(r&31);}
        if(w[3]&1)break;
    }
}
/* Smooth mode: the constant rows of the program's position transform. The first
 * DP4 with a constant that writes each component of the position output (o[0] or
 * its r12 alias) names that component's row of the 4x4 transform; -1 if any is
 * missing or indexed. Smooth mode blends these four rows as a whole transform
 * (lean_interp.inc, interp_rotate). */
static void vp_pos_rows(const NFVertexProgram *s,int rows[4]){
    rows[0]=rows[1]=rows[2]=rows[3]=-1;
    for(unsigned pc=s->start;pc<136;pc++){
        const uint32_t *w=s->code[pc];
        unsigned mac=nf_vp_field(w,85,4),ci=nf_vp_field(w,77,8),dst=nf_vp_field(w,20,4),mm=nf_vp_field(w,24,4);
        unsigned om=nf_vp_field(w,12,4),output=nf_vp_field(w,3,8),outreg=nf_vp_field(w,11,1),scalar=nf_vp_field(w,2,1),indexed=nf_vp_field(w,1,1);
        unsigned wr=0;
        if(mac==7){
            if(mm&&dst==12)wr|=mm;
            if(om&&outreg&&output==0&&!scalar)wr|=om;
            if(wr){
                int cA=(nf_vp_field(w,58,15)&3)==3,cB=(nf_vp_field(w,43,15)&3)==3;
                for(unsigned k=0;k<4;k++)if((wr&(8u>>k))&&rows[k]<0)rows[k]=(cA||cB)&&!indexed&&ci<192?(int)ci:-2;
            }
        }else{   /* anything else writing the position before its DP4 */
            if(mac&&mm&&dst==12)wr|=mm;
            if(om&&outreg&&output==0)wr|=om;
            for(unsigned k=0;k<4;k++)if((wr&(8u>>k))&&rows[k]==-1)rows[k]=-2;
        }
        if(w[3]&1)break;
    }
    for(unsigned k=0;k<4;k++)if(rows[k]<0){rows[0]=rows[1]=rows[2]=rows[3]=-1;return;}
}
static int vp_hlsl(Text *b,const NFVertexProgram *s,unsigned mask){
    tx(b,"cbuffer VC:register(b0){float4 kc[192];float4 surf;float4 fogp;float4 pt;};\n");
    tx(b,"struct VI{");for(unsigned i=0;i<16;i++)if(mask&(1u<<i))tx(b,"float4 a%u:TEXCOORD%u;",i,i);tx(b,"};\n");
    tx(b,"struct VO{float4 p:SV_Position;float4 d0:COLOR0;float4 d1:COLOR1;float4 t0:TEXCOORD0;float4 t1:TEXCOORD1;float4 t2:TEXCOORD2;float4 t3:TEXCOORD3;float3 fog:TEXCOORD4;float ps:TEXCOORD5;};\n");
    tx(b,"VO main(VI v){precise float4 r[13];precise float4 o[16];int a0=0;[unroll]for(int i=0;i<13;i++)r[i]=0;[unroll]for(int j=0;j<16;j++)o[j]=float4(0,0,0,1);o[1]=0;o[2]=0;r[12]=float4(0,0,0,1);\n");
    int ended=0;
    for(unsigned pc=s->start;pc<136;pc++){
        if(s->valid[pc]!=15){WARN("vertex program slot %u not loaded\n",pc);return 0;}
        const uint32_t *w=s->code[pc];
        unsigned mac=nf_vp_field(w,85,4),ilu=nf_vp_field(w,89,3),ci=nf_vp_field(w,77,8),vi=nf_vp_field(w,73,4);
        unsigned dst=nf_vp_field(w,20,4),mm=nf_vp_field(w,24,4),im=nf_vp_field(w,16,4),om=nf_vp_field(w,12,4);
        unsigned indexed=nf_vp_field(w,1,1),output=nf_vp_field(w,3,8),outreg=nf_vp_field(w,11,1),scalar=nf_vp_field(w,2,1);
        tx(b,"{precise float4 A=");if(mac)vp_src(b,nf_vp_field(w,58,15),vi,ci,indexed);else tx(b,"0");
        tx(b,",B=");if(mac==2||mac==4||(mac>=5&&mac<=12))vp_src(b,nf_vp_field(w,43,15),vi,ci,indexed);else tx(b,"0");
        tx(b,",C=");if(mac==3||mac==4||ilu)vp_src(b,nf_vp_field(w,28,15),vi,ci,indexed);else tx(b,"0");
        tx(b,",M=0,I=0;");
        switch(mac){
        case 0:break;case 1:tx(b,"M=A;");break;case 2:tx(b,"M=A*B;");break;case 3:tx(b,"M=A+C;");break;case 4:tx(b,"M=A*B+C;");break;
        case 5:tx(b,"M=dot(A.xyz,B.xyz).xxxx;");break;case 6:tx(b,"M=(dot(A.xyz,B.xyz)+B.w).xxxx;");break;case 7:tx(b,"M=dot(A,B).xxxx;");break;
        case 8:tx(b,"M=float4(1,A.y*B.y,A.z,B.w);");break;case 9:tx(b,"M=min(A,B);");break;case 10:tx(b,"M=max(A,B);");break;
        case 11:tx(b,"M=float4(A<B);");break;case 12:tx(b,"M=float4(A>=B);");break;case 13:tx(b,"a0=(int)floor(A.x);");break;
        default:WARN("vertex program MAC %u unsupported\n",mac);return 0;
        }
        switch(ilu){
        case 0:break;case 1:tx(b,"I=C;");break;case 2:tx(b,"I=(1.0/C.x).xxxx;");break;
        case 3:tx(b,"{float q=1.0/C.x;float m=clamp(abs(q),5.42101e-20,1.84467e19);I=(q<0?-m:m).xxxx;}");break;
        case 4:tx(b,"I=(1.0/sqrt(abs(C.x))).xxxx;");break;
        case 5:tx(b,"I=float4(exp2(floor(C.x)),C.x-floor(C.x),exp2(C.x),1);");break;
        case 6:tx(b,"{float l=log2(abs(C.x));float f=floor(l);I=float4(f,abs(C.x)/exp2(f),l,1);}");break;
        case 7:tx(b,"I=float4(1,max(C.x,0),C.x>0?pow(max(C.y,1e-30),clamp(C.w,-127.9961,127.9961)):0,1);");break;
        }
        if(mac&&mac!=13&&mm&&!(ilu&&dst==1)){char d[16];snprintf(d,sizeof d,"r[%u]",dst>12?12:dst);vp_mask(b,d,"M",mm);if(dst==12)vp_mask(b,"o[0]","M",mm);}
        if(ilu&&im){unsigned reg=mac?1:dst;char d[16];snprintf(d,sizeof d,"r[%u]",reg>12?12:reg);vp_mask(b,d,"I",im);if(reg==12)vp_mask(b,"o[0]","I",im);}
        if(om&&outreg&&output<16){char d[16];snprintf(d,sizeof d,"o[%u]",output);vp_mask(b,d,scalar?"I":"M",om);if(!output)vp_mask(b,"r[12]",scalar?"I":"M",om);}
        tx(b,"}\n");
        if(w[3]&1){ended=1;break;}
    }
    if(!ended){WARN("vertex program has no end\n");return 0;}
    /* surf.w=1: W-buffer (CONTROL0 Z_PERSPECTIVE, see wbuffer_mode): depth is the
     * position's w, carried perspective-correct in fog.y for the pixel shader; the
     * vertex z (used for clipping and as the conservative depth bound) is w too.
     * fog.z: the clip-range minimum in depth units (or -2: clamp, no cull). */
    tx(b,"VO q;float4 P=o[0];q.p=float4((P.x*surf.x-1)*P.w,(1-P.y*surf.y)*P.w,(surf.w>0.5?P.w:P.z)*surf.z*P.w,P.w);\n"
         "q.d0=saturate(o[3]);q.d1=saturate(o[4]);q.t0=o[9];q.t1=o[10];q.t2=o[11];q.t3=o[12];\n"
         "float d=o[5].x;if(fogp.y>0)d=abs(d);float f=1;\n"
         "if(fogp.x==1)f=d*fogp.w+fogp.z-1;else if(fogp.x==2)f=exp2(d*16*fogp.w)+fogp.z-1.5;else if(fogp.x==3){float e=d*16*fogp.w;f=exp2(-e*e)+fogp.z-1.5;}\n"
         "q.fog=float3(f,P.w*surf.z,pt.w);q.ps=o[6].x;return q;}\n");
    return !b->bad;
}
static VSE *vs_get(const NFVertexProgram *s){
    unsigned end=s->start;while(end<136&&!(s->code[end][3]&1))end++;
    if(end>=136)end=135;
    uint64_t key=hash_words(&s->code[s->start][0],(size_t)(end-s->start+1)*4,s->start);
    unsigned slot=(unsigned)(key%SH_SLOTS);
    for(unsigned i=0;i<64;i++){VSE *e=&vss[(slot+i)%SH_SLOTS];if(e->vs&&e->key==key)return e;if(!e->vs){
        unsigned mask=vp_input_mask(s);Text b;text_init(&b);
        if(!vp_hlsl(&b,s,mask)){free(b.t);return NULL;}{static int dump=-1;if(dump<0){const char *v=getenv("LEAN_DUMP_VS");dump=v&&*v=='1';}   /* LEAN_DUMP_VS=1: save each new vertex program */const char *dir=getenv("DRIVING_CAPTURE_DIR");if(dump&&dir){char path[MAX_PATH];snprintf(path,sizeof path,"%s/vs-%016llX.hlsl",dir,(unsigned long long)key);FILE *f=fopen(path,"wb");if(f){fwrite(b.t,1,b.n,f);fclose(f);}}}
        ID3DBlob *code=compile(b.t,b.n,"vs_5_0","lean-vs");free(b.t);if(!code)return NULL;
        if(FAILED(ID3D11Device_CreateVertexShader(dev,ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),NULL,&e->vs))){ID3D10Blob_Release(code);return NULL;}
        D3D11_INPUT_ELEMENT_DESC el[16];unsigned n=0;
        for(unsigned k=0;k<16;k++)if(mask&(1u<<k)){el[n].SemanticName="TEXCOORD";el[n].SemanticIndex=k;el[n].Format=DXGI_FORMAT_R32G32B32A32_FLOAT;el[n].InputSlot=0;el[n].AlignedByteOffset=n*16;el[n].InputSlotClass=D3D11_INPUT_PER_VERTEX_DATA;el[n].InstanceDataStepRate=0;n++;}
        HRESULT hr=ID3D11Device_CreateInputLayout(dev,el,n,ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),&e->il);
        ID3D10Blob_Release(code);if(FAILED(hr)){ID3D11VertexShader_Release(e->vs);e->vs=NULL;return NULL;}
        e->key=key;e->mask=mask;vp_const_mask(s,e->cmask);vp_pos_rows(s,e->prows);st.vs_compiles++;
        LOG("[LEAN] vertex program %016llX: position rows %d %d %d %d\n",(unsigned long long)key,e->prows[0],e->prows[1],e->prows[2],e->prows[3]);return e;}}
    return NULL;
}
unsigned lean_d3d_vp_mask(const void *vp){VSE *e=vs_get((const NFVertexProgram*)vp);return e?e->mask:0;}

/* Pixel stage. Register numbering follows the Kelvin combiner inputs. */
static void cin(char *out,size_t cap,unsigned byte,int alpha_unit){
    unsigned reg=byte&15,sel=(byte>>4)&1,map=byte>>5;char x[48];
    if(alpha_unit)snprintf(x,sizeof x,"r[%u].%s",reg,sel?"a":"b");
    else snprintf(x,sizeof x,"r[%u].%s",reg,sel?"aaa":"rgb");
    switch(map){
    case 0:snprintf(out,cap,"max(%s,0)",x);break;case 1:snprintf(out,cap,"(1-saturate(%s))",x);break;
    case 2:snprintf(out,cap,"(2*max(%s,0)-1)",x);break;case 3:snprintf(out,cap,"(1-2*max(%s,0))",x);break;
    case 4:snprintf(out,cap,"(max(%s,0)-0.5)",x);break;case 5:snprintf(out,cap,"(0.5-max(%s,0))",x);break;
    case 6:snprintf(out,cap,"(%s)",x);break;default:snprintf(out,cap,"(-%s)",x);break;
    }
}
static const char *op_expr(unsigned op){
    switch(op){case 1:return "(X-0.5)";case 2:return "(X*2)";case 3:return "((X-0.5)*2)";case 4:return "(X*4)";case 6:return "(X*0.5)";}
    return "(X)";
}
static void emit_op(Text *b,const char *var,unsigned op){
    const char *e=op_expr(op);char buf[64];size_t j=0;for(const char *p=e;*p&&j<60;p++){if(*p=='X'){size_t l=strlen(var);memcpy(buf+j,var,l);j+=l;}else buf[j++]=*p;}buf[j]=0;
    tx(b,"%s=clamp(%s,-1,1);",var,buf);
}
static void fin(char *out,size_t cap,unsigned byte,int alpha){
    unsigned reg=byte&15,sel=(byte>>4)&1,inv=(byte>>5)&1;char x[48];
    if(alpha)snprintf(x,sizeof x,"r[%u].%s",reg,sel?"a":"b");else snprintf(x,sizeof x,"r[%u].%s",reg,sel?"aaa":"rgb");
    if(inv)snprintf(out,cap,"(1-saturate(%s))",x);else snprintf(out,cap,"saturate(%s)",x);
}
/* W-buffer (2026-10-08, exchange-shadows). NV097 SET_CONTROL0 (0x290) bit 16
 * Z_PERSPECTIVE is D3DRS_ZENABLE = D3DZB_USEW: the chip stores the position's w
 * (the title's projection-viewport transform scales it to depth units, and the
 * clip range 0x394..0x398 is in those units), interpolated perspective-correct,
 * not z/w. The Action title (The Exchange) draws with CONTROL0 00110001 and a
 * near/far ratio of about 3e-6, so z/w is ~1 everywhere and its float rounding
 * differs between vertex programs: coplanar passes drawn with LESSEQUAL and no
 * depth write (NPC shadow receivers, the painting lamps' light, decals) passed
 * or failed in triangle-shaped patches that changed with the camera. With W the
 * pixel shader writes w / max as the depth (SV_DepthLessEqual: the screen-linear
 * vertex w bounds the true w from above), culls outside the clip range when
 * ZMIN_MAX_CONTROL culls, and applies polygon offset itself. Driving does not
 * use W (CONTROL0 00100001). LEAN_W_BUFFER=0: z/w as before. */
static int ps_wdepth;                          /* 0 z/w, 1 W (LessEqual), 2 W with positive offset (SV_Depth) */
static float ps_wbias_units,ps_wbias_slope;    /* polygon offset in depth units 0..1 (W only) */
static int wbuffer_on(void){static int on=-1;if(on<0){const char *v=getenv("LEAN_W_BUFFER");on=!(v&&*v=='0');}return on;}
typedef struct PSKey {uint32_t w[64];unsigned n;} PSKey;
static void pskey_add(PSKey *k,uint32_t v){if(k->n<64)k->w[k->n++]=v;}
static int ps_hlsl(Text *b,const uint32_t *K,const unsigned *stage_kind){
    uint32_t ctl=K[0x1e60/4];unsigned count=ctl&255;if(!count)count=1;if(count>8)count=8;
    unsigned mux_msb=(ctl>>8)&15,each0=(ctl>>12)&15,each1=(ctl>>16)&15;
    uint32_t prog=K[0x1e70/4];
    tx(b,"cbuffer PC:register(b0){float4 f0[8];float4 f1[8];float4 ff0;float4 ff1;float4 fogc;float4 aref;float4 tdim[4];};\n");
    for(unsigned i=0;i<4;i++){unsigned mode=(prog>>(5*i))&31;
        if(mode==3)tx(b,"TextureCube tx%u:register(t%u);",i,i);else tx(b,"Texture2D tx%u:register(t%u);",i,i);
        tx(b,"SamplerState sm%u:register(s%u);\n",i,i);}
    tx(b,"struct VO{float4 p:SV_Position;float4 d0:COLOR0;float4 d1:COLOR1;float4 t0:TEXCOORD0;float4 t1:TEXCOORD1;float4 t2:TEXCOORD2;float4 t3:TEXCOORD3;float3 fog:TEXCOORD4;};\n");
    if(ps_wdepth){   /* W-buffer depth (see wbuffer_mode): w, perspective-correct, in depth units */
        tx(b,"float4 main(VO p,out float od:%s):SV_Target{float4 r[16];[unroll]for(int i=0;i<16;i++)r[i]=0;\n",ps_wdepth==2?"SV_Depth":"SV_DepthLessEqual");
        tx(b,"{float wd=p.fog.y;");
        if(ps_wbias_units!=0||ps_wbias_slope!=0)tx(b,"wd+=%.9g+%.9g*max(abs(ddx(p.fog.y)),abs(ddy(p.fog.y)));",ps_wbias_units,ps_wbias_slope);
        tx(b,"if(p.fog.z>-1&&(wd<p.fog.z||wd>1))discard;od=saturate(wd);}\n");
    }else tx(b,"float4 main(VO p):SV_Target{float4 r[16];[unroll]for(int i=0;i<16;i++)r[i]=0;\n");
    {static int nofog=-1,flat=-1;if(nofog<0){const char*v=getenv("LEAN_NO_FOG");nofog=v&&*v=='1';v=getenv("LEAN_WHITE_DIFFUSE");flat=v&&*v=='1';}
     tx(b,nofog?"r[3]=float4(fogc.rgb,1);":"r[3]=float4(fogc.rgb,saturate(p.fog.x));");tx(b,flat?"r[4]=1;r[5]=p.d1;\n":"r[4]=p.d0;r[5]=p.d1;\n");}
    for(unsigned i=0;i<4;i++){
        unsigned mode=(prog>>(5*i))&31;uint32_t ctl0=K[(0x1b0c+i*64)/4];int enabled=(ctl0>>30)&1;
        const char *tc=i==0?"p.t0":i==1?"p.t1":i==2?"p.t2":"p.t3";
        if(mode==0)continue;
        if(!enabled&&mode!=4){tx(b,"r[%u]=0;\n",8+i);continue;}
        switch(mode){
        case 1:case 2:
            if(stage_kind[i]&6){
                /* NV097 TEXTURE_FILTER MIN=CONVOLUTION_2D_LOD0 (7) / MAG=CONVOLUTION_2D_LOD0 (4):
                 * a 3x3 texel kernel at LOD 0. The title's swap (AA resolve) of its 2x
                 * supersampled scene uses it (D3DRS_SWAPFILTER QUINCUNX/GAUSSIANCUBIC);
                 * plain bilinear there leaves sub-pixel detail as hard specks.
                 * Kernel 1 QUINCUNX: centre 1/2 + four diagonals 1/8. Kernel 2 GAUSSIAN_3:
                 * [1 2 1;2 4 2;1 2 1]/16. */
                tx(b,"{float2 uv=%s.xy/(%s.w==0?1:%s.w);float2 o=tdim[%u].xy;\n",tc,tc,tc,i);
                if(stage_kind[i]&1)tx(b,"uv*=o;\n");   /* rect texture: texel coordinates */
                if(nfo_enabled())tx(b,"if(any(tdim[%u].zw))o=tdim[%u].zw;\n",i,i);   /* scaled render target: taps one stored texel apart */
                if(stage_kind[i]&2)tx(b,"r[%u]=0.5*tx%u.Sample(sm%u,uv)+0.125*(tx%u.Sample(sm%u,uv+o*float2(-1,-1))+tx%u.Sample(sm%u,uv+o*float2(1,-1))+tx%u.Sample(sm%u,uv+o*float2(-1,1))+tx%u.Sample(sm%u,uv+o*float2(1,1)));}\n",
                    8+i,i,i,i,i,i,i,i,i,i,i);
                else tx(b,"float4 s=0;[unroll]for(int y=-1;y<=1;y++)[unroll]for(int x=-1;x<=1;x++)s+=((2-abs(x))*(2-abs(y))/16.0)*tx%u.Sample(sm%u,uv+o*float2(x,y));r[%u]=s;}\n",i,i,8+i);
            }
            else if(stage_kind[i]&1)tx(b,"r[%u]=tx%u.Sample(sm%u,(%s.xy/(%s.w==0?1:%s.w))*tdim[%u].xy);\n",8+i,i,i,tc,tc,tc,i);
            else tx(b,"r[%u]=tx%u.Sample(sm%u,%s.xy/(%s.w==0?1:%s.w));\n",8+i,i,i,tc,tc,tc);
            break;
        case 3:tx(b,"r[%u]=tx%u.Sample(sm%u,%s.xyz);\n",8+i,i,i,tc);break;
        case 4:tx(b,"r[%u]=saturate(%s);\n",8+i,tc);break;
        case 5:tx(b,"if(any(%s<0))discard;\n",tc);break;
        case 6:case 7:
            /* Bump environment map approximated without the du/dv offset. */
            WARN("bump-env stage %u approximated without offset\n",i);
            if(stage_kind[i]&1)tx(b,"r[%u]=tx%u.Sample(sm%u,(%s.xy/(%s.w==0?1:%s.w))*tdim[%u].xy);\n",8+i,i,i,tc,tc,tc,i);
            else tx(b,"r[%u]=tx%u.Sample(sm%u,%s.xy/(%s.w==0?1:%s.w));\n",8+i,i,i,tc,tc,tc);
            break;
        default:
            WARN("texture stage %u mode %u approximated as plain lookup\n",i,mode);
            tx(b,"r[%u]=tx%u.Sample(sm%u,%s.xy/(%s.w==0?1:%s.w));\n",8+i,i,i,tc,tc,tc);break;
        }
        {static int white=-1;if(white<0){const char*v=getenv("LEAN_WHITE_STAGES");white=v?atoi(v):0;}if(white&(1<<i))tx(b,"r[%u]=1;\n",8+i);}
        if((ctl0>>2)&1)tx(b,"if(r[%u].a<=0)discard;\n",8+i);
        if((ctl0&3)==3){WARN("colour-key kill on stage %u not emulated\n",i);}
    }
    tx(b,"r[12].a=r[8].a;\n");
    for(unsigned s=0;s<count;s++){
        uint32_t cw[2]={K[(0xac0+s*4)/4],K[(0x260+s*4)/4]},ow[2]={K[(0x1e40+s*4)/4],K[(0xaa0+s*4)/4]};
        tx(b,"{r[1]=f0[%u];r[2]=f1[%u];\n",each0?s:0,each1?s:0);
        for(unsigned a=0;a<2;a++){
            char in[4][96];for(unsigned k=0;k<4;k++)cin(in[k],sizeof in[k],(cw[a]>>(24-8*k))&255,a);
            uint32_t o=ow[a];unsigned op=(o>>15)&7,mux=(o>>14)&1;
            const char *T=a?"float":"float3";
            if(!a&&(o>>13)&1)tx(b,"float3 ab%u=dot(%s,%s).xxx;",a,in[0],in[1]);else tx(b,"%s ab%u=%s*%s;",T,a,in[0],in[1]);
            if(!a&&(o>>12)&1)tx(b,"float3 cd%u=dot(%s,%s).xxx;",a,in[2],in[3]);else tx(b,"%s cd%u=%s*%s;",T,a,in[2],in[3]);
            if(mux){if(mux_msb)tx(b,"%s sm%u=r[12].a>=0.5?cd%u:ab%u;",T,a,a,a);else tx(b,"%s sm%u=(((uint)(saturate(r[12].a)*255+0.5))&1)?cd%u:ab%u;",T,a,a,a);}
            else tx(b,"%s sm%u=ab%u+cd%u;",T,a,a,a);
            char v[8];snprintf(v,sizeof v,"ab%u",a);emit_op(b,v,op);snprintf(v,sizeof v,"cd%u",a);emit_op(b,v,op);snprintf(v,sizeof v,"sm%u",a);emit_op(b,v,op);
            tx(b,"\n");
        }
        for(unsigned a=0;a<2;a++){
            uint32_t o=ow[a];unsigned dcd=o&15,dab=(o>>4)&15,dsum=(o>>8)&15;const char *c=a?"a":"rgb";
            if(dab)tx(b,"r[%u].%s=ab%u;",dab,c,a);
            if(dcd)tx(b,"r[%u].%s=cd%u;",dcd,c,a);
            if(dsum)tx(b,"r[%u].%s=sm%u;",dsum,c,a);
        }
        {uint32_t o=ow[0];unsigned dcd=o&15,dab=(o>>4)&15;
            if(((o>>19)&1)&&dab)tx(b,"r[%u].a=ab0.b;",dab);
            if(((o>>18)&1)&&dcd)tx(b,"r[%u].a=cd0.b;",dcd);}
        tx(b,"}\n");
    }
    uint32_t c0=K[0x288/4],c1=K[0x28c/4];
    tx(b,"r[1]=ff0;r[2]=ff1;\n");
    tx(b,"r[14]=float4(saturate(%s)+%s,0);\n",(c1>>5)&1?"1-r[5].rgb":"r[5].rgb",(c1>>6)&1?"(1-saturate(r[12].rgb))":"r[12].rgb");
    if((c1>>7)&1)tx(b,"r[14].rgb=saturate(r[14].rgb);\n");
    {char e[96],f[96];fin(e,sizeof e,(c1>>24)&255,0);fin(f,sizeof f,(c1>>16)&255,0);tx(b,"r[15]=float4(%s*%s,0);\n",e,f);}
    {char A[96],B[96],C[96],D[96],G[96];fin(A,sizeof A,(c0>>24)&255,0);fin(B,sizeof B,(c0>>16)&255,0);fin(C,sizeof C,(c0>>8)&255,0);fin(D,sizeof D,c0&255,0);fin(G,sizeof G,(c1>>8)&255,1);
     tx(b,"float4 res=saturate(float4(%s*%s+(1-%s)*%s+%s,%s));\n",A,B,A,C,D,G);}
    if(K[0x300/4]){
        unsigned func=K[0x33c/4]&7;static const char *cmp[8]={"false","a<ref","a==ref","a<=ref","a>ref","a!=ref","a>=ref","true"};
        tx(b,"{float a=floor(res.a*255+0.5);float ref=aref.x;if(!(%s))discard;}\n",cmp[func]);
    }
    tx(b,"return res;}\n");
    return !b->bad;
}
static ID3D11PixelShader *ps_get(const uint32_t *K,const unsigned *stage_kind){
    PSKey k={0};uint32_t ctl=K[0x1e60/4];unsigned count=ctl&255;if(!count)count=1;if(count>8)count=8;
    pskey_add(&k,ctl);pskey_add(&k,K[0x1e70/4]);pskey_add(&k,K[0x288/4]);pskey_add(&k,K[0x28c/4]);
    for(unsigned s=0;s<count;s++){pskey_add(&k,K[(0xac0+s*4)/4]);pskey_add(&k,K[(0x260+s*4)/4]);pskey_add(&k,K[(0x1e40+s*4)/4]);pskey_add(&k,K[(0xaa0+s*4)/4]);}
    for(unsigned i=0;i<4;i++){pskey_add(&k,stage_kind[i]);pskey_add(&k,(K[(0x1b0c+i*64)/4]>>30)<<8|(K[(0x1b0c+i*64)/4]&7));}
    pskey_add(&k,K[0x300/4]?0x100|(K[0x33c/4]&7):0);
    if(ps_wdepth){uint32_t bu,bs;memcpy(&bu,&ps_wbias_units,4);memcpy(&bs,&ps_wbias_slope,4);pskey_add(&k,0x57000000u|(uint32_t)ps_wdepth);pskey_add(&k,bu);pskey_add(&k,bs);}
    uint64_t key=hash_words(k.w,k.n,k.n);unsigned slot=(unsigned)(key%SH_SLOTS);
    for(unsigned i=0;i<64;i++){PSE *e=&pss[(slot+i)%SH_SLOTS];if(e->ps&&e->key==key)return e->ps;if(!e->ps){
        Text b;text_init(&b);if(!ps_hlsl(&b,K,stage_kind)){free(b.t);return NULL;}
        {   /* LEAN_DUMP_PS=1: save each new pixel shader with its combiner registers. */
            static int dump=-1;static unsigned n;if(dump<0){const char *v=getenv("LEAN_DUMP_PS");dump=v&&*v=='1';}
            const char *dir=getenv("DRIVING_CAPTURE_DIR");
            if(dump&&dir&&n<200){char path[MAX_PATH];snprintf(path,sizeof path,"%s/ps-%03u-prog%05X.hlsl",dir,n++,K[0x1e70/4]);
                FILE *f=fopen(path,"wb");if(f){
                    fprintf(f,"// shader_program=%08X combiner_ctl=%08X final0=%08X final1=%08X\n",K[0x1e70/4],K[0x1e60/4],K[0x288/4],K[0x28c/4]);
                    for(unsigned s2=0;s2<8;s2++)fprintf(f,"// stage%u color_icw=%08X color_ocw=%08X alpha_icw=%08X alpha_ocw=%08X\n",s2,K[(0xac0+s2*4)/4],K[(0x1e40+s2*4)/4],K[(0x260+s2*4)/4],K[(0xaa0+s2*4)/4]);
                    fwrite(b.t,1,b.n,f);fclose(f);}}
        }
        ID3DBlob *code=compile(b.t,b.n,"ps_5_0","lean-ps");free(b.t);if(!code)return NULL;
        HRESULT hr=ID3D11Device_CreatePixelShader(dev,ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),NULL,&e->ps);
        ID3D10Blob_Release(code);if(FAILED(hr)){e->ps=NULL;return NULL;}
        e->key=key;st.ps_compiles++;return e->ps;}}
    return NULL;
}

/* ------------------------------------------------------------- buffers */
static ID3D11Buffer *vb,*ib,*vcb,*pcb,*clear_cb;
/* LEAN_PICK=N (diagnostic): every draw into an AA scene surface is painted a
 * flat colour encoding its index in the frame (r=id&15, g=(id>>4)&15,
 * b=(id>>8)&15, each *16+8), and the draws of presented frames N-1 and N are
 * logged with their state, so a pixel in gpu201-frameN.bmp names its draw. */
static unsigned lean_frame_no,lean_draw_no,lean_pick_end,lean_pick_step=1;static int lean_pick=-1;static ID3D11PixelShader *pick_ps;
/* Point sprites (NV097 point smooth on a point list): D3D11 draws 1-pixel
 * points, so expand each point to a screen-aligned quad of the oPts size (or
 * the fixed size) and generate stage-3 texture coordinates across it, as the
 * NV2A does. LEAN_POINT_SPRITES=0 keeps 1-pixel points. */
static ID3D11GeometryShader *sprite_gs;static int sprite_on=-1;static int sprite_bound;
static const char sprite_src[]=
"cbuffer VC:register(b0){float4 kc[192];float4 surf;float4 fogp;float4 pt;};\n"
"struct VO{float4 p:SV_Position;float4 d0:COLOR0;float4 d1:COLOR1;float4 t0:TEXCOORD0;float4 t1:TEXCOORD1;float4 t2:TEXCOORD2;float4 t3:TEXCOORD3;float3 fog:TEXCOORD4;float ps:TEXCOORD5;};\n"
"struct GO{float4 p:SV_Position;float4 d0:COLOR0;float4 d1:COLOR1;float4 t0:TEXCOORD0;float4 t1:TEXCOORD1;float4 t2:TEXCOORD2;float4 t3:TEXCOORD3;float3 fog:TEXCOORD4;};\n"
"[maxvertexcount(4)]void main(point VO i[1],inout TriangleStream<GO> s){\n"
"float sz=i[0].ps>0?i[0].ps:pt.x;if(sz<=0)return;\n"
"float2 h=float2(sz*pt.y*surf.x*0.5,sz*pt.z*surf.y*0.5)*i[0].p.w;\n"
"GO o;o.d0=i[0].d0;o.d1=i[0].d1;o.t0=i[0].t0;o.t1=i[0].t1;o.t2=i[0].t2;o.fog=i[0].fog;\n"
"[unroll]for(int k=0;k<4;k++){float2 c=float2((k&1)?1:-1,(k&2)?-1:1);\n"
"o.p=i[0].p+float4(c*h,0,0);o.t3=float4((c.x+1)*0.5,(1-c.y)*0.5,0,1);s.Append(o);}}\n";
static UINT vb_size=64u<<20,ib_size=16u<<20,vb_at,ib_at;
static int vp_dirty=1;
static ID3D11VertexShader *clear_vs;static ID3D11PixelShader *clear_ps;
static ID3D11DepthStencilState *clear_ds[4];static ID3D11BlendState *clear_bs[16];static ID3D11RasterizerState *clear_rs;

static int interp_enabled(void); /* lean_interp.inc */
static int ring_mapped[2];
static void *ring(ID3D11Buffer *buf,UINT size,UINT *at,UINT need,UINT *offset){
    D3D11_MAPPED_SUBRESOURCE m;D3D11_MAP mode=D3D11_MAP_WRITE_NO_OVERWRITE;
    /* LEAN_INTERP replays draw straight from these rings, so a wrap must not
     * discard: data is overwritten about 20 frames later instead. */
    int *first=&ring_mapped[buf==vb?0:1];
    if(*at+need>size){*at=0;if(!interp_enabled())mode=D3D11_MAP_WRITE_DISCARD;}
    if(!*first){mode=D3D11_MAP_WRITE_DISCARD;*first=1;}
    if(FAILED(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)buf,0,mode,0,&m)))return NULL;
    *offset=*at;*at+=(need+15)&~15u;return (uint8_t*)m.pData+*offset;
}
static ID3D11Buffer *mkbuf(UINT size,UINT bind,int dynamic){
    D3D11_BUFFER_DESC d={0};d.ByteWidth=size;d.BindFlags=bind;d.Usage=dynamic?D3D11_USAGE_DYNAMIC:D3D11_USAGE_DEFAULT;d.CPUAccessFlags=dynamic?D3D11_CPU_ACCESS_WRITE:0;
    ID3D11Buffer *b=NULL;ID3D11Device_CreateBuffer(dev,&d,NULL,&b);return b;
}

/* Internal resolution: a textured rectangle from one surface onto another (the
 * picture so far when a surface changes scale, guest pixels onto a scaled
 * surface, copies between surfaces of different scale, readbacks). c is the
 * immediate context (replay 0) or the in-between-frames display thread's
 * deferred context (replay 1, with its own constant buffer). Leaves no view
 * bound; every caller binds its own state afterwards. */
static ID3D11VertexShader *stretch_vs;static ID3D11PixelShader *stretch_ps;static ID3D11SamplerState *stretch_smp;static ID3D11Buffer *stretch_cb[2];
static void rt_stretch(ID3D11DeviceContext *c,int replay,ID3D11RenderTargetView *dst,D3D11_RECT r,ID3D11ShaderResourceView *src,float u0,float v0,float u1,float v1){
    ID3D11Buffer *b=stretch_cb[replay?1:0];
    if(!stretch_vs||!stretch_ps||!stretch_smp||!b||!dst||!src||r.right<=r.left||r.bottom<=r.top)return;
    float uv[4]={u0,v0,u1,v1};ID3D11DeviceContext_UpdateSubresource(c,(ID3D11Resource*)b,0,NULL,uv,0,0);
    ID3D11DeviceContext_OMSetRenderTargets(c,1,&dst,NULL);
    D3D11_VIEWPORT vp={(float)r.left,(float)r.top,(float)(r.right-r.left),(float)(r.bottom-r.top),0,1};ID3D11DeviceContext_RSSetViewports(c,1,&vp);
    ID3D11DeviceContext_RSSetScissorRects(c,1,&r);ID3D11DeviceContext_RSSetState(c,clear_rs);
    ID3D11DeviceContext_OMSetBlendState(c,clear_bs[15],NULL,0xffffffffu);ID3D11DeviceContext_OMSetDepthStencilState(c,clear_ds[0],0);
    ID3D11DeviceContext_IASetInputLayout(c,NULL);ID3D11DeviceContext_IASetPrimitiveTopology(c,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D11DeviceContext_VSSetShader(c,stretch_vs,NULL,0);ID3D11DeviceContext_VSSetConstantBuffers(c,0,1,&b);ID3D11DeviceContext_GSSetShader(c,NULL,NULL,0);
    ID3D11DeviceContext_PSSetShader(c,stretch_ps,NULL,0);ID3D11DeviceContext_PSSetShaderResources(c,0,1,&src);ID3D11DeviceContext_PSSetSamplers(c,0,1,&stretch_smp);
    ID3D11DeviceContext_Draw(c,4,0);
    ID3D11ShaderResourceView *none=NULL;ID3D11DeviceContext_PSSetShaderResources(c,0,1,&none);ID3D11DeviceContext_OMSetRenderTargets(c,0,NULL,NULL);
}
/* A scaled colour surface filtered down to its guest size (readbacks and copies
 * back to guest RAM); an unscaled one is returned as it is. */
static ID3D11Texture2D *gt_tex;static ID3D11RenderTargetView *gt_rtv;static unsigned gt_w,gt_h;
static ID3D11Texture2D *rt_guest_tex(RT *r){
    if(r->tw==r->w&&r->th==r->h)return r->tex;
    if(!gt_tex||gt_w!=r->w||gt_h!=r->h){
        if(gt_rtv)ID3D11RenderTargetView_Release(gt_rtv);if(gt_tex)ID3D11Texture2D_Release(gt_tex);gt_rtv=NULL;gt_tex=NULL;
        D3D11_TEXTURE2D_DESC td={r->w,r->h,1,1,DXGI_FORMAT_B8G8R8A8_UNORM,{1,0},D3D11_USAGE_DEFAULT,D3D11_BIND_RENDER_TARGET,0,0};
        if(FAILED(ID3D11Device_CreateTexture2D(dev,&td,NULL,&gt_tex)))return NULL;
        if(FAILED(ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)gt_tex,NULL,&gt_rtv))){ID3D11Texture2D_Release(gt_tex);gt_tex=NULL;return NULL;}
        gt_w=r->w;gt_h=r->h;}
    D3D11_RECT all={0,0,(LONG)r->w,(LONG)r->h};rt_stretch(ctx,0,gt_rtv,all,r->srv,0,0,1,1);
    return gt_tex;
}

int lean_d3d_init(void){
    if(ready)return ready>0;
    D3D_FEATURE_LEVEL want[2]={D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0},got;
    UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    const char *dbg=getenv("LEAN_D3D_DEBUG");if(dbg&&*dbg=='1')flags|=D3D11_CREATE_DEVICE_DEBUG;
    HRESULT hr=D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,flags,want,2,D3D11_SDK_VERSION,&dev,&got,&ctx);
    if(FAILED(hr))hr=D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,flags,want+1,1,D3D11_SDK_VERSION,&dev,&got,&ctx);
    if(FAILED(hr)){LOG("[LEAN] D3D11 device creation failed %08lX\n",hr);ready=-1;return 0;}
    ID3D11DeviceContext_QueryInterface(ctx,&IID_ID3D11DeviceContext1,(void**)&ctx1);
    vb=mkbuf(vb_size,D3D11_BIND_VERTEX_BUFFER,1);ib=mkbuf(ib_size,D3D11_BIND_INDEX_BUFFER,1);
    vcb=mkbuf(192*16+48,D3D11_BIND_CONSTANT_BUFFER,0);pcb=mkbuf(sizeof(float)*4*(8+8+4+4),D3D11_BIND_CONSTANT_BUFFER,0);
    clear_cb=mkbuf(32,D3D11_BIND_CONSTANT_BUFFER,0);
    {uint32_t px=0xffff00ffu;D3D11_TEXTURE2D_DESC td={1,1,1,1,DXGI_FORMAT_B8G8R8A8_UNORM,{1,0},D3D11_USAGE_DEFAULT,D3D11_BIND_SHADER_RESOURCE,0,0};
     D3D11_SUBRESOURCE_DATA sd={&px,4,0};ID3D11Texture2D *t;if(SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&td,&sd,&t))){ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)t,NULL,&magenta_srv);ID3D11Texture2D_Release(t);t=NULL;px=0xffffffffu;if(SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&td,&sd,&t)))ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)t,NULL,&white_srv);ID3D11Texture2D_Release(t);}}
    {static const char cs[]="cbuffer C:register(b0){float4 col;float4 z;};float4 vmain(uint i:SV_VertexID):SV_Position{float2 p=float2((i<<1)&2,i&2);return float4(p*float2(2,-2)+float2(-1,1),z.x,1);}\n"
        "float4 main(float4 p:SV_Position):SV_Target{return col;}\n";
     ID3DBlob *c=NULL,*e=NULL;
     if(SUCCEEDED(D3DCompile(cs,sizeof cs-1,"lean-clear",NULL,NULL,"vmain","vs_5_0",0,0,&c,&e))){ID3D11Device_CreateVertexShader(dev,ID3D10Blob_GetBufferPointer(c),ID3D10Blob_GetBufferSize(c),NULL,&clear_vs);ID3D10Blob_Release(c);}
     if(e){ID3D10Blob_Release(e);e=NULL;}
     if(SUCCEEDED(D3DCompile(cs,sizeof cs-1,"lean-clear",NULL,NULL,"main","ps_5_0",0,0,&c,&e))){ID3D11Device_CreatePixelShader(dev,ID3D10Blob_GetBufferPointer(c),ID3D10Blob_GetBufferSize(c),NULL,&clear_ps);ID3D10Blob_Release(c);}
     if(e)ID3D10Blob_Release(e);
     for(unsigned i=0;i<4;i++){D3D11_DEPTH_STENCIL_DESC d={0};d.DepthEnable=(i&1)!=0;d.DepthWriteMask=(i&1)?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;d.DepthFunc=D3D11_COMPARISON_ALWAYS;
         d.StencilEnable=(i&2)!=0;d.StencilReadMask=0xff;d.StencilWriteMask=0xff;d.FrontFace.StencilFunc=D3D11_COMPARISON_ALWAYS;d.FrontFace.StencilPassOp=D3D11_STENCIL_OP_REPLACE;d.FrontFace.StencilFailOp=D3D11_STENCIL_OP_REPLACE;d.FrontFace.StencilDepthFailOp=D3D11_STENCIL_OP_REPLACE;d.BackFace=d.FrontFace;
         ID3D11Device_CreateDepthStencilState(dev,&d,&clear_ds[i]);}
     for(unsigned m=0;m<16;m++){D3D11_BLEND_DESC d={0};d.RenderTarget[0].RenderTargetWriteMask=(UINT8)m;d.RenderTarget[0].SrcBlend=D3D11_BLEND_ONE;d.RenderTarget[0].DestBlend=D3D11_BLEND_ZERO;d.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;d.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;d.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ZERO;d.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;ID3D11Device_CreateBlendState(dev,&d,&clear_bs[m]);}
     D3D11_RASTERIZER_DESC rd={0};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.ScissorEnable=TRUE;rd.DepthClipEnable=FALSE;ID3D11Device_CreateRasterizerState(dev,&rd,&clear_rs);}
    if(nfo_enabled()){   /* internal resolution (rt_stretch) */
        static const char ss[]="cbuffer S:register(b0){float4 uv;};Texture2D tx:register(t0);SamplerState sm:register(s0);struct V{float4 pos:SV_Position;float2 tc:TEXCOORD0;};\n"
            "V vmain(uint i:SV_VertexID){V o;float2 q=float2(i&1,i>>1);o.pos=float4(q.x*2-1,1-q.y*2,0,1);o.tc=lerp(uv.xy,uv.zw,q);return o;}\n"
            "float4 pmain(V i):SV_Target{return tx.Sample(sm,i.tc);}\n";
        ID3DBlob *c=NULL,*e=NULL;
        if(SUCCEEDED(D3DCompile(ss,sizeof ss-1,"lean-stretch",NULL,NULL,"vmain","vs_5_0",0,0,&c,&e))){ID3D11Device_CreateVertexShader(dev,ID3D10Blob_GetBufferPointer(c),ID3D10Blob_GetBufferSize(c),NULL,&stretch_vs);ID3D10Blob_Release(c);}
        if(e){LOG("[LEAN] stretch vs: %s\n",(const char*)ID3D10Blob_GetBufferPointer(e));ID3D10Blob_Release(e);e=NULL;}
        if(SUCCEEDED(D3DCompile(ss,sizeof ss-1,"lean-stretch",NULL,NULL,"pmain","ps_5_0",0,0,&c,&e))){ID3D11Device_CreatePixelShader(dev,ID3D10Blob_GetBufferPointer(c),ID3D10Blob_GetBufferSize(c),NULL,&stretch_ps);ID3D10Blob_Release(c);}
        if(e){LOG("[LEAN] stretch ps: %s\n",(const char*)ID3D10Blob_GetBufferPointer(e));ID3D10Blob_Release(e);}
        D3D11_SAMPLER_DESC sd={0};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;sd.ComparisonFunc=D3D11_COMPARISON_NEVER;
        ID3D11Device_CreateSamplerState(dev,&sd,&stretch_smp);
        stretch_cb[0]=mkbuf(16,D3D11_BIND_CONSTANT_BUFFER,0);stretch_cb[1]=mkbuf(16,D3D11_BIND_CONSTANT_BUFFER,0);
        LOG("[NF-OVERLAY] internal resolution scaling %s\n",stretch_vs&&stretch_ps&&stretch_smp&&stretch_cb[0]&&stretch_cb[1]?"ready":"unavailable (copies between sizes are skipped)");
    }
    LOG("[LEAN] D3D11 ready feature_level=%X context1=%d resident-surfaces=1\n",(unsigned)got,ctx1!=NULL);
    ready=1;return 1;
}
void lean_d3d_epoch(void){epoch++;}
void lean_d3d_vp_dirty(void){vp_dirty=1;}

/* The immediate context is shared with the in-between-frames display thread
 * (lean_interp.inc). One coarse lock per public call keeps it consistent; it
 * is far cheaper than D3D's per-call multithread protection. */
static SRWLOCK lean_ctx_lock=SRWLOCK_INIT;
#define CTX_LOCK() AcquireSRWLockExclusive(&lean_ctx_lock)
#define CTX_UNLOCK() ReleaseSRWLockExclusive(&lean_ctx_lock)
/* Game-side entry: also totals how long the game waited for the lock (QPC
 * ticks), which LEAN_PRESENT_UNCAPPED uses to keep out of the game's way. */
static volatile LONG64 lean_game_lock_wait;
#define GAME_LOCK() do{if(!TryAcquireSRWLockExclusive(&lean_ctx_lock)){LARGE_INTEGER w0_,w1_;QueryPerformanceCounter(&w0_);AcquireSRWLockExclusive(&lean_ctx_lock);QueryPerformanceCounter(&w1_);InterlockedAdd64(&lean_game_lock_wait,w1_.QuadPart-w0_.QuadPart);}}while(0)
/* --------------------------------------------------------------- targets */
static UINT bt_vw,bt_vh;static float bt_k=1;   /* the bound target's stored size and internal-resolution scale */
static int bind_target(const LeanTarget *t,int want_depth,RT **color,RT **depth){
    unsigned sw=t->width*t->aa_x,sh=t->height*t->aa_y;*color=*depth=NULL;
    float k=rt_render_scale();
    if(t->color&&t->color_fmt){*color=rt_get(t->color,sw,sh,t->color_fmt,t->color_pitch,t->swizzled,0,k);if(!*color)return 0;}
    if(want_depth&&t->zeta&&t->zeta_fmt){*depth=rt_get(t->zeta,sw,sh,t->zeta_fmt,t->zeta_pitch,t->swizzled,1,k);}
    if(*color&&*depth&&((*color)->tw!=(*depth)->tw||(*color)->th!=(*depth)->th)){   /* one of them fell back to Xbox size: next call matches */
        WARN("colour %ux%u and depth %ux%u stored at different sizes; depth skipped\n",(*color)->tw,(*color)->th,(*depth)->tw,(*depth)->th);*depth=NULL;}
    ID3D11RenderTargetView *rtv=*color?(*color)->rtv:NULL;ID3D11DepthStencilView *dsv=*depth?(*depth)->dsv:NULL;
    if(!rtv&&!dsv)return 0;
    ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&rtv,dsv);
    {RT *s=*color?*color:*depth;bt_vw=s->tw;bt_vh=s->th;bt_k=s->k;rs_slope_k=bt_k;}
    D3D11_VIEWPORT vp={0,0,(float)bt_vw,(float)bt_vh,0,1};ID3D11DeviceContext_RSSetViewports(ctx,1,&vp);
    return 1;
}

static float u8f(uint32_t v,unsigned shift){return ((v>>shift)&255)/255.0f;}
static void argb4(float *o,uint32_t v){o[0]=u8f(v,16);o[1]=u8f(v,8);o[2]=u8f(v,0);o[3]=u8f(v,24);}

/* lean_interp.inc (included at the end) */
static int interp_enabled(void);
static void interp_end_frame(ID3D11Texture2D *tex,ID3D11ShaderResourceView *srv);
static void interp_rec_draw(ID3D11VertexShader*,ID3D11InputLayout*,ID3D11PixelShader*,int,ID3D11ShaderResourceView *const*,ID3D11SamplerState *const*,ID3D11BlendState*,const float*,ID3D11DepthStencilState*,UINT,ID3D11RasterizerState*,UINT,RT*,RT*,UINT,UINT,UINT,UINT,UINT,UINT,UINT,const float*,const float*);
static void interp_rec_clear(RT*,RT*,UINT,const float*,float,UINT,UINT,int,D3D11_RECT,UINT,UINT);
static void interp_rec_blit(RT*,RT*,D3D11_BOX,UINT,UINT);
static void interp_rec_stretch(RT*,RT*,D3D11_RECT,float,float,float,float);
static float interp_vconst[192*4+12],interp_pconst[96];
static const uint32_t *interp_cur_cmask;   /* constant rows the current draw's program reads */
static const int *interp_cur_prows;        /* its position-transform rows (VSE.prows) */
static int vis_active_flag(void);          /* a visibility test is counting (diagnostics) */
static ID3D11Buffer *interp_cur_vb;        /* vertex buffer the current draw used (ring or cache) */
static const uint8_t *interp_cur_verts;    /* its vertices in memory (NULL: not available) */
int lean_d3d_vc_hit;                       /* set by the native path (nd3d_draw.c) while it submits a vertex-cache hit: d->verts is then stale */
static void lean_d3d_draw_impl(const LeanDraw *d){
    if(!ready||!d->nidx)return;
    const uint32_t *K=d->K;const NFVertexProgram *vp=(const NFVertexProgram*)d->vp;
    VSE *vs=vs_get(vp);if(!vs){st.refused++;return;}
    if(vs->mask!=d->mask){WARN("vertex input mask mismatch %X/%X\n",vs->mask,d->mask);st.refused++;return;}
    int need_depth=K[0x30c/4]||K[0x35c/4]||K[0x32c/4];
    RT *color,*depth;if(!bind_target(&d->t,need_depth,&color,&depth)){st.refused++;return;}
    /* Textures */
    ID3D11ShaderResourceView *srv[4]={0};ID3D11SamplerState *smp[4]={0};unsigned kind[4]={0};float tdim[4][4]={{0}};
    uint32_t prog=K[0x1e70/4];
    for(unsigned i=0;i<4;i++){
        unsigned mode=(prog>>(5*i))&31;if(!mode||mode==4||mode==5)continue;
        if(!((K[(0x1b0c+i*64)/4]>>30)&1))continue;
        TexInfo ti;
        if(!tex_lookup(K,i,d->tex_addr[i],d->pal_addr[i],&ti,color?color->rtv:NULL)){srv[i]=magenta_srv;continue;}
        srv[i]=ti.srv;smp[i]=sampler_state(K,i,&ti);kind[i]=ti.linear?1:0;
        {   /* convolution filter (see ps_hlsl); LEAN_CONV_FILTER=0 = plain bilinear (old) */
            static int conv=-1;if(conv<0){const char *v=getenv("LEAN_CONV_FILTER");conv=!(v&&*v=='0');LOG("[LEAN] texture convolution filter %s\n",conv?"on":"off (bilinear)");}
            uint32_t fw=K[(0x1b14+i*64)/4];unsigned mn=(fw>>16)&255,mg=(fw>>24)&15,kern=(fw>>13)&7;
            if(conv&&mode!=3&&(mn==7||mg==4)&&(kern==1||kern==2))kind[i]|=kern==1?2u:4u;
        }
        tdim[i][0]=1.0f/ti.w;tdim[i][1]=1.0f/ti.h;
        if(ti.tw){tdim[i][2]=1.0f/ti.tw;tdim[i][3]=1.0f/ti.th;}   /* scaled render target: one stored texel (convolution taps) */
        if(mode==6||mode==7){memcpy(&tdim[i][2],&K[(0x1b28+i*64)/4],4);memcpy(&tdim[i][3],&K[(0x1b2c+i*64)/4],4);}
    }
    /* W-buffer (see ps_wdepth): needs a depth buffer bound and CONTROL0 Z_PERSPECTIVE. */
    const unsigned zunit=d->t.zeta_fmt==1?256u:1u;const float zmax=d->t.zeta_fmt==1?65535.0f:16777215.0f;
    int wmode=depth&&wbuffer_on()&&(K[0x290/4]&0x10000u);
    ps_wdepth=0;ps_wbias_units=ps_wbias_slope=0;
    if(wmode){
        if(depth_bias_on<0){const char*v=getenv("LEAN_DEPTH_BIAS");depth_bias_on=!(v&&*v=='0');}
        int ofs=depth_bias_on&&K[(d->topo==2?0x330:d->topo==1?0x334:0x338)/4];
        if(ofs){float f,u;memcpy(&f,&K[0x384/4],4);memcpy(&u,&K[0x388/4],4);ps_wbias_slope=f*bt_k;ps_wbias_units=u/zmax;}
        ps_wdepth=(ps_wbias_units!=0||ps_wbias_slope!=0)?2:1;   /* offset: the rasterizer's biased z is no bound */
        static int said;if(!said){said=1;LOG("[LEAN] W-buffer depth (CONTROL0 %08X): pixel depth = w, perspective-correct (LEAN_W_BUFFER=0: z/w)\n",K[0x290/4]);}
    }
    ID3D11PixelShader *ps=ps_get(K,kind);if(!ps){st.refused++;return;}
    {   /* LEAN_SHOW_DIFFUSE=1 (diagnostic): 3D draws (more than 2 triangles)
         * output only their vertex colours (diffuse + specular), to see the
         * per-vertex lighting without textures or combiners. */
        static int show=-1;static ID3D11PixelShader *dps;
        if(show<0){const char *v=getenv("LEAN_SHOW_DIFFUSE");show=v?atoi(v):0;   /* =2: diffuse alpha as grey */
            if(show){static const char src[]="struct VO{float4 p:SV_Position;float4 d0:COLOR0;float4 d1:COLOR1;float4 t0:TEXCOORD0;float4 t1:TEXCOORD1;float4 t2:TEXCOORD2;float4 t3:TEXCOORD3;float fog:TEXCOORD4;};float4 main(VO p):SV_Target{return SHOWA==1?float4(p.d0.aaa,1):SHOWA==2?float4(p.d0.rgb,1):float4(p.d0.rgb+p.d1.rgb,1);}\n";
                char src2[700];snprintf(src2,sizeof src2,"#define SHOWA %d\n%s",show==2?1:show==3?2:0,src);ID3DBlob *c=compile(src2,strlen(src2),"ps_5_0","lean-diffuse");if(c){ID3D11Device_CreatePixelShader(dev,ID3D10Blob_GetBufferPointer(c),ID3D10Blob_GetBufferSize(c),NULL,&dps);ID3D10Blob_Release(c);}}}
        if(show&&dps&&d->topo==0&&d->nidx>6)ps=dps;
        if(lean_pick<0){const char *v=getenv("LEAN_PICK");lean_pick=v?atoi(v):0;{const char *dash=v?strchr(v,45):NULL;lean_pick_end=dash?(unsigned)atoi(dash+1):(unsigned)lean_pick;}{const char *sl=v?strchr(v,47):NULL;lean_pick_step=sl?(unsigned)atoi(sl+1):1;if(!lean_pick_step)lean_pick_step=1;}
            if(lean_pick){static const char src[]="cbuffer PC:register(b0){float4 f0[8];float4 f1[8];float4 ff0;float4 ff1;float4 fogc;float4 aref;float4 tdim[4];};struct VO{float4 p:SV_Position;float4 d0:COLOR0;float4 d1:COLOR1;float4 t0:TEXCOORD0;float4 t1:TEXCOORD1;float4 t2:TEXCOORD2;float4 t3:TEXCOORD3;float fog:TEXCOORD4;};float4 main(VO p):SV_Target{return float4(aref.yzw,1);}\n";
                ID3DBlob *c=compile(src,sizeof src-1,"ps_5_0","lean-pick");if(c){ID3D11Device_CreatePixelShader(dev,ID3D10Blob_GetBufferPointer(c),ID3D10Blob_GetBufferSize(c),NULL,&pick_ps);ID3D10Blob_Release(c);}}}
        if(lean_pick&&pick_ps&&d->t.aa_x>1){ps=pick_ps;if(K[0x304/4]&&K[0x348/4]==1){lean_draw_no++;return;}}   /* pick: skip additive (glow) passes so IDs stay exact */
        {   /* LEAN_SHOW_TEX0=1 (diagnostic): scene draws with stage 0 enabled output
             * the raw stage-0 sample (=2: its alpha as grey), no lighting/blend math. */
            static int t0=-1;static ID3D11PixelShader *tps;
            if(t0<0){const char *v=getenv("LEAN_SHOW_TEX0");t0=v?atoi(v):0;
                if(t0){char src[512];snprintf(src,sizeof src,"Texture2D tx0:register(t0);SamplerState sm0:register(s0);struct VO{float4 p:SV_Position;float4 d0:COLOR0;float4 d1:COLOR1;float4 t0:TEXCOORD0;float4 t1:TEXCOORD1;float4 t2:TEXCOORD2;float4 t3:TEXCOORD3;float fog:TEXCOORD4;};float4 main(VO p):SV_Target{float4 c=tx0.Sample(sm0,p.t0.xy/(p.t0.w==0?1:p.t0.w));return %s;}",t0==2?"float4(c.aaa,1)":"float4(c.rgb,1)");
                    ID3DBlob *c=compile(src,strlen(src),"ps_5_0","lean-tex0");if(c){ID3D11Device_CreatePixelShader(dev,ID3D10Blob_GetBufferPointer(c),ID3D10Blob_GetBufferSize(c),NULL,&tps);ID3D10Blob_Release(c);}}}
            if(t0&&tps&&d->t.aa_x>1&&d->topo==0&&(K[0x1e70/4]&31)==1)ps=tps;
        }
    }
    /* Constants */
    if(vp_dirty||1){
        float buf[192*4+12];
        for(unsigned i=0;i<192;i++)memcpy(&buf[i*4],vp->constant_words[i],16);
        float *s=buf+192*4;
        s[0]=2.0f/d->t.width;s[1]=2.0f/d->t.height;s[2]=1.0f/zmax;s[3]=wmode?1.0f:0.0f;
        float bias,slope;memcpy(&bias,&K[0x9c0/4],4);memcpy(&slope,&K[0x9c4/4],4);
        uint32_t fm=K[0x29c/4];float mode=0,absf=0;
        if(K[0x2a4/4]){if(fm==0x2601||fm==0x804)mode=1;else if(fm==0x800||fm==0x802)mode=2;else if(fm==0x801||fm==0x803)mode=3;absf=(fm>=0x802&&fm<=0x804)?1.0f:0.0f;}
        s[4]=mode;s[5]=absf;s[6]=bias;s[7]=slope;
        /* point sprites: fixed size (NV097 0x43C, 1/8 pixel units) and AA scale */
        s[8]=(float)(K[0x43c/4]&0x1ff)/8.0f;s[9]=(float)(d->t.aa_x?d->t.aa_x:1);s[10]=(float)(d->t.aa_y?d->t.aa_y:1);
        /* W-buffer: the clip-range minimum in depth units 0..1 when out-of-range fragments are culled, else -2 */
        {float cmin;memcpy(&cmin,&K[0x394/4],4);s[11]=wmode&&depth_clip_mode(K,zunit)?cmin/zmax:-2.0f;}
        ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)vcb,0,NULL,buf,0,0);vp_dirty=0;
        memcpy(interp_vconst,buf,sizeof interp_vconst);
    }
    {
        float pc[(8+8+4+4)*4];
        for(unsigned i=0;i<8;i++){argb4(&pc[i*4],K[(0xa60+i*4)/4]);argb4(&pc[32+i*4],K[(0xa80+i*4)/4]);}
        argb4(&pc[64],K[0x1e20/4]);argb4(&pc[68],K[0x1e24/4]);
        uint32_t fc=K[0x2a8/4];pc[72]=u8f(fc,0);pc[73]=u8f(fc,8);pc[74]=u8f(fc,16);pc[75]=u8f(fc,24);
        pc[76]=(float)(K[0x340/4]&255);pc[77]=pc[78]=pc[79]=0;
        if(lean_pick>0){unsigned id=lean_draw_no;pc[77]=((id&15)*16+8)/255.0f;pc[78]=(((id>>4)&15)*16+8)/255.0f;pc[79]=(((id>>8)&15)*16+8)/255.0f;
            if(lean_frame_no+1>=(unsigned)lean_pick&&lean_frame_no+1<=lean_pick_end&&!((lean_frame_no+1-(unsigned)lean_pick)%lean_pick_step))
                LOG("[LEAN-PICK] frame=%u draw=%u rgb=%02X%02X%02X topo=%u idx=%u aa=%u tex=%08X/%08X/%08X/%08X prog=%05X blend=%u %04X/%04X atest=%u func=%u ref=%u ztest=%u zwrite=%u comb=%08X fin=%08X/%08X d0w=%08X vs=%016llX dfmt=%08X sfmt=%08X\n",
                    lean_frame_no+1,id,(id&15)*16+8,((id>>4)&15)*16+8,((id>>8)&15)*16+8,d->topo,d->nidx,d->t.aa_x,d->tex_addr[0],d->tex_addr[1],d->tex_addr[2],d->tex_addr[3],K[0x1e70/4],
                    K[0x304/4],K[0x344/4],K[0x348/4],K[0x300/4],K[0x33c/4],K[0x340/4],K[0x30c/4],K[0x35c/4],K[0x1e60/4],K[0x288/4],K[0x28c/4],K[0x1e70/4],(unsigned long long)vs->key,K[(0x1760+12)/4],K[(0x1760+16)/4]);}
        lean_draw_no++;
        memcpy(&pc[80],tdim,sizeof tdim);
        ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)pcb,0,NULL,pc,0,0);
        memcpy(interp_pconst,pc,sizeof interp_pconst);
    }
    /* Geometry */
    UINT stride=0;for(unsigned k=0;k<16;k++)if(d->mask&(1u<<k))stride+=16;
    UINT voff=0,ioff;ID3D11Buffer *vbuf=(ID3D11Buffer*)d->gpu_vb;
    if(!vbuf){void *vm=ring(vb,vb_size,&vb_at,d->nverts*stride,&voff);
        if(!vm){st.refused++;return;}memcpy(vm,d->verts,(size_t)d->nverts*stride);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)vb,0);vbuf=vb;}
    void *im=ring(ib,ib_size,&ib_at,d->nidx*4,&ioff);if(!im){st.refused++;return;}
    memcpy(im,d->idx,(size_t)d->nidx*4);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)ib,0);
    ID3D11DeviceContext_IASetInputLayout(ctx,vs->il);
    ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&vbuf,&stride,&voff);
    ID3D11DeviceContext_IASetIndexBuffer(ctx,ib,DXGI_FORMAT_R32_UINT,ioff);
    ID3D11DeviceContext_IASetPrimitiveTopology(ctx,d->topo==1?D3D11_PRIMITIVE_TOPOLOGY_LINELIST:d->topo==2?D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(ctx,vs->vs,NULL,0);ID3D11DeviceContext_VSSetConstantBuffers(ctx,0,1,&vcb);
    {
        if(sprite_on<0){const char *v=getenv("LEAN_POINT_SPRITES");sprite_on=!(v&&*v=='0');
            if(sprite_on){ID3DBlob *gc=compile(sprite_src,sizeof sprite_src-1,"gs_5_0","lean-sprite-gs");
                if(gc){ID3D11Device_CreateGeometryShader(dev,ID3D10Blob_GetBufferPointer(gc),ID3D10Blob_GetBufferSize(gc),NULL,&sprite_gs);ID3D10Blob_Release(gc);}
                LOG("[LEAN] point sprites %s\n",sprite_gs?"expanded by geometry shader":"unavailable");}}
        int sprite=sprite_gs&&d->topo==2&&K[0x31c/4];
        if(sprite)ID3D11DeviceContext_GSSetShader(ctx,sprite_gs,NULL,0);sprite_bound=sprite;
        if(sprite)ID3D11DeviceContext_GSSetConstantBuffers(ctx,0,1,&vcb);
    }
    ID3D11DeviceContext_PSSetShader(ctx,ps,NULL,0);ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&pcb);
    ID3D11DeviceContext_PSSetShaderResources(ctx,0,4,srv);ID3D11DeviceContext_PSSetSamplers(ctx,0,4,smp);
    /* Output merger */
    uint32_t cm=K[0x358/4];unsigned mask=((cm>>16)&1?1:0)|((cm>>8)&1?2:0)|((cm)&1?4:0)|((cm>>24)&1?8:0);
    ID3D11BlendState *bs=blend_state(K[0x304/4]!=0,K[0x344/4],K[0x348/4],K[0x350/4],mask);
    float bf[4];argb4(bf,K[0x34c/4]);
    ID3D11DeviceContext_OMSetBlendState(ctx,bs,bf,0xffffffffu);
    ID3D11DepthStencilState *dss=depth_state(K,depth!=NULL);ID3D11RasterizerState *rss=raster_state(K,0,d->topo,d->t.zeta_fmt==1?256u:1u);
    ID3D11DeviceContext_OMSetDepthStencilState(ctx,dss,K[0x368/4]&255);
    ID3D11DeviceContext_RSSetState(ctx,rss);
    {   /* census for the stats line: what kinds of draws a scene uses */
        int ofs_on=K[(d->topo==2?0x330:d->topo==1?0x334:0x338)/4]!=0;
        cen.by_topo[d->topo&3]++;if(ofs_on)cen.offset++;if(K[0x318/4])cen.point_params++;if(K[0x31c/4])cen.point_smooth++;
        if(d->topo==2){cen.point_size=K[0x43c/4];
            static unsigned n;if(n++<12){
                const float *v0=d->verts;
                LOG("[LEAN-POINTS] n=%u mask=%04X size=%08X params=%u smooth=%u blend=%u src=%04X dst=%04X ztest=%u zwrite=%u tex0=%08X ctl0=%08X/%08X/%08X/%08X fmt0=%08X shader=%08X v0=(%.1f,%.1f,%.1f,%.1f)\n",
                    d->nidx,d->mask,K[0x43c/4],K[0x318/4],K[0x31c/4],K[0x304/4],K[0x344/4],K[0x348/4],K[0x30c/4],K[0x35c/4],
                    d->tex_addr[0],K[0x1b0c/4],K[(0x1b0c+64)/4],K[(0x1b0c+128)/4],K[(0x1b0c+192)/4],K[0x1b04/4],K[0x1e70/4],
                    v0[0],v0[1],v0[2],v0[3]);
            }}
        if(K[0x304/4]&&(K[0x348/4]==1||K[0x348/4]==0x301))cen.additive++;
        /* LEAN_LOG_BLEND=1: first 24 blended triangle draws per distinct
         * (blend, texture) pair, to identify decals such as light pools. */
        {static int on=-1;if(on<0){const char *v=getenv("LEAN_LOG_BLEND");on=v?atoi(v):0;}
         if(on&&d->topo==0&&K[0x304/4]&&(on!=2||d->nidx<=48)){
            static uint64_t seen[24];static unsigned n;
            uint64_t key=((uint64_t)K[0x344/4]<<48)^((uint64_t)K[0x348/4]<<32)^d->tex_addr[0]^((uint64_t)d->tex_addr[1]<<20);
            unsigned i;for(i=0;i<n&&seen[i]!=key;i++){}
            if(i==n&&n<24){seen[n++]=key;
                LOG("[LEAN-BLEND] src=%04X dst=%04X eq=%04X ztest=%u zfunc=%04X zwrite=%u alpha_test=%u ofs=%u bias=%08X tris=%u tex=%08X/%08X/%08X/%08X fmt0=%08X fmt1=%08X shader=%08X cull=%u\n",
                    K[0x344/4],K[0x348/4],K[0x350/4],K[0x30c/4],K[0x354/4],K[0x35c/4],K[0x300/4],K[0x338/4],K[0x388/4],d->nidx/3,
                    d->tex_addr[0],d->tex_addr[1],d->tex_addr[2],d->tex_addr[3],K[0x1b04/4],K[(0x1b04+64)/4],K[0x1e70/4],K[0x308/4]);}
         }}
    }
    {   /* LEAN_DRAWLOG=a-b (diagnostic): every draw of frames a..b (lean_frame_no,
         * matched by the [LEAN-WALL] capture lines) with its textures and states. */
        static int init;static unsigned a,b,ms,cnt;static ULONGLONG t0;   /* LEAN_DRAWLOG_MS=start,count: frames from start ms after the first draw */
        if(!init){const char *v=getenv("LEAN_DRAWLOG");init=1;if(v)sscanf(v,"%u-%u",&a,&b);v=getenv("LEAN_DRAWLOG_MS");if(v)sscanf(v,"%u,%u",&ms,&cnt);t0=GetTickCount64();}
        if(cnt&&!b&&GetTickCount64()-t0>=ms){a=lean_frame_no;b=a+cnt-1;}
        static int rinit;static uint32_t only;static unsigned lastf=~0u;   /* LEAN_DRAWLOG_RT=addr: only draws into that surface, plus its first draw's constants */
        if(!rinit){const char *v=getenv("LEAN_DRAWLOG_RT");only=v?(uint32_t)strtoul(v,NULL,16):0;rinit=1;}
        if(b&&lean_frame_no>=a&&lean_frame_no<=b&&(!only||d->t.color==only)){
            if(only&&lastf!=lean_frame_no){lastf=lean_frame_no;
                for(unsigned r=0;r<16;r++){const float *c=(const float*)vp->constant_words[r];
                    LOG("[LEAN-DRAWC] f=%u c%u=%.4f %.4f %.4f %.4f\n",lean_frame_no,r,c[0],c[1],c[2],c[3]);}
                const float *v0=d->verts;LOG("[LEAN-DRAWC] f=%u v0=%.3f %.3f %.3f %.3f nverts=%u\n",lean_frame_no,v0[0],v0[1],v0[2],v0[3],d->nverts);}
            uint64_t ch=hash_words(&K[0x260/4],16,1);ch=hash_words(&K[0xa60/4],16,ch);ch=hash_words(&K[0x1e20/4],16,ch);ch=hash_words(&K[0x288/4],2,ch);
            LOG("[LEAN-DRAW] f=%u n=%u rt=%08X topo=%u idx=%u tex=%08X/%08X/%08X/%08X fmt=%08X/%08X/%08X/%08X srv=%p/%p/%p/%p prog=%05X blend=%u %04X/%04X z=%u/%04X/%u cull=%u comb=%016llX f0=%08X/%08X fc=%08X/%08X vs=%016llX filt0=%08X ctl0=%08X\n",
                lean_frame_no,lean_draw_no,d->t.color,d->topo,d->nidx,d->tex_addr[0],d->tex_addr[1],d->tex_addr[2],d->tex_addr[3],
                K[0x1b04/4],K[(0x1b04+64)/4],K[(0x1b04+128)/4],K[(0x1b04+192)/4],(void*)srv[0],(void*)srv[1],(void*)srv[2],(void*)srv[3],K[0x1e70/4],
                K[0x304/4],K[0x344/4],K[0x348/4],K[0x30c/4],K[0x354/4],K[0x35c/4],K[0x308/4],(unsigned long long)ch,K[0xa60/4],K[0xa80/4],K[0x1e20/4],K[0x1e24/4],
                (unsigned long long)vs->key,K[0x1b14/4],K[0x1b0c/4]);
            {   /* first vertex and its position-row transform (clip x,y,w), vis test state */
                const float *v0=d->verts;float cx=0,cy=0,cw=0;
                if(v0&&vs->prows[0]>=0){const float *c0=(const float*)vp->constant_words[vs->prows[0]],*c1=(const float*)vp->constant_words[vs->prows[1]],*c3=(const float*)vp->constant_words[vs->prows[3]];
                    cx=c0[0]*v0[0]+c0[1]*v0[1]+c0[2]*v0[2]+c0[3]*v0[3];cy=c1[0]*v0[0]+c1[1]*v0[1]+c1[2]*v0[2]+c1[3]*v0[3];cw=c3[0]*v0[0]+c3[1]*v0[1]+c3[2]*v0[2]+c3[3]*v0[3];}
                LOG("[LEAN-DRAWV] f=%u n=%u v0=%.2f %.2f %.2f %.2f ndc=%.3f %.3f w=%.3f vis=%d nverts=%u aa=%u\n",lean_frame_no,lean_draw_no,v0?v0[0]:0,v0?v0[1]:0,v0?v0[2]:0,v0?v0[3]:0,cw?cx/cw:0,cw?cy/cw:0,cw,vis_active_flag(),d->nverts,d->t.aa_x);
            }
        }
    }
    ID3D11DeviceContext_DrawIndexed(ctx,d->nidx,0,0);
    interp_cur_cmask=vs->cmask;interp_cur_prows=vs->prows;interp_cur_vb=vbuf;interp_cur_verts=lean_d3d_vc_hit?NULL:(const uint8_t*)d->verts;
    if(interp_enabled())interp_rec_draw(vs->vs,vs->il,ps,sprite_bound,srv,smp,bs,bf,dss,K[0x368/4]&255,rss,d->topo,color,depth,
        bt_vw,bt_vh,stride,voff,d->nverts,ioff,d->nidx,interp_vconst,interp_pconst);
    if(sprite_bound){ID3D11DeviceContext_GSSetShader(ctx,NULL,NULL,0);sprite_bound=0;}
    if(color){color->gpu_written=1;color->gen++;color->gen_frame=lean_frame_no;}if(depth)depth->gpu_written=1;
    st.draws++;st.prims+=d->nidx;
    {ID3D11ShaderResourceView *none[4]={0};ID3D11DeviceContext_PSSetShaderResources(ctx,0,4,none);}
}

static void lean_d3d_clear_impl(const LeanTarget *t,uint32_t flags,unsigned x0,unsigned x1,unsigned y0,unsigned y1,uint32_t color,uint32_t zs){
    if(!ready)return;
    int want_color=(flags&0xf0)!=0,want_depth=(flags&3)!=0;
    LeanTarget tt=*t;if(!want_color)tt.color=0;
    RT *c=NULL,*z=NULL;
    unsigned sw=t->width*t->aa_x,sh=t->height*t->aa_y;float kw=rt_render_scale();
    if(want_color&&t->color&&t->color_fmt)c=rt_get(t->color,sw,sh,t->color_fmt,t->color_pitch,t->swizzled,0,kw);
    if(want_depth&&t->zeta&&t->zeta_fmt)z=rt_get(t->zeta,sw,sh,t->zeta_fmt,t->zeta_pitch,t->swizzled,1,kw);
    unsigned rx0=x0*t->aa_x,rx1=(x1+1)*t->aa_x,ry0=y0*t->aa_y,ry1=(y1+1)*t->aa_y;if(rx1>sw)rx1=sw;if(ry1>sh)ry1=sh;
    int full=rx0==0&&ry0==0&&rx1>=sw&&ry1>=sh;
    {RT *s=c?c:z;   /* internal resolution: the rectangle in stored pixels */
     if(s&&s->k!=1.0f){float k=s->k;rx0=rt_scaled(rx0,k);rx1=rt_scaled(rx1,k);ry0=rt_scaled(ry0,k);ry1=rt_scaled(ry1,k);sw=s->tw;sh=s->th;if(rx1>sw)rx1=sw;if(ry1>sh)ry1=sh;}}
    float col[4];argb4(col,color);
    /* 16-bit surfaces receive an already packed clear value. */
    if(t->color_fmt==3){col[0]=x5(color>>11)/255.0f;col[1]=x6(color>>5)/255.0f;col[2]=x5(color)/255.0f;col[3]=1;}
    else if(t->color_fmt==1||t->color_fmt==2){col[0]=x5(color>>10)/255.0f;col[1]=x5(color>>5)/255.0f;col[2]=x5(color)/255.0f;col[3]=1;}
    unsigned cmask=((flags>>4)&1?1:0)|((flags>>5)&1?2:0)|((flags>>6)&1?4:0)|((flags>>7)&1?8:0); /* R,G,B,A */
    float zv=t->zeta_fmt==1?(zs>>16)/65535.0f:(zs>>8)/16777215.0f;unsigned sv=zs&255;
    st.clears++;if(c){c->gen++;c->gen_frame=lean_frame_no;}
    if(interp_enabled()){D3D11_RECT ir={rx0,ry0,rx1,ry1};interp_rec_clear(c,z,flags,col,zv,sv,cmask,full,ir,sw,sh);}
    if(c&&full&&cmask==15){ID3D11DeviceContext_ClearRenderTargetView(ctx,c->rtv,col);c->gpu_written=1;c=NULL;}
    else if(c&&ctx1&&cmask==15){D3D11_RECT r={rx0,ry0,rx1,ry1};ID3D11DeviceContext1_ClearView(ctx1,(ID3D11View*)c->rtv,col,&r,1);c->gpu_written=1;c=NULL;}
    if(z&&full){UINT f=((flags&1)?D3D11_CLEAR_DEPTH:0)|((flags&2)?D3D11_CLEAR_STENCIL:0);ID3D11DeviceContext_ClearDepthStencilView(ctx,z->dsv,f,zv,(UINT8)sv);z->gpu_written=1;z=NULL;}
    if(!c&&!z)return;
    /* Partial or masked clear through a full-screen triangle with scissor. */
    ID3D11RenderTargetView *rtv=c?c->rtv:NULL;ID3D11DepthStencilView *dsv=z?z->dsv:NULL;
    ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&rtv,dsv);
    D3D11_VIEWPORT vp={0,0,(float)sw,(float)sh,0,1};ID3D11DeviceContext_RSSetViewports(ctx,1,&vp);
    D3D11_RECT r={rx0,ry0,rx1,ry1};ID3D11DeviceContext_RSSetScissorRects(ctx,1,&r);
    float cb[8]={col[0],col[1],col[2],col[3],zv,0,0,0};ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)clear_cb,0,NULL,cb,0,0);
    ID3D11DeviceContext_IASetInputLayout(ctx,NULL);ID3D11DeviceContext_IASetPrimitiveTopology(ctx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(ctx,clear_vs,NULL,0);ID3D11DeviceContext_VSSetConstantBuffers(ctx,0,1,&clear_cb);
    ID3D11DeviceContext_PSSetShader(ctx,clear_ps,NULL,0);ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&clear_cb);
    ID3D11DeviceContext_OMSetBlendState(ctx,clear_bs[c?cmask:0],NULL,0xffffffffu);
    ID3D11DeviceContext_OMSetDepthStencilState(ctx,clear_ds[(z&&(flags&1)?1:0)|(z&&(flags&2)?2:0)],sv);
    ID3D11DeviceContext_RSSetState(ctx,clear_rs);
    ID3D11DeviceContext_Draw(ctx,3,0);
    if(c)c->gpu_written=1;if(z)z->gpu_written=1;
}

static int lean_d3d_blit_impl(uint32_t src,unsigned spitch,uint32_t dst,unsigned dpitch,unsigned sx,unsigned sy,unsigned dx,unsigned dy,unsigned w,unsigned h){
    if(!ready)return 0;
    RT *s=NULL;
    for(unsigned i=0;i<MAX_RT;i++){RT *r=&rts[i];if(r->tex&&!r->depth&&r->gpu_written&&r->addr==src&&r->pitch==spitch){s=r;break;}}
    if(!s)return 0;
    if(sx+w>s->w||sy+h>s->h){WARN("blit outside resident source\n");return 0;}
    unsigned dw=dpitch/4,dh=dy+h;if(dh<480&&dpitch==2560)dh=480;
    RT *d=rt_get(dst,dw,dh,8,dpitch,0,0,rt_render_scale());
    if(!d)return 0;
    if(!s->tex||s->addr!=src||s->depth||s->pitch!=spitch){   /* rt_get replaced or moved it */
        s=NULL;for(unsigned i=0;i<MAX_RT;i++){RT *r=&rts[i];if(r->tex&&!r->depth&&r->gpu_written&&r->addr==src&&r->pitch==spitch){s=r;break;}}
        if(!s||sx+w>s->w||sy+h>s->h)return 0;}
    if(s->k==d->k){   /* same internal resolution: a plain copy, in stored pixels */
        float k=s->k;D3D11_BOX box={rt_scaled(sx,k),rt_scaled(sy,k),0,rt_scaled(sx+w,k),rt_scaled(sy+h,k),1};
        if(box.right>s->tw)box.right=s->tw;if(box.bottom>s->th)box.bottom=s->th;
        UINT ox=rt_scaled(dx,k),oy=rt_scaled(dy,k);
        ID3D11DeviceContext_CopySubresourceRegion(ctx,(ID3D11Resource*)d->tex,0,ox,oy,0,(ID3D11Resource*)s->tex,0,&box);
        if(interp_enabled())interp_rec_blit(s,d,box,ox,oy);
    }else{   /* a full-screen surface into a small one or back: filtered to the destination's scale */
        D3D11_RECT r={(LONG)rt_scaled(dx,d->k),(LONG)rt_scaled(dy,d->k),(LONG)rt_scaled(dx+w,d->k),(LONG)rt_scaled(dy+h,d->k)};
        if(r.right>(LONG)d->tw)r.right=(LONG)d->tw;if(r.bottom>(LONG)d->th)r.bottom=(LONG)d->th;
        float u0=(float)sx/s->w,v0=(float)sy/s->h,u1=(float)(sx+w)/s->w,v1=(float)(sy+h)/s->h;
        rt_stretch(ctx,0,d->rtv,r,s->srv,u0,v0,u1,v1);
        if(interp_enabled())interp_rec_stretch(s,d,r,u0,v0,u1,v1);
    }
    d->gpu_written=1;d->gen++;d->gen_frame=lean_frame_no;st.blits++;return 1;
}

static ID3D11Texture2D *staging;static unsigned staging_w,staging_h;
static int lean_d3d_readback_impl(uint32_t addr,unsigned w,unsigned h,uint32_t *out){
    if(!ready)return 0;
    RT *r=NULL;for(unsigned i=0;i<MAX_RT;i++)if(rts[i].tex&&!rts[i].depth&&rts[i].addr==addr&&rts[i].gpu_written){r=&rts[i];break;}
    if(!r)return 0;
    if(!staging||staging_w!=r->w||staging_h!=r->h){
        if(staging)ID3D11Texture2D_Release(staging);staging=NULL;
        D3D11_TEXTURE2D_DESC td={r->w,r->h,1,1,DXGI_FORMAT_B8G8R8A8_UNORM,{1,0},D3D11_USAGE_STAGING,0,D3D11_CPU_ACCESS_READ,0};
        if(FAILED(ID3D11Device_CreateTexture2D(dev,&td,NULL,&staging)))return 0;staging_w=r->w;staging_h=r->h;
    }
    ID3D11Texture2D *gt=rt_guest_tex(r);if(!gt)return 0;
    ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)staging,(ID3D11Resource*)gt);
    D3D11_MAPPED_SUBRESOURCE m;if(FAILED(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)staging,0,D3D11_MAP_READ,0,&m)))return 0;
    unsigned sx=r->w/w;if(!sx)sx=1;
    for(unsigned y=0;y<h&&y<r->h;y++){const uint32_t *row=(const uint32_t*)((const uint8_t*)m.pData+(size_t)y*m.RowPitch);
        if(sx==1)memcpy(out+(size_t)y*w,row,(size_t)(w<r->w?w:r->w)*4);
        else for(unsigned x=0;x<w;x++){uint32_t a=row[x*sx],b=row[x*sx+1];out[(size_t)y*w+x]=((a>>1)&0x7f7f7f7fu)+((b>>1)&0x7f7f7f7fu);}}
    ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)staging,0);
    st.readbacks++;return 1;
}

/* Visibility tests (native D3D): every game draw between begin and end is
 * wrapped in its own occlusion query; the result is their sum (samples that
 * passed). Draws replayed by the in-between-frames thread are never inside. */
typedef struct {ID3D11Query *q[32];float k2[32];unsigned n;ULONGLONG first_poll;} VisTest;   /* k2: each draw's internal-resolution area factor */
static VisTest vis_cur,vis_done[256];static int vis_active;
static void vis_release(VisTest *t){for(unsigned i=0;i<t->n;i++)if(t->q[i])ID3D11Query_Release(t->q[i]);t->n=0;}
void lean_d3d_vis_begin(void){GAME_LOCK();vis_release(&vis_cur);vis_active=ready;CTX_UNLOCK();}
void lean_d3d_vis_end(unsigned index){GAME_LOCK();vis_active=0;VisTest *t=&vis_done[index&255];vis_release(t);*t=vis_cur;t->first_poll=0;vis_cur.n=0;CTX_UNLOCK();}
/* 1 = ready (count set), 0 = still running. */
int lean_d3d_vis_result(unsigned index,uint32_t *count){
    GAME_LOCK();VisTest *t=&vis_done[index&255];UINT64 sum=0;int ok=1;
    for(unsigned i=0;i<t->n&&ok;i++){UINT64 v=0;HRESULT hr=ID3D11DeviceContext_GetData(ctx,(ID3D11Asynchronous*)t->q[i],&v,sizeof v,0);
        if(hr==S_OK)sum+=t->k2[i]>1.0f?(UINT64)((double)v/t->k2[i]+0.5):v;else if(hr==S_FALSE)ok=0;}
    /* A query that never completes must not hang the game (it polls this):
     * after 250 ms report the object visible, once logged. */
    if(!ok){ULONGLONG now=GetTickCount64();if(!t->first_poll)t->first_poll=now;
        else if(now-t->first_poll>250){static unsigned w;if(w++<8)LOG("[LEAN] visibility test %u not ready after 250 ms; reported visible\n",index);sum=640u*480u*2u;ok=1;}}
    CTX_UNLOCK();if(ok)*count=sum>0xFFFFFFFFull?0xFFFFFFFFu:(uint32_t)sum;
    if(ok){static int lg=-1;static unsigned nl;if(lg<0){const char *e=getenv("LEAN_VIS_LOG");lg=e?atoi(e):0;}   /* LEAN_VIS_LOG=n (diagnostic): the first n results */
        if(lg&&nl<(unsigned)lg){nl++;LOG("[LEAN-VIS] f=%u test %u: %u samples from %u draws\n",lean_frame_no,index,*count,t->n);}}
    return ok;
}
static int vis_active_flag(void){return vis_active;}
void lean_d3d_draw(const LeanDraw *d){
    GAME_LOCK();
    ID3D11Query *q=NULL;
    if(vis_active&&vis_cur.n<32){D3D11_QUERY_DESC qd={D3D11_QUERY_OCCLUSION,0};if(SUCCEEDED(ID3D11Device_CreateQuery(dev,&qd,&q)))ID3D11DeviceContext_Begin(ctx,(ID3D11Asynchronous*)q);}
    lean_d3d_draw_impl(d);
    if(q){ID3D11DeviceContext_End(ctx,(ID3D11Asynchronous*)q);vis_cur.k2[vis_cur.n]=bt_k*bt_k;vis_cur.q[vis_cur.n++]=q;}
    CTX_UNLOCK();
}
void lean_d3d_clear(const LeanTarget *t,uint32_t flags,unsigned x0,unsigned x1,unsigned y0,unsigned y1,uint32_t color,uint32_t zs){
    GAME_LOCK();lean_d3d_clear_impl(t,flags,x0,x1,y0,y1,color,zs);CTX_UNLOCK();}
int lean_d3d_blit(uint32_t src,unsigned spitch,uint32_t dst,unsigned dpitch,unsigned sx,unsigned sy,unsigned dx,unsigned dy,unsigned w,unsigned h){
    GAME_LOCK();int r=lean_d3d_blit_impl(src,spitch,dst,dpitch,sx,sy,dx,dy,w,h);CTX_UNLOCK();return r;}
int lean_d3d_readback(uint32_t addr,unsigned w,unsigned h,uint32_t *out){
    GAME_LOCK();int r=lean_d3d_readback_impl(addr,w,h,out);CTX_UNLOCK();return r;}

/* GPU results the game reads back as vertex data (2026-10-08, the gunfire glare).
 * Paris's light-glare effect draws the scene into a 128x128 surface (834C5000), then
 * draws 16384 point sprites whose colours are that surface's pixels, read as a
 * vertex stream from its memory (a render-to-vertex-buffer trick; on the console
 * the GPU wrote the pixels to that RAM). Surfaces here live on the GPU, so the
 * points read stale RAM: glare from long-gone muzzle flashes and lights lit cells
 * all over the screen. Before a draw reads vertices from guest RAM that a resident
 * colour surface has rendered into since its last copy, the surface is copied back
 * to RAM in the guest's layout (32-bit linear or swizzled), and its hash updated so
 * it stays resident as a texture. OFF by default, LEAN_SYNC_VERTEX_RT=1 turns it on:
 * the old behaviour reads all-zero RAM there (no glare points ever drawn; dumps in
 * run 20261008-181712). With the copy on, the scene's alpha is 255 almost everywhere
 * (run 20261008-181410), so the whole scene glows and lines appear on the top/left
 * screen edges (run 20261008-181052) -- the scene alpha has to be checked against
 * the console before this can be on. Only surfaces rendered this or last frame. */
static ID3D11Texture2D *sync_stg;static unsigned sync_w,sync_h;
static uint64_t sync_count;
int lean_d3d_sync_guest(uint32_t va,uint32_t bytes){
    static int on=-1;if(on<0){const char *e=getenv("LEAN_SYNC_VERTEX_RT");on=e&&e[0]=='1';LOG("[LEAN] vertex reads of rendered surfaces: %s\n",on?"copied back to RAM first (LEAN_SYNC_VERTEX_RT=1)":"read RAM as is (default)");}
    if(!on&&ready&&bytes){   /* LEAN_SYNC_DUMP with the copy off: save what the vertex read sees in RAM */
        static int df=-1;static unsigned dn,lastf;if(df<0){const char *e=getenv("LEAN_SYNC_DUMP");df=e?atoi(e):0;}
        const char *dir=getenv("DRIVING_CAPTURE_DIR");
        if(df&&dir&&lean_frame_no>=(unsigned)df&&dn<5&&(!dn||lean_frame_no>=lastf+20))for(unsigned i=0;i<MAX_RT;i++){RT *r=&rts[i];
            if(!r->tex||r->depth||r->swz||r->fmt==3||!(r->addr<va+bytes&&va<r->addr+r->pitch*r->h))continue;
            const uint8_t *g=lean_guest(r->addr,r->pitch*r->h);if(!g)continue;dn++;lastf=lean_frame_no;
            for(int pass=0;pass<2;pass++){char path[MAX_PATH];snprintf(path,sizeof path,"%s/ram-%08X-f%u-%s.bmp",dir,r->addr,lean_frame_no,pass?"alpha":"rgb");FILE *fo=fopen(path,"wb");if(!fo)continue;
                uint32_t W=r->w,H=r->h,img=W*H*4;uint8_t hd[54]={'B','M'};*(uint32_t*)(hd+2)=54+img;*(uint32_t*)(hd+10)=54;*(uint32_t*)(hd+14)=40;*(int32_t*)(hd+18)=(int32_t)W;*(int32_t*)(hd+22)=-(int32_t)H;*(uint16_t*)(hd+26)=1;*(uint16_t*)(hd+28)=32;*(uint32_t*)(hd+34)=img;fwrite(hd,1,54,fo);
                for(uint32_t y=0;y<H;y++)for(uint32_t x=0;x<W;x++){uint32_t p=*(const uint32_t*)(g+(size_t)y*r->pitch+x*4);if(pass){uint32_t a=p>>24;p=a|a<<8|a<<16;}p|=0xFF000000u;fwrite(&p,4,1,fo);}
                fclose(fo);}
        }
    }
    if(!on||!ready||!bytes)return 0;
    int n=0;
    for(unsigned i=0;i<MAX_RT;i++){RT *r=&rts[i];
        if(!r->tex||r->depth||!r->gpu_written||r->gen==r->synced)continue;
        if(lean_frame_no-r->gen_frame>1)continue;   /* rendered long ago: the memory may hold other data now (82484000) */
        uint32_t rb=r->pitch*r->h;if(!(r->addr<va+bytes&&va<r->addr+rb))continue;
        if(!n){GAME_LOCK();}
        n++;r->synced=r->gen;
        if(r->fmt==3||r->pitch<r->w*4u){static unsigned w;if(w++<4)LOG("[LEAN] vertex read of surface %08X fmt %u pitch %u not copied back\n",r->addr,r->fmt,r->pitch);continue;}
        if(!sync_stg||sync_w!=r->w||sync_h!=r->h){if(sync_stg)ID3D11Texture2D_Release(sync_stg);sync_stg=NULL;
            D3D11_TEXTURE2D_DESC td={r->w,r->h,1,1,DXGI_FORMAT_B8G8R8A8_UNORM,{1,0},D3D11_USAGE_STAGING,0,D3D11_CPU_ACCESS_READ,0};
            if(FAILED(ID3D11Device_CreateTexture2D(dev,&td,NULL,&sync_stg)))continue;sync_w=r->w;sync_h=r->h;}
        uint8_t *g=lean_guest(r->addr,rb);if(!g)continue;
        {ID3D11Texture2D *gt=rt_guest_tex(r);if(!gt)continue;ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)sync_stg,(ID3D11Resource*)gt);}
        D3D11_MAPPED_SUBRESOURCE m;if(FAILED(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)sync_stg,0,D3D11_MAP_READ,0,&m)))continue;
        if(r->swz){uint8_t *lin=malloc((size_t)r->w*r->h*4);
            if(lin){for(unsigned y=0;y<r->h;y++)memcpy(lin+(size_t)y*r->w*4,(uint8_t*)m.pData+(size_t)y*m.RowPitch,(size_t)r->w*4);
                xbox_swizzle_rect(g,lin,r->w,r->h,4);free(lin);}}
        else for(unsigned y=0;y<r->h;y++)memcpy(g+(size_t)y*r->pitch,(uint8_t*)m.pData+(size_t)y*m.RowPitch,(size_t)r->w*4);
        {   /* LEAN_SYNC_DUMP=frame (diagnostic): save the copied pixels (colour, alpha as grey) at 5 frames from there */
            static int df=-1;static unsigned dn;if(df<0){const char *e=getenv("LEAN_SYNC_DUMP");df=e?atoi(e):0;}
            const char *dir=getenv("DRIVING_CAPTURE_DIR");
            if(df&&dir&&lean_frame_no>=(unsigned)df&&dn<5&&!r->swz){dn++;
                for(int pass=0;pass<2;pass++){char path[MAX_PATH];snprintf(path,sizeof path,"%s/sync-%08X-f%u-%s.bmp",dir,r->addr,lean_frame_no,pass?"alpha":"rgb");FILE *fo=fopen(path,"wb");if(!fo)continue;
                    uint32_t W=r->w,H=r->h,img=W*H*4;uint8_t hd[54]={'B','M'};*(uint32_t*)(hd+2)=54+img;*(uint32_t*)(hd+10)=54;*(uint32_t*)(hd+14)=40;*(int32_t*)(hd+18)=(int32_t)W;*(int32_t*)(hd+22)=-(int32_t)H;*(uint16_t*)(hd+26)=1;*(uint16_t*)(hd+28)=32;*(uint32_t*)(hd+34)=img;fwrite(hd,1,54,fo);
                    for(uint32_t y=0;y<H;y++)for(uint32_t x=0;x<W;x++){uint32_t p=*(const uint32_t*)((const uint8_t*)m.pData+(size_t)y*m.RowPitch+x*4);if(pass){uint32_t a=p>>24;p=a|a<<8|a<<16;}p|=0xFF000000u;fwrite(&p,4,1,fo);}
                    fclose(fo);}}
        }
        ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)sync_stg,0);
        r->ram_hash=ram_sample_hash(r->addr,r->ram_bytes?r->ram_bytes:rb);
        if(sync_count++<4||!(sync_count%3000))LOG("[LEAN] surface %08X %ux%u copied back to RAM for a vertex read (%llu so far)\n",r->addr,r->w,r->h,(unsigned long long)sync_count);
    }
    if(n)CTX_UNLOCK();
    return n;
}
void lean_d3d_report(void){
    LOG("[LEAN-CENSUS] tris=%llu lines=%llu points=%llu (point_size_word=%08X) offset=%llu point_params=%llu point_smooth=%llu additive=%llu\n",
        (unsigned long long)cen.by_topo[0],(unsigned long long)cen.by_topo[1],(unsigned long long)cen.by_topo[2],cen.point_size,
        (unsigned long long)cen.offset,(unsigned long long)cen.point_params,(unsigned long long)cen.point_smooth,(unsigned long long)cen.additive);
    memset(&cen,0,sizeof cen);
    LOG("[LEAN-STATS] draws=%llu indices=%llu tex_uploads=%llu tex_hits=%llu tex_hash_MB=%.1f vs=%llu ps=%llu clears=%llu blits=%llu readbacks=%llu surfaces=%llu surface_uploads=%llu refused=%llu textures=%u tex_MB=%.1f\n",
        (unsigned long long)st.draws,(unsigned long long)st.prims,(unsigned long long)st.tex_uploads,(unsigned long long)st.tex_hits,st.tex_hash_bytes/1048576.0,
        (unsigned long long)st.vs_compiles,(unsigned long long)st.ps_compiles,(unsigned long long)st.clears,(unsigned long long)st.blits,(unsigned long long)st.readbacks,
        (unsigned long long)st.rt_creates,(unsigned long long)st.rt_uploads,(unsigned long long)st.refused,tex_count,tex_memory/1048576.0);
}

/* LEAN_SURFACE_GUARD=1 (diagnostic): twice a second, guard-page the guest
 * memory behind every GPU-written resident surface. Surfaces live on the GPU,
 * so guest RAM there is stale; a CPU read of it (e.g. a depth or colour pick)
 * raises a one-shot guard fault that src/main.c logs with the reader. */
void lean_d3d_guard_surfaces(void){
    static int on=-1;static unsigned frame;
    lean_frame_no++;lean_draw_no=0;
    {   /* LEAN_GAMMA_GUARD=1 (diagnostic): once a second, guard the NV2A RAMDAC
         * pages (0xFD680000 PRAMDAC, 0xFD681000 PRMDIO/VGA DAC ports) so the
         * first access to each after re-arming is logged with its code site. */
        static int g=-1;static unsigned gf;
        if(g<0){const char *v=getenv("LEAN_GAMMA_GUARD");g=v&&*v=='1';if(g)LOG("[LEAN-GUARD] RAMDAC guard pages on\n");}
        if(g&&!(gf++%50))for(uint32_t pg=0xFD680000u;pg<=0xFD681000u;pg+=0x1000){
            extern ptrdiff_t xbox_GetMemoryOffset(void);uint8_t *p=(uint8_t*)((uintptr_t)xbox_GetMemoryOffset()+pg);MEMORY_BASIC_INFORMATION mbi;DWORD old;
            if(p&&VirtualQuery(p,&mbi,sizeof mbi)&&mbi.State==MEM_COMMIT&&!(mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS))){
                static int told;
                if(told++<2){   /* read the registers before arming the guard */
                    const uint8_t *r0=(const uint8_t*)((uintptr_t)xbox_GetMemoryOffset()+0xFD680600u),*r1=(const uint8_t*)((uintptr_t)xbox_GetMemoryOffset()+0xFD6813C6u);
                    LOG("[LEAN-GUARD] page %08X protect=%lX PRAMDAC_GENERAL_CONTROL=%02X%02X%02X%02X DAC 3C6..3C9=%02X %02X %02X %02X\n",pg,(unsigned long)mbi.Protect,
                        r0[3],r0[2],r0[1],r0[0],r1[0],r1[1],r1[2],r1[3]);
                }
                if(!VirtualProtect(p,4096,mbi.Protect|PAGE_GUARD,&old)&&told<3)LOG("[LEAN-GUARD] VirtualProtect %08X failed %lu\n",pg,GetLastError());
            }
        }
    }
    if(on<0){const char *v=getenv("LEAN_SURFACE_GUARD");on=v&&*v=='1';if(on)LOG("[LEAN-GUARD] surface guard pages on\n");}
    if(!on||!ready||(++frame%25))return;
    for(unsigned i=0;i<MAX_RT;i++){
        RT *r=&rts[i];if(!r->tex||!r->gpu_written)continue;
        uint32_t bytes=r->pitch*r->h;uint8_t *g=lean_guest(r->addr,bytes);if(!g)continue;
        uintptr_t p=(uintptr_t)g&~(uintptr_t)4095,end=(uintptr_t)g+bytes;
        for(;p<end;p+=4096){
            MEMORY_BASIC_INFORMATION mbi;DWORD old;
            if(!VirtualQuery((void*)p,&mbi,sizeof mbi)||mbi.State!=MEM_COMMIT||(mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)))continue;
            VirtualProtect((void*)p,4096,mbi.Protect|PAGE_GUARD,&old);
        }
    }
}
/* Which resident surface (if any) holds guest address va; for the guard log. */
unsigned lean_d3d_frame_no(void){return lean_frame_no;}

/* Vertex cache (native D3D, LEAN_VCACHE=0 to disable in nd3d_draw.c). Most of a
 * scene's meshes are the same bytes every frame; decoding them from the Xbox
 * formats and uploading them again each frame was most of the Action game
 * thread's time (2026-10-08 profile: decode_attr and the ring copy). Entries are
 * immutable GPU buffers; a changed hash or eviction makes a new one, and the old
 * one is released through defer_release (smooth mode may still replay it). */
/* Only sources seen twice with the same bytes are cached, and a source whose
 * bytes keep changing (a vertex buffer the game rewrites every frame) is marked
 * dynamic and left to the per-draw path: creating a GPU buffer per draw for those
 * cost more than it saved (Driving, run 20261008-105010: half the lookups missed). */
enum {VC_EMPTY,VC_SEEN,VC_CACHED,VC_DYNAMIC};
typedef struct {uint64_t key,hash;ID3D11Buffer *buf;unsigned bytes,last,state,changes;} VCE;
#define VC_SLOTS 16384
#define VC_PROBE 16
static VCE vcache[VC_SLOTS];static unsigned vc_clock;static size_t vc_bytes;
static struct {uint64_t hits,puts,skips,lookups,bytes_up;} vcst;
static void vc_report(void){
    if(!(++vcst.lookups%200000))LOG("[LEAN-VCACHE] lookups=%llu hits=%llu uploads=%llu (%.1f MB) per-draw=%llu resident=%.1f MB\n",(unsigned long long)vcst.lookups,
        (unsigned long long)vcst.hits,(unsigned long long)vcst.puts,vcst.bytes_up/1048576.0,(unsigned long long)vcst.skips,vc_bytes/1048576.0);
}
static void vc_drop(VCE *e){if(e->buf){GAME_LOCK();defer_release(e->buf);CTX_UNLOCK();vc_bytes-=e->bytes;e->buf=NULL;e->bytes=0;}}
/* 1: hit (*buf set); 0: decode, then lean_d3d_vcache_put; -1: decode, do not cache. */
int lean_d3d_vcache_find(uint64_t key,uint64_t hash,void **buf){
    vc_report();
    unsigned s=(unsigned)(key%VC_SLOTS);VCE *free_e=NULL,*old=NULL;
    for(unsigned i=0;i<VC_PROBE;i++){VCE *e=&vcache[(s+i)%VC_SLOTS];
        if(e->state!=VC_EMPTY&&e->key==key){
            e->last=++vc_clock;
            if(e->state==VC_DYNAMIC){vcst.skips++;return -1;}
            if(e->hash==hash){if(e->state==VC_CACHED){*buf=e->buf;vcst.hits++;return 1;}return 0;}
            e->hash=hash;
            if(e->state==VC_CACHED&&++e->changes>=2){vc_drop(e);e->state=VC_DYNAMIC;}
            vcst.skips++;return e->state==VC_CACHED?0:-1;}
        if(e->state==VC_EMPTY){if(!free_e)free_e=e;}
        else if(!old||e->last<old->last)old=e;}
    VCE *e=free_e?free_e:old;
    if(e){vc_drop(e);e->state=VC_SEEN;e->key=key;e->hash=hash;e->changes=0;e->last=++vc_clock;}
    vcst.skips++;return -1;
}
void *lean_d3d_vcache_put(uint64_t key,uint64_t hash,const void *data,unsigned bytes){
    if(!ready||!bytes)return NULL;
    unsigned s=(unsigned)(key%VC_SLOTS);VCE *slot=NULL;
    for(unsigned i=0;i<VC_PROBE;i++){VCE *e=&vcache[(s+i)%VC_SLOTS];if(e->state!=VC_EMPTY&&e->key==key){slot=e;break;}}
    if(!slot)return NULL;
    D3D11_BUFFER_DESC bd={0};bd.ByteWidth=bytes;bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd={data,0,0};ID3D11Buffer *b=NULL;
    if(FAILED(ID3D11Device_CreateBuffer(dev,&bd,&sd,&b)))return NULL;
    vc_drop(slot);
    slot->hash=hash;slot->buf=b;slot->bytes=bytes;slot->state=VC_CACHED;vc_bytes+=bytes;
    vcst.puts++;vcst.bytes_up+=bytes;
    return b;
}
/* Diagnostic: save a resident colour surface as BMP (LEAN_CAPTURE_RTS). */
void lean_d3d_dump_rt(uint32_t addr,const char *path){
    GAME_LOCK();
    RT *r=NULL;for(unsigned i=0;i<MAX_RT;i++)if(rts[i].tex&&!rts[i].depth&&rts[i].addr==addr){r=&rts[i];break;}
    if(r){D3D11_TEXTURE2D_DESC sd;ID3D11Texture2D_GetDesc(r->tex,&sd);
        sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;sd.MiscFlags=0;ID3D11Texture2D *stg=NULL;
        if(SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&sd,NULL,&stg))){D3D11_MAPPED_SUBRESOURCE m;
            ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)stg,(ID3D11Resource*)r->tex);
            if(SUCCEEDED(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)stg,0,D3D11_MAP_READ,0,&m))){
                FILE *fo=fopen(path,"wb");
                if(fo){unsigned char hd[54]={0};uint32_t W=sd.Width,H=sd.Height,size=54+W*H*4,off=54,dib=40;int32_t ht=-(int32_t)H;uint16_t pl=1,bits=32;
                    memcpy(hd,"BM",2);memcpy(hd+2,&size,4);memcpy(hd+10,&off,4);memcpy(hd+14,&dib,4);memcpy(hd+18,&W,4);memcpy(hd+22,&ht,4);memcpy(hd+26,&pl,2);memcpy(hd+28,&bits,2);
                    fwrite(hd,1,54,fo);for(uint32_t y=0;y<H;y++)fwrite((uint8_t*)m.pData+y*m.RowPitch,4,W,fo);fclose(fo);}
                ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)stg,0);}
            ID3D11Texture2D_Release(stg);}}
    else LOG("[LEAN-RT] dump %08X: no resident surface\n",addr);
    CTX_UNLOCK();
}
int lean_d3d_surface_at(uint32_t va,uint32_t *base,unsigned *w,unsigned *h,unsigned *depth){
    for(unsigned i=0;i<MAX_RT;i++){RT *r=&rts[i];if(!r->tex)continue;
        if(va>=r->addr&&va<r->addr+r->pitch*r->h){*base=r->addr;*w=r->w;*h=r->h;*depth=r->depth;return 1;}}
    return 0;
}

/* NF_OVERLAY=1: F10 settings menu and window-sized presentation (lean_overlay.inc). */
#include "lean_overlay.inc"
/* Direct3D presentation into the game window (shares dev, ctx and rts). */
#include "lean_present.inc"
/* In-between frames (LEAN_INTERP=1): recorded here, replayed on its own thread. */
#include "lean_interp.inc"
