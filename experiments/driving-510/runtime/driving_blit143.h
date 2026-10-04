/* Original bounded helper for the observed NV09F SRCCOPY / NV062 A8R8G8B8 path.
 * Caller resolves DMA objects, supplies valid mapped spans, synchronizes GPU
 * rendering and marks destination textures/surfaces dirty after success.
 * No allocation, format conversion, clipping or GPU state is inferred here.
 */
#ifndef DRIVING_BLIT143_H
#define DRIVING_BLIT143_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct DrivingBlit143Rect {
    uint32_t operation, format;
    uint32_t source_pitch, destination_pitch;
    uint32_t source_x, source_y, destination_x, destination_y;
    uint32_t width, height;
} DrivingBlit143Rect;

enum DrivingBlit143Result {
    DRIVING_BLIT143_OK = 1,
    DRIVING_BLIT143_UNSUPPORTED = 0,
    DRIVING_BLIT143_INVALID = -1,
    DRIVING_BLIT143_NO_MEMORY = -2
};

/* All arithmetic is wider than the original 16-bit coordinates and pitches.
 * Spans begin at each already-resolved NV062 surface offset, not DMA base.
 */
static int driving_blit143_range(size_t span,uint32_t pitch,uint32_t x,
    uint32_t y,uint32_t width,uint32_t height,size_t *first,size_t *end)
{
    uint64_t row_end=((uint64_t)x+width)*4u;
    uint64_t begin=(uint64_t)y*pitch+(uint64_t)x*4u;
    uint64_t finish=((uint64_t)y+height-1u)*pitch+row_end;
    if(!pitch || pitch>0xffffu || row_end>pitch || finish>span || finish>SIZE_MAX)
        return 0;
    *first=(size_t)begin;*end=(size_t)finish;return 1;
}

/* Transactional validation: unsupported/invalid/allocation failures leave both
 * mappings untouched. Aliasing copies snapshot the full source rectangle before
 * writing any row, preserving source pixels despite cross-row overlap. This is
 * a defined helper policy; NV2A cross-row overlap behavior is not yet validated.
 */
static int driving_blit143_copy(const void *source,size_t source_bytes,
    void *destination,size_t destination_bytes,const DrivingBlit143Rect *r)
{
    size_t source_first,source_end,destination_first,destination_end,row_bytes;
    uintptr_t source_address=(uintptr_t)source,destination_address=(uintptr_t)destination;
    const unsigned char *src;unsigned char *dst,*snapshot=NULL;
    if(!r)return DRIVING_BLIT143_INVALID;
    if(r->operation!=3 || r->format!=0x0a)return DRIVING_BLIT143_UNSUPPORTED;
    if(r->source_x>0xffffu || r->source_y>0xffffu || r->destination_x>0xffffu
       || r->destination_y>0xffffu || r->width>0xffffu || r->height>0xffffu)
        return DRIVING_BLIT143_INVALID;
    if(!r->width || !r->height)return DRIVING_BLIT143_OK;
    if(!source || !destination
       || !driving_blit143_range(source_bytes,r->source_pitch,r->source_x,r->source_y,
             r->width,r->height,&source_first,&source_end)
       || !driving_blit143_range(destination_bytes,r->destination_pitch,r->destination_x,
             r->destination_y,r->width,r->height,&destination_first,&destination_end)
       || source_end>UINTPTR_MAX-source_address
       || destination_end>UINTPTR_MAX-destination_address)
        return DRIVING_BLIT143_INVALID;
    row_bytes=(size_t)r->width*4u;
    src=(const unsigned char *)source+source_first;
    dst=(unsigned char *)destination+destination_first;
    if(source_address+source_first<destination_address+destination_end
       && destination_address+destination_first<source_address+source_end){
        size_t total;
        if(r->height>SIZE_MAX/row_bytes)return DRIVING_BLIT143_INVALID;
        total=row_bytes*r->height;snapshot=(unsigned char *)malloc(total);
        if(!snapshot)return DRIVING_BLIT143_NO_MEMORY;
        for(uint32_t y=0;y<r->height;y++)
            memcpy(snapshot+(size_t)y*row_bytes,src+(size_t)y*r->source_pitch,row_bytes);
    }
    for(uint32_t y=0;y<r->height;y++)
        memcpy(dst+(size_t)y*r->destination_pitch,
               snapshot?snapshot+(size_t)y*row_bytes:src+(size_t)y*r->source_pitch,row_bytes);
    free(snapshot);return DRIVING_BLIT143_OK;
}
#endif
