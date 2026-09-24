/* Optional raw-input cache in immutable 64-KiB pages. Caller must validate
 * descriptors/indices and finish pre-prepare surface alias guards first.
 * Every successful get compares all current submitted bytes (padding too),
 * and the exact ordered compact map. Bind the borrowed buffer at *offset.
 * NULL preserves the fresh ring path and leaves *offset untouched.
 *
 * First sighting copies into an owned, 16-byte-aligned probation slot. A key's
 * first exact later-frame use seals its page, if needed. Each sibling key must
 * independently pass later-frame admission, even when its page is sealed.
 * Sealed pages never receive new slots or writes. Changed keys and failed
 * pages stay disabled; no eviction or production releases, hence no queued
 * resource lifetime waits. Budget includes full CPU/GPU page capacities and
 * owned maps; fixed metadata and unknown driver overhead are separate.
 * Single backend/device/thread lifetime. OFF observes neither source nor map
 * nor offset. No per-call clocks. Test reset requires all GPU users finished. */
#ifndef NIGHTFIRE_VERTEX_CACHE107_H
#define NIGHTFIRE_VERTEX_CACHE107_H
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <d3d11.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#ifdef NIGHTFIRE_VERTEX_CACHE107
#include <stdlib.h>
#include <string.h>
#include "nightfire_bytes_equal.h"
enum {NF_VC107_ENTRIES=2048,NF_VC107_BUCKETS=4096,NF_VC107_PROBES=64,
      NF_VC107_BUDGET=16*1024*1024,NF_VC107_PAGE_BYTES=65536,NF_VC107_PAGES=128,
      NF_VC107_PROBATION=1,NF_VC107_RESIDENT=2,NF_VC107_DISABLED=3,
      NF_VC107_OPEN=1,NF_VC107_SEALED=2,NF_VC107_FAILED=3};
typedef struct {
    uint8_t *data;
    ID3D11Buffer *buffer;
    unsigned used,state;
} NFVertexCache107Page;
typedef struct {
    uintptr_t source;
    uint64_t hash;
    unsigned stride,vertices,source_vertices,compact,state,first_frame,last_hit_frame;
    unsigned page,offset;
    uint16_t *map;
} NFVertexCache107Entry;
typedef struct {
    ID3D11Device *device;
    unsigned entries,pages,open_page,admission_stopped; /* open_page is index+1. */
    size_t budget_reserved,cpu_bytes,gpu_reserved,gpu_created_bytes,payload_bytes,padding_bytes,map_bytes;
    uint64_t calls,hits,avoided_bytes,later_present_hits,probation,probation_calls,promotions,page_creates;
    uint64_t mismatch,suppressed,exclusions,oversize,entry_limit,byte_limit,probe_limit,invalid;
    uint64_t allocation_failures,create_failures,admission_suppressed,comparisons,compare_bytes,probes;
    uint64_t kind_calls[2],kind_hits[2],kind_avoided_bytes[2];
    NFVertexCache107Page page[NF_VC107_PAGES];
    NFVertexCache107Entry entry[NF_VC107_ENTRIES];
    uint16_t buckets[NF_VC107_BUCKETS];
} NFVertexCache107State;
static NFVertexCache107State nf_vertex_cache107;
static int nf_vertex_cache107_on=-1;
#ifdef NF_VERTEX_CACHE107_TEST
static unsigned nf_vertex_cache107_force_collision,nf_vertex_cache107_fail_alloc,nf_vertex_cache107_fail_create;
static HRESULT (*nf_vertex_cache107_create_hook)(ID3D11Device*,const D3D11_BUFFER_DESC*,const D3D11_SUBRESOURCE_DATA*,ID3D11Buffer**);
static void (*nf_vertex_cache107_release_hook)(ID3D11Buffer*);
static void nf_vertex_cache107_test_reset(void){
    for(unsigned i=0;i<nf_vertex_cache107.pages;i++){
        NFVertexCache107Page *p=&nf_vertex_cache107.page[i];
        if(p->buffer){if(nf_vertex_cache107_release_hook)nf_vertex_cache107_release_hook(p->buffer);else ID3D11Buffer_Release(p->buffer);}
        free(p->data);
    }
    for(unsigned i=0;i<nf_vertex_cache107.entries;i++)free(nf_vertex_cache107.entry[i].map);
    memset(&nf_vertex_cache107,0,sizeof nf_vertex_cache107);nf_vertex_cache107_on=-1;
    nf_vertex_cache107_force_collision=nf_vertex_cache107_fail_alloc=nf_vertex_cache107_fail_create=0;
    nf_vertex_cache107_create_hook=NULL;nf_vertex_cache107_release_hook=NULL;
}
#endif
static int nf_vertex_cache107_enabled(void){
    if(nf_vertex_cache107_on<0){const char *v=getenv("NIGHTFIRE_VERTEX_CACHE107");nf_vertex_cache107_on=v && !strcmp(v,"1");}
    return nf_vertex_cache107_on;
}
static uint64_t nf_vc107_mix(uint64_t x){x^=x>>30;x*=0xbf58476d1ce4e5b9ull;x^=x>>27;x*=0x94d049bb133111ebull;return x^(x>>31);}
static uint64_t nf_vc107_hash(uintptr_t source,unsigned stride,unsigned vertices,unsigned span,const uint16_t *map){
#ifdef NF_VERTEX_CACHE107_TEST
    if(nf_vertex_cache107_force_collision)return 1;
#endif
    uint64_t h=nf_vc107_mix((uint64_t)source)^nf_vc107_mix(((uint64_t)span<<32)|vertices);
    h^=((uint64_t)stride<<1)|(map!=NULL);
    if(map)for(unsigned i=0;i<vertices;i++){h=(h<<13)|(h>>51);h=(h^map[i])*0x9e3779b185ebca87ull;}
    return nf_vc107_mix(h);
}
static int nf_vc107_payload_equal(const NFVertexCache107Entry *e,const void *source,unsigned bytes,const uint16_t *map){
    const uint8_t *saved=nf_vertex_cache107.page[e->page].data+e->offset;
    nf_vertex_cache107.comparisons++;nf_vertex_cache107.compare_bytes+=bytes;
    if(!map)return nf_bytes_equal(saved,source,bytes);
    for(unsigned i=0;i<e->vertices;i++)if(!nf_bytes_equal(saved+(size_t)i*e->stride,(const uint8_t*)source+(size_t)map[i]*e->stride,e->stride))return 0;
    return 1;
}
static void *nf_vc107_alloc(size_t bytes){
#ifdef NF_VERTEX_CACHE107_TEST
    if(nf_vertex_cache107_fail_alloc)return NULL;
#endif
    return malloc(bytes);
}
static ID3D11Buffer *nf_vertex_cache107_get(ID3D11Device *dev,unsigned frame,const void *source,unsigned stride,unsigned vertices,unsigned source_vertices,const uint16_t *map,unsigned *offset){
    if(!nf_vertex_cache107_enabled())return NULL;
    NFVertexCache107State *s=&nf_vertex_cache107;unsigned kind=map!=NULL;s->calls++;s->kind_calls[kind]++;
    if(!dev || !source || !offset || (stride!=28 && stride!=32) || !vertices || vertices>65535 || !source_vertices || source_vertices>65535 || (!map && source_vertices!=vertices) || (s->device && s->device!=dev)){
        s->invalid++;s->exclusions++;return NULL;
    }
    unsigned bytes=vertices*stride;size_t map_bytes=map?(size_t)vertices*2:0;
    if(bytes>NF_VC107_PAGE_BYTES){s->oversize++;s->exclusions++;return NULL;}
    if(map)for(unsigned i=0;i<vertices;i++)if(map[i]>=source_vertices){s->invalid++;s->exclusions++;return NULL;}
    uint64_t hash=nf_vc107_hash((uintptr_t)source,stride,vertices,source_vertices,map);
    unsigned bucket=(unsigned)hash&(NF_VC107_BUCKETS-1),empty=NF_VC107_BUCKETS;
    for(unsigned probe=0;probe<NF_VC107_PROBES;probe++,bucket=(bucket+1)&(NF_VC107_BUCKETS-1)){
        s->probes++;unsigned index=s->buckets[bucket];if(!index){empty=bucket;break;}
        NFVertexCache107Entry *e=&s->entry[index-1];
        if(e->hash!=hash || e->source!=(uintptr_t)source || e->stride!=stride || e->vertices!=vertices || e->source_vertices!=source_vertices || e->compact!=kind)continue;
        if(map && memcmp(e->map,map,map_bytes))continue;
        NFVertexCache107Page *p=&s->page[e->page];
        if(e->state==NF_VC107_DISABLED || p->state==NF_VC107_FAILED){s->suppressed++;return NULL;}
        if(!nf_vc107_payload_equal(e,source,bytes,map)){e->state=NF_VC107_DISABLED;s->mismatch++;return NULL;}
        if(e->state==NF_VC107_PROBATION){
            s->probation_calls++;
            if((int32_t)(frame-e->first_frame)<=0)return NULL;
            if(p->state==NF_VC107_OPEN){
                /* Freeze first: no admission may ever append to this page,
                 * including after a failed CreateBuffer. Unused bytes are zero. */
                p->state=NF_VC107_FAILED;if(s->open_page==e->page+1)s->open_page=0;
                D3D11_BUFFER_DESC desc={0};desc.ByteWidth=NF_VC107_PAGE_BYTES;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
                D3D11_SUBRESOURCE_DATA initial={0};initial.pSysMem=p->data;HRESULT result;
#ifdef NF_VERTEX_CACHE107_TEST
                if(nf_vertex_cache107_fail_create)result=E_OUTOFMEMORY;
                else if(nf_vertex_cache107_create_hook)result=nf_vertex_cache107_create_hook(dev,&desc,&initial,&p->buffer);
                else
#endif
                    result=ID3D11Device_CreateBuffer(dev,&desc,&initial,&p->buffer);
                if(FAILED(result) || !p->buffer){s->create_failures++;return NULL;}
                p->state=NF_VC107_SEALED;s->page_creates++;s->gpu_created_bytes+=NF_VC107_PAGE_BYTES;
            }
            e->state=NF_VC107_RESIDENT;e->last_hit_frame=frame;s->promotions++;
            /* Count promotion separately even if a sibling already uploaded
             * the page. GPU byte counters count each full page exactly once. */
        }else{
            s->hits++;s->avoided_bytes+=bytes;s->kind_hits[kind]++;s->kind_avoided_bytes[kind]+=bytes;
            if(frame!=e->last_hit_frame)s->later_present_hits++;
            e->last_hit_frame=frame;
        }
        *offset=e->offset;return p->buffer;
    }
    if(empty==NF_VC107_BUCKETS){s->probe_limit++;s->exclusions++;return NULL;}
    if(s->admission_stopped){s->admission_suppressed++;s->exclusions++;return NULL;}
    if(s->entries==NF_VC107_ENTRIES){s->entry_limit++;s->exclusions++;return NULL;}
    unsigned page_index=s->open_page?s->open_page-1:s->pages;
    unsigned at=s->open_page?(s->page[page_index].used+15u)&~15u:0;
    if(s->open_page && bytes>NF_VC107_PAGE_BYTES-at){page_index=s->pages;at=0;}
    unsigned new_page=page_index==s->pages;
    size_t reservation=map_bytes+(new_page?(size_t)NF_VC107_PAGE_BYTES*2:0);
    if((new_page && s->pages==NF_VC107_PAGES) || reservation>NF_VC107_BUDGET-s->budget_reserved){s->byte_limit++;s->exclusions++;return NULL;}
    uint8_t *page_data=new_page?(uint8_t*)nf_vc107_alloc(NF_VC107_PAGE_BYTES):NULL;
    uint16_t *owned_map=map?(uint16_t*)nf_vc107_alloc(map_bytes):NULL;
    if((new_page && !page_data) || (map && !owned_map)){
        free(page_data);free(owned_map);s->allocation_failures++;s->admission_stopped=1;s->exclusions++;return NULL;
    }
    if(new_page){
        memset(page_data,0,NF_VC107_PAGE_BYTES);
        s->page[page_index].data=page_data;s->page[page_index].state=NF_VC107_OPEN;
        s->pages++;s->open_page=page_index+1;s->cpu_bytes+=NF_VC107_PAGE_BYTES;s->gpu_reserved+=NF_VC107_PAGE_BYTES;
    }
    NFVertexCache107Page *p=&s->page[page_index];uint8_t *snapshot=p->data+at;
    if(map){for(unsigned i=0;i<vertices;i++)memcpy(snapshot+(size_t)i*stride,(const uint8_t*)source+(size_t)map[i]*stride,stride);memcpy(owned_map,map,map_bytes);}
    else memcpy(snapshot,source,bytes);
    NFVertexCache107Entry *e=&s->entry[s->entries];e->source=(uintptr_t)source;e->hash=hash;e->stride=stride;e->vertices=vertices;e->source_vertices=source_vertices;e->compact=kind;
    e->state=NF_VC107_PROBATION;e->first_frame=frame;e->page=page_index;e->offset=at;e->map=owned_map;
    s->padding_bytes+=at-p->used;p->used=at+bytes;s->payload_bytes+=bytes;
    s->buckets[empty]=(uint16_t)(++s->entries);s->budget_reserved+=reservation;s->cpu_bytes+=map_bytes;s->map_bytes+=map_bytes;s->probation++;s->device=dev;
    return NULL;
}
static void nf_vertex_cache107_report(FILE *file){
    if(!file)return;const NFVertexCache107State *s=&nf_vertex_cache107;
#define NF_VC107_U(v) (unsigned long long)s->v
    fprintf(file,"[VERTEX-CACHE107] enabled=%d calls=%llu hits=%llu avoided_bytes=%llu later_present_hits=%llu probation=%llu probation_calls=%llu promotions=%llu mismatch=%llu suppressed=%llu exclusions=%llu oversize=%llu entry_limit=%llu byte_limit=%llu probe_limit=%llu invalid=%llu alloc_failures=%llu create_failures=%llu admission_stopped=%u admission_suppressed=%llu entries=%u pages=%u page_creates=%llu budget=%llu cpu_bytes=%llu gpu_reserved=%llu gpu_created_bytes=%llu payload_bytes=%llu slot_padding_bytes=%llu map_bytes=%llu comparisons=%llu compare_requested_bytes=%llu probes=%llu contiguous_calls=%llu contiguous_hits=%llu contiguous_avoided=%llu compact_calls=%llu compact_hits=%llu compact_avoided=%llu\n",
        nf_vertex_cache107_enabled(),NF_VC107_U(calls),NF_VC107_U(hits),NF_VC107_U(avoided_bytes),NF_VC107_U(later_present_hits),NF_VC107_U(probation),NF_VC107_U(probation_calls),NF_VC107_U(promotions),NF_VC107_U(mismatch),NF_VC107_U(suppressed),NF_VC107_U(exclusions),NF_VC107_U(oversize),NF_VC107_U(entry_limit),NF_VC107_U(byte_limit),NF_VC107_U(probe_limit),NF_VC107_U(invalid),NF_VC107_U(allocation_failures),NF_VC107_U(create_failures),s->admission_stopped,NF_VC107_U(admission_suppressed),s->entries,s->pages,NF_VC107_U(page_creates),NF_VC107_U(budget_reserved),NF_VC107_U(cpu_bytes),NF_VC107_U(gpu_reserved),NF_VC107_U(gpu_created_bytes),NF_VC107_U(payload_bytes),NF_VC107_U(padding_bytes),NF_VC107_U(map_bytes),NF_VC107_U(comparisons),NF_VC107_U(compare_bytes),NF_VC107_U(probes),NF_VC107_U(kind_calls[0]),NF_VC107_U(kind_hits[0]),NF_VC107_U(kind_avoided_bytes[0]),NF_VC107_U(kind_calls[1]),NF_VC107_U(kind_hits[1]),NF_VC107_U(kind_avoided_bytes[1]));
#undef NF_VC107_U
}
#else
static int nf_vertex_cache107_enabled(void){return 0;}
static ID3D11Buffer *nf_vertex_cache107_get(ID3D11Device *dev,unsigned frame,const void *source,unsigned stride,unsigned vertices,unsigned source_vertices,const uint16_t *map,unsigned *offset){(void)dev;(void)frame;(void)source;(void)stride;(void)vertices;(void)source_vertices;(void)map;(void)offset;return NULL;}
static void nf_vertex_cache107_report(FILE *file){(void)file;}
#endif
#endif
