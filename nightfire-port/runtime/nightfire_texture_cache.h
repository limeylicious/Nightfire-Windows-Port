/* Memoize the pinned DXT decoder; compare source bytes on every access so
 * address reuse and guest texture writes cannot leave stale decoded texels.
 * Called only by the synchronous software GPU consumer. */
#ifndef NIGHTFIRE_TEXTURE_CACHE_H
#define NIGHTFIRE_TEXTURE_CACHE_H
#include "d3d8_swizzle.h"
typedef struct {
    const uint8_t *address;
    uint8_t bytes[16];
    uint32_t pixels[16];
    uint32_t format, valid;
} NFTextureBlock;
static NFTextureBlock nf_texture_blocks[4096];
static int nf_texture_cached(const uint8_t *base,uint32_t format,uint32_t u,
                             uint32_t v,uint32_t width,uint32_t *out) {
    uint32_t size=d3d8_format_dxt_block_bytes(format);
    uint32_t row=(width+3u)/4u;
    if(!size || !row)return 0;
    const uint8_t *block=base+((size_t)(v>>2)*row+(u>>2))*size;
    uintptr_t key=(uintptr_t)block;
    NFTextureBlock *entry=&nf_texture_blocks[((key>>3)^(key>>15))&4095];
    if(entry->address!=block || entry->format!=format ||
       memcmp(entry->bytes,block,size)!=0){
        entry->address=block;entry->format=format;entry->valid=0;
        memcpy(entry->bytes,block,size);
    }
    unsigned index=(v&3)*4+(u&3);
    if(!(entry->valid&(1u<<index))){
        d3d8_dxt_decode_texel(entry->bytes,format,u&3,v&3,4,&entry->pixels[index]);
        entry->valid|=1u<<index;
    }
    *out=entry->pixels[index];return 1;
}
#endif
