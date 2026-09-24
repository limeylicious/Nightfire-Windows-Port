/* Diagnostic only: exact visible COLOR bytes, never depth or upload selection.
 * Arm once for [frame,frame+60). capture receives an already mapped GPU color
 * readback BEFORE guest depth packing; compare receives the next guest color
 * upload source BEFORE the unchanged upload. Copy/compare width*4 bytes per
 * row, not pitch padding. Both input allocations and pitches remain caller
 * validated/readable for these calls; no guest/source/resource writes.
 *
 * Resource is an opaque physical GPU identity, never dereferenced. Generation
 * must match capture->compare. Caller MUST invalidate(resource) for every color
 * upload, GPU draw, resource reuse/destruction or other GPU color mutation.
 * A subsequent completed mapped readback reestablishes validity. No protection,
 * GPU calls, waits or new completion boundaries. CPU writes between endpoints
 * may occur: exact means current bytes equal the saved GPU result, not no use.
 *
 * Four private CPU snapshots, 16 MiB logical allocation budget. A fifth physical
 * identity replaces the least recently captured/compared slot. Allocation is
 * transactional (temporary peak <=32 MiB); failure invalidates the observation
 * and leaves rendering alone. No rearm; report at most one partial + one final
 * summary. Extra copies/comparisons/QPC perturb timing; this is no FPS baseline.
 * OFF calls do not inspect source pointers or obtain clocks. */
#ifndef NIGHTFIRE_COLOR_REUPLOAD109_H
#define NIGHTFIRE_COLOR_REUPLOAD109_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "nightfire_bytes_equal.h"
enum {NF_CR109_SLOTS=4,NF_CR109_FRAMES=60,NF_CR109_BUDGET=16*1024*1024};
typedef struct {
    const void *resource;
    uint64_t generation,used;
    unsigned width,height,valid;
    size_t bytes;
    uint8_t *snapshot;
} NFColorReupload109Slot;
typedef struct {
    unsigned started,finished,start_frame,last_frame,reported;
    size_t resident_bytes,peak_bytes;
    uint64_t sequence,frequency,clock_failures;
    uint64_t captures,captured_bytes,snapshot_failures,budget_failures,capture_invalid;
    uint64_t slot_replacements,generation_replacements,invalidations;
    uint64_t comparisons,compare_requested_bytes,compared,compared_bytes,compare_ticks;
    uint64_t exact,exact_bytes,changed,changed_bytes,missing,missing_bytes;
    uint64_t replaced,replaced_bytes,invalidated,invalidated_bytes,compare_invalid;
    NFColorReupload109Slot slot[NF_CR109_SLOTS];
} NFColorReupload109State;
static NFColorReupload109State nf_color_reupload109;
#ifdef NF_COLOR_REUPLOAD109_TEST
static unsigned nf_color_reupload109_fail_alloc;
static void nf_color_reupload109_test_reset(void){
    for(unsigned i=0;i<NF_CR109_SLOTS;i++)free(nf_color_reupload109.slot[i].snapshot);
    memset(&nf_color_reupload109,0,sizeof nf_color_reupload109);nf_color_reupload109_fail_alloc=0;
}
#endif
static int nf_color_reupload109_begin(unsigned frame){
    NFColorReupload109State *s=&nf_color_reupload109;if(s->started)return 0;
    s->started=1;s->start_frame=s->last_frame=frame;
    LARGE_INTEGER f;if(QueryPerformanceFrequency(&f) && f.QuadPart>0)s->frequency=(uint64_t)f.QuadPart;else s->clock_failures++;
    return 1;
}
static int nf_color_reupload109_wants(unsigned frame){
    NFColorReupload109State *s=&nf_color_reupload109;
    if(!s->started || s->finished)return 0;
    unsigned delta=frame-s->start_frame;if((int32_t)delta<0)return 0;
    if(delta>=NF_CR109_FRAMES){s->finished=1;return 0;}
    return 1;
}
static NFColorReupload109Slot *nf_cr109_find(const void *resource){
    for(unsigned i=0;i<NF_CR109_SLOTS;i++)if(nf_color_reupload109.slot[i].resource==resource)return &nf_color_reupload109.slot[i];
    return NULL;
}
static int nf_cr109_layout(const void *resource,unsigned w,unsigned h,const void *data,size_t pitch){
    if(!resource || !data || !w || !h || w>2048 || h>2048 || pitch<(size_t)w*4)return 0;
    size_t row=(size_t)w*4;
    if(h>1 && pitch>(SIZE_MAX-row)/(h-1))return 0;
    size_t span=(size_t)(h-1)*pitch+row;
    return span<=UINTPTR_MAX-(uintptr_t)data;
}
static void nf_color_reupload109_invalidate(const void *resource){
    NFColorReupload109State *s=&nf_color_reupload109;
    if(!s->started || s->finished || !resource)return;
    NFColorReupload109Slot *e=nf_cr109_find(resource);
    if(e && e->valid){e->valid=0;s->invalidations++;}
}
static void nf_color_reupload109_capture(unsigned frame,const void *resource,uint64_t generation,unsigned w,unsigned h,const void *mapped,size_t pitch){
    NFColorReupload109State *s=&nf_color_reupload109;if(!nf_color_reupload109_wants(frame))return;
    s->last_frame=frame;s->captures++;
    NFColorReupload109Slot *e=resource?nf_cr109_find(resource):NULL;
    if(e)e->valid=0; /* Never let a failed new observation preserve old truth. */
    if(!nf_cr109_layout(resource,w,h,mapped,pitch)){s->capture_invalid++;return;}
    size_t row=(size_t)w*4,bytes=row*h;
    if(!e){
        e=&s->slot[0];
        for(unsigned i=0;i<NF_CR109_SLOTS;i++){
            if(!s->slot[i].resource){e=&s->slot[i];break;}
            if(s->slot[i].used<e->used)e=&s->slot[i];
        }
    }
    if(bytes>NF_CR109_BUDGET-(s->resident_bytes-e->bytes)){s->budget_failures++;return;}
    uint8_t *snapshot=e->snapshot;
    if(e->bytes!=bytes){
#ifdef NF_COLOR_REUPLOAD109_TEST
        if(nf_color_reupload109_fail_alloc)snapshot=NULL;else
#endif
            snapshot=(uint8_t*)malloc(bytes);
        if(!snapshot){s->snapshot_failures++;return;}
    }
    /* CPU-owned destination is distinct from mapped readback/source memory. */
    for(unsigned y=0;y<h;y++)memcpy(snapshot+(size_t)y*row,(const uint8_t*)mapped+(size_t)y*pitch,row);
    if(e->resource && e->resource!=resource)s->slot_replacements++;
    else if(e->resource && e->generation!=generation)s->generation_replacements++;
    if(snapshot!=e->snapshot){free(e->snapshot);s->resident_bytes-=e->bytes;s->resident_bytes+=bytes;}
    if(s->resident_bytes>s->peak_bytes)s->peak_bytes=s->resident_bytes;
    e->resource=resource;e->generation=generation;e->width=w;e->height=h;e->snapshot=snapshot;e->bytes=bytes;e->valid=1;e->used=++s->sequence;
    s->captured_bytes+=bytes;
}
static uint64_t nf_cr109_clock(void){
    LARGE_INTEGER q;if(!nf_color_reupload109.frequency)return 0;
    if(!QueryPerformanceCounter(&q)){nf_color_reupload109.clock_failures++;return 0;}
    return (uint64_t)q.QuadPart;
}
static void nf_color_reupload109_compare(unsigned frame,const void *resource,uint64_t generation,unsigned w,unsigned h,const void *guest,size_t pitch){
    NFColorReupload109State *s=&nf_color_reupload109;if(!nf_color_reupload109_wants(frame))return;
    s->last_frame=frame;s->comparisons++;
    if(!nf_cr109_layout(resource,w,h,guest,pitch)){s->compare_invalid++;return;}
    size_t row=(size_t)w*4,bytes=row*h;s->compare_requested_bytes+=bytes;
    NFColorReupload109Slot *e=nf_cr109_find(resource);
    if(!e){s->missing++;s->missing_bytes+=bytes;return;}
    e->used=++s->sequence;
    if(e->generation!=generation || e->width!=w || e->height!=h){s->replaced++;s->replaced_bytes+=bytes;return;}
    if(!e->valid){s->invalidated++;s->invalidated_bytes+=bytes;return;}
    uint64_t start=nf_cr109_clock();int same=1;
    for(unsigned y=0;y<h;y++)if(!nf_bytes_equal(e->snapshot+(size_t)y*row,(const uint8_t*)guest+(size_t)y*pitch,row)){same=0;break;}
    uint64_t end=nf_cr109_clock();
    if(start && end>=start)s->compare_ticks+=end-start;
    else if(start && end && end<start)s->clock_failures++;
    s->compared++;s->compared_bytes+=bytes;
    if(same){s->exact++;s->exact_bytes+=bytes;}else{s->changed++;s->changed_bytes+=bytes;}
}
static void nf_color_reupload109_report(FILE *file){
    NFColorReupload109State *s=&nf_color_reupload109;if(!file || !s->started)return;
    unsigned bit=s->finished?2:1;if(s->reported&bit)return;s->reported|=bit;
#define NF_CR109_U(v) (unsigned long long)s->v
    fprintf(file,"[COLOR-REUPLOAD109] complete=%u frames=%u..%u captures=%llu captured_bytes=%llu snapshot_failures=%llu budget_failures=%llu capture_invalid=%llu slot_replacements=%llu generation_replacements=%llu invalidations=%llu comparisons=%llu requested_bytes=%llu compared=%llu compared_bytes=%llu exact=%llu exact_bytes=%llu changed=%llu changed_bytes=%llu missing=%llu missing_bytes=%llu replaced=%llu replaced_bytes=%llu invalidated=%llu invalidated_bytes=%llu compare_invalid=%llu resident_bytes=%llu peak_bytes=%llu compare_ticks=%llu qpc_frequency=%llu clock_failures=%llu compare_ms=%.6f\n",
        s->finished,s->start_frame,s->last_frame,NF_CR109_U(captures),NF_CR109_U(captured_bytes),NF_CR109_U(snapshot_failures),NF_CR109_U(budget_failures),NF_CR109_U(capture_invalid),NF_CR109_U(slot_replacements),NF_CR109_U(generation_replacements),NF_CR109_U(invalidations),NF_CR109_U(comparisons),NF_CR109_U(compare_requested_bytes),NF_CR109_U(compared),NF_CR109_U(compared_bytes),NF_CR109_U(exact),NF_CR109_U(exact_bytes),NF_CR109_U(changed),NF_CR109_U(changed_bytes),NF_CR109_U(missing),NF_CR109_U(missing_bytes),NF_CR109_U(replaced),NF_CR109_U(replaced_bytes),NF_CR109_U(invalidated),NF_CR109_U(invalidated_bytes),NF_CR109_U(compare_invalid),NF_CR109_U(resident_bytes),NF_CR109_U(peak_bytes),NF_CR109_U(compare_ticks),NF_CR109_U(frequency),NF_CR109_U(clock_failures),s->frequency?1000.0*s->compare_ticks/s->frequency:-1.0);
#undef NF_CR109_U
}
#else
static int nf_color_reupload109_begin(unsigned frame){(void)frame;return 0;}
static int nf_color_reupload109_wants(unsigned frame){(void)frame;return 0;}
static void nf_color_reupload109_capture(unsigned frame,const void *resource,uint64_t generation,unsigned w,unsigned h,const void *mapped,size_t pitch){(void)frame;(void)resource;(void)generation;(void)w;(void)h;(void)mapped;(void)pitch;}
static void nf_color_reupload109_compare(unsigned frame,const void *resource,uint64_t generation,unsigned w,unsigned h,const void *guest,size_t pitch){(void)frame;(void)resource;(void)generation;(void)w;(void)h;(void)guest;(void)pitch;}
static void nf_color_reupload109_invalidate(const void *resource){(void)resource;}
static void nf_color_reupload109_report(FILE *file){(void)file;}
#endif
#endif
