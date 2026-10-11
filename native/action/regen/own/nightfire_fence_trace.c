/* Bounded host-only diagnostics. No guest registers or memory are modified. */
#include "../../nightfire_diagnostics.h"
#include <stdio.h>
void nightfire_fence_trace(const char *stage, uint32_t device,
                          uint32_t target, uint32_t completed)
{
    static unsigned lines;
    if (lines++ >= 32) return;
    fprintf(stderr, "[GPU-FENCE] %s device=%08X target=%08X completed=%08X\n",
            stage, (unsigned)device, (unsigned)target, (unsigned)completed);
    fflush(stderr);
}
