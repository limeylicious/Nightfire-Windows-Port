#ifndef DRIVING_PC508_STATE_H
#define DRIVING_PC508_STATE_H
#include "driving_pc508.h"
/* Single atomic source of truth for pending content/protection in508.
 * Legacy450.dirty/451.protected_flag are used only with508 OFF.
 * ARMING and PUBLISHING are detectable transitions, not clean/RW residency. */
enum {PC508_CLEAN,PC508_ARMING,PC508_RESIDENT,PC508_PUBLISHING};
volatile LONG driving_pc508_active;
__declspec(thread) unsigned driving_pc508_depth[4];
static struct {volatile LONG phase;CRITICAL_SECTION owner_lock;INIT_ONCE once;
 unsigned lifetime_pass;uint64_t guarded,admitted;} pc508={0};
static __declspec(thread) unsigned pc508_lock_depth,pc508_drain_depth;
static LONG pc508_phase(void){return InterlockedCompareExchange(&pc508.phase,0,0);}
static void pc508_open(void);
static void pc508_publication_enter(void);
static void pc508_publication_leave(void);
static void pc508_drain_return(void);
static void *pc508_target_map(uint32_t va,size_t bytes,int write);
#endif
