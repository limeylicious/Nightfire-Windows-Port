/* Diagnostic only: exact reuse of bytes actually submitted by raw_indexed92.
 * Integration: arm once at a chosen present; wants(frame) gates private scratch
 * capture. After successful VB Map, capture the retained ordered source reads
 * into ordinary CPU scratch, upload THAT scratch, then observe it. Never read
 * write-combined mapped GPU memory. Keep all existing pre-prepare alias guards,
 * validation, uploads, draws, synchronization and source lifetime rules.
 * Counts are taken after the VB copy, before successful IB upload/DrawIndexed;
 * a subsequent IB/draw failure means the last observed upload did not draw.
 *
 * source is an identity integer, never dereferenced. submitted_bytes holds
 * vertices*stride bytes. A compact map has vertices ordered source indices;
 * source_vertices describes its validated source span. A NULL map denotes a
 * contiguous packet and requires source_vertices==vertices. No index packet,
 * shader, constants or topology are cached: this surveys input bytes only.
 *
 * Immutable FIRST snapshot per exact key: B after A remains changed even if B
 * repeats. Returning to A is an exact match. No eviction or alternate versions.
 * Results are bounded lower-coverage observations, not static-geometry proof or
 * an FPS benchmark. Extra private copies/allocations and QPC perturb the run. */
#ifndef NIGHTFIRE_VERTEX_REUSE105_H
#define NIGHTFIRE_VERTEX_REUSE105_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef NIGHTFIRE_VERTEX_REUSE105_DIAGNOSTIC
#include <windows.h>
#include <stdlib.h>
#include <string.h>
enum {NF_VR105_ENTRIES=2048,NF_VR105_BUCKETS=4096,NF_VR105_PROBES=64,
      NF_VR105_BUDGET=16*1024*1024,NF_VR105_FRAMES=60};
typedef struct {
    uintptr_t source;
    uint64_t hash,last_match_sequence;
    unsigned stride,vertices,source_vertices,compact,first_frame,last_match_frame;
    unsigned changed;
    uint8_t *snapshot; /* payload then complete ordered map; owned and immutable */
} NFVertexReuse105Entry;
typedef struct {
    uint64_t observed,covered,excluded,first_sightings,exact_matches,changed_calls,cross_present;
    uint64_t observed_bytes,covered_bytes,excluded_bytes,first_bytes,matched_bytes,changed_bytes;
} NFVertexReuse105Kind;
typedef struct {
    unsigned started,finished,start_frame,last_frame,entries,reported;
    size_t resident_bytes;
    uint64_t frequency,clock_failures,sequence,observed,covered,excluded;
    uint64_t first_sightings,exact_matches,changed_calls,changed_keys,cross_present;
    uint64_t observed_bytes,covered_bytes,matched_bytes,changed_bytes,excluded_bytes;
    uint64_t invalid,entry_limit,byte_limit,probe_limit,allocation_failures;
    uint64_t hash_collisions,probes,key_map_bytes,compare_requested_bytes,compare_ticks;
    uint64_t reuse[7]; /* distance since previous exact match, in observed calls */
    NFVertexReuse105Kind kind[2]; /* 0 contiguous, 1 compact; same shared budget */
    NFVertexReuse105Entry entry[NF_VR105_ENTRIES];
    uint16_t buckets[NF_VR105_BUCKETS]; /* entry index+1; no deletion */
} NFVertexReuse105State;
static NFVertexReuse105State nf_vertex_reuse105;
#ifdef NF_VERTEX_REUSE105_TEST
static unsigned nf_vertex_reuse105_force_collision;
static void nf_vertex_reuse105_test_reset(void){
    for(unsigned i=0;i<nf_vertex_reuse105.entries;i++)free(nf_vertex_reuse105.entry[i].snapshot);
    memset(&nf_vertex_reuse105,0,sizeof nf_vertex_reuse105);
    nf_vertex_reuse105_force_collision=0;
}
#endif
static uint64_t nf_vr105_clock(void){
    LARGE_INTEGER t;if(!nf_vertex_reuse105.frequency)return 0;
    if(!QueryPerformanceCounter(&t)){nf_vertex_reuse105.clock_failures++;return 0;}
    return (uint64_t)t.QuadPart;
}
static int nf_vr105_equal(const void *a,const void *b,size_t n){
    uint64_t start=nf_vr105_clock();int same=!memcmp(a,b,n);uint64_t end=nf_vr105_clock();
    if(start && end>=start)nf_vertex_reuse105.compare_ticks+=end-start;
    return same;
}
static uint64_t nf_vr105_hash_word(uint64_t h,uint64_t word){
    for(unsigned i=0;i<8;i++){h^=(uint8_t)word;h*=1099511628211ull;word>>=8;}return h;
}
static uint64_t nf_vr105_hash(uintptr_t source,unsigned stride,unsigned vertices,unsigned source_vertices,const uint16_t *map){
#ifdef NF_VERTEX_REUSE105_TEST
    if(nf_vertex_reuse105_force_collision)return 1;
#endif
    uint64_t h=14695981039346656037ull;
    h=nf_vr105_hash_word(h,source);h=nf_vr105_hash_word(h,stride);
    h=nf_vr105_hash_word(h,vertices);h=nf_vr105_hash_word(h,source_vertices);
    h=nf_vr105_hash_word(h,map!=NULL);
    if(map)for(unsigned i=0;i<vertices;i++){
        h^=(uint8_t)map[i];h*=1099511628211ull;
        h^=(uint8_t)(map[i]>>8);h*=1099511628211ull;
    }
    return h;
}
static int nf_vertex_reuse105_begin(unsigned frame){
    NFVertexReuse105State *s=&nf_vertex_reuse105;if(s->started)return 0;
    s->started=1;s->start_frame=s->last_frame=frame;
    LARGE_INTEGER f;if(QueryPerformanceFrequency(&f) && f.QuadPart>0)s->frequency=(uint64_t)f.QuadPart;else s->clock_failures++;
    return 1;
}
static int nf_vertex_reuse105_wants(unsigned frame){
    NFVertexReuse105State *s=&nf_vertex_reuse105;
    if(!s->started || s->finished)return 0;
    unsigned delta=frame-s->start_frame;
    if((int32_t)delta<0)return 0; /* Earlier frame, not a new/rolled-back window. */
    if(delta>=NF_VR105_FRAMES){s->finished=1;return 0;}
    return 1; /* Present counter wrap is accepted within the sixty-frame span. */
}
static void nf_vertex_reuse105_observe(unsigned frame,uintptr_t source,unsigned stride,unsigned vertices,unsigned source_vertices,const uint16_t *map,const void *submitted_bytes){
    NFVertexReuse105State *s=&nf_vertex_reuse105;
    if(!nf_vertex_reuse105_wants(frame))return;
    NFVertexReuse105Kind *k=&s->kind[map!=NULL];
    s->last_frame=frame;s->observed++;s->sequence++;k->observed++;
    if(!submitted_bytes || (stride!=28 && stride!=32) || !vertices || vertices>65535 ||
       !source_vertices || source_vertices>65535 || (!map && source_vertices!=vertices)){
        s->invalid++;s->excluded++;k->excluded++;return;
    }
    size_t bytes=(size_t)vertices*stride,map_bytes=map?(size_t)vertices*2:0;
    s->observed_bytes+=bytes;k->observed_bytes+=bytes;
    if(map)for(unsigned i=0;i<vertices;i++)if(map[i]>=source_vertices){s->invalid++;s->excluded++;s->excluded_bytes+=bytes;k->excluded++;k->excluded_bytes+=bytes;return;}
    uint64_t hash=nf_vr105_hash(source,stride,vertices,source_vertices,map);
    unsigned bucket=(unsigned)hash&(NF_VR105_BUCKETS-1),empty=NF_VR105_BUCKETS;
    for(unsigned probe=0;probe<NF_VR105_PROBES;probe++,bucket=(bucket+1)&(NF_VR105_BUCKETS-1)){
        s->probes++;unsigned index=s->buckets[bucket];
        if(!index){empty=bucket;break;}
        NFVertexReuse105Entry *e=&s->entry[index-1];
        if(e->hash!=hash || e->source!=source || e->stride!=stride || e->vertices!=vertices ||
           e->source_vertices!=source_vertices || e->compact!=(map!=NULL)){s->hash_collisions++;continue;}
        if(map){s->key_map_bytes+=map_bytes;if(!nf_vr105_equal(e->snapshot+bytes,map,map_bytes)){s->hash_collisions++;continue;}}
        s->covered++;s->covered_bytes+=bytes;s->compare_requested_bytes+=bytes;k->covered++;k->covered_bytes+=bytes;
        if(nf_vr105_equal(e->snapshot,submitted_bytes,bytes)){
            uint64_t distance=s->sequence-e->last_match_sequence;unsigned bin=distance<=1?0:distance<=8?1:distance<=64?2:distance<=256?3:distance<=1024?4:distance<=4096?5:6;
            s->reuse[bin]++;s->exact_matches++;s->matched_bytes+=bytes;k->exact_matches++;k->matched_bytes+=bytes;
            if(frame!=e->last_match_frame){s->cross_present++;k->cross_present++;}
            e->last_match_frame=frame;e->last_match_sequence=s->sequence;
        }else{s->changed_calls++;s->changed_bytes+=bytes;k->changed_calls++;k->changed_bytes+=bytes;if(!e->changed){e->changed=1;s->changed_keys++;}}
        return;
    }
    if(empty==NF_VR105_BUCKETS){s->probe_limit++;s->excluded++;s->excluded_bytes+=bytes;k->excluded++;k->excluded_bytes+=bytes;return;}
    if(s->entries==NF_VR105_ENTRIES){s->entry_limit++;s->excluded++;s->excluded_bytes+=bytes;k->excluded++;k->excluded_bytes+=bytes;return;}
    if(bytes+map_bytes>NF_VR105_BUDGET-s->resident_bytes){s->byte_limit++;s->excluded++;s->excluded_bytes+=bytes;k->excluded++;k->excluded_bytes+=bytes;return;}
    uint8_t *copy=malloc(bytes+map_bytes);
    if(!copy){s->allocation_failures++;s->excluded++;s->excluded_bytes+=bytes;k->excluded++;k->excluded_bytes+=bytes;return;}
    memcpy(copy,submitted_bytes,bytes);if(map)memcpy(copy+bytes,map,map_bytes);
    NFVertexReuse105Entry *e=&s->entry[s->entries];
    e->source=source;e->hash=hash;e->stride=stride;e->vertices=vertices;e->source_vertices=source_vertices;e->compact=map!=NULL;
    e->first_frame=e->last_match_frame=frame;e->last_match_sequence=s->sequence;e->snapshot=copy;
    s->buckets[empty]=(uint16_t)(++s->entries);s->resident_bytes+=bytes+map_bytes;
    s->first_sightings++;s->covered++;s->covered_bytes+=bytes;k->first_sightings++;k->first_bytes+=bytes;k->covered++;k->covered_bytes+=bytes;
}
static void nf_vertex_reuse105_report(FILE *file){
    NFVertexReuse105State *s=&nf_vertex_reuse105;if(!file || !s->started)return;
    unsigned bit=s->finished?2:1;if(s->reported&bit)return;s->reported|=bit;
    fprintf(file,"[VERTEX-REUSE105] status=%s start=%u last=%u window=%u entries=%u resident=%llu observed=%llu covered=%llu excluded=%llu first=%llu exact=%llu changed_calls=%llu changed_keys=%llu cross_present=%llu observed_bytes=%llu covered_bytes=%llu matched_bytes=%llu changed_bytes=%llu excluded_bytes=%llu\n",
        s->finished?"complete":"partial",s->start_frame,s->last_frame,NF_VR105_FRAMES,s->entries,(unsigned long long)s->resident_bytes,
        (unsigned long long)s->observed,(unsigned long long)s->covered,(unsigned long long)s->excluded,(unsigned long long)s->first_sightings,
        (unsigned long long)s->exact_matches,(unsigned long long)s->changed_calls,(unsigned long long)s->changed_keys,(unsigned long long)s->cross_present,
        (unsigned long long)s->observed_bytes,(unsigned long long)s->covered_bytes,(unsigned long long)s->matched_bytes,(unsigned long long)s->changed_bytes,(unsigned long long)s->excluded_bytes);
    fprintf(file,"[VERTEX-REUSE105-DETAIL] invalid=%llu entries_full=%llu bytes_full=%llu probe_full=%llu allocation_failures=%llu probes=%llu collisions=%llu key_map_bytes=%llu compare_requested_bytes=%llu compare_ticks=%llu qpc_frequency=%llu clock_failures=%llu reuse_dist_1_8_64_256_1024_4096_more=%llu,%llu,%llu,%llu,%llu,%llu,%llu first_snapshot_only=1 comparison_includes_map=1 no_eviction=1 scope=validated_raw_uploads diagnostic_not_fps=1\n",
        (unsigned long long)s->invalid,(unsigned long long)s->entry_limit,(unsigned long long)s->byte_limit,(unsigned long long)s->probe_limit,(unsigned long long)s->allocation_failures,
        (unsigned long long)s->probes,(unsigned long long)s->hash_collisions,(unsigned long long)s->key_map_bytes,(unsigned long long)s->compare_requested_bytes,
        (unsigned long long)s->compare_ticks,(unsigned long long)s->frequency,(unsigned long long)s->clock_failures,
        (unsigned long long)s->reuse[0],(unsigned long long)s->reuse[1],(unsigned long long)s->reuse[2],(unsigned long long)s->reuse[3],(unsigned long long)s->reuse[4],(unsigned long long)s->reuse[5],(unsigned long long)s->reuse[6]);
    for(unsigned i=0;i<2;i++){
        const NFVertexReuse105Kind *k=&s->kind[i];
        fprintf(file,"[VERTEX-REUSE105-KIND] kind=%s observed=%llu covered=%llu excluded=%llu first=%llu exact=%llu changed=%llu cross_present=%llu observed_bytes=%llu covered_bytes=%llu excluded_bytes=%llu first_bytes=%llu matched_bytes=%llu changed_bytes=%llu\n",
            i?"compact":"contiguous",(unsigned long long)k->observed,(unsigned long long)k->covered,(unsigned long long)k->excluded,
            (unsigned long long)k->first_sightings,(unsigned long long)k->exact_matches,(unsigned long long)k->changed_calls,(unsigned long long)k->cross_present,
            (unsigned long long)k->observed_bytes,(unsigned long long)k->covered_bytes,(unsigned long long)k->excluded_bytes,
            (unsigned long long)k->first_bytes,(unsigned long long)k->matched_bytes,(unsigned long long)k->changed_bytes);
    }
}
#else
static int nf_vertex_reuse105_begin(unsigned frame){(void)frame;return 0;}
static int nf_vertex_reuse105_wants(unsigned frame){(void)frame;return 0;}
static void nf_vertex_reuse105_observe(unsigned frame,uintptr_t source,unsigned stride,unsigned vertices,unsigned source_vertices,const uint16_t *map,const void *submitted_bytes){(void)frame;(void)source;(void)stride;(void)vertices;(void)source_vertices;(void)map;(void)submitted_bytes;}
static void nf_vertex_reuse105_report(FILE *file){(void)file;}
#endif
#endif
