/* CP100: exact RGBA Morton decode on private, nonoverlapping buffers.
 * Contract: w/h are powers of two in 1..2048; both buffers hold w*h*4
 * bytes with no row padding. Each mip is a separate call. opaque implements
 * format 7's existing DWORD OR 0xff000000, without interpreting float bits.
 * BC textures must not enter here. No guest access/cache policy lives here. */
#ifndef NIGHTFIRE_TEXTURE_DECODE100_H
#define NIGHTFIRE_TEXTURE_DECODE100_H
#include <stdint.h>
#include <string.h>
#include "d3d/d3d8_swizzle.h"
#if defined(_M_X64) || defined(__x86_64__)
#include <emmintrin.h>
#endif

static void nf_texture_decode100(void *dst,const void *src,unsigned w,unsigned h,unsigned opaque){
    uint8_t *d=(uint8_t*)dst;
    const uint8_t *s=(const uint8_t*)src;
    uint32_t mx,my;
    xbox_swizzle_masks(w,h,&mx,&my);
#if defined(_M_X64) || defined(__x86_64__)
    if(w>=2 && h>=2){
        /* Low Morton bits select x+1 then y+1. Removing those bits from
         * the masks advances whole 2x2 tiles, including rectangular mips. */
        const __m128i alpha=_mm_set1_epi32(opaque?(int)0xff000000u:0);
        mx&=~1u;my&=~2u;
        uint32_t oy=0;
        for(unsigned y=0;y<h;y+=2){
            uint8_t *row=d+(size_t)y*w*4;
            uint32_t ox=0;
            for(unsigned x=0;x<w;x+=2){
                __m128i tile=_mm_loadu_si128((const __m128i*)(s+(size_t)(oy+ox)*4));
                tile=_mm_or_si128(tile,alpha);
                _mm_storel_epi64((__m128i*)(row+(size_t)x*4),tile);
                _mm_storel_epi64((__m128i*)(row+(size_t)(w+x)*4),_mm_srli_si128(tile,8));
                ox=(ox-mx)&mx;
            }
            oy=(oy-my)&my;
        }
        return;
    }
#endif
    /* Equivalent masked walk for tiny mip tails and non-x64 targets.
     * memcpy keeps deliberately unaligned buffers valid in scalar C too. */
    uint32_t oy=0;
    for(unsigned y=0;y<h;y++){
        uint32_t ox=0;
        for(unsigned x=0;x<w;x++){
            uint32_t value;
            memcpy(&value,s+(size_t)(oy+ox)*4,4);
            if(opaque)value|=0xff000000u;
            memcpy(d+((size_t)y*w+x)*4,&value,4);
            ox=(ox-mx)&mx;
        }
        oy=(oy-my)&my;
    }
}
#endif
