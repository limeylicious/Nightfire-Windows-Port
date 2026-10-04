/* Thread-local, bounded visitor history. Read-only; only dumped on a crash. */
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../runtime/driving_pc508_guards.h"
extern ptrdiff_t g_xbox_mem_offset;
typedef struct Trace156 {uint32_t site,regs[8],stack[12],visitor[4];unsigned valid;} Trace156;
static __declspec(thread) Trace156 history[512];
static __declspec(thread) unsigned sequence;
static int read156(uint64_t address,void *out,size_t size)
{
    uint64_t end=(uint64_t)address+size; SIZE_T got=0;
    if(!address || !((address<0x04000000u&&end<=0x04000000u)||(address>=0x80000000u&&end<=0x84000000u)))return 0;
    PC508_GUARD_GUEST((uint32_t)address,size,"resource_trace156.read");
    return ReadProcessMemory(GetCurrentProcess(),(void *)((uintptr_t)g_xbox_mem_offset+address),out,size,&got)&&got==size;
}
void driving_resource_trace156(uint32_t site,uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t s,uint32_t i,uint32_t bp,uint32_t sp)
{
    DWORD saved=GetLastError();Trace156 *t=&history[sequence++%512];
    memset(t,0,sizeof *t);t->site=site;
    uint32_t regs[8]={a,b,c,d,s,i,bp,sp};memcpy(t->regs,regs,sizeof regs);
    if(read156(sp,t->stack,sizeof t->stack))t->valid|=1;
    if(read156(i,t->visitor,sizeof t->visitor))t->valid|=2;
    SetLastError(saved);
}
void driving_resource_dump156(void)
{
    DWORD saved=GetLastError();const char *dir=getenv("DRIVING_CAPTURE_DIR");char path[1200];
    if(!dir || strlen(dir)>1000 || strlen(dir)<3 || dir[1]!=':' || (dir[2]!='/'&&dir[2]!='\\'))goto done;
    snprintf(path,sizeof path,"%s/resource-trace156-%lu.txt",dir,GetCurrentThreadId());
    FILE *f=fopen(path,"wb");if(!f)goto done;
    fprintf(f,"thread=%lu total=%u retained=%u order=eax,ebx,ecx,edx,esi,edi,localEBP,esp\n",GetCurrentThreadId(),sequence,sequence<512?sequence:512);
    for(unsigned n=sequence>512?sequence-512:0;n<sequence;n++){
        Trace156 *t=&history[n%512];fprintf(f,"%u site=%08X valid=%u regs=",n,t->site,t->valid);
        for(unsigned j=0;j<8;j++)fprintf(f,"%08X,",t->regs[j]);fprintf(f," stack=");
        for(unsigned j=0;j<12;j++)fprintf(f,"%08X,",t->stack[j]);fprintf(f," visitor=");
        for(unsigned j=0;j<4;j++)fprintf(f,"%08X,",t->visitor[j]);fputc('\n',f);
    }
    fclose(f);
done:SetLastError(saved);
}

/* Failure-only best-effort survey of the original121070 update list.
 * PAL121070 loads manager=[243A58], sentinel=[manager+4], first=[sentinel].
 * Nodes use +0 next/+8 object; original call uses object's vtable+0C and +98
 * channel mask. Bounded reads/cycle detection; never executes or edits entries. */
void driving_scheduler_dump173(void)
{
    DWORD saved=GetLastError();uint32_t manager=0,head=0,node=0,seen[256];unsigned count=0;
    if(!read156(0x243a58,&manager,4)||!manager||!read156((uint64_t)manager+4,&head,4)
       ||!head||!read156(head,&node,4))goto done173;
    fprintf(stderr,"[SCHEDULER173] failure-only list manager=%08X sentinel=%08X first=%08X\n",manager,head,node);
    while(node&&node!=head&&count<256){
        uint32_t links[3],object[40],target=0;
        for(unsigned i=0;i<count;i++)if(seen[i]==node){fprintf(stderr,"[SCHEDULER173] cycle at %08X\n",node);goto done173;}
        seen[count++]=node;
        if(!read156(node,links,sizeof links)||!links[2]||!read156(links[2],object,sizeof object)){
            fprintf(stderr,"[SCHEDULER173] unreadable node/object at %08X\n",node);goto done173;}
        int valid=read156((uint64_t)object[0]+12,&target,4);
        fprintf(stderr,"[SCHEDULER173] entry=%u node=%08X object=%08X vtable=%08X target=%08X readable=%d mask=%08X channel_bits=%08X,%08X,%08X\n",
            count,node,links[2],object[0],target,valid,object[0x98/4],object[0x84/4],object[0x88/4],object[0x8c/4]);
        node=links[0];
    }
    fprintf(stderr,"[SCHEDULER173] end count=%u next=%08X bounded=%d\n",count,node,count==256);
done173:SetLastError(saved);
}
