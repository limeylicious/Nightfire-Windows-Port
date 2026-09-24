/* Optional immutable RAW input cache, never a cache of transformed vertices.
 * Caller must already have completed normal descriptor/index validation and
 * pre-prepare guest-surface alias guards. source is live ordinary CPU memory;
 * an address is only a lookup lead. Every returned buffer passed exact current
 * payload comparison, including padding and every ordered compact record.
 *
 * First sighting captures owned probation bytes. An exact later-present match
 * creates an immutable VB from those owned bytes; later hits avoid only VB
 * upload. Caller retains IB upload, topology, draw, alias/depth effects and all
 * completion boundaries; bind returned borrowed buffer at slot0/offset0.
 * NULL means the unmodified fresh ring path. No caller source/map writes.
 *
 * No eviction, buffer updates or production releases. Even disabled entries
 * retain resources until process end, so queued lifetimes need no new waits.
 * Budget counts CPU payload/maps plus reserved GPU ByteWidth (not undocumented
 * driver allocation overhead). Failed malloc stops new admissions; existing
 * entries remain exact-checkable. Changed/CreateBuffer-failed keys are disabled
 * permanently and no longer compared, since they always use fresh ring data.
 * This helper belongs to one backend device; another device is rejected.
 * OFF performs no key/source observations. No per-call clocks. */
#ifndef NIGHTFIRE_VERTEX_CACHE106_H
#define NIGHTFIRE_VERTEX_CACHE106_H
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <d3d11.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef NIGHTFIRE_VERTEX_CACHE106
#include <stdlib.h>
#include <string.h>
#include "nightfire_bytes_equal.h"
enum {NF_VC106_ENTRIES=2048,NF_VC106_BUCKETS=4096,NF_VC106_PROBES=64,
      NF_VC106_BUDGET=16*1024*1024,NF_VC106_MAX_PAYLOAD=256*1024,
      NF_VC106_PROBATION=1,NF_VC106_RESIDENT=2,NF_VC106_DISABLED=3};
typedef struct {
    uintptr_t source;
    uint64_t hash;
    unsigned stride,vertices,source_vertices,compact,state,first_frame,last_hit_frame;
    uint8_t *snapshot; /* Payload immediately followed by owned compact map. */
    ID3D11Buffer *buffer;
} NFVertexCache106Entry;
typedef struct {
    ID3D11Device *device; /* Identity only; backend owns the device lifetime. */
    unsigned entries,admission_stopped;
    size_t budget_reserved,cpu_bytes,gpu_reserved,gpu_created_bytes;
    uint64_t calls,hits,avoided_bytes,later_present_hits,probation,probation_calls,promotions;
    uint64_t mismatch,suppressed,exclusions,oversize,entry_limit,byte_limit,probe_limit,invalid;
    uint64_t allocation_failures,create_failures,admission_suppressed,comparisons,compare_bytes,probes;
    uint64_t kind_calls[2],kind_hits[2],kind_avoided_bytes[2];
    NFVertexCache106Entry entry[NF_VC106_ENTRIES];
    uint16_t buckets[NF_VC106_BUCKETS];
} NFVertexCache106State;
static NFVertexCache106State nf_vertex_cache106;
static int nf_vertex_cache106_on=-1;
#ifdef NF_VERTEX_CACHE106_TEST
static unsigned nf_vertex_cache106_force_collision,nf_vertex_cache106_fail_alloc,nf_vertex_cache106_fail_create;
static HRESULT (*nf_vertex_cache106_create_hook)(ID3D11Device*,const D3D11_BUFFER_DESC*,const D3D11_SUBRESOURCE_DATA*,ID3D11Buffer**);
static void (*nf_vertex_cache106_release_hook)(ID3D11Buffer*);
/* Only for fixtures after all GPU users are synchronized; never call in play. */
static void nf_vertex_cache106_test_reset(void){
    for(unsigned i=0;i<nf_vertex_cache106.entries;i++){
        NFVertexCache106Entry *e=&nf_vertex_cache106.entry[i];
        if(e->buffer){if(nf_vertex_cache106_release_hook)nf_vertex_cache106_release_hook(e->buffer);else ID3D11Buffer_Release(e->buffer);}
        free(e->snapshot);
    }
    memset(&nf_vertex_cache106,0,sizeof nf_vertex_cache106);nf_vertex_cache106_on=-1;
    nf_vertex_cache106_force_collision=nf_vertex_cache106_fail_alloc=nf_vertex_cache106_fail_create=0;
    nf_vertex_cache106_create_hook=NULL;nf_vertex_cache106_release_hook=NULL;
}
#endif
static int nf_vertex_cache106_enabled(void){
    if(nf_vertex_cache106_on<0){const char *v=getenv("NIGHTFIRE_VERTEX_CACHE106");nf_vertex_cache106_on=v && !strcmp(v,"1");}
    return nf_vertex_cache106_on;
}
static uint64_t nf_vc106_mix(uint64_t x){x^=x>>30;x*=0xbf58476d1ce4e5b9ull;x^=x>>27;x*=0x94d049bb133111ebull;return x^(x>>31);}
static uint64_t nf_vc106_hash(uintptr_t source,unsigned stride,unsigned vertices,unsigned source_vertices,const uint16_t *map){
#ifdef NF_VERTEX_CACHE106_TEST
    if(nf_vertex_cache106_force_collision)return 1;
#endif
    uint64_t h=nf_vc106_mix((uint64_t)source)^nf_vc106_mix(((uint64_t)source_vertices<<32)|vertices);
    h^=((uint64_t)stride<<1)|(map!=NULL);
    if(map)for(unsigned i=0;i<vertices;i++){h=(h<<13)|(h>>51);h=(h^map[i])*0x9e3779b185ebca87ull;}
    return nf_vc106_mix(h);
}
static int nf_vc106_payload_equal(const NFVertexCache106Entry *e,const void *source,unsigned bytes,const uint16_t *map){
    nf_vertex_cache106.comparisons++;nf_vertex_cache106.compare_bytes+=bytes;
    if(!map)return nf_bytes_equal(e->snapshot,source,bytes);
    const uint8_t *p=(const uint8_t*)source;
    for(unsigned i=0;i<e->vertices;i++)if(!nf_bytes_equal(e->snapshot+(size_t)i*e->stride,p+(size_t)map[i]*e->stride,e->stride))return 0;
    return 1;
}
static ID3D11Buffer *nf_vertex_cache106_get(ID3D11Device *dev,unsigned frame,const void *source,unsigned stride,unsigned vertices,unsigned source_vertices,const uint16_t *map){
    if(!nf_vertex_cache106_enabled())return NULL;
    NFVertexCache106State *s=&nf_vertex_cache106;unsigned kind=map!=NULL;s->calls++;s->kind_calls[kind]++;
    if(!dev || !source || (stride!=28 && stride!=32) || !vertices || vertices>65535 || !source_vertices || source_vertices>65535 || (!map && source_vertices!=vertices)){
        s->invalid++;s->exclusions++;return NULL;
    }
    if(s->device && s->device!=dev){s->invalid++;s->exclusions++;return NULL;}s->device=dev;
    unsigned bytes=vertices*stride;size_t map_bytes=map?(size_t)vertices*2:0;
    if(bytes>NF_VC106_MAX_PAYLOAD){s->oversize++;s->exclusions++;return NULL;}
    if(map)for(unsigned i=0;i<vertices;i++)if(map[i]>=source_vertices){s->invalid++;s->exclusions++;return NULL;}
    uint64_t hash=nf_vc106_hash((uintptr_t)source,stride,vertices,source_vertices,map);
    unsigned bucket=(unsigned)hash&(NF_VC106_BUCKETS-1),empty=NF_VC106_BUCKETS;
    for(unsigned probe=0;probe<NF_VC106_PROBES;probe++,bucket=(bucket+1)&(NF_VC106_BUCKETS-1)){
        s->probes++;unsigned index=s->buckets[bucket];if(!index){empty=bucket;break;}
        NFVertexCache106Entry *e=&s->entry[index-1];
        if(e->hash!=hash || e->source!=(uintptr_t)source || e->stride!=stride || e->vertices!=vertices || e->source_vertices!=source_vertices || e->compact!=kind)continue;
        if(map && memcmp(e->snapshot+bytes,map,map_bytes))continue;
        if(e->state==NF_VC106_DISABLED){s->suppressed++;return NULL;}
        if(!nf_vc106_payload_equal(e,source,bytes,map)){e->state=NF_VC106_DISABLED;s->mismatch++;return NULL;}
        if(e->state==NF_VC106_PROBATION){
            s->probation_calls++;
            /* unsigned frame wrap works; rollback is not a later frame. */
            if((int32_t)(frame-e->first_frame)<=0)return NULL;
            D3D11_BUFFER_DESC desc={0};desc.ByteWidth=bytes;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA initial={0};initial.pSysMem=e->snapshot;
            HRESULT result;
#ifdef NF_VERTEX_CACHE106_TEST
            if(nf_vertex_cache106_fail_create)result=E_OUTOFMEMORY;
            else if(nf_vertex_cache106_create_hook)result=nf_vertex_cache106_create_hook(dev,&desc,&initial,&e->buffer);
            else
#endif
                result=ID3D11Device_CreateBuffer(dev,&desc,&initial,&e->buffer);
            if(FAILED(result) || !e->buffer){e->state=NF_VC106_DISABLED;s->create_failures++;return NULL;}
            e->state=NF_VC106_RESIDENT;e->last_hit_frame=frame;s->promotions++;s->gpu_created_bytes+=bytes;
            /* Promotion substitutes immutable upload for this draw's ring
             * upload. It is not counted as an avoided upload or cache hit. */
            return e->buffer;
        }
        s->hits++;s->avoided_bytes+=bytes;s->kind_hits[kind]++;s->kind_avoided_bytes[kind]+=bytes;
        if(frame!=e->last_hit_frame)s->later_present_hits++;
        e->last_hit_frame=frame;return e->buffer;
    }
    if(empty==NF_VC106_BUCKETS){s->probe_limit++;s->exclusions++;return NULL;}
    if(s->admission_stopped){s->admission_suppressed++;s->exclusions++;return NULL;}
    if(s->entries==NF_VC106_ENTRIES){s->entry_limit++;s->exclusions++;return NULL;}
    size_t reservation=(size_t)bytes*2+map_bytes;
    if(reservation>NF_VC106_BUDGET-s->budget_reserved){s->byte_limit++;s->exclusions++;return NULL;}
    uint8_t *snapshot;
#ifdef NF_VERTEX_CACHE106_TEST
    if(nf_vertex_cache106_fail_alloc)snapshot=NULL;else
#endif
        snapshot=(uint8_t*)malloc(bytes+map_bytes);
    if(!snapshot){s->allocation_failures++;s->admission_stopped=1;s->exclusions++;return NULL;}
    if(map){for(unsigned i=0;i<vertices;i++)memcpy(snapshot+(size_t)i*stride,(const uint8_t*)source+(size_t)map[i]*stride,stride);memcpy(snapshot+bytes,map,map_bytes);}
    else memcpy(snapshot,source,bytes);
    NFVertexCache106Entry *e=&s->entry[s->entries];e->source=(uintptr_t)source;e->hash=hash;e->stride=stride;e->vertices=vertices;e->source_vertices=source_vertices;e->compact=kind;
    e->state=NF_VC106_PROBATION;e->first_frame=frame;e->snapshot=snapshot;
    s->buckets[empty]=(uint16_t)(++s->entries);s->budget_reserved+=reservation;s->cpu_bytes+=bytes+map_bytes;s->gpu_reserved+=bytes;s->probation++;
    return NULL;
}
static void nf_vertex_cache106_report(FILE *file){
    if(!file)return;const NFVertexCache106State *s=&nf_vertex_cache106;
    fprintf(file,"[VERTEX-CACHE106] enabled=%d calls=%llu hits=%llu avoided_bytes=%llu later_present_hits=%llu probation=%llu probation_calls=%llu promotions=%llu mismatch=%llu suppressed=%llu exclusions=%llu oversize=%llu entry_limit=%llu byte_limit=%llu probe_limit=%llu invalid=%llu alloc_failures=%llu create_failures=%llu admission_stopped=%u admission_suppressed=%llu entries=%u budget=%llu cpu_bytes=%llu gpu_reserved=%llu gpu_created_bytes=%llu comparisons=%llu compare_requested_bytes=%llu probes=%llu contiguous_calls=%llu contiguous_hits=%llu contiguous_avoided=%llu compact_calls=%llu compact_hits=%llu compact_avoided=%llu\n",
        nf_vertex_cache106_enabled(),(unsigned long long)s->calls,(unsigned long long)s->hits,(unsigned long long)s->avoided_bytes,(unsigned long long)s->later_present_hits,
        (unsigned long long)s->probation,(unsigned long long)s->probation_calls,(unsigned long long)s->promotions,(unsigned long long)s->mismatch,(unsigned long long)s->suppressed,
        (unsigned long long)s->exclusions,(unsigned long long)s->oversize,(unsigned long long)s->entry_limit,(unsigned long long)s->byte_limit,(unsigned long long)s->probe_limit,(unsigned long long)s->invalid,
        (unsigned long long)s->allocation_failures,(unsigned long long)s->create_failures,s->admission_stopped,(unsigned long long)s->admission_suppressed,s->entries,
        (unsigned long long)s->budget_reserved,(unsigned long long)s->cpu_bytes,(unsigned long long)s->gpu_reserved,(unsigned long long)s->gpu_created_bytes,
        (unsigned long long)s->comparisons,(unsigned long long)s->compare_bytes,(unsigned long long)s->probes,
        (unsigned long long)s->kind_calls[0],(unsigned long long)s->kind_hits[0],(unsigned long long)s->kind_avoided_bytes[0],
        (unsigned long long)s->kind_calls[1],(unsigned long long)s->kind_hits[1],(unsigned long long)s->kind_avoided_bytes[1]);
}
#else
static int nf_vertex_cache106_enabled(void){return 0;}
static ID3D11Buffer *nf_vertex_cache106_get(ID3D11Device *dev,unsigned frame,const void *source,unsigned stride,unsigned vertices,unsigned source_vertices,const uint16_t *map){(void)dev;(void)frame;(void)source;(void)stride;(void)vertices;(void)source_vertices;(void)map;return NULL;}
static void nf_vertex_cache106_report(FILE *file){(void)file;}
#endif
#endif
