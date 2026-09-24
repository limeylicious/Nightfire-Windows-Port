#ifndef NIGHTFIRE_CONSUMER135_H
#define NIGHTFIRE_CONSUMER135_H
#include <stdint.h>
#include <stdio.h>
/* Diagnostic census, NOT a memory ownership/permission/coherence barrier.
 * Compile and exact runtime NIGHTFIRE_CONSUMER135=1 are both required.
 * Callers provide existing scalar values only; the census never reads guest
 * memory, invokes guest/GPU work, blocks an actor, or changes synchronization.
 * Range addresses are guest VAs, not bare physical offsets. Counters describe
 * instrumented call sites only, never complete translated-memory coverage.
 * Every enabled API preserves incoming Win32 GetLastError. Normal code only:
 * do not call from an exception handler or while holding the census lock.
 * Report is a point-in-time snapshot; background actors may continue afterward.
 */
#ifdef NIGHTFIRE_CONSUMER135
void nf_consumer135_event(const char *kind,uint32_t a,uint32_t b,uint32_t c);
void nf_consumer135_range(const char *kind,uint32_t guest_address,uint32_t bytes,unsigned write);
void nf_consumer135_apu(unsigned vp_active,uint32_t sectl,uint32_t fectl,uint32_t vpv,uint32_t vpsge,uint32_t vpssl);
void nf_consumer135_report(FILE *stream);
#else
/* No argument evaluation or observer calls in normal binaries. */
#define nf_consumer135_event(kind,a,b,c) ((void)0)
#define nf_consumer135_range(kind,address,bytes,write) ((void)0)
#define nf_consumer135_apu(active,sectl,fectl,vpv,sge,ssl) ((void)0)
#define nf_consumer135_report(stream) ((void)0)
#endif
#endif
