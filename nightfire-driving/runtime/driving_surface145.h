/* Observed PAL pitched A8R8G8B8 and scoped RGB565 clear storage. A clear writes both AA lanes
 * identically, so it needs no claim about their geometric sample positions.
 * This is not a rasterizer, depth/stencil implementation or quincunx resolve. */
#ifndef DRIVING_SURFACE145_H
#define DRIVING_SURFACE145_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#ifdef DRIVING_ACCESS321
#include "driving_access321.h"
#endif
typedef struct DrivingSurface145 {
    uint32_t format,width,height,pitch,lanes,storage_width;
    size_t span;
} DrivingSurface145;
static int driving_surface145(DrivingSurface145 *out,uint32_t format,
    uint32_t horizontal,uint32_t vertical,uint32_t pitch)
{
    DrivingSurface145 s;
    /*209: PAL Clear temporarily selects pitched storage while retaining the
     * original log2 dimensions. Decode size separately from the format/type.
     * Swizzled types, other color/depth formats and new AA combinations reject. */
    unsigned base=format&0xffffu,dimensions=format>>16;
    if(!out || (base!=0x128 && base!=0x1128 && base!=0x113) || (horizontal&0xffff)
       || (vertical&0xffff) || !pitch || pitch>0xffff)return 0;
    if(base==0x1128 && dimensions)return 0;
    if(base==0x113 && !dimensions)return 0;
    if(dimensions){
        unsigned lw=(format>>16)&255u,lh=format>>24;
        if(lw>15||lh>15||(horizontal>>16)!=(1u<<lw)||(vertical>>16)!=(1u<<lh))return 0;
    }
    s.format=format;s.width=horizontal>>16;s.height=vertical>>16;s.pitch=pitch;
    s.lanes=base==0x1128?2:1;s.storage_width=s.width*s.lanes;
    unsigned bytes=base==0x113?2u:4u;
    if(!s.width || !s.height || s.storage_width*bytes>pitch)return 0;
    s.span=(size_t)(s.height-1u)*pitch+(size_t)s.storage_width*bytes;
    *out=s;return 1;
}
static int driving_color_clear145(void *memory,size_t size,const DrivingSurface145 *s,
    uint32_t horizontal,uint32_t vertical,uint32_t flags,uint32_t color)
{
    uint32_t x0=horizontal&0xffff,x1=horizontal>>16;
    uint32_t y0=vertical&0xffff,y1=vertical>>16,mask=0;
    if(!s || !memory || size<s->span || x0>x1 || y0>y1
       || x1>=s->width || y1>=s->height || (flags&~0xf3u))return 0;
    unsigned bytes=(s->format&0xffffu)==0x113?2u:4u;
    /* Original PAL Clear already packed COLOR_CLEAR_VALUE for RGB565. */
    if(flags&0x10)mask|=bytes==2?0xf800:0x00ff0000;
    if(flags&0x20)mask|=bytes==2?0x07e0:0x0000ff00;
    if(flags&0x40)mask|=bytes==2?0x001f:0x000000ff;
    if((flags&0x80)&&bytes==4)mask|=0xff000000;
    if(!mask)return 1;
#ifdef DRIVING_ACCESS321
    for(uint32_t y=y0;y<=y1;y++){
        const void *row=(unsigned char*)memory+(size_t)y*s->pitch+(size_t)x0*s->lanes*bytes;
        size_t n=(size_t)(x1-x0+1)*s->lanes*bytes;
        driving_access321_note(row,n,DA321_COLOR_CLEAR_READ);
        driving_access321_note(row,n,DA321_COLOR_CLEAR_WRITE);
    }
#endif
    for(uint32_t y=y0;y<=y1;y++)for(uint32_t x=x0*s->lanes;x<(x1+1)*s->lanes;x++){
        unsigned char *p=(unsigned char *)memory+(size_t)y*s->pitch+(size_t)x*bytes;
        uint32_t old=0;memcpy(&old,p,bytes);old=(old&~mask)|(color&mask);memcpy(p,&old,bytes);
    }
    return 1;
}
#endif
