/* Exact COLOR upload reuse support. Caller owns the runtime gate and all GPU
 * operations. capture receives a completed mapped GPU color readback BEFORE
 * depth packing; same receives the current guest color upload source. Neither
 * function writes caller memory or issues GPU work. Only width*4 visible bytes
 * per row participate; pitch padding is outside the GPU color resource.
 *
 * A successful same permits retaining ONLY that unchanged physical GPU color
 * resource. Caller must invalidate after every actual color draw/upload/other
 * GPU color mutation and before release/reuse. Capture reestablishes validity.
 * Generation identifies physical lifetime even when a pointer is recycled.
 * No depth reuse, readback suppression, guest-content caching or QPC/env calls.
 *
 * Four owned CPU snapshots; 16 MiB logical capacity, at most 32 MiB temporary
 * allocation during transactional replacement. Fixed metadata is additional.
 * Allocation failure invalidates prior truth for the same resource. A fifth
 * identity replaces an LRU snapshot, never a GPU resource. Single owner thread.
 * TEST reset frees CPU snapshots only; production retains them until exit. */
#ifndef NIGHTFIRE_COLOR_REUSE110_H
#define NIGHTFIRE_COLOR_REUSE110_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "nightfire_bytes_equal.h"
enum {NF_CR110_SLOTS=4,NF_CR110_BUDGET=16*1024*1024};
typedef struct {
    const void *resource;
    uint64_t generation,used;
    unsigned width,height,valid;
    size_t bytes;
    uint8_t *snapshot;
} NFColorReuse110Slot;
typedef struct {
    size_t resident_bytes,peak_bytes;
    uint64_t sequence,captures,captured,captured_bytes,capture_invalid;
    uint64_t allocation_failures,budget_failures,failed_bytes;
    uint64_t comparisons,requested_bytes,exact,exact_bytes,changed,changed_bytes;
    uint64_t missing,missing_bytes,replaced,replaced_bytes,invalidated,invalidated_bytes,compare_invalid;
    uint64_t slot_replacements,generation_replacements,invalidations;
    NFColorReuse110Slot slot[NF_CR110_SLOTS];
} NFColorReuse110State;
static NFColorReuse110State nf_color_reuse110;
#ifdef NF_COLOR_REUSE110_TEST
static unsigned nf_color_reuse110_fail_alloc;
static void nf_color_reuse110_test_reset(void){
    for(unsigned i=0;i<NF_CR110_SLOTS;i++)free(nf_color_reuse110.slot[i].snapshot);
    memset(&nf_color_reuse110,0,sizeof nf_color_reuse110);nf_color_reuse110_fail_alloc=0;
}
#endif
static NFColorReuse110Slot *nf_cr110_find(const void *resource){
    for(unsigned i=0;i<NF_CR110_SLOTS;i++)if(nf_color_reuse110.slot[i].resource==resource)return &nf_color_reuse110.slot[i];
    return NULL;
}
static int nf_cr110_layout(const void *resource,unsigned w,unsigned h,const void *data,size_t pitch){
    if(!resource || !data || !w || !h || w>2048 || h>2048 || pitch<(size_t)w*4)return 0;
    size_t row=(size_t)w*4;
    if(h>1 && pitch>(SIZE_MAX-row)/(h-1))return 0;
    return (size_t)(h-1)*pitch+row<=UINTPTR_MAX-(uintptr_t)data;
}
static void nf_color_reuse110_invalidate(const void *resource){
    if(!resource)return;NFColorReuse110Slot *e=nf_cr110_find(resource);
    if(e && e->valid){e->valid=0;nf_color_reuse110.invalidations++;}
}
static void nf_color_reuse110_capture(const void *resource,uint64_t generation,unsigned w,unsigned h,const void *mapped,size_t pitch){
    NFColorReuse110State *s=&nf_color_reuse110;s->captures++;
    NFColorReuse110Slot *e=resource?nf_cr110_find(resource):NULL;
    if(e)e->valid=0;
    if(!nf_cr110_layout(resource,w,h,mapped,pitch)){s->capture_invalid++;return;}
    size_t row=(size_t)w*4,bytes=row*h;
    if(!e){
        e=&s->slot[0];
        for(unsigned i=0;i<NF_CR110_SLOTS;i++){
            if(!s->slot[i].resource){e=&s->slot[i];break;}
            if(s->slot[i].used<e->used)e=&s->slot[i];
        }
    }
    if(bytes>NF_CR110_BUDGET-(s->resident_bytes-e->bytes)){s->budget_failures++;s->failed_bytes+=bytes;return;}
    uint8_t *snapshot=e->snapshot;
    if(e->bytes!=bytes){
#ifdef NF_COLOR_REUSE110_TEST
        if(nf_color_reuse110_fail_alloc)snapshot=NULL;else
#endif
            snapshot=(uint8_t*)malloc(bytes);
        if(!snapshot){s->allocation_failures++;s->failed_bytes+=bytes;return;}
    }
    for(unsigned y=0;y<h;y++)memcpy(snapshot+(size_t)y*row,(const uint8_t*)mapped+(size_t)y*pitch,row);
    if(e->resource && e->resource!=resource)s->slot_replacements++;
    else if(e->resource && e->generation!=generation)s->generation_replacements++;
    if(snapshot!=e->snapshot){free(e->snapshot);s->resident_bytes-=e->bytes;s->resident_bytes+=bytes;}
    if(s->resident_bytes>s->peak_bytes)s->peak_bytes=s->resident_bytes;
    e->resource=resource;e->generation=generation;e->width=w;e->height=h;e->snapshot=snapshot;e->bytes=bytes;e->valid=1;e->used=++s->sequence;
    s->captured++;s->captured_bytes+=bytes;
}
static int nf_color_reuse110_same(const void *resource,uint64_t generation,unsigned w,unsigned h,const void *guest,size_t pitch){
    NFColorReuse110State *s=&nf_color_reuse110;s->comparisons++;
    if(!nf_cr110_layout(resource,w,h,guest,pitch)){s->compare_invalid++;return 0;}
    size_t row=(size_t)w*4,bytes=row*h;s->requested_bytes+=bytes;
    NFColorReuse110Slot *e=nf_cr110_find(resource);
    if(!e){s->missing++;s->missing_bytes+=bytes;return 0;}
    e->used=++s->sequence;
    if(e->generation!=generation || e->width!=w || e->height!=h){s->replaced++;s->replaced_bytes+=bytes;return 0;}
    if(!e->valid){s->invalidated++;s->invalidated_bytes+=bytes;return 0;}
    for(unsigned y=0;y<h;y++)if(!nf_bytes_equal(e->snapshot+(size_t)y*row,(const uint8_t*)guest+(size_t)y*pitch,row)){
        s->changed++;s->changed_bytes+=bytes;return 0;
    }
    s->exact++;s->exact_bytes+=bytes;return 1;
}
static void nf_color_reuse110_report(FILE *file){
    if(!file)return;const NFColorReuse110State *s=&nf_color_reuse110;
#define NF_CR110_U(v) (unsigned long long)s->v
    fprintf(file,"[COLOR-REUSE110] captures=%llu captured=%llu captured_bytes=%llu capture_invalid=%llu alloc_failures=%llu budget_failures=%llu failed_bytes=%llu comparisons=%llu requested_bytes=%llu exact=%llu exact_bytes=%llu changed=%llu changed_bytes=%llu missing=%llu missing_bytes=%llu replaced=%llu replaced_bytes=%llu invalidated=%llu invalidated_bytes=%llu compare_invalid=%llu slot_replacements=%llu generation_replacements=%llu invalidations=%llu resident_bytes=%llu peak_bytes=%llu\n",
        NF_CR110_U(captures),NF_CR110_U(captured),NF_CR110_U(captured_bytes),NF_CR110_U(capture_invalid),NF_CR110_U(allocation_failures),NF_CR110_U(budget_failures),NF_CR110_U(failed_bytes),NF_CR110_U(comparisons),NF_CR110_U(requested_bytes),NF_CR110_U(exact),NF_CR110_U(exact_bytes),NF_CR110_U(changed),NF_CR110_U(changed_bytes),NF_CR110_U(missing),NF_CR110_U(missing_bytes),NF_CR110_U(replaced),NF_CR110_U(replaced_bytes),NF_CR110_U(invalidated),NF_CR110_U(invalidated_bytes),NF_CR110_U(compare_invalid),NF_CR110_U(slot_replacements),NF_CR110_U(generation_replacements),NF_CR110_U(invalidations),NF_CR110_U(resident_bytes),NF_CR110_U(peak_bytes));
#undef NF_CR110_U
}
#endif
