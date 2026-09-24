/* Original AC97 reset write boundary. No active bus-master output/descriptor
 * exists in this bootstrap runtime. An active channel is rejected until that
 * ownership is implemented; this acknowledgement does not produce audio. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "driving_ac97_reset148.h"
extern ptrdiff_t g_xbox_mem_offset;
void driving_ac97_write148(uint32_t address,uint32_t value)
{
    DWORD saved=GetLastError();
    if((address!=0xFEC0011Bu && address!=0xFEC0017Bu) || value!=2){
        fprintf(stderr,"[AC97148] Unsupported reset write %08X=%08X\n",address,value);abort();
    }
    uint8_t *mmio=(uint8_t *)((uintptr_t)g_xbox_mem_offset+0xFEC00000u);
    size_t control=address-0xFEC00000u;
    if(mmio[control]&1){
        fprintf(stderr,"[AC97148] Active bus-master reset requires output ownership; stopping\n");abort();
    }
    int channel=driving_ac97_reset148(mmio,0x180,address,(uint8_t)value);
    if(channel<0)abort();
    static unsigned resets;
    if(++resets<=4)fprintf(stderr,"[AC97148] Original channel%d reset completed synchronously; no sound-output claim\n",channel);
    SetLastError(saved);
}
