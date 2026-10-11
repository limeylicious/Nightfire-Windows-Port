/* Explicit diagnostic acknowledgment, NOT implementation of the Xbox DSP. */
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#include "../../nightfire_diagnostics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void nightfire_dsp_startup_command(uint32_t address)
{
    const char *mode = getenv("NIGHTFIRE_DIAGNOSTIC_DSP");
    uint32_t base = MEM32(0x0012FD08u);
    /* Validate against the live descriptor, not the last run's allocation. */
    if (base < 0x80000000u || base > 0x83FFF7ECu || address != base + 0x810u) {
        nightfire_diagnostic_stop("DSP startup", "unexpected command location; no acknowledgment written");
        return;
    }
    uint32_t command = MEM32(address);
    fprintf(stderr, "[DSP-DIAGNOSTIC] command=%08X address=%08X mode=%s\n",
            (unsigned)command, (unsigned)address,
            mode && strcmp(mode,"1")==0 ? "simulated" : "hardware wait");
    if (!mode || strcmp(mode,"1")!=0) return;
    if (command == 0) return;
    if (command != 3) {
        nightfire_diagnostic_stop("DSP startup", "unknown command; no acknowledgment written");
        return;
    }
    fprintf(stderr, "[DSP-DIAGNOSTIC] Acknowledging startup command 3 WITHOUT DSP execution. Audio effects/results are not implemented.\n");
    MEM32(address) = 0;
}
