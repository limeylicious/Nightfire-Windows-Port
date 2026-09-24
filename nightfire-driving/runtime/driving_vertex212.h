/* Bounded decoder for the three vertex formats in owned Driving capture210.
 * Caller supplies the resolved DMA byte span, including its limit. This does
 * not enable live drawing, decode textures, or interpret primitive topology.
 */
#ifndef DRIVING_VERTEX212_H
#define DRIVING_VERTEX212_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
static int driving_vertex212(const uint8_t *dma,size_t available,uint32_t format,
                             uint32_t offset,uint32_t index,float output[4])
{
    unsigned kind=format&15,count=(format>>4)&15,stride=format>>8,bytes;
    if(!dma || count<1 || count>4)return 0;
    if(kind==0){if(count!=4)return 0;bytes=4;}
    else if(kind==2)bytes=count*4;
    else if(kind==5)bytes=count*2;
    else return 0;
    uint64_t position=(uint64_t)offset+(uint64_t)index*stride;
    if(position+bytes>available)return 0;
    const uint8_t *p=dma+(size_t)position;float value[4]={0,0,0,1};
    if(kind==0){
        value[0]=p[2]/255.0f;value[1]=p[1]/255.0f;
        value[2]=p[0]/255.0f;value[3]=p[3]/255.0f;
    }else for(unsigned i=0;i<count;i++){
        if(kind==2)memcpy(&value[i],p+i*4,4);
        else{int16_t component;memcpy(&component,p+i*2,2);value[i]=(float)component;}
    }
    for(unsigned i=0;i<4;i++)if(!isfinite(value[i]))return 0;
    memcpy(output,value,sizeof value);return 1;
}
#endif
