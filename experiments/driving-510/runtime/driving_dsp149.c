/* Explicit startup handshake simulation, not execution of the Xbox DSP. */
#include <windows.h>
#include "driving_pc508_guards.h"
#include "driving_writeguard270.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driving_dsp_policy149.h"
extern ptrdiff_t g_xbox_mem_offset;
static int read149(void *unused,uint32_t address,uint32_t *value)
{
    SIZE_T got=0; (void)unused;
    if(!driving_dsp_span149(address,4))return 0;
    PC508_GUARD_GUEST(address,4,"dsp149.read");
    return ReadProcessMemory(GetCurrentProcess(),(void *)((uintptr_t)g_xbox_mem_offset+address),value,4,&got)&&got==4;
}
static int write149(void *unused,uint32_t address,uint32_t value)
{
    SIZE_T got=0; (void)unused;
    if(!driving_dsp_span149(address,4))return 0;
    return driving_write_process270(GetCurrentProcess(),(void *)((uintptr_t)g_xbox_mem_offset+address),&value,4,&got)&&got==4;
}
void driving_dsp_command149(uint32_t context,uint32_t address)
{
    DWORD saved=GetLastError(); uint32_t command=0xFFFFFFFFu;
    const char *mode=getenv("DRIVING_DIAGNOSTIC_DSP149");
    int enabled=mode&&strcmp(mode,"1")==0;
    int result=driving_dsp_startup149(NULL,read149,write149,enabled,0x17C8ED,context,address,&command);
    fprintf(stderr,"[DSP149] context=%08X address=%08X command=%08X mode=%s result=%d\n",context,address,command,enabled?"simulated":"original-wait",result);
    if(result<0){fprintf(stderr,"[DSP149] Unexpected descriptor/command; stopping without acknowledgement\n");abort();}
    if(result==DSP149_ACK)fprintf(stderr,"[DSP149] Startup command3 acknowledged WITHOUT DSP execution; no working-audio claim\n");
    SetLastError(saved);
}
