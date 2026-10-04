#ifndef DRIVING_ASYNC295_H
#define DRIVING_ASYNC295_H
#include <stdint.h>
#include <stddef.h>
#include <fenv.h>
#include <xmmintrin.h>
typedef struct DrivingFP295 {fenv_t env;unsigned mxcsr;} DrivingFP295;
static __inline void driving_fp295_save(DrivingFP295 *p){fegetenv(&p->env);p->mxcsr=_mm_getcsr();}
static __inline void driving_fp295_restore(const DrivingFP295 *p){fesetenv(&p->env);_mm_setcsr(p->mxcsr);}
/* Entry/leave must bracket every producer or AV consumer entry. The worker
 * never owns this gate and never calls an API which joins itself. */
int driving_async295_enabled(void);
void driving_async295_enter(void);
void driving_async295_leave(void);
void driving_async295_shutdown(void);
/* Gate held; callback/context lifetime extends through join. Arm cannot run
 * work. Kick follows producer-only scope cleanup and scanout lock release. */
typedef int (*DrivingTail295)(void *);
int driving_async295_arm(DrivingTail295 fn,void *ctx,unsigned packets,size_t bytes);
void driving_async295_kick(void);
void driving_async295_reject(unsigned reason);
void driving_async295_fp_observed(const DrivingFP295 *before);
int driving_async295_pending(void);
#endif
