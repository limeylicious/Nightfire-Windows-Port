/* Minimal channel control model for Nightfire's AC97 reset handshake.
 * The two channel offsets (0x10, 0x70) come from the original table at
 * 0x12F4BC. This does not implement DMA consumption or audible output. */
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#include "../../nightfire_diagnostics.h"
#include <stdio.h>
void nightfire_ac97_write8(uint32_t address, uint8_t value)
{
    if (address != 0xFEC0011Bu && address != 0xFEC0017Bu) {
        nightfire_diagnostic_stop("AC97 channel control", "unrecognized channel address");
        return;
    }
    /* RESET (bit 1) completes synchronously in this register model. The
     * original routine loads it once and loops on that cached value, so a
     * delayed background clear cannot satisfy the handshake reliably. */
    MEM8(address) = (uint8_t)(value & ~2u);
    if (value & 2u)
        fprintf(stderr, "[AC97] channel reset completed at %08X\n", (unsigned)address);
}
