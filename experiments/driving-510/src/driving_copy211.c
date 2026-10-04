#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#ifdef DRIVING_ACCESS321
#include "../runtime/driving_access321.h"
#endif
#include "apu/apu.h"
#include "../runtime/driving_copy211_core.h"
extern MCPXAPUState *g_apu_state;
extern ptrdiff_t g_xbox_mem_offset;
extern void driving_translation_stop(const char *,const char *);
static uint32_t read211(void *ctx,uint32_t address,unsigned width)
{
    (void)ctx;
    if(address>=0xfe800000u && address<0xfe880000u){
        if(!g_apu_state)driving_translation_stop(__func__,"APU is unavailable");
        return (uint32_t)mcpx_apu_mmio_read(g_apu_state,address-0xfe800000u,width);
    }
#ifdef DRIVING_ACCESS321
    driving_access321_note((void*)(g_xbox_mem_offset+address),width,DA321_COPY211_READ);
#endif
    if(width==4)return *(volatile uint32_t *)(g_xbox_mem_offset+address);
    return *(volatile uint8_t *)(g_xbox_mem_offset+address);
}
static void write211(void *ctx,uint32_t address,uint32_t value,unsigned width)
{
    (void)ctx;
#ifdef DRIVING_ACCESS321
    driving_access321_note((void*)(g_xbox_mem_offset+address),width,DA321_COPY211_WRITE);
#endif
    if(width==4)*(volatile uint32_t *)(g_xbox_mem_offset+address)=value;
    else *(volatile uint8_t *)(g_xbox_mem_offset+address)=(uint8_t)value;
}
void driving_copy_read211(uint32_t destination,uint32_t source,uint32_t count,
                          unsigned width,int backward)
{
    if(!driving_copy211(NULL,destination,source,count,width,backward,read211,write211)){
        fprintf(stderr,"[COPY211] rejected dst=%08X src=%08X count=%u width=%u df=%d\n",
                destination,source,count,width,backward);
        driving_translation_stop(__func__,"unsupported bulk device-memory transfer");
    }
}
