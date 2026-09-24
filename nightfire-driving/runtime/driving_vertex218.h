/* Opt-in, unused CMP vertex decoder extension. Other formats retain decoder212.
 * CMP packs signed X/Y/Z in 11/11/10 bits; its count field is one DWORD.
 * The normalized minimum intentionally remains below -1 (no guessed clamp).
 */
#ifndef DRIVING_VERTEX218_H
#define DRIVING_VERTEX218_H
#include "driving_vertex212.h"
static int driving_vertex218(const uint8_t *dma,size_t available,uint32_t format,
                             uint32_t offset,uint32_t index,float output[4])
{
    if(!output)return 0;
    if((format&15)!=6)
        return driving_vertex212(dma,available,format,offset,index,output);
    if(!dma || ((format>>4)&15)!=1)return 0;
    uint64_t position=(uint64_t)offset+(uint64_t)index*(format>>8);
    if(position+4>available)return 0;
    uint32_t packed;
    memcpy(&packed,dma+(size_t)position,4);
    int32_t x=(int32_t)(packed&2047),y=(int32_t)((packed>>11)&2047);
    int32_t z=(int32_t)(packed>>22);
    if(x&1024)x-=2048;
    if(y&1024)y-=2048;
    if(z&512)z-=1024;
    float value[4]={(float)x/1023.0f,(float)y/1023.0f,
                    (float)z/511.0f,1.0f};
    memcpy(output,value,sizeof value);
    return 1;
}
#endif
