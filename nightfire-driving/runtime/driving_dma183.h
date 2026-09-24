/* Conservative live-renderer DMA preflight. Uses original channel-0 RAMHT.
 * No host pointer or persistent mapping: caller owns committed-page checks,
 * mapping lifetime and completion of GPU aliases before reading/writing bytes.
 * Only captured IN_MEMORY objects / selector A are admitted initially. */
#ifndef DRIVING_DMA183_H
#define DRIVING_DMA183_H
#include "driving_gpu143_core.h"
typedef struct DrivingSpan183 {
    uint32_t address, available, handle, instance, offset;
} DrivingSpan183;
enum { DRIVING_COLOR183, DRIVING_DEPTH183, DRIVING_TEXTURE183,
       DRIVING_PALETTE183, DRIVING_VERTEX183 };
static int driving_span183(void *ctx, Read143 read, uint32_t ramht,
    const uint32_t state[2048], const unsigned char known[2048],
    unsigned kind, unsigned slot, uint32_t minimum, uint32_t allocated,
    DrivingSpan183 *out)
{
    uint32_t binding, offset, instance, entry, flags, limit, frame;
    uint64_t base, physical, available;
    DrivingSpan183 result;
    if(!state||!known||!out||!minimum||!allocated||allocated>0x08000000u)return 0;
#define FIELD183(m) (known[(m)/4])
    if(kind==DRIVING_COLOR183||kind==DRIVING_DEPTH183){
        unsigned m=kind==DRIVING_COLOR183?0x194:0x198;
        unsigned o=kind==DRIVING_COLOR183?0x210:0x214;
        if(!FIELD183(m)||!FIELD183(o))return 0;
        binding=state[m/4];offset=state[o/4];
    }else if(kind==DRIVING_TEXTURE183){
        if(slot>3||!FIELD183(0x184)||!FIELD183(0x1b04+slot*64)||
           !FIELD183(0x1b00+slot*64))return 0;
        if((state[(0x1b04+slot*64)/4]&3)!=1)return 0;
        binding=state[0x184/4];offset=state[(0x1b00+slot*64)/4];
    }else if(kind==DRIVING_PALETTE183){
        if(slot>3||!FIELD183(0x184)||!FIELD183(0x1b20+slot*64))return 0;
        offset=state[(0x1b20+slot*64)/4];
        /* Captured A context,256 entries, reserved low bits clear. */
        if(offset&63)return 0;
        binding=state[0x184/4];
    }else if(kind==DRIVING_VERTEX183){
        if(slot>15||!FIELD183(0x19c)||!FIELD183(0x1720+slot*4))return 0;
        offset=state[0x1720/4+slot];if(offset&0x80000000u)return 0;
        binding=state[0x19c/4];
    }else return 0;
#undef FIELD183
    if(!driving_object143(ctx,read,ramht,binding,&instance,&entry)||(entry&0x30000u))return 0;
#ifdef DRIVING_DMA183_DESCRIPTOR_READ
    uint32_t tuple252[3];if(!DRIVING_DMA183_DESCRIPTOR_READ(ctx,read,instance,tuple252))return 0;
    flags=tuple252[0];limit=tuple252[1];frame=tuple252[2];
#else
    if(!read(ctx,instance,&flags)||!read(ctx,instance+4,&limit)||!read(ctx,instance+8,&frame))return 0;
#endif
    /* Retain143's linear NVM/PCI target contract; do not broaden protection
     * semantics. IN_MEMORY is needed for render target seed reads and writes. */
    if((flags&0xfff)!=0x3d||!(flags&0x2000)||
       ((flags&0x30000)!=0&&(flags&0x30000)!=0x20000))return 0;
    base=(uint64_t)(frame&0xfffff000u)+(flags>>20);physical=base+offset;
    if((uint64_t)offset+minimum>(uint64_t)limit+1||physical>=allocated)return 0;
    available=(uint64_t)limit+1-offset;
    if(available>allocated-physical)available=allocated-physical;
    if(minimum>available)return 0;
    result.address=0x80000000u+(uint32_t)physical;
    result.available=(uint32_t)available;result.handle=binding;
    result.instance=instance;result.offset=offset;*out=result;return 1;
}
#endif
