/* Counter-only observation of map230; no time queries or permission policy. */
#ifndef DRIVING_MAPPING259_H
#define DRIVING_MAPPING259_H
#ifdef DRIVING_MAPPING259
#include <stdio.h>
#include <stdlib.h>
enum {DM259_INPUT,DM259_QUERY_FAILED,DM259_METADATA,DM259_PERMISSION,DM259_OK,DM259_RESULTS};
typedef struct DrivingMapSpan259 {
 unsigned enabled,write,steps,hits,queries,probes,inserts,saturated,max_cache;
 uint64_t bytes,region_bytes,max_region,regions[5];
} DrivingMapSpan259;
static struct {
 uint64_t contexts,ends,spans,read_spans,write_spans,bytes,steps,hits,queries,probes,inserts,saturated;
 uint64_t full_spans,results[DM259_RESULTS],step_hist[8],query_hist[8],size_hist[5],region_hist[5];
 uint64_t region_bytes,max_region;unsigned max_steps,max_queries,max_cache;
} dm259;
#ifndef DRIVING_MAP259_ENABLED
static int driving_map259_enabled(void){static int on=-1;if(on<0){const char*v=getenv("DRIVING_MAPPING259");on=v&&!strcmp(v,"1");}return on;}
#define DRIVING_MAP259_ENABLED() driving_map259_enabled()
#endif
static unsigned dm259_step_bin(unsigned n){return !n?0:n==1?1:n==2?2:n<=4?3:n<=8?4:n<=16?5:n<=64?6:7;}
static unsigned dm259_size_bin(uint64_t n){return n<=4096?0:n<=65536?1:n<=1048576?2:n<=16777216?3:4;}
static void dm259_report(void){
 fprintf(stderr,"[MAPPING259] spans=%llu contexts=%llu ends=%llu read_spans=%llu write_spans=%llu requested_bytes=%llu queries=%llu cache_hits=%llu region_steps=%llu cache_probes=%llu insertions=%llu uncached_full=%llu full_spans=%llu evictions=0 max_cache=%u max_steps=%u max_queries=%u queried_region_bytes=%llu max_region_bytes=%llu results_input_query_metadata_permission_ok=%llu/%llu/%llu/%llu/%llu",
  (unsigned long long)dm259.spans,(unsigned long long)dm259.contexts,(unsigned long long)dm259.ends,(unsigned long long)dm259.read_spans,(unsigned long long)dm259.write_spans,(unsigned long long)dm259.bytes,
  (unsigned long long)dm259.queries,(unsigned long long)dm259.hits,(unsigned long long)dm259.steps,(unsigned long long)dm259.probes,(unsigned long long)dm259.inserts,(unsigned long long)dm259.saturated,(unsigned long long)dm259.full_spans,dm259.max_cache,dm259.max_steps,dm259.max_queries,(unsigned long long)dm259.region_bytes,(unsigned long long)dm259.max_region,
  (unsigned long long)dm259.results[0],(unsigned long long)dm259.results[1],(unsigned long long)dm259.results[2],(unsigned long long)dm259.results[3],(unsigned long long)dm259.results[4]);
 fprintf(stderr," span_steps_0_1_2_4_8_16_64_more=");for(unsigned i=0;i<8;i++)fprintf(stderr,"%s%llu",i?"/":"",(unsigned long long)dm259.step_hist[i]);
 fprintf(stderr," span_queries_0_1_2_4_8_16_64_more=");for(unsigned i=0;i<8;i++)fprintf(stderr,"%s%llu",i?"/":"",(unsigned long long)dm259.query_hist[i]);
 fprintf(stderr," span_bytes_le4K_64K_1M_16M_more=");for(unsigned i=0;i<5;i++)fprintf(stderr,"%s%llu",i?"/":"",(unsigned long long)dm259.size_hist[i]);
 fprintf(stderr," queried_region_bytes_le4K_64K_1M_16M_more=");for(unsigned i=0;i<5;i++)fprintf(stderr,"%s%llu",i?"/":"",(unsigned long long)dm259.region_hist[i]);
 fprintf(stderr," counters-only=1 map230-only=1\n");
}
#ifndef DRIVING_MAP259_REPORT
#define DRIVING_MAP259_REPORT() dm259_report()
#endif
static void dm259_span_begin(DrivingMapSpan259*s,size_t bytes,int write,unsigned count){
 s->enabled=DRIVING_MAP259_ENABLED();if(!s->enabled)return;
 memset(s,0,sizeof*s);s->enabled=1;s->bytes=bytes;s->write=write!=0;s->max_cache=count<=16?count:0;
}
static void dm259_region(DrivingMapSpan259*s,size_t bytes){
 if(!s->enabled)return;s->region_bytes+=bytes;if(bytes>s->max_region)s->max_region=bytes;s->regions[dm259_size_bin(bytes)]++;
}
static void *dm259_span_end(DrivingMapSpan259*s,void*result,unsigned why){
 if(!s->enabled)return result;
 dm259.spans++;dm259.results[why]++;dm259.read_spans+=!s->write;dm259.write_spans+=s->write;
#define DM259_ADD(f) dm259.f+=s->f
 DM259_ADD(bytes);DM259_ADD(steps);DM259_ADD(hits);DM259_ADD(queries);DM259_ADD(probes);DM259_ADD(inserts);DM259_ADD(saturated);DM259_ADD(region_bytes);
#undef DM259_ADD
 dm259.full_spans+=s->saturated!=0;
 if(s->max_cache>dm259.max_cache)dm259.max_cache=s->max_cache;
 if(s->max_region>dm259.max_region)dm259.max_region=s->max_region;
 if(s->steps>dm259.max_steps)dm259.max_steps=s->steps;
 if(s->queries>dm259.max_queries)dm259.max_queries=s->queries;
 dm259.step_hist[dm259_step_bin(s->steps)]++;dm259.query_hist[dm259_step_bin(s->queries)]++;dm259.size_hist[dm259_size_bin(s->bytes)]++;
 for(unsigned i=0;i<5;i++)dm259.region_hist[i]+=s->regions[i];
 if(dm259.spans==1||!(dm259.spans%16384))DRIVING_MAP259_REPORT();return result;
}
#define DM259_DECLARE(ctx,bytes,write) DrivingMapSpan259 span259;dm259_span_begin(&span259,bytes,write,0)
#define DM259_INC(f) do{if(span259.enabled)span259.f++;}while(0)
#define DM259_REGION(n) dm259_region(&span259,n)
#define DM259_CACHE(n) do{if(span259.enabled&&(n)>span259.max_cache)span259.max_cache=(n);}while(0)
#define DM259_RETURN(v,reason) return dm259_span_end(&span259,(void*)(v),reason)
#define DM259_CONTEXT(begin) do{if(DRIVING_MAP259_ENABLED()){if(begin)dm259.contexts++;else dm259.ends++;}}while(0)
#else
#define DM259_DECLARE(ctx,bytes,write)
#define DM259_INC(f) ((void)0)
#define DM259_REGION(n) ((void)0)
#define DM259_CACHE(n) ((void)0)
#define DM259_RETURN(v,reason) return (v)
#define DM259_CONTEXT(begin) ((void)0)
#endif
#endif
