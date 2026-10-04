/* Explicit startup handshake simulation, not execution of the Xbox DSP. */
#include <windows.h>
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
    return ReadProcessMemory(GetCurrentProcess(),(void *)((uintptr_t)g_xbox_mem_offset+address),value,4,&got)&&got==4;
}
static int write149(void *unused,uint32_t address,uint32_t value)
{
    SIZE_T got=0; (void)unused;
    if(!driving_dsp_span149(address,4))return 0;
    return driving_write_process270(GetCurrentProcess(),(void *)((uintptr_t)g_xbox_mem_offset+address),&value,4,&got)&&got==4;
}
#ifdef DRIVING_LEAN_RENDERER
static void lean_mailbox_start149(uint32_t address);
#endif
void driving_dsp_command149(uint32_t context,uint32_t address)
{
    DWORD saved=GetLastError(); uint32_t command=0xFFFFFFFFu;
    const char *mode=getenv("DRIVING_DIAGNOSTIC_DSP149");
    int enabled=mode&&strcmp(mode,"1")==0;
    int result=driving_dsp_startup149(NULL,read149,write149,enabled,0x17C8ED,context,address,&command);
    fprintf(stderr,"[DSP149] context=%08X address=%08X command=%08X mode=%s result=%d\n",context,address,command,enabled?"simulated":"original-wait",result);
    if(result<0){fprintf(stderr,"[DSP149] Unexpected descriptor/command; stopping without acknowledgement\n");abort();}
    if(result==DSP149_ACK)fprintf(stderr,"[DSP149] Startup command3 acknowledged WITHOUT DSP execution; no working-audio claim\n");
#ifdef DRIVING_LEAN_RENDERER
    if(result==DSP149_ACK||result==DSP149_IDLE)lean_mailbox_start149(address);
#endif
    SetLastError(saved);
}
#ifdef DRIVING_LEAN_RENDERER
/* Lean build: DSound posts every later GP-DSP command to the same mailbox
 * (page0+0x810) and spins until the DSP clears it. No DSP program runs here,
 * so acknowledge each posted command about 1 ms later, like a DSP finishing
 * its next frame. Effects computed by DSP code are not produced.
 * LEAN_DSP_MAILBOX=0 restores the original wait. */
static volatile uint32_t lean_mailbox149;
static DWORD WINAPI lean_mailbox_thread149(void *unused)
{
    (void)unused;
    HANDLE timer=CreateWaitableTimerExW(NULL,NULL,2,TIMER_ALL_ACCESS);
    if(!timer)timer=CreateWaitableTimerW(NULL,FALSE,NULL);
    LARGE_INTEGER due;due.QuadPart=-10000;
    SetWaitableTimer(timer,&due,1,NULL,NULL,FALSE);
    volatile LONG *mailbox=(volatile LONG *)((uintptr_t)g_xbox_mem_offset+lean_mailbox149);
    unsigned long long counts[8]={0},total=0;
    for(;;){
        WaitForSingleObject(timer,5);
        LONG command=*mailbox;
        if(!command)continue;
        if(InterlockedCompareExchange(mailbox,0,command)!=command)continue;
        total++;if((unsigned)command<8)counts[command]++;
        if(total<=8||!(total%5000))fprintf(stderr,"[LEAN-DSP] acknowledged command=%ld total=%llu (cmd2=%llu cmd3=%llu) no DSP execution\n",(long)command,total,counts[2],counts[3]);
    }
    return 0;
}
static void lean_mailbox_start149(uint32_t address)
{
    const char *v=getenv("LEAN_DSP_MAILBOX");
    if(v&&!strcmp(v,"0"))return;
    if(InterlockedCompareExchange((volatile LONG *)&lean_mailbox149,(LONG)address,0)!=0)return;
    HANDLE h=CreateThread(NULL,0,lean_mailbox_thread149,NULL,0,NULL);
    if(h)CloseHandle(h);
    fprintf(stderr,"[LEAN-DSP] mailbox responder at %08X\n",address);
}
#endif
