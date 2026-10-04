/* Exact observed color-only 256x256/128x128 ARGB8 Morton storage. No depth swizzling.
 * The GPU texture stays linear; only guest import/export changes layout.
 * Pitch is size*4; the full backing interval is size*size*4 bytes.
 */
#ifndef NIGHTFIRE_SWIZZLED_SHADOW131_H
#define NIGHTFIRE_SWIZZLED_SHADOW131_H
#if defined(NIGHTFIRE_SWIZZLED_SHADOW132) && !defined(NIGHTFIRE_SWIZZLED_SHADOW131)
#error NIGHTFIRE_SWIZZLED_SHADOW132 requires NIGHTFIRE_SWIZZLED_SHADOW131
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
enum {NF_COLOR_PITCHED=0,NF_COLOR_SWIZZLED_256_131=1,NF_COLOR_SWIZZLED_128_132=2};
static int nf_swizzled131_enabled(void){
#ifdef NIGHTFIRE_SWIZZLED_SHADOW131
    static int on=-1;if(on<0){const char *v=getenv("NIGHTFIRE_SWIZZLED_SHADOW131");on=v && !strcmp(v,"1");}return on;
#else
    return 0;
#endif
}
static int nf_swizzled131_descriptor(unsigned format,unsigned pitch,unsigned x,unsigned y,unsigned w,unsigned h,unsigned depth_enable,unsigned zeta){
    return format==0x08080228 && pitch==1024 && !depth_enable && !zeta &&
        w && h && x<256 && y<256 && w<=256-x && h<=256-y;
}
static int nf_swizzled132_enabled(void){
#ifdef NIGHTFIRE_SWIZZLED_SHADOW132
    static int on=-1;if(on<0){const char *v=getenv("NIGHTFIRE_SWIZZLED_SHADOW132");on=v && !strcmp(v,"1");}return on && nf_swizzled131_enabled();
#else
    return 0;
#endif
}
static int nf_swizzled132_descriptor(unsigned format,unsigned pitch,unsigned x,unsigned y,unsigned w,unsigned h,unsigned depth_enable,unsigned zeta){
    return format==0x07070228 && pitch==512 && !depth_enable && !zeta &&
        w && h && x<128 && y<128 && w<=128-x && h<=128-y;
}
static unsigned nf_swizzled131_size(unsigned layout){
    return layout==NF_COLOR_SWIZZLED_256_131?256:layout==NF_COLOR_SWIZZLED_128_132?128:0;
}
static unsigned nf_swizzled131_layout(unsigned format,unsigned pitch,unsigned x,unsigned y,unsigned w,unsigned h,unsigned depth_enable,unsigned zeta){
    if(nf_swizzled131_enabled() && nf_swizzled131_descriptor(format,pitch,x,y,w,h,depth_enable,zeta))return NF_COLOR_SWIZZLED_256_131;
    if(nf_swizzled132_enabled() && nf_swizzled132_descriptor(format,pitch,x,y,w,h,depth_enable,zeta))return NF_COLOR_SWIZZLED_128_132;
    return NF_COLOR_PITCHED;
}
static unsigned nf_swizzled131_spread(unsigned x){
    x&=255;x=(x|(x<<4))&0x0f0f;x=(x|(x<<2))&0x3333;return (x|(x<<1))&0x5555;
}
static unsigned nf_swizzled131_index(unsigned x,unsigned y){return nf_swizzled131_spread(x)|(nf_swizzled131_spread(y)<<1);}
static void nf_swizzled131_import_original417(void *linear,unsigned pitch,const void *guest,unsigned size){
    const uint32_t *src=guest;
    for(unsigned y=0;y<size;y++){
        uint32_t *dst=(uint32_t*)((uint8_t*)linear+(size_t)y*pitch);unsigned by=nf_swizzled131_spread(y)<<1;
        for(unsigned x=0;x<size;x++)dst[x]=src[by|nf_swizzled131_spread(x)];
    }
}
static void nf_swizzled131_export_original417(void *guest,const void *linear,unsigned pitch,unsigned size){
    uint32_t *dst=guest;
    for(unsigned y=0;y<size;y++){
        const uint32_t *src=(const uint32_t*)((const uint8_t*)linear+(size_t)y*pitch);unsigned by=nf_swizzled131_spread(y)<<1;
        for(unsigned x=0;x<size;x++)dst[by|nf_swizzled131_spread(x)]=src[x];
    }
}
#include "nightfire_layout417.h"
static void nf_swizzled131_import(void *linear,unsigned pitch,const void *guest){nf_swizzled131_import_size(linear,pitch,guest,256);}
static void nf_swizzled131_export(void *guest,const void *linear,unsigned pitch){nf_swizzled131_export_size(guest,linear,pitch,256);}
#endif
