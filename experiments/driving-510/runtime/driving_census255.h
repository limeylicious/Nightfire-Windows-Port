/* Completed owned flush census only. Consumer thread; no new clock reads. */
#ifndef DRIVING_CENSUS255_H
#define DRIVING_CENSUS255_H
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define DC255_REASONS(X) \
 X(METHOD,"method-barrier") X(GET,"drain-before-GET") \
 X(BEGIN,"unadmitted-BEGIN") X(CAPACITY,"capacity") \
 X(DESCRIPTOR,"descriptor-read") X(TEXTURE,"texture-read") \
 X(TARGET,"target-change") X(VERTEX,"vertex-read") \
 X(UNSUPPORTED,"backend-unsupported") X(REFUSED,"current-refused") \
 X(ALIAS,"source-alias") X(EXIT,"drain-exit") \
 X(GENERIC,"generic-executor") X(CLASS,"non-Kelvin") \
 X(UNCOLLECTED,"uncollected-draw") X(DISABLED,"disabled") \
 X(DIRECT,"direct-span") X(BIND,"object-bind") \
 X(IMMEDIATE,"immediate-replay") X(MOVIE,"movie-resolve") \
 X(COLLECTOR,"collector-replay") X(OFFSCREEN,"offscreen243-before-read") \
 X(UNKNOWN,"unknown")
#define DC255_ENUM(k,n) DC255_##k,
enum {DC255_REASONS(DC255_ENUM) DC255_REASON_COUNT};
#undef DC255_ENUM
#define DC255_NAME(k,n) n,
static const char *const dc255_names[]={DC255_REASONS(DC255_NAME)};
#undef DC255_NAME
typedef struct DrivingCensusRow255 {
 uint64_t flushes,draws,bytes,maximum_bytes,backend_ticks,publish_ticks,total_ticks;
 uint64_t histogram[18]; /* Exact sizes1..16;17 holds unexpected larger sizes. */
} DrivingCensusRow255;
static struct {
 DrivingCensusRow255 total,reason[DC255_REASON_COUNT],capacity[4];
 uint64_t incoming_bytes[4],reported;
 uint64_t histogram260[17]; /*17..32, then over32; legacy histogram retained. */
} dc255;
static int driving_census255_enabled(void){
 static int on=-1;if(on<0){DWORD saved=GetLastError();const char*v=getenv("DRIVING_BATCH_CENSUS255");on=v&&!strcmp(v,"1");SetLastError(saved);}return on;
}
static void dc255_add(DrivingCensusRow255*r,unsigned draws,size_t bytes,uint64_t backend,uint64_t publish,uint64_t elapsed){
 r->flushes++;r->draws+=draws;r->bytes+=bytes;if(bytes>r->maximum_bytes)r->maximum_bytes=bytes;
 r->backend_ticks+=backend;r->publish_ticks+=publish;r->total_ticks+=elapsed;
 r->histogram[draws<=16?draws:17]++;
}
/* Call only after complete successful publication/cleanup. Empty/fatal flushes
 * never reach this call. capacity bits:1 count limit,2 byte limit;0 unspecified. */
static void driving_census255_commit(const char*reason,unsigned draws,size_t bytes,
 unsigned capacity,size_t incoming,uint64_t backend,uint64_t publish,uint64_t elapsed){
 if(!driving_census255_enabled()||!draws)return;
 unsigned at=DC255_UNKNOWN;
 if(reason)for(unsigned i=0;i<DC255_UNKNOWN;i++)if(!strcmp(reason,dc255_names[i])){at=i;break;}
 dc255_add(&dc255.total,draws,bytes,backend,publish,elapsed);
 if(draws>16)dc255.histogram260[draws<=32?draws-17:16]++;
 dc255_add(&dc255.reason[at],draws,bytes,backend,publish,elapsed);
 if(at==DC255_CAPACITY){unsigned kind=capacity<=3?capacity:0;
  dc255_add(&dc255.capacity[kind],draws,bytes,backend,publish,elapsed);dc255.incoming_bytes[kind]+=incoming;
 }
}
static void dc255_print(const char*name,const DrivingCensusRow255*r,double ms){
 fprintf(stderr,"[BATCH-CENSUS255] reason=%s flushes=%llu draws=%llu queue_bytes_sum=%llu queue_bytes_max=%llu backend_ms=%.3f publish_ms=%.3f total_ms=%.3f\n",name,
  (unsigned long long)r->flushes,(unsigned long long)r->draws,(unsigned long long)r->bytes,(unsigned long long)r->maximum_bytes,
  r->backend_ticks*ms,r->publish_ticks*ms,r->total_ticks*ms);
}
/* Called from existing coarse batch report, reusing its frequency conversion.
 * All row counts are cumulative and exact through this report; logs are sparse. */
static void driving_census255_report(double ms){
 if(!driving_census255_enabled()||!dc255.total.flushes||
    (dc255.reported&&dc255.total.flushes-dc255.reported<512))return;
 DWORD saved=GetLastError();dc255.reported=dc255.total.flushes;
 dc255_print("all",&dc255.total,ms);
 for(unsigned i=0;i<DC255_REASON_COUNT;i++)if(dc255.reason[i].flushes)dc255_print(dc255_names[i],&dc255.reason[i],ms);
 const char*const names[]={"capacity-unspecified","capacity-count-only","capacity-bytes-only","capacity-both"};
 for(unsigned i=0;i<4;i++)if(dc255.capacity[i].flushes){dc255_print(names[i],&dc255.capacity[i],ms);
  fprintf(stderr,"[BATCH-CENSUS255] subtype=%s incoming_payload_bytes_sum=%llu\n",names[i],(unsigned long long)dc255.incoming_bytes[i]);}
 fprintf(stderr,"[BATCH-CENSUS255] histogram_draws_1_to_16_then_over16=");
 for(unsigned i=1;i<18;i++)fprintf(stderr,"%s%llu",i==1?"":",",(unsigned long long)dc255.total.histogram[i]);
 fprintf(stderr," successful_only=1 queue_bytes_not_transfer_bytes=1 wall_clock_not_GPU_duration=1\n");
 fprintf(stderr,"[BATCH-CAPACITY260] histogram_draws_17_to_32_then_over32=");
 for(unsigned i=0;i<17;i++)fprintf(stderr,"%s%llu",i?",":"",(unsigned long long)dc255.histogram260[i]);
 fputc('\n',stderr);SetLastError(saved);
}
#undef DC255_REASONS
#endif
