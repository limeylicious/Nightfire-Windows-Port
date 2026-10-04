/*508: instrumentation for the separate runtime-disabled residency layer.
 * Counts describe locks actually acquired; they do not authorize publication.
 * Guard implementation owns runtime enable, overlap, FP/error preservation. */
#ifndef DRIVING_PC508_GUARDS_H
#define DRIVING_PC508_GUARDS_H
#ifdef DRIVING_PC508
#include "driving_pc508.h"
#define PC508_GUARD_NATIVE(p,n,s) driving_pc508_guard_native((p),(n),(s))
#define PC508_GUARD_GUEST(p,n,s) driving_pc508_guard_guest((p),(n),(s))
#define PC508_DEPTH_ENTER(k) (++driving_pc508_depth[(k)])
#define PC508_DEPTH_LEAVE(k) (--driving_pc508_depth[(k)])
#else
#define PC508_GUARD_NATIVE(p,n,s) ((void)0)
#define PC508_GUARD_GUEST(p,n,s) ((void)0)
#define PC508_DEPTH_ENTER(k) ((void)0)
#define PC508_DEPTH_LEAVE(k) ((void)0)
#endif
#endif
