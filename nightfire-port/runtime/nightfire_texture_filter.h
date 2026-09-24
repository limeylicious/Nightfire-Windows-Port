/* Independent 2D DXT trilinear sampler for captured world state.
 * Consecutive mip levels round up to whole compressed blocks.
 * Float interpolation does not reproduce hardware precision/anisotropy. */
#ifndef NIGHTFIRE_TEXTURE_FILTER_H
#define NIGHTFIRE_TEXTURE_FILTER_H
#include <math.h>
#include "nightfire_texture_cache.h"
#if defined(_M_IX86) || defined(_M_X64) || defined(__SSE2__)
#include <emmintrin.h>
#define NF_FILTER_SSE2 1
#endif
typedef struct {
    const uint8_t *base[16];
    unsigned width[16],height[16],levels,format,u_mode,v_mode;
    float bias;
    int cached;
} NFTextureFilter;
static int nf_filter_init(NFTextureFilter *p,const uint8_t *base,size_t available,
                         unsigned format,unsigned filter,unsigned control,unsigned address,int cached){
    memset(p,0,sizeof *p);
    unsigned bytes=d3d8_format_dxt_block_bytes((format>>8)&255);
    unsigned levels=(format>>16)&15,w=1u<<((format>>20)&15),h=1u<<((format>>24)&15);
    unsigned logw=(format>>20)&15,logh=(format>>24)&15;
    unsigned um=address&15,vm=(address>>8)&15;
    if(!bytes || !levels || levels>(logw>logh?logw:logh)+1 || (format&0xfc)!=0x28 ||
       ((filter>>16)&255)!=6 || ((filter>>24)&255)!=2 || control!=0x4003ffc0 ||
       (um!=1 && um!=3) || (vm!=1 && vm!=3))return 0;
    size_t offset=0;
    for(unsigned i=0;i<levels;i++){
        size_t length=(size_t)((w+3)/4)*((h+3)/4)*bytes;
        if(offset>available || length>available-offset)return 0;
        p->base[i]=base+offset;p->width[i]=w;p->height[i]=h;offset+=length;
        if(w>1)w/=2;if(h>1)h/=2;
    }
    int bias=(int)(filter&0x1fff);if(bias&0x1000)bias-=0x2000;
    p->levels=levels;p->format=(format>>8)&255;p->bias=bias/256.0f;
    p->u_mode=um;p->v_mode=vm;p->cached=cached;return 1;
}
static unsigned nf_filter_coord(int x,unsigned size,unsigned mode){
    if(mode==1)return (unsigned)x&(size-1);
    return x<0?0:(unsigned)x>=size?size-1:(unsigned)x;
}
static uint32_t nf_filter_mix(uint32_t a,uint32_t b,float t){
#ifdef NF_FILTER_SSE2
    /* Four independent float channels, with the same rounding as scalar. */
    __m128i zero=_mm_setzero_si128();
    __m128i ax=_mm_unpacklo_epi16(_mm_unpacklo_epi8(_mm_cvtsi32_si128((int)a),zero),zero);
    __m128i bx=_mm_unpacklo_epi16(_mm_unpacklo_epi8(_mm_cvtsi32_si128((int)b),zero),zero);
    __m128 x=_mm_cvtepi32_ps(ax),y=_mm_cvtepi32_ps(bx);
    __m128 value=_mm_add_ps(_mm_add_ps(x,_mm_mul_ps(_mm_sub_ps(y,x),_mm_set1_ps(t))),_mm_set1_ps(0.5f));
    __m128i result=_mm_cvttps_epi32(value);
    result=_mm_packs_epi32(result,zero);result=_mm_packus_epi16(result,zero);
    return (uint32_t)_mm_cvtsi128_si32(result);
#else
    uint32_t out=0;
    for(unsigned shift=0;shift<32;shift+=8){
        float x=(float)((a>>shift)&255),y=(float)((b>>shift)&255);
        out|=(uint32_t)(x+(y-x)*t+0.5f)<<shift;
    }return out;
#endif
}
static uint32_t nf_filter_level(const NFTextureFilter *p,unsigned level,float u,float v){
    unsigned w=p->width[level],h=p->height[level];
    float x=u*((float)w/p->width[0])-0.5f,y=v*((float)h/p->height[0])-0.5f;
    if(p->u_mode==1){if(x<-2147483648.0f || x>2147483520.0f)x=fmodf(x,(float)w);}
    else x=fminf((float)w,fmaxf(-1,x));
    if(p->v_mode==1){if(y<-2147483648.0f || y>2147483520.0f)y=fmodf(y,(float)h);}
    else y=fminf((float)h,fmaxf(-1,y));
    int ix=(int)floorf(x),iy=(int)floorf(y);float tx=x-ix,ty=y-iy;
    uint32_t c[4];
    for(unsigned i=0;i<4;i++){
        unsigned sx=nf_filter_coord(ix+(i&1),w,p->u_mode),sy=nf_filter_coord(iy+(i>>1),h,p->v_mode);
        if(p->cached)nf_texture_cached(p->base[level],p->format,sx,sy,w,&c[i]);
        else d3d8_dxt_decode_texel(p->base[level],p->format,sx,sy,w,&c[i]);
    }
    return nf_filter_mix(nf_filter_mix(c[0],c[1],tx),nf_filter_mix(c[2],c[3],tx),ty);
}
static float nf_filter_lod(float dudx,float dvdx,float dudy,float dvdy){
    float rho2=fmaxf(dudx*dudx+dvdx*dvdx,dudy*dudy+dvdy*dvdy);
    return rho2>0?0.5f*log2f(rho2):-32.0f;
}
static int nf_filter_sample(const NFTextureFilter *p,float u,float v,float lod,uint32_t *out){
    if(!p->levels || !isfinite(u) || !isfinite(v) || !isfinite(lod))return 0;
    float selected=fminf((float)(p->levels-1),fmaxf(0,lod+p->bias));
    unsigned low=(unsigned)selected;float fraction=selected-low;
    uint32_t a=nf_filter_level(p,low,u,v);
    *out=fraction>0?nf_filter_mix(a,nf_filter_level(p,low+1,u,v),fraction):a;
    return 1;
}
#endif
