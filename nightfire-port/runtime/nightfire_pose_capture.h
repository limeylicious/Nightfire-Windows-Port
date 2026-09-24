/* One-shot original guest-state fixture. Explicit diagnostic only; no guest
 * memory edits. Snapshot disk I/O perturbs timing, so never use for benchmarks. */
#include <windows.h>
static void nightfire_pose_capture(uint32_t va,unsigned after){
    if((va!=0x13e50 && va!=0x160a0) || after>1)return;
    static int enabled=-1;static unsigned pending,index;static uint32_t output;
    if(enabled<0){const char *e=getenv("NIGHTFIRE_POSE_CAPTURE");enabled=e?(int)strtoul(e,NULL,16):0;}
    if(!enabled || va!=(uint32_t)enabled)return;
    static ULONGLONG next;
    if(!after){
        if(va==0x160a0 && getenv("NIGHTFIRE_POSE_WEAPON")){
            uint32_t player=MEM32(0x1f6654);if(!player)return;
            uint32_t state=MEM32(player+0xbc);if(!state)return;
            if(MEM32(g_esp+4)!=MEM32(state+0x778))return;
        }
        ULONGLONG now=GetTickCount64();if(now<next)return;next=now+1000;
        FILE *request=fopen("analysis/checkpoint-68/capture.request","r");if(!request)return;
        fclose(request);remove("analysis/checkpoint-68/capture.request");
        char path[160];
        for(;;){snprintf(path,sizeof path,"analysis/checkpoint-68/pose-%u-before.bin",index);FILE *old=fopen(path,"rb");if(!old)break;fclose(old);index++;}
        FILE *f=fopen(path,"wb");if(!f)return;
        uint32_t r[]={0x504f5345,va,g_eax,g_ebx,g_ecx,g_edx,g_esi,g_edi,g_ebp,g_esp,g_fp_top,g_fp_control_word,g_seh_ebp};
        fwrite(r,sizeof r,1,f);fwrite(g_fp_stack,sizeof(double),8,f);
        output=va==0x160a0?MEM32(MEM32(g_esp+8)+0x34):MEM32(g_esp+4);
        const uint32_t starts[]={0,0x80000000u},ends[]={0x4000000,0x84000000u};
        for(unsigned range=0;range<2;range++){
            uint64_t at=starts[range];
            while(at<ends[range]){
                MEMORY_BASIC_INFORMATION info;
                uintptr_t host=(uintptr_t)(g_xbox_mem_offset+at);
                if(!VirtualQuery((void *)host,&info,sizeof info))break;
                uint64_t stop=(uintptr_t)info.BaseAddress+info.RegionSize-(uintptr_t)g_xbox_mem_offset;
                if(stop>ends[range])stop=ends[range];if(stop<=at)break;
                if(info.State==MEM_COMMIT && !(info.Protect&(PAGE_NOACCESS|PAGE_GUARD))){
                    SIZE_T bytes=(SIZE_T)(stop-at),got=0;void *copy=malloc(bytes);
                    if(copy && ReadProcessMemory(GetCurrentProcess(),(void *)host,copy,bytes,&got) && got==bytes){
                        uint32_t block[]={(uint32_t)at,(uint32_t)bytes};fwrite(block,sizeof block,1,f);fwrite(copy,1,bytes,f);
                    }
                    free(copy);
                }
                at=stop;
            }
        }
        fclose(f);pending=1;fprintf(stderr,"[POSE-CAPTURE] before index=%u output=%08X\n",index,output);
    }else if(pending){
        pending=0;char path[160];snprintf(path,sizeof path,"analysis/checkpoint-68/pose-%u-after.bin",index++);
        FILE *f=fopen(path,"wb");if(!f)return;
        uint32_t r[]={output,g_eax,g_ebx,g_ecx,g_edx,g_esi,g_edi,g_ebp,g_esp,g_fp_top};fwrite(r,sizeof r,1,f);
        fwrite((void *)(g_xbox_mem_offset+(uintptr_t)output),1,0xa80,f);
        fclose(f);fprintf(stderr,"[POSE-CAPTURE] after saved\n");
    }
}
