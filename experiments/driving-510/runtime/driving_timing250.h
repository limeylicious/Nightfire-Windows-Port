#ifndef DRIVING_TIMING250_H
#define DRIVING_TIMING250_H
/* Serialized consumer CPU/wait accounting. No GPU queries, waits or commands.
 * Inclusive offscreen attempts subtract child main flushes; successful phase
 * sums are subsets. The drain remainder excludes the four disjoint categories. */
#include <stdint.h>
typedef struct {uint64_t start,flush;} DrivingAttempt250;
typedef struct {uint64_t start,queue,fallback,flush,offscreen,software;} DrivingDrain250;
#ifdef DRIVING_TIMING250
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static struct {
 uint64_t flush,accepted,refused,accepted_ticks,refused_ticks,phase[5],profile[6];
 uint64_t software_calls[3],software_ticks[3],drains,drain_ticks,main_queue,main_fallback,main_flush;
 uint64_t drain_offscreen,drain_software,clock_reads,invalid_intervals;
} dt250;
static int dt250_setting=-1;
static int dt326_generic_setting=-1;
static __inline int dt326_generic_enabled(void){
 if(dt326_generic_setting<0){DWORD saved=GetLastError();int crt=errno;
  const char*v=getenv("DRIVING_GENERIC_TIMING326");dt326_generic_setting=v&&!strcmp(v,"1");
  errno=crt;SetLastError(saved);}
 return dt326_generic_setting;
}
static __inline int dt250_enabled(void){
 if(dt250_setting<0){DWORD saved=GetLastError();const char*v=getenv("DRIVING_TIMING250");dt250_setting=v&&!strcmp(v,"1");SetLastError(saved);}return dt250_setting;
}
static __inline uint64_t dt250_clock(void){if(!dt250_enabled())return 0;LARGE_INTEGER t;QueryPerformanceCounter(&t);dt250.clock_reads++;return t.QuadPart;}
static __inline void dt250_flush(uint64_t ticks){if(dt250_enabled())dt250.flush+=ticks;}
static __inline uint64_t dt250_flush_ticks(void){return dt250.flush;}
static __inline DrivingAttempt250 dt250_attempt(void){DrivingAttempt250 a={dt250_clock(),dt250.flush};return a;}
static __inline void dt250_attempt_end(DrivingAttempt250 a,int accepted){
 if(!a.start)return;uint64_t ticks=dt250_clock()-a.start,child=dt250.flush-a.flush;
 if(child>ticks){dt250.invalid_intervals++;return;}ticks-=child;
 if(accepted){dt250.accepted++;dt250.accepted_ticks+=ticks;}else{dt250.refused++;dt250.refused_ticks+=ticks;}
}
static __inline void dt250_phases(unsigned profile,const uint64_t t[6],uint64_t flush_before){
 if(!t[0])return;uint64_t child=dt250.flush-flush_before;
 if(profile>=6||t[1]-t[0]<child){dt250.invalid_intervals++;return;}
 dt250.profile[profile]++;for(unsigned i=0;i<5;i++)dt250.phase[i]+=t[i+1]-t[i]-(i?0:child);
}
static __inline uint64_t dt250_software_begin(unsigned kind){
 if(!dt250_enabled())return 0;dt250.software_calls[kind]++;
 return (kind<2||(kind==2&&dt326_generic_enabled()))?dt250_clock():0;
}
static __inline void dt250_software_end(unsigned kind,uint64_t start){if(start&&kind<3)dt250.software_ticks[kind]+=dt250_clock()-start;}
static __inline DrivingDrain250 dt250_drain_begin(uint64_t queue,uint64_t fallback,uint64_t flush){
 DrivingDrain250 d={dt250_clock(),queue,fallback,flush,dt250.accepted_ticks+dt250.refused_ticks,dt250.software_ticks[0]+dt250.software_ticks[1]+dt250.software_ticks[2]};return d;
}
static __inline void dt250_drain_end(DrivingDrain250 d,uint64_t queue,uint64_t fallback,uint64_t flush){
 if(!d.start)return;dt250.drain_ticks+=dt250_clock()-d.start;dt250.drains++;
 dt250.main_queue+=queue-d.queue;dt250.main_fallback+=fallback-d.fallback;dt250.main_flush+=flush-d.flush;
 dt250.drain_offscreen+=dt250.accepted_ticks+dt250.refused_ticks-d.offscreen;
 dt250.drain_software+=dt250.software_ticks[0]+dt250.software_ticks[1]+dt250.software_ticks[2]-d.software;
 if(dt250.drains!=1&&dt250.drains%512)return;
 uint64_t assigned=dt250.main_queue+dt250.main_fallback+dt250.main_flush+dt250.drain_offscreen+dt250.drain_software;
 int64_t other=(int64_t)dt250.drain_ticks-(int64_t)assigned;
 DWORD saved=GetLastError();LARGE_INTEGER hz;QueryPerformanceFrequency(&hz);double ms=1000.0/hz.QuadPart;
 fprintf(stderr,"[TIMING250] drains=%llu locked_drain_ms=%.3f main_queue_exclusive_ms=%.3f main_sync_fallback_ms=%.3f main_flush_ms=%.3f offscreen_excluding_main_flush_ms=%.3f software_end_clear_ms=%.3f command_other_ms=%.3f clock_reads=%llu invalid=%llu excludes-lock-and-scanout-leave=1 CPU-wall-not-GPU-duration=1\n",
  (unsigned long long)dt250.drains,dt250.drain_ticks*ms,dt250.main_queue*ms,dt250.main_fallback*ms,dt250.main_flush*ms,dt250.drain_offscreen*ms,dt250.drain_software*ms,other*ms,(unsigned long long)dt250.clock_reads,(unsigned long long)dt250.invalid_intervals);
 fprintf(stderr,"[OFFSCREEN250] accepted=%llu refused=%llu accepted_exclusive_ms=%.3f refused_exclusive_ms=%.3f success_resources_excluding_main_flush_ms=%.3f success_vertices_ms=%.3f success_import_ms=%.3f success_backend_ms=%.3f success_publish_ms=%.3f profiles=%llu,%llu,%llu,%llu,%llu,%llu phases-are-success-only-subsets=1\n",
  (unsigned long long)dt250.accepted,(unsigned long long)dt250.refused,dt250.accepted_ticks*ms,dt250.refused_ticks*ms,dt250.phase[0]*ms,dt250.phase[1]*ms,dt250.phase[2]*ms,dt250.phase[3]*ms,dt250.phase[4]*ms,(unsigned long long)dt250.profile[0],(unsigned long long)dt250.profile[1],(unsigned long long)dt250.profile[2],(unsigned long long)dt250.profile[3],(unsigned long long)dt250.profile[4],(unsigned long long)dt250.profile[5]);
 fprintf(stderr,"[SOFTWARE250] end_calls=%llu clear_calls=%llu other_calls=%llu end_ms=%.3f clear_ms=%.3f generic_ms=%.3f generic_timed=%d includes-replay=1\n",
  (unsigned long long)dt250.software_calls[0],(unsigned long long)dt250.software_calls[1],(unsigned long long)dt250.software_calls[2],dt250.software_ticks[0]*ms,dt250.software_ticks[1]*ms,dt250.software_ticks[2]*ms,dt326_generic_enabled());
 SetLastError(saved);
}
#else
static __inline uint64_t dt250_clock(void){return 0;}
static __inline void dt250_flush(uint64_t ticks){(void)ticks;}
static __inline uint64_t dt250_flush_ticks(void){return 0;}
static __inline DrivingAttempt250 dt250_attempt(void){DrivingAttempt250 a={0};return a;}
static __inline void dt250_attempt_end(DrivingAttempt250 a,int accepted){(void)a;(void)accepted;}
static __inline void dt250_phases(unsigned p,const uint64_t t[6],uint64_t f){(void)p;(void)t;(void)f;}
static __inline uint64_t dt250_software_begin(unsigned kind){(void)kind;return 0;}
static __inline void dt250_software_end(unsigned kind,uint64_t start){(void)kind;(void)start;}
static __inline DrivingDrain250 dt250_drain_begin(uint64_t a,uint64_t b,uint64_t c){DrivingDrain250 d={0};(void)a;(void)b;(void)c;return d;}
static __inline void dt250_drain_end(DrivingDrain250 d,uint64_t a,uint64_t b,uint64_t c){(void)d;(void)a;(void)b;(void)c;}
#endif
#endif
