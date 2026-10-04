/* Synchronous packet walker adapted from nightfire-port/runtime/nv2a_pb_scan.c.
 * No guest addresses or renderer dependencies; callbacks supply the memory bus.
 * The retained action path is unchanged. */
#ifndef DRIVING_GPU143_CORE_H
#define DRIVING_GPU143_CORE_H
#include <stdint.h>
typedef struct DrivingPB143 {
    uint32_t cursor, remaining, method, subchannel, increment, return_address;
    uint64_t words, jumps, methods;
    int initialized;
} DrivingPB143;
typedef int (*Read143)(void *,uint32_t,uint32_t *);
typedef int (*Method143)(void *,uint32_t,uint32_t,uint32_t);
static int driving_pb143_consume(DrivingPB143 *s,uint32_t put,void *ctx,Read143 read,Method143 execute)
{
    unsigned budget=0;
#ifdef DRIVING_COMMAND245_SNAPSHOT
    uint32_t owned245[256];unsigned count245=0,next245=0;
#endif
    if(put>=0x08000000u || (put&3))return 0;
    if(!s->initialized){s->cursor=put;s->initialized=1;return 1;}
    while(s->cursor!=put){
        uint32_t w;
        if(++budget>0x100000 || s->cursor>=0x08000000u || (s->cursor&3))return 0;
#ifdef DRIVING_COMMAND245_SNAPSHOT
        if(next245==count245){
            next245=count245=0;
            if(s->remaining){
                unsigned cap245=0x100001u-budget;if(cap245>256)cap245=256;
                count245=DRIVING_COMMAND245_SNAPSHOT(ctx,s,put,owned245,cap245);
                if(count245>cap245||count245>s->remaining)return 0;
            }
        }
        if(next245<count245){w=owned245[next245++];
#ifdef DRIVING_COMMAND245_FETCH
            DRIVING_COMMAND245_FETCH(1);
#endif
        }else
#endif
        {
#ifdef DRIVING_COMMAND245_FETCH
            DRIVING_COMMAND245_FETCH(0);
#endif
            if(!read(ctx,s->cursor,&w))return 0;
        }
        s->cursor+=4;s->words++;
        if(s->remaining){
            if(!execute(ctx,s->subchannel,s->method,w))return 0;
            s->method+=s->increment;s->remaining--;s->methods++;continue;
        }
        if((w&3)==1 || (w&0xe0000003u)==0x20000000u){
            s->cursor=(w&3)==1?w&0xfffffffcu:w&0x1ffffffcu;s->jumps++;continue;
        }
        if((w&3)==2){if(s->return_address)return 0;s->return_address=s->cursor;s->cursor=w&0xfffffffcu;continue;}
        if(w==0x00020000u){if(!s->return_address)return 0;s->cursor=s->return_address;s->return_address=0;continue;}
        if((w&0xe0030003u)==0 || (w&0xe0030003u)==0x40000000u){
            s->remaining=(w>>18)&0x7ff;s->method=w&0x1ffc;s->subchannel=(w>>13)&7;
            s->increment=(w&0x40000000u)?0:4;continue;
        }
        return 0;
    }
    return 1;
}
/* Resolve a channel-0 object through the guest RAMHT. The bounded scan avoids
 * duplicating the GPU hash collision algorithm while retaining identity. */
static int driving_object143(void *ctx,Read143 read,uint32_t ramht,uint32_t handle,uint32_t *instance,uint32_t *entry)
{
    uint32_t base=(ramht&0x1f0u)<<8,bytes=4096u<<((ramht>>16)&3);
    unsigned matches=0;
#ifdef DRIVING_OBJECT143_BULK_READ
    /* A fresh owned RAMHT snapshot per lookup removes thousands of same-process
     * read syscalls. Never retain entries across lookups: the title may replace
     * a binding. The callback-only path remains available to existing fixtures.
     * On any read failure outputs are unspecified, as in the scalar path. */
    uint32_t snapshot143[8192];
    if(!DRIVING_OBJECT143_BULK_READ(ctx,0xfd700000u+base,snapshot143,bytes))return 0;
#endif
    for(uint32_t i=0;i<bytes;i+=8){
        uint32_t h,c;
#ifdef DRIVING_OBJECT143_BULK_READ
        h=snapshot143[i/4];c=snapshot143[i/4+1];
#else
        if(!read(ctx,0xfd700000u+base+i,&h)||!read(ctx,0xfd700004u+base+i,&c))return 0;
#endif
        if(h==handle && (c&0x80000000u) && !(c&0x1f000000u)){
            *instance=0xfd700000u+((c&0xffffu)<<4);*entry=c;matches++;
        }
    }
    return matches==1;
}
static int driving_dma143(void *ctx,Read143 read,uint32_t ramht,uint32_t handle,uint32_t offset,uint32_t bytes,int writable,uint32_t *destination)
{
    uint32_t instance,entry,flags,limit,frame;
    if(!driving_object143(ctx,read,ramht,handle,&instance,&entry)||(entry&0x30000u))return 0;
    if(!read(ctx,instance,&flags)||!read(ctx,instance+4,&limit)||!read(ctx,instance+8,&frame))return 0;
    /* Linear DMA_IN_MEMORY or DMA_TO_MEMORY in the known physical aperture.
     * Nonlinear/page-table mappings require a separate decoder. */
    if(((flags&0xfff)!=0x3d && (flags&0xfff)!=(writable?3u:2u)) || !(flags&0x2000)
       || ((flags&0x30000)!=0 && (flags&0x30000)!=0x20000))return 0;
    uint64_t base=(frame&0xfffff000u)+(flags>>20),physical=base+offset;
    if(!bytes||(uint64_t)offset+bytes>(uint64_t)limit+1||physical+bytes>0x08000000u)return 0;
    *destination=0x80000000u+(uint32_t)physical;
    return 1;
}
static int driving_semaphore143(void *ctx,Read143 read,uint32_t ramht,uint32_t handle,uint32_t offset,uint32_t *destination)
{
    return !(offset&3) && driving_dma143(ctx,read,ramht,handle,offset,4,1,destination) && !(*destination&3);
}
#endif
