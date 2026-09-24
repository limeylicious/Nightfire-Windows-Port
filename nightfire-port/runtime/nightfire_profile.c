#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static struct {uint32_t va;const char *name;ULONGLONG start,total,max;unsigned calls;} samples[]={
 {0x130624,"video step"},{0x1328e1,"video decode"},{0x130d2f,"video convert"},
 {0x103730,"D3D swap"},{0x1065a0,"insert fence"},{0x106650,"wait fence"},
 {0x1035e0,"rotate surfaces"},{0x1036f0,"swap finish"},{0x1064b0,"publish commands"},
 {0xffff0001,"GPU execution"},{0xdffb0,"frontend update"},{0x85240,"intro handler"}};
/* Opt-in bounded call-frequency survey for focused level starts. Counts are
 * leads for profiling, not execution time or translated-instruction coverage. */
static void call_survey(uint32_t va,unsigned after){
    static int enabled=-1,done;static unsigned frames;static uint32_t *counts;
    if(enabled<0)enabled=getenv("NIGHTFIRE_CALL_SURVEY")!=NULL;
    if(!enabled || done)return;
    if(va==0x103730 && after==1){
        frames++;
        if(frames==100){counts=calloc(0x200000,sizeof *counts);if(!counts){done=1;return;}fprintf(stderr,"[CALL-SURVEY] begin swap=100\n");}
        if(frames==120){
            uint64_t total=0;for(unsigned i=0;i<0x200000;i++)total+=counts[i];
            fprintf(stderr,"[CALL-SURVEY] end swaps=20 calls=%llu\n",(unsigned long long)total);
            for(unsigned rank=0;rank<40;rank++){
                unsigned top=0;for(unsigned i=0;i<0x200000;i++)if(counts[i]>counts[top])top=i;
                if(!counts[top])break;
                fprintf(stderr,"[CALL-SURVEY] rank=%u va=%08X calls=%u\n",rank+1,top,counts[top]);counts[top]=0;
            }
            free(counts);counts=NULL;done=1;
        }
    }
    if(counts && !after && va<0x200000)counts[va]++;
}
void nightfire_profile(uint32_t va,unsigned after)
{
    call_survey(va,after);
    static int enabled=-1;static ULONGLONG report;
    if(enabled<0) enabled=getenv("NIGHTFIRE_PROFILE")!=NULL;
    if(!enabled || after>1) return;
    /* Most translated calls are not timed: dispatch only the named boundaries
     * instead of scanning every timing entry at every call/return. */
    unsigned i;
    switch(va){
    case 0x130624:i=0;break;case 0x1328e1:i=1;break;case 0x130d2f:i=2;break;
    case 0x103730:i=3;break;case 0x1065a0:i=4;break;case 0x106650:i=5;break;
    case 0x1035e0:i=6;break;case 0x1036f0:i=7;break;case 0x1064b0:i=8;break;
    case 0xffff0001:i=9;break;case 0xdffb0:i=10;break;case 0x85240:i=11;break;
    default:return;
    }
    {
        ULONGLONG now=GetTickCount64();
        if(!after) samples[i].start=now;
        else {
            ULONGLONG ms=now-samples[i].start;samples[i].total+=ms;samples[i].calls++;
            if(ms>samples[i].max)samples[i].max=ms;
            if(now-report>=5000) {
                report=now;
                for(unsigned j=0;j<sizeof samples/sizeof samples[0];j++)
                    fprintf(stderr,"[PROFILE] %s calls=%u total_ms=%llu max_ms=%llu\n",samples[j].name,samples[j].calls,samples[j].total,samples[j].max);
            }
        }
    }
}
