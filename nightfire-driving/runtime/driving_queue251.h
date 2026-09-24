/* Opt-in coarse preparation timings. No QPC or printing inside vertex loops. */
#ifndef DRIVING_QUEUE251_H
#define DRIVING_QUEUE251_H
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef DRIVING_QUEUE251_CLOCK
#define DRIVING_QUEUE251_CLOCK() driving_clock227()
#endif
#ifndef DRIVING_QUEUE251_ENABLED
static int driving_queue251_enabled(void){static int value=-1;if(value<0){const char*v=getenv("DRIVING_QUEUE_TIMING251");value=v&&!strcmp(v,"1");}return value;}
#define DRIVING_QUEUE251_ENABLED() driving_queue251_enabled()
#endif
enum {DQ251_PLAN,DQ251_RESOURCES,DQ251_ALLOCATIONS,DQ251_TEXTURES,DQ251_VERTICES,DQ251_EXPAND,DQ251_STAGE,DQ251_PHASES};
typedef struct DrivingQueue251 {
 int enabled;unsigned phase;
 uint64_t last,flush_last,ticks[DQ251_PHASES],flush_ticks;
 uint64_t spans,allocations,texture_bytes,inputs,transforms,memo_hits,expanded,seed_bytes;
} DrivingQueue251;
typedef struct DrivingQueueTotal251 {
 uint64_t calls,ticks[DQ251_PHASES],flush_ticks;
 uint64_t spans,allocations,texture_bytes,inputs,transforms,memo_hits,expanded,seed_bytes;
} DrivingQueueTotal251;
static DrivingQueueTotal251 driving_queue251_totals[2];
static uint64_t driving_queue251_bad_clock;
static void driving_queue251_begin(DrivingQueue251*q,uint64_t flushed){
 q->enabled=DRIVING_QUEUE251_ENABLED();if(!q->enabled)return;
 memset(q,0,sizeof *q);q->enabled=1;q->phase=DQ251_PLAN;q->last=DRIVING_QUEUE251_CLOCK();q->flush_last=flushed;
}
static void driving_queue251_mark(DrivingQueue251*q,unsigned phase,uint64_t flushed){
 if(!q->enabled)return;
 uint64_t now=DRIVING_QUEUE251_CLOCK(),wall=now-q->last,excluded=flushed-q->flush_last;
 if(excluded>wall){driving_queue251_bad_clock++;excluded=wall;}
 q->ticks[q->phase]+=wall-excluded;q->flush_ticks+=excluded;
 q->phase=phase;q->last=now;q->flush_last=flushed;
}
#ifndef DRIVING_QUEUE251_REPORT
static void driving_queue251_report(void){
 LARGE_INTEGER hz;QueryPerformanceFrequency(&hz);double ms=1000.0/hz.QuadPart;
 for(unsigned result=0;result<2;result++){
  const DrivingQueueTotal251*t=&driving_queue251_totals[result];uint64_t total=0;for(unsigned i=0;i<DQ251_PHASES;i++)total+=t->ticks[i];
  fprintf(stderr,"[QUEUE251] result=%s calls=%llu total_ms=%.3f plan_ms=%.3f resources_ms=%.3f allocations_ms=%.3f textures_ms=%.3f vertices_ms=%.3f expand_ms=%.3f stage_cleanup_ms=%.3f nested_flush_excluded_ms=%.3f spans=%llu allocations=%llu texture_bytes=%llu inputs=%llu transforms=%llu memo_hits=%llu expanded_vertices=%llu seed_bytes=%llu bad_clock=%llu wall-clock-not-GPU-duration=1\n",result?"accepted":"refused",(unsigned long long)t->calls,total*ms,t->ticks[0]*ms,t->ticks[1]*ms,t->ticks[2]*ms,t->ticks[3]*ms,t->ticks[4]*ms,t->ticks[5]*ms,t->ticks[6]*ms,t->flush_ticks*ms,(unsigned long long)t->spans,(unsigned long long)t->allocations,(unsigned long long)t->texture_bytes,(unsigned long long)t->inputs,(unsigned long long)t->transforms,(unsigned long long)t->memo_hits,(unsigned long long)t->expanded,(unsigned long long)t->seed_bytes,(unsigned long long)driving_queue251_bad_clock);
 }
}
#define DRIVING_QUEUE251_REPORT() driving_queue251_report()
#endif
static int driving_queue251_finish(DrivingQueue251*q,int accepted,uint64_t flushed){
 if(!q->enabled)return accepted;
 driving_queue251_mark(q,q->phase,flushed);DrivingQueueTotal251*t=&driving_queue251_totals[accepted!=0];t->calls++;
 for(unsigned i=0;i<DQ251_PHASES;i++)t->ticks[i]+=q->ticks[i];t->flush_ticks+=q->flush_ticks;
#define DQ251_ADD(f) t->f+=q->f
 DQ251_ADD(spans);DQ251_ADD(allocations);DQ251_ADD(texture_bytes);DQ251_ADD(inputs);DQ251_ADD(transforms);DQ251_ADD(memo_hits);DQ251_ADD(expanded);DQ251_ADD(seed_bytes);
#undef DQ251_ADD
 uint64_t calls=driving_queue251_totals[0].calls+driving_queue251_totals[1].calls;
 if(calls==1||!(calls%512))DRIVING_QUEUE251_REPORT();
 return accepted;
}
#endif
