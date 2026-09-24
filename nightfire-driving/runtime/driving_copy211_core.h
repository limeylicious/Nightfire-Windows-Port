/* Original REP widths/order, with bounded device aperture admission.
 * This does not implement GP DSP memory: reads use the existing APU model. */
#ifndef DRIVING_COPY211_CORE_H
#define DRIVING_COPY211_CORE_H
#include <stdint.h>
typedef uint32_t (*Read211)(void *, uint32_t, unsigned);
typedef void (*Write211)(void *, uint32_t, uint32_t, unsigned);
static int span211(uint32_t address,uint32_t count,unsigned width,int backward,
                   uint64_t *lo,uint64_t *hi)
{
    uint64_t travel=(uint64_t)(count-1)*width;
    if(backward && travel>address)return 0;
    *lo=backward?(uint64_t)address-travel:address;
    *hi=backward?(uint64_t)address+width:(uint64_t)address+travel+width;
    return *hi<=0x100000000ULL;
}
static int driving_copy211(void *ctx,uint32_t destination,uint32_t source,
                           uint32_t count,unsigned width,int backward,
                           Read211 read,Write211 write)
{
    uint64_t sl,sh,dl,dh;
    if(width!=1 && width!=4)return 0;
    if(!count)return 1;
    if(!span211(source,count,width,backward,&sl,&sh) ||
       !span211(destination,count,width,backward,&dl,&dh))return 0;
    /* Only ordinary memory or the complete known APU aperture is admitted.
     * Reject unsupported device regions before performing partial writes. */
    if(dh>0xfd000000ULL)return 0;
    if(sh>0xfd000000ULL && !(sl>=0xfe800000ULL && sh<=0xfe880000ULL))return 0;
    for(uint32_t i=0;i<count;i++){
        uint32_t value=read(ctx,source,width);
        write(ctx,destination,value,width);
        if(backward){source-=width;destination-=width;}
        else{source+=width;destination+=width;}
    }
    return 1;
}
#endif
