/* Native D3D: native versions of library-internal helpers that other native
 * routines call. Each keeps the original helper's guest ABI (arguments on the
 * guest stack and/or ECX/EDX, same `ret N`), so callers use nd3d_call():
 *     nd3d_call(nd3d_h_00167F80, ecx, edx, nargs, args...)
 * Helpers that originally took a push-buffer pointer in EDX and returned the
 * advanced pointer in EAX now store state words instead and return EDX
 * unchanged in EAX. Implemented in nd3d_g1_state.c. */
#ifndef ND3D_INTERNAL_H
#define ND3D_INTERNAL_H
#include "nd3d_api.h"

void nd3d_h_00167F80(void);   /* viewport offset/scale constants + clip range */
void nd3d_h_00168170(void);   /* CONTROL0 word (W-buffer, float Z, YUV) */
void nd3d_h_0016AC90(void);   /* pass-through program constants (XYZRHW FVF) */
void nd3d_h_00167F30(void);   /* bump-environment matrices for a stage */

/* Recompiled routines that native code may call (guest ABI, via nd3d_call). */
void sub_001716A0(void);      /* lazy state flush dispatcher (CPU only; calls the native helpers) */
#endif
