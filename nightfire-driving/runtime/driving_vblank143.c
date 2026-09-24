/* Narrow PCRTC interrupt-register boundary. Original game ISR/DPC performs
 * the event signaling; this only models a write-one-to-clear register. */
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
extern ptrdiff_t xbox_GetMemoryOffset(void);
/* Optional bounded observation at original queue boundaries. No guest writes;
 * snapshots are observations, not an atomic model of the display queue. */
void driving_cadence204(unsigned tag,uint32_t context,uint32_t argument)
{
    DWORD saved=GetLastError();
    const char *flag=getenv("DRIVING_CADENCE204");
    static volatile LONG counts[5];
    if(!flag||flag[0]!='1'||tag>=5)goto done;
    LONG count=InterlockedIncrement(&counts[tag]);
    if(count>16 && (count%128 || count>4096))goto done;
    uint32_t data[0x1dc/4]={0};SIZE_T got=0;
    if(context<0x10000 || (uint64_t)context+sizeof(data)>0x88000000ull)goto done;
    if(!ReadProcessMemory(GetCurrentProcess(),(void*)((uintptr_t)xbox_GetMemoryOffset()+context),data,sizeof(data),&got)||got!=sizeof(data))goto done;
    fprintf(stderr,"[CADENCE204] ms=%llu tid=%lu tag=%u n=%ld ctx=%08X arg=%08X base=%08X flags=%08X inhibit=%u read=%u serial=%u next=%u write=%u q0=%u,%u,%08X q1=%u,%u,%08X\n",
        (unsigned long long)GetTickCount64(),GetCurrentThreadId(),tag,count,context,argument,data[0],
        data[0x1b4/4],data[0x1b8/4],data[0x1bc/4],data[0x1c0/4],data[0x1c8/4],data[0x1cc/4],
        data[0x174/4],data[0x178/4],data[0x17c/4],data[0x180/4],data[0x184/4],data[0x188/4]);
done:SetLastError(saved);
}
void driving_pcrtc_ack143(uint32_t base,uint32_t value)
{
    if(base!=0xfd000000u || (value&~1u)){
        fprintf(stderr,"[VBLANK143] unsupported PCRTC acknowledgment %08X %08X\n",base,value);fflush(stderr);abort();
    }
    volatile LONG *pending=(volatile LONG *)((uintptr_t)xbox_GetMemoryOffset()+base+0x600100u);
    volatile LONG *master=(volatile LONG *)((uintptr_t)xbox_GetMemoryOffset()+base+0x100u);
    InterlockedAnd(pending,(LONG)~value);
    if(!(*pending&1))InterlockedAnd(master,(LONG)~0x01000000u);
}
