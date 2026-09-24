#ifndef NIGHTFIRE_RGBA_MIPS129_H
#define NIGHTFIRE_RGBA_MIPS129_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/* Captured NPC corridor family only: 64x64 swizzled A8R8G8B8, five mips,
 * address4 on U/V. The CPU RGBA sampler reads base level and clamps; the
 * existing compatibility sampler reproduces that with point + MaxLOD0.
 * Ordinary texture creation still validates the complete mip-chain bounds. */
static int nf_rgba_mips129_enabled(void){
#ifdef NIGHTFIRE_RGBA_MIPS129
    static int enabled=-1;
    if(enabled<0){const char *v=getenv("NIGHTFIRE_RGBA_MIPS129");enabled=v && !strcmp(v,"1");}
    return enabled;
#else
    return 0;
#endif
}
static int nf_rgba_mips129_match(uint32_t format,uint32_t address){
    return format==0x06650629u && address==0x00010404u && nf_rgba_mips129_enabled();
}
#endif
