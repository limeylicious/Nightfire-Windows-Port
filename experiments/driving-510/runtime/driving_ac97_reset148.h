#ifndef DRIVING_AC97_RESET148_H
#define DRIVING_AC97_RESET148_H
#include <stdint.h>
#include <stddef.h>
/* Independently expressed AC97 bus-master register-reset semantics.
 * Research references/licensing are documented in FINDINGS.md; no upstream
 * implementation is copied. This updates a mirror only. The caller must also
 * invalidate any host descriptor and stop the corresponding active output.
 * Narrow operation: PAL18213A writes exactly2 to one of these two channels.
 * Returns -1 without writes for an unsupported address/value/span, otherwise
 * 0=PCM-out or1=S/PDIF. Does not claim to run a codec/DSP or deliver interrupts.
 */
static int driving_ac97_reset148(uint8_t *mmio,size_t span,
                                 uint32_t address,uint8_t value)
{
 size_t base; int channel; uint8_t retained;
 if (!mmio || value!=2) return -1;
 if(address==0xFEC0011Bu){base=0x110;channel=0;}
 else if(address==0xFEC0017Bu){base=0x170;channel=1;}
 else return -1;
 if(span<base+12)return -1;
 retained=(uint8_t)(mmio[base+11]&0x1C);
 /* BDBAR, current/last indices, status, count, prefetched index, control. */
 for(size_t i=0;i<12;i++)mmio[base+i]=0;
 mmio[base+6]=1; /* DMA channel halted. */
 mmio[base+11]=retained;
 return channel;
}
#endif
