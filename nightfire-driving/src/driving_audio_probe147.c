/* Bounded read-only DSP constructor diagnostics; no synthetic audio success. */
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern ptrdiff_t g_xbox_mem_offset;
static LONG counts[5];
static int read147(uint64_t va,void *out,size_t n){
    SIZE_T got=0;
    if(!va || !n || n>128 || !((va<0x4000000 && va+n<=0x4000000)
        || (va>=0x80000000 && va+n<=0x84000000)))return 0;
    return ReadProcessMemory(GetCurrentProcess(),(void *)((uintptr_t)g_xbox_mem_offset+(uintptr_t)va),out,n,&got)&&got==n;
}
static uint32_t word147(FILE *f,const char *label,uint64_t va){
    uint32_t v=0;int ok=read147(va,&v,4);
    fprintf(f,"%s at=%08llX readable=%d value=%08X\n",label,(unsigned long long)va,ok,v);
    return ok?v:0;
}
static void dump147(FILE *f,const char *label,uint32_t va,unsigned n){
    unsigned char b[128];int ok=read147(va,b,n);
    fprintf(f,"%s at=%08X readable=%d hex=",label,va,ok);
    if(ok)for(unsigned i=0;i<n;i++)fprintf(f,"%02X",b[i]);fputc('\n',f);
}
void driving_audio_probe147(uint32_t site,uint32_t object,uint32_t result,uint32_t stack){
    DWORD saved=GetLastError();FILE *f=NULL;char path[1200];
    int slot=site==0x17AD05?0:site==0x17C9F5?1:site==0x17C23A?2:site==0x17C253?3:site==0x1818A4?4:-1;
    if(slot<0 || InterlockedCompareExchange(&counts[slot],0,0)>=2)goto done;
    const char *dir=getenv("DRIVING_CAPTURE_DIR");
    if(!dir || strlen(dir)>1000 || strlen(dir)<3 || dir[1]!=':' || (dir[2]!='/'&&dir[2]!='\\'))goto done;
    DWORD attrs=GetFileAttributesA(dir);
    if(attrs==INVALID_FILE_ATTRIBUTES || !(attrs&FILE_ATTRIBUTE_DIRECTORY))goto done;
    LONG sample=InterlockedIncrement(&counts[slot]);if(sample>2)goto done;
    int n=snprintf(path,sizeof path,"%s/audio147-%08X-%ld.txt",dir,site,sample);
    if(n<0 || n>=sizeof path)goto done;
    f=fopen(path,"wb");if(!f)goto done;
    fprintf(f,"site=%08X object=%08X result=%08X stack=%08X thread=%lu\n",site,object,result,stack,GetCurrentThreadId());
    word147(f,"global.device",0x1838F0);word147(f,"game.device",0x244C84);
    dump147(f,"stack",stack,32);dump147(f,"object",object,64);
    if(site==0x17C253){uint32_t out=word147(f,"output.slot",(uint64_t)stack+8);word147(f,"output.value",out);}
    if(site==0x17AD05){uint32_t child=word147(f,"DSP.child",(uint64_t)object+0x14);dump147(f,"DSP.object",child,64);}
    if(site==0x17C9F5){uint32_t a=word147(f,"DSP.owner",(uint64_t)object+8);uint32_t b=word147(f,"DSP.pages",(uint64_t)a+0x10);word147(f,"DSP.page0",b);}
done:
    if(f)fclose(f);SetLastError(saved);
}
