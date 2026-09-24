#ifndef NIGHTFIRE_SURFACE_PROBE83_H
#define NIGHTFIRE_SURFACE_PROBE83_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
/* Private diagnostic only. One attempted arm/process; never defers GPU work.
 * Call only after actual completed hardware drawing, pending == 0. Both bases
 * must name the owned shared mapping views. Exact whole-page surface ranges
 * only. arena_bytes bounds both views; caller also checks allocation identity.
 * Before OS-buffer access, protection/lifetime changes or next nonempty drain,
 * call end. No observer/pose/RPM sampler may run uncoordinated with this probe.
 * Code and metadata remain resident until process termination. */
enum NF83SurfaceReason {
 NF83_END_NEXT_DRAIN=1, NF83_END_PUBLISH=2, NF83_END_FILE_IO=3,
 NF83_END_PROTECT_QUERY=4, NF83_END_PROTECT_CHANGE=5, NF83_END_LIFETIME=6,
 NF83_END_SHUTDOWN=7, NF83_END_TIMEOUT=8, NF83_END_TEST=9
};
enum NF83SurfaceStatus {
 NF83_UNUSED=0, NF83_VALIDATING=1, NF83_ARMING=2, NF83_ARMED=3,
 NF83_ENDED=4, NF83_REJECTED=5, NF83_FAILED=6
};
typedef struct NF83SurfaceSnapshot {
 unsigned attempted,status,reason,range_count,protected_mask;
 unsigned first_hit,hit_thread,access,hit_during_arming,late_retries,restore_calls;
 unsigned arm_thread,arm_finished,active_handlers,error,external_calls;
 uintptr_t rip,address,color,depth,contiguous_base,tiled_base;
 size_t color_bytes,depth_bytes,arena_bytes;
 unsigned classification; /* bit0 color, bit1 depth, bit2 tiled */
} NF83SurfaceSnapshot;
#ifdef NIGHTFIRE_SURFACE_PROBE_DIAGNOSTIC
int nf_surface_probe_arm(const void *color,size_t cb,const void *depth,size_t db,
 uintptr_t contiguous_base,uintptr_t tiled_base,size_t arena_bytes);
void nf_surface_probe_end(unsigned reason);
/* Bracket the ENTIRE kernel-mediated operation. Enter excludes a future arm
 * or ends an existing attempt before the OS sees its buffer. Preserves last
 * error. Every enter requires one leave, including failed operations. */
void nf_surface_probe_external_enter(unsigned reason);
void nf_surface_probe_external_leave(void);
/* Windows/MSVC C scopes: brackets all returns and exceptional unwinds. These
 * paired macros are intentionally a scope, not standalone statements. */
#define NF_SURFACE_PROBE_EXTERNAL_BEGIN(reason) nf_surface_probe_external_enter(reason); __try {
#define NF_SURFACE_PROBE_EXTERNAL_END() } __finally { nf_surface_probe_external_leave(); }
/* Ordinary code only. Snapshot fields may be provisional while arm/handler is
 * active; after end returns they are stable except bounded late retry count. */
void nf_surface_probe_snapshot(NF83SurfaceSnapshot *out);
void nf_surface_probe_report(FILE *stream);
#ifdef NF_SURFACE_PROBE83_TEST
#include <windows.h>
LONG nf_surface_probe_test_exception(EXCEPTION_POINTERS *ep);
#endif
#else
static __inline int nf_surface_probe_arm(const void *c,size_t cb,const void *d,size_t db,uintptr_t b,uintptr_t t,size_t n){(void)c;(void)cb;(void)d;(void)db;(void)b;(void)t;(void)n;return 0;}
static __inline void nf_surface_probe_end(unsigned reason){(void)reason;}
static __inline void nf_surface_probe_external_enter(unsigned reason){(void)reason;}
static __inline void nf_surface_probe_external_leave(void){}
#define NF_SURFACE_PROBE_EXTERNAL_BEGIN(reason)
#define NF_SURFACE_PROBE_EXTERNAL_END()
static __inline void nf_surface_probe_snapshot(NF83SurfaceSnapshot *out){if(out){NF83SurfaceSnapshot empty={0};*out=empty;}}
static __inline void nf_surface_probe_report(FILE *stream){(void)stream;}
#endif
#endif
