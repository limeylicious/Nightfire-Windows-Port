/* Lean Driving renderer: GPU-resident D3D11 backend.
 * Independently written for the lean experiment. Kelvin vertex programs and
 * register combiners are translated to HLSL; surfaces stay on the GPU. */
#define COBJMACROS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11_1.h>
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
    uint64_t ram_hash;unsigned last;int gpu_written;
} RT;
#define MAX_RT 48
static RT rts[MAX_RT];
static unsigned rt_clock,epoch;

static void rt_release(RT *r){
    if(r->rtv)ID3D11RenderTargetView_Release(r->rtv);
    if(r->dsv)ID3D11DepthStencilView_Release(r->dsv);
    if(r->srv)ID3D11ShaderResourceView_Release(r->srv);
    if(r->tex)ID3D11Texture2D_Release(r->tex);
    memset(r,0,sizeof *r);
}
static uint64_t ram_sample_hash(uint32_t addr,uint32_t bytes){
    /* Sparse: 64 lines of 64 bytes spread over the span. */
    uint8_t *g=lean_guest(addr,bytes);if(!g)return 0;
    uint64_t h=0;uint32_t step=bytes/64;if(step<64)step=64;
    for(uint32_t o=0;o+64<=bytes;o+=step)h=hash_bytes(g+o,64,h);
    return h^bytes;
}
static void rt_upload_color(RT *r){
    uint32_t bytes=r->pitch*r->h;uint8_t *g=lean_guest(r->addr,bytes);if(!g)return;
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
static RT *rt_find(uint32_t addr,int depth){
    for(unsigned i=0;i<MAX_RT;i++)if(rts[i].tex&&rts[i].addr==addr&&rts[i].depth==(unsigned)depth)return &rts[i];
    return NULL;
}
static RT *rt_get(uint32_t addr,unsigned w,unsigned h,unsigned fmt,unsigned pitch,unsigned swz,int depth){
    RT *r=rt_find(addr,depth);
    if(r&&(r->w!=w||r->h!=h||(!depth&&r->fmt!=fmt&&(r->fmt==3)!=(fmt==3)))){rt_release(r);r=NULL;}
    if(r){r->last=++rt_clock;if(!depth)r->fmt=fmt;r->pitch=pitch;return r;}
    /* A colour surface replacing older storage at an overlapping address. */
    unsigned slot=MAX_RT;
    for(unsigned i=0;i<MAX_RT;i++)if(!rts[i].tex){slot=i;break;}
    if(slot==MAX_RT){unsigned best=0;for(unsigned i=1;i<MAX_RT;i++)if(rts[i].last<rts[best].last)best=i;rt_release(&rts[best]);slot=best;}
    r=&rts[slot];
    D3D11_TEXTURE2D_DESC td={0};td.Width=w;td.Height=h;td.MipLevels=1;td.ArraySize=1;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;
    HRESULT hr;
    if(depth){
        td.Format=DXGI_FORMAT_R24G8_TYPELESS;td.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
        hr=ID3D11Device_CreateTexture2D(dev,&td,NULL,&r->tex);if(FAILED(hr)){WARN("depth create %ux%u failed %08lX\n",w,h,hr);memset(r,0,sizeof *r);return NULL;}
        D3D11_DEPTH_STENCIL_VIEW_DESC dv={0};dv.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;dv.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
        ID3D11Device_CreateDepthStencilView(dev,(ID3D11Resource*)r->tex,&dv,&r->dsv);
        D3D11_SHADER_RESOURCE_VIEW_DESC sv={0};sv.Format=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sv.Texture2D.MipLevels=1;
        ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)r->tex,&sv,&r->srv);
        ID3D11DeviceContext_ClearDepthStencilView(ctx,r->dsv,D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1.0f,0);
    }else{
        td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        hr=ID3D11Device_CreateTexture2D(dev,&td,NULL,&r->tex);if(FAILED(hr)){WARN("color create %ux%u failed %08lX\n",w,h,hr);memset(r,0,sizeof *r);return NULL;}
        ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)r->tex,NULL,&r->rtv);
        ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)r->tex,NULL,&r->srv);
    }
    r->addr=addr;r->w=w;r->h=h;r->fmt=fmt;r->pitch=pitch;r->swz=swz;r->depth=depth;r->last=++rt_clock;
    if(!depth){rt_upload_color(r);}
    r->ram_hash=ram_sample_hash(addr,pitch*h);
    st.rt_creates++;
    LOG("[LEAN] surface %s %08X %ux%u fmt=%u pitch=%u swz=%u\n",depth?"depth":"color",addr,w,h,fmt,pitch,swz);
    return r;
}
void lean_d3d_forget(uint32_t addr,uint32_t bytes){
    for(unsigned i=0;i<MAX_RT;i++)if(rts[i].tex&&rts[i].addr<addr+bytes&&addr<rts[i].addr+rts[i].pitch*rts[i].h)rt_release(&rts[i]);
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
    if(t->srv)ID3D11ShaderResourceView_Release(t->srv);
    if(t->tex)ID3D11Texture2D_Release(t->tex);
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
typedef struct TexInfo {unsigned w,h,levels,cube,linear,depth,kind;ID3D11ShaderResourceView *srv;} TexInfo;
static ID3D11ShaderResourceView *magenta_srv;
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
    /* Render-target alias: the title samples a surface it rendered. */
    if(!cube){
        RT *r=rt_find(addr,ti->depth);
        if(!r&&ti->depth)r=rt_find(addr,0);
        if(r&&r->gpu_written){
            uint64_t hh=ram_sample_hash(r->addr,r->pitch*r->h);
            if(hh==r->ram_hash){
                if(r->rtv&&r->rtv==bound_rtv){WARN("texture samples the bound target %08X\n",addr);return 0;}
                ti->srv=r->srv;
                /* Linear coordinates address the guest storage; our storage has the same texel grid. */
                return 1;
            }
            /* CPU replaced the memory: the surface no longer describes it. */
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
static ID3D11RasterizerState *raster_state(const uint32_t *K,int scissor){
    if(no_cull<0){const char*v=getenv("LEAN_NO_CULL");no_cull=v&&*v=='1';v=getenv("LEAN_FLIP_WINDING");flip_winding=v&&*v=='1';}
    D3D11_CULL_MODE cull=D3D11_CULL_NONE;
    if(!no_cull&&K[0x308/4]){uint32_t f=K[0x39c/4];cull=f==0x404?D3D11_CULL_FRONT:f==0x405?D3D11_CULL_BACK:D3D11_CULL_NONE;}
    int ccw=(K[0x3a0/4]==0x901)^flip_winding;
    int fill=K[0x38c/4]==0x1b01?1:0;
    uint64_t key=(uint64_t)cull|(uint64_t)ccw<<4|(uint64_t)scissor<<5|(uint64_t)fill<<6;
    ID3D11RasterizerState *r=(ID3D11RasterizerState*)cache_get(&raster_cache,key);if(r)return r;
    D3D11_RASTERIZER_DESC d={0};d.FillMode=fill?D3D11_FILL_WIREFRAME:D3D11_FILL_SOLID;d.CullMode=cull;d.FrontCounterClockwise=ccw;d.DepthClipEnable=FALSE;d.ScissorEnable=scissor;
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
    uint64_t key=hash_words((uint32_t[]){mn,mg,address,border,aniso,ti->levels>1},6,99);
    ID3D11SamplerState *s=(ID3D11SamplerState*)cache_get(&sampler_cache,key);if(s)return s;
    D3D11_SAMPLER_DESC d={0};
    int minlin=mn==2||mn==4||mn==6||mn==7,maglin=mg==2||mg==4,miplin=mn==5||mn==6,mip=mn>=3&&mn<=6;
    if(aniso>1&&minlin)d.Filter=D3D11_FILTER_ANISOTROPIC;
    else d.Filter=(D3D11_FILTER)((minlin?0x10:0)|(maglin?0x4:0)|(miplin?0x1:0));
    d.AddressU=addr_mode(address&15);d.AddressV=addr_mode((address>>8)&15);d.AddressW=addr_mode((address>>16)&15);
    d.MaxAnisotropy=aniso;d.ComparisonFunc=D3D11_COMPARISON_NEVER;
    d.BorderColor[0]=((border>>16)&255)/255.0f;d.BorderColor[1]=((border>>8)&255)/255.0f;d.BorderColor[2]=(border&255)/255.0f;d.BorderColor[3]=(border>>24)/255.0f;
    d.MinLOD=0;d.MaxLOD=mip?D3D11_FLOAT32_MAX:0;
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

typedef struct VSE {uint64_t key;ID3D11VertexShader *vs;ID3D11InputLayout *il;unsigned mask;} VSE;
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
static int vp_hlsl(Text *b,const NFVertexProgram *s,unsigned mask){
    tx(b,"cbuffer VC:register(b0){float4 kc[192];float4 surf;float4 fogp;};\n");
    tx(b,"struct VI{");for(unsigned i=0;i<16;i++)if(mask&(1u<<i))tx(b,"float4 a%u:TEXCOORD%u;",i,i);tx(b,"};\n");
    tx(b,"struct VO{float4 p:SV_Position;float4 d0:COLOR0;float4 d1:COLOR1;float4 t0:TEXCOORD0;float4 t1:TEXCOORD1;float4 t2:TEXCOORD2;float4 t3:TEXCOORD3;float fog:TEXCOORD4;};\n");
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
    tx(b,"VO q;float4 P=o[0];q.p=float4((P.x*surf.x-1)*P.w,(1-P.y*surf.y)*P.w,P.z*surf.z*P.w,P.w);\n"
         "q.d0=saturate(o[3]);q.d1=saturate(o[4]);q.t0=o[9];q.t1=o[10];q.t2=o[11];q.t3=o[12];\n"
         "float d=o[5].x;if(fogp.y>0)d=abs(d);float f=1;\n"
         "if(fogp.x==1)f=d*fogp.w+fogp.z-1;else if(fogp.x==2)f=exp2(d*16*fogp.w)+fogp.z-1.5;else if(fogp.x==3){float e=d*16*fogp.w;f=exp2(-e*e)+fogp.z-1.5;}\n"
         "q.fog=f;return q;}\n");
    return !b->bad;
}
static VSE *vs_get(const NFVertexProgram *s){
    unsigned end=s->start;while(end<136&&!(s->code[end][3]&1))end++;
    if(end>=136)end=135;
    uint64_t key=hash_words(&s->code[s->start][0],(size_t)(end-s->start+1)*4,s->start);
    unsigned slot=(unsigned)(key%SH_SLOTS);
    for(unsigned i=0;i<64;i++){VSE *e=&vss[(slot+i)%SH_SLOTS];if(e->vs&&e->key==key)return e;if(!e->vs){
        unsigned mask=vp_input_mask(s);Text b;text_init(&b);
        if(!vp_hlsl(&b,s,mask)){free(b.t);return NULL;}
        ID3DBlob *code=compile(b.t,b.n,"vs_5_0","lean-vs");free(b.t);if(!code)return NULL;
        if(FAILED(ID3D11Device_CreateVertexShader(dev,ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),NULL,&e->vs))){ID3D10Blob_Release(code);return NULL;}
        D3D11_INPUT_ELEMENT_DESC el[16];unsigned n=0;
        for(unsigned k=0;k<16;k++)if(mask&(1u<<k)){el[n].SemanticName="TEXCOORD";el[n].SemanticIndex=k;el[n].Format=DXGI_FORMAT_R32G32B32A32_FLOAT;el[n].InputSlot=0;el[n].AlignedByteOffset=n*16;el[n].InputSlotClass=D3D11_INPUT_PER_VERTEX_DATA;el[n].InstanceDataStepRate=0;n++;}
        HRESULT hr=ID3D11Device_CreateInputLayout(dev,el,n,ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),&e->il);
        ID3D10Blob_Release(code);if(FAILED(hr)){ID3D11VertexShader_Release(e->vs);e->vs=NULL;return NULL;}
        e->key=key;e->mask=mask;st.vs_compiles++;return e;}}
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
    tx(b,"struct VO{float4 p:SV_Position;float4 d0:COLOR0;float4 d1:COLOR1;float4 t0:TEXCOORD0;float4 t1:TEXCOORD1;float4 t2:TEXCOORD2;float4 t3:TEXCOORD3;float fog:TEXCOORD4;};\n");
    tx(b,"float4 main(VO p):SV_Target{float4 r[16];[unroll]for(int i=0;i<16;i++)r[i]=0;\n");
    {static int nofog=-1,flat=-1;if(nofog<0){const char*v=getenv("LEAN_NO_FOG");nofog=v&&*v=='1';v=getenv("LEAN_WHITE_DIFFUSE");flat=v&&*v=='1';}
     tx(b,nofog?"r[3]=float4(fogc.rgb,1);":"r[3]=float4(fogc.rgb,saturate(p.fog));");tx(b,flat?"r[4]=1;r[5]=p.d1;\n":"r[4]=p.d0;r[5]=p.d1;\n");}
    for(unsigned i=0;i<4;i++){
        unsigned mode=(prog>>(5*i))&31;uint32_t ctl0=K[(0x1b0c+i*64)/4];int enabled=(ctl0>>30)&1;
        const char *tc=i==0?"p.t0":i==1?"p.t1":i==2?"p.t2":"p.t3";
        if(mode==0)continue;
        if(!enabled&&mode!=4){tx(b,"r[%u]=0;\n",8+i);continue;}
        switch(mode){
        case 1:case 2:
            if(stage_kind[i]&1)tx(b,"r[%u]=tx%u.Sample(sm%u,(%s.xy/(%s.w==0?1:%s.w))*tdim[%u].xy);\n",8+i,i,i,tc,tc,tc,i);
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
    uint64_t key=hash_words(k.w,k.n,k.n);unsigned slot=(unsigned)(key%SH_SLOTS);
    for(unsigned i=0;i<64;i++){PSE *e=&pss[(slot+i)%SH_SLOTS];if(e->ps&&e->key==key)return e->ps;if(!e->ps){
        Text b;text_init(&b);if(!ps_hlsl(&b,K,stage_kind)){free(b.t);return NULL;}
        ID3DBlob *code=compile(b.t,b.n,"ps_5_0","lean-ps");free(b.t);if(!code)return NULL;
        HRESULT hr=ID3D11Device_CreatePixelShader(dev,ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),NULL,&e->ps);
        ID3D10Blob_Release(code);if(FAILED(hr)){e->ps=NULL;return NULL;}
        e->key=key;st.ps_compiles++;return e->ps;}}
    return NULL;
}

/* ------------------------------------------------------------- buffers */
static ID3D11Buffer *vb,*ib,*vcb,*pcb,*clear_cb;
static UINT vb_size=64u<<20,ib_size=16u<<20,vb_at,ib_at;
static int vp_dirty=1;
static ID3D11VertexShader *clear_vs;static ID3D11PixelShader *clear_ps;
static ID3D11DepthStencilState *clear_ds[4];static ID3D11BlendState *clear_bs[16];static ID3D11RasterizerState *clear_rs;

static void *ring(ID3D11Buffer *buf,UINT size,UINT *at,UINT need,UINT *offset){
    D3D11_MAPPED_SUBRESOURCE m;D3D11_MAP mode=D3D11_MAP_WRITE_NO_OVERWRITE;
    if(*at+need>size){*at=0;mode=D3D11_MAP_WRITE_DISCARD;}
    if(FAILED(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)buf,0,mode,0,&m)))return NULL;
    *offset=*at;*at+=(need+15)&~15u;return (uint8_t*)m.pData+*offset;
}
static ID3D11Buffer *mkbuf(UINT size,UINT bind,int dynamic){
    D3D11_BUFFER_DESC d={0};d.ByteWidth=size;d.BindFlags=bind;d.Usage=dynamic?D3D11_USAGE_DYNAMIC:D3D11_USAGE_DEFAULT;d.CPUAccessFlags=dynamic?D3D11_CPU_ACCESS_WRITE:0;
    ID3D11Buffer *b=NULL;ID3D11Device_CreateBuffer(dev,&d,NULL,&b);return b;
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
    vcb=mkbuf(192*16+32,D3D11_BIND_CONSTANT_BUFFER,0);pcb=mkbuf(sizeof(float)*4*(8+8+4+4),D3D11_BIND_CONSTANT_BUFFER,0);
    clear_cb=mkbuf(32,D3D11_BIND_CONSTANT_BUFFER,0);
    {uint32_t px=0xffff00ffu;D3D11_TEXTURE2D_DESC td={1,1,1,1,DXGI_FORMAT_B8G8R8A8_UNORM,{1,0},D3D11_USAGE_DEFAULT,D3D11_BIND_SHADER_RESOURCE,0,0};
     D3D11_SUBRESOURCE_DATA sd={&px,4,0};ID3D11Texture2D *t;if(SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&td,&sd,&t))){ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)t,NULL,&magenta_srv);ID3D11Texture2D_Release(t);}}
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
    LOG("[LEAN] D3D11 ready feature_level=%X context1=%d resident-surfaces=1\n",(unsigned)got,ctx1!=NULL);
    ready=1;return 1;
}
void lean_d3d_epoch(void){epoch++;}
void lean_d3d_vp_dirty(void){vp_dirty=1;}

/* --------------------------------------------------------------- targets */
static int bind_target(const LeanTarget *t,int want_depth,RT **color,RT **depth){
    unsigned sw=t->width*t->aa_x,sh=t->height*t->aa_y;*color=*depth=NULL;
    if(t->color&&t->color_fmt){*color=rt_get(t->color,sw,sh,t->color_fmt,t->color_pitch,t->swizzled,0);if(!*color)return 0;}
    if(want_depth&&t->zeta&&t->zeta_fmt){*depth=rt_get(t->zeta,sw,sh,t->zeta_fmt,t->zeta_pitch,t->swizzled,1);}
    ID3D11RenderTargetView *rtv=*color?(*color)->rtv:NULL;ID3D11DepthStencilView *dsv=*depth?(*depth)->dsv:NULL;
    if(!rtv&&!dsv)return 0;
    ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&rtv,dsv);
    D3D11_VIEWPORT vp={0,0,(float)sw,(float)sh,0,1};ID3D11DeviceContext_RSSetViewports(ctx,1,&vp);
    return 1;
}

static float u8f(uint32_t v,unsigned shift){return ((v>>shift)&255)/255.0f;}
static void argb4(float *o,uint32_t v){o[0]=u8f(v,16);o[1]=u8f(v,8);o[2]=u8f(v,0);o[3]=u8f(v,24);}

void lean_d3d_draw(const LeanDraw *d){
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
        tdim[i][0]=1.0f/ti.w;tdim[i][1]=1.0f/ti.h;
        if(mode==6||mode==7){memcpy(&tdim[i][2],&K[(0x1b28+i*64)/4],4);memcpy(&tdim[i][3],&K[(0x1b2c+i*64)/4],4);}
    }
    ID3D11PixelShader *ps=ps_get(K,kind);if(!ps){st.refused++;return;}
    /* Constants */
    if(vp_dirty||1){
        float buf[192*4+8];
        for(unsigned i=0;i<192;i++)memcpy(&buf[i*4],vp->constant_words[i],16);
        float *s=buf+192*4;
        s[0]=2.0f/d->t.width;s[1]=2.0f/d->t.height;s[2]=1.0f/((d->t.zeta_fmt==1)?65535.0f:16777215.0f);s[3]=0;
        float bias,slope;memcpy(&bias,&K[0x9c0/4],4);memcpy(&slope,&K[0x9c4/4],4);
        uint32_t fm=K[0x29c/4];float mode=0,absf=0;
        if(K[0x2a4/4]){if(fm==0x2601||fm==0x804)mode=1;else if(fm==0x800||fm==0x802)mode=2;else if(fm==0x801||fm==0x803)mode=3;absf=(fm>=0x802&&fm<=0x804)?1.0f:0.0f;}
        s[4]=mode;s[5]=absf;s[6]=bias;s[7]=slope;
        ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)vcb,0,NULL,buf,0,0);vp_dirty=0;
    }
    {
        float pc[(8+8+4+4)*4];
        for(unsigned i=0;i<8;i++){argb4(&pc[i*4],K[(0xa60+i*4)/4]);argb4(&pc[32+i*4],K[(0xa80+i*4)/4]);}
        argb4(&pc[64],K[0x1e20/4]);argb4(&pc[68],K[0x1e24/4]);
        uint32_t fc=K[0x2a8/4];pc[72]=u8f(fc,0);pc[73]=u8f(fc,8);pc[74]=u8f(fc,16);pc[75]=u8f(fc,24);
        pc[76]=(float)(K[0x340/4]&255);pc[77]=pc[78]=pc[79]=0;
        memcpy(&pc[80],tdim,sizeof tdim);
        ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)pcb,0,NULL,pc,0,0);
    }
    /* Geometry */
    UINT stride=0;for(unsigned k=0;k<16;k++)if(d->mask&(1u<<k))stride+=16;
    UINT voff,ioff;void *vm=ring(vb,vb_size,&vb_at,d->nverts*stride,&voff);
    if(!vm){st.refused++;return;}memcpy(vm,d->verts,(size_t)d->nverts*stride);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)vb,0);
    void *im=ring(ib,ib_size,&ib_at,d->nidx*4,&ioff);if(!im){st.refused++;return;}
    memcpy(im,d->idx,(size_t)d->nidx*4);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)ib,0);
    ID3D11DeviceContext_IASetInputLayout(ctx,vs->il);
    ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&vb,&stride,&voff);
    ID3D11DeviceContext_IASetIndexBuffer(ctx,ib,DXGI_FORMAT_R32_UINT,ioff);
    ID3D11DeviceContext_IASetPrimitiveTopology(ctx,d->topo==1?D3D11_PRIMITIVE_TOPOLOGY_LINELIST:d->topo==2?D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(ctx,vs->vs,NULL,0);ID3D11DeviceContext_VSSetConstantBuffers(ctx,0,1,&vcb);
    ID3D11DeviceContext_PSSetShader(ctx,ps,NULL,0);ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&pcb);
    ID3D11DeviceContext_PSSetShaderResources(ctx,0,4,srv);ID3D11DeviceContext_PSSetSamplers(ctx,0,4,smp);
    /* Output merger */
    uint32_t cm=K[0x358/4];unsigned mask=((cm>>16)&1?1:0)|((cm>>8)&1?2:0)|((cm)&1?4:0)|((cm>>24)&1?8:0);
    ID3D11BlendState *bs=blend_state(K[0x304/4]!=0,K[0x344/4],K[0x348/4],K[0x350/4],mask);
    float bf[4];argb4(bf,K[0x34c/4]);
    ID3D11DeviceContext_OMSetBlendState(ctx,bs,bf,0xffffffffu);
    ID3D11DeviceContext_OMSetDepthStencilState(ctx,depth_state(K,depth!=NULL),K[0x368/4]&255);
    ID3D11DeviceContext_RSSetState(ctx,raster_state(K,0));
    ID3D11DeviceContext_DrawIndexed(ctx,d->nidx,0,0);
    if(color)color->gpu_written=1;if(depth)depth->gpu_written=1;
    st.draws++;st.prims+=d->nidx;
    {ID3D11ShaderResourceView *none[4]={0};ID3D11DeviceContext_PSSetShaderResources(ctx,0,4,none);}
}

void lean_d3d_clear(const LeanTarget *t,uint32_t flags,unsigned x0,unsigned x1,unsigned y0,unsigned y1,uint32_t color,uint32_t zs){
    if(!ready)return;
    int want_color=(flags&0xf0)!=0,want_depth=(flags&3)!=0;
    LeanTarget tt=*t;if(!want_color)tt.color=0;
    RT *c=NULL,*z=NULL;
    unsigned sw=t->width*t->aa_x,sh=t->height*t->aa_y;
    if(want_color&&t->color&&t->color_fmt)c=rt_get(t->color,sw,sh,t->color_fmt,t->color_pitch,t->swizzled,0);
    if(want_depth&&t->zeta&&t->zeta_fmt)z=rt_get(t->zeta,sw,sh,t->zeta_fmt,t->zeta_pitch,t->swizzled,1);
    unsigned rx0=x0*t->aa_x,rx1=(x1+1)*t->aa_x,ry0=y0*t->aa_y,ry1=(y1+1)*t->aa_y;if(rx1>sw)rx1=sw;if(ry1>sh)ry1=sh;
    int full=rx0==0&&ry0==0&&rx1>=sw&&ry1>=sh;
    float col[4];argb4(col,color);
    /* 16-bit surfaces receive an already packed clear value. */
    if(t->color_fmt==3){col[0]=x5(color>>11)/255.0f;col[1]=x6(color>>5)/255.0f;col[2]=x5(color)/255.0f;col[3]=1;}
    else if(t->color_fmt==1||t->color_fmt==2){col[0]=x5(color>>10)/255.0f;col[1]=x5(color>>5)/255.0f;col[2]=x5(color)/255.0f;col[3]=1;}
    unsigned cmask=((flags>>4)&1?1:0)|((flags>>5)&1?2:0)|((flags>>6)&1?4:0)|((flags>>7)&1?8:0); /* R,G,B,A */
    float zv=t->zeta_fmt==1?(zs>>16)/65535.0f:(zs>>8)/16777215.0f;unsigned sv=zs&255;
    st.clears++;
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

int lean_d3d_blit(uint32_t src,unsigned spitch,uint32_t dst,unsigned dpitch,unsigned sx,unsigned sy,unsigned dx,unsigned dy,unsigned w,unsigned h){
    if(!ready)return 0;
    RT *s=NULL;
    for(unsigned i=0;i<MAX_RT;i++){RT *r=&rts[i];if(r->tex&&!r->depth&&r->gpu_written&&r->addr==src&&r->pitch==spitch){s=r;break;}}
    if(!s)return 0;
    if(sx+w>s->w||sy+h>s->h){WARN("blit outside resident source\n");return 0;}
    unsigned dw=dpitch/4,dh=dy+h;if(dh<480&&dpitch==2560)dh=480;
    RT *d=rt_get(dst,dw,dh,8,dpitch,0,0);
    if(!d)return 0;
    D3D11_BOX box={sx,sy,0,sx+w,sy+h,1};
    ID3D11DeviceContext_CopySubresourceRegion(ctx,(ID3D11Resource*)d->tex,0,dx,dy,0,(ID3D11Resource*)s->tex,0,&box);
    d->gpu_written=1;st.blits++;return 1;
}

static ID3D11Texture2D *staging;static unsigned staging_w,staging_h;
int lean_d3d_readback(uint32_t addr,unsigned w,unsigned h,uint32_t *out){
    if(!ready)return 0;
    RT *r=NULL;for(unsigned i=0;i<MAX_RT;i++)if(rts[i].tex&&!rts[i].depth&&rts[i].addr==addr&&rts[i].gpu_written){r=&rts[i];break;}
    if(!r)return 0;
    if(!staging||staging_w!=r->w||staging_h!=r->h){
        if(staging)ID3D11Texture2D_Release(staging);staging=NULL;
        D3D11_TEXTURE2D_DESC td={r->w,r->h,1,1,DXGI_FORMAT_B8G8R8A8_UNORM,{1,0},D3D11_USAGE_STAGING,0,D3D11_CPU_ACCESS_READ,0};
        if(FAILED(ID3D11Device_CreateTexture2D(dev,&td,NULL,&staging)))return 0;staging_w=r->w;staging_h=r->h;
    }
    ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)staging,(ID3D11Resource*)r->tex);
    D3D11_MAPPED_SUBRESOURCE m;if(FAILED(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)staging,0,D3D11_MAP_READ,0,&m)))return 0;
    unsigned sx=r->w/w;if(!sx)sx=1;
    for(unsigned y=0;y<h&&y<r->h;y++){const uint32_t *row=(const uint32_t*)((const uint8_t*)m.pData+(size_t)y*m.RowPitch);
        if(sx==1)memcpy(out+(size_t)y*w,row,(size_t)(w<r->w?w:r->w)*4);
        else for(unsigned x=0;x<w;x++){uint32_t a=row[x*sx],b=row[x*sx+1];out[(size_t)y*w+x]=((a>>1)&0x7f7f7f7fu)+((b>>1)&0x7f7f7f7fu);}}
    ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)staging,0);
    st.readbacks++;return 1;
}

void lean_d3d_report(void){
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
int lean_d3d_surface_at(uint32_t va,uint32_t *base,unsigned *w,unsigned *h,unsigned *depth){
    for(unsigned i=0;i<MAX_RT;i++){RT *r=&rts[i];if(!r->tex)continue;
        if(va>=r->addr&&va<r->addr+r->pitch*r->h){*base=r->addr;*w=r->w;*h=r->h;*depth=r->depth;return 1;}}
    return 0;
}

/* Direct3D presentation into the game window (shares dev, ctx and rts). */
#include "lean_present.inc"
