/* Bounded original-PCRTC snapshot. Incomplete software graphics; no timing or
 * Xbox pixel fidelity claim. No recursive drain, guest writes or synthetic image. */
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driving_scanout152.h"
#include "driving_pc508_guards.h"
#include "driving_async295.h"
#ifdef DRIVING_PUBLISH324
#include "driving_publication324.h"
#endif
extern ptrdiff_t xbox_GetMemoryOffset(void);
extern uint32_t xbox_ContiguousAllocatedBytes(void);
static INIT_ONCE once=INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION lock;
static unsigned depth,complete,primitive;
static uint32_t av_mode,av_format,av_pitch,av_physical,av_version;
static volatile LONG requests,claimed,busy,incomplete,rejected,failed,written;
static char directory[MAX_PATH];
static volatile LONG report_ticks;
static BOOL CALLBACK initialize(PINIT_ONCE o,PVOID p,PVOID *c)
{
    (void)o;(void)p;(void)c;InitializeCriticalSection(&lock);
    const char *v=getenv("DRIVING_CAPTURE_DIR");
    /* Existing absolute directory only. Never create directories or use CWD. */
    if(v && strlen(v)<MAX_PATH-80 && ((v[0]&&v[1]==':'&&(v[2]=='\\'||v[2]=='/'))
        || (v[0]=='\\'&&v[1]=='\\'))){
        DWORD a=GetFileAttributesA(v);
        if(a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_DIRECTORY))strcpy(directory,v);
    }
    return TRUE;
}
static void init(void){InitOnceExecuteOnce(&once,initialize,NULL,NULL);}
void driving_scanout152_enter(void){DWORD e=GetLastError();init();EnterCriticalSection(&lock);depth++;complete=0;SetLastError(e);}
void driving_scanout152_leave(unsigned ready){DWORD e=GetLastError();complete=ready&&!primitive;depth--;LeaveCriticalSection(&lock);SetLastError(e);}
void driving_scanout152_begin(unsigned value){primitive=value;}
void driving_scanout152_report(void)
{
    DWORD e=GetLastError();
    if(!(InterlockedIncrement(&report_ticks)%128) && InterlockedCompareExchange(&requests,0,0))
        fprintf(stderr,"[SCANOUT152] aggregate requests=%ld busy=%ld incomplete=%ld rejected=%ld failed=%ld written=%ld diagnostic=incomplete-graphics\n",
            InterlockedCompareExchange(&requests,0,0),InterlockedCompareExchange(&busy,0,0),
            InterlockedCompareExchange(&incomplete,0,0),InterlockedCompareExchange(&rejected,0,0),
            InterlockedCompareExchange(&failed,0,0),InterlockedCompareExchange(&written,0,0));
    SetLastError(e);
}
void driving_scanout152_av(uint32_t mode,uint32_t format,uint32_t pitch,uint32_t physical)
{DWORD e=GetLastError();driving_async295_enter();init();EnterCriticalSection(&lock);if(av_mode!=mode || av_format!=format || av_pitch!=pitch || av_physical!=physical)av_version++;
    av_mode=mode;av_format=format;av_pitch=pitch;av_physical=physical;LeaveCriticalSection(&lock);driving_async295_leave();SetLastError(e);}
static int read_guest(uint32_t va,void *out,size_t bytes)
{
    uint64_t end=(uint64_t)va+bytes;
    /* Low image/stack, contiguous arena, and the two exact PUT/GET registers. */
    if(!va || !((va<0x04000000u&&end<=0x04000000u)
       || (va>=0x80000000u&&end<=0x88000000ull)
       || (va==0xfd800040u&&bytes==8)))return 0;
    PC508_GUARD_GUEST(va,bytes,"scanout152.read_guest");
    SIZE_T got=0;
#ifdef DRIVING_PUBLISH324
    return xbox_PublicationRead324(GetCurrentProcess(),
#else
    return ReadProcessMemory(GetCurrentProcess(),
#endif
        (const void *)((uintptr_t)xbox_GetMemoryOffset()+va),out,bytes,&got)&&got==bytes;
}
static int write_bmp(const char *path,const unsigned char *pixels)
{
    unsigned char h[54]={0};uint32_t size=54+2560*480,offset=54,dib=40,w=640,compression=0;
    int32_t height=-480;uint16_t planes=1,bits=32;
    h[0]='B';h[1]='M';memcpy(h+2,&size,4);memcpy(h+10,&offset,4);memcpy(h+14,&dib,4);
    memcpy(h+18,&w,4);memcpy(h+22,&height,4);memcpy(h+26,&planes,2);memcpy(h+28,&bits,2);memcpy(h+30,&compression,4);
    FILE *f=fopen(path,"wb");if(!f)return 0;
    int ok=fwrite(h,1,54,f)==54 && fwrite(pixels,1,2560*480,f)==2560*480;
    if(fclose(f))ok=0;return ok;
}
void driving_scanout152_capture(uint32_t context,uint32_t base,uint32_t physical,uint32_t argument_address)
{
    DWORD saved=GetLastError();init();
    if(!directory[0] || InterlockedCompareExchange(&claimed,0,0)>=12)goto done;
    LONG request=InterlockedIncrement(&requests);
    /* First, then every eighth request. Bounded successes, no persistent queue. */
    if((request-1)%8)goto done;
    if(!TryEnterCriticalSection(&lock)){InterlockedIncrement(&busy);goto done;}
    if(depth || !complete || primitive){InterlockedIncrement(&incomplete);LeaveCriticalSection(&lock);goto done;}
    uint32_t ctx[0x1d4/4],original,io[2];
    if(base!=0xfd000000u || !read_guest(context,ctx,sizeof(ctx)) || ctx[0]!=base
        || !read_guest(argument_address,&original,4) || !read_guest(0xfd800040u,io,8)
        || io[0]!=io[1] || !driving_scanout152_bounds(physical,original,xbox_ContiguousAllocatedBytes(),
            ctx[1],ctx[2],ctx[0x1b4/4],ctx[0x1b8/4],av_mode,av_format,av_pitch)){
        InterlockedIncrement(&rejected);LeaveCriticalSection(&lock);goto done;
    }
    if(claimed>=12){LeaveCriticalSection(&lock);goto done;}
    unsigned char *snapshot=(unsigned char *)malloc(2560*480);
    if(!snapshot || !read_guest(0x80000000u+physical,snapshot,2560*480)){
        free(snapshot);InterlockedIncrement(&failed);LeaveCriticalSection(&lock);goto done;
    }
    LONG index=InterlockedIncrement(&claimed);LeaveCriticalSection(&lock);
    /* GPU executor is unlocked before all file I/O. The owned bytes are stable. */
    char path[MAX_PATH];snprintf(path,sizeof(path),"%s/scanout152-%02ld-%08X.bmp",directory,index,physical);
    int ok=write_bmp(path,snapshot);free(snapshot);
    if(ok)InterlockedIncrement(&written);else InterlockedIncrement(&failed);
    fprintf(stderr,"[SCANOUT152] snapshot=%ld physical=%08X pitch=2560 size=640x480 put=%08X ok=%d requests=%ld busy=%ld incomplete=%ld rejected=%ld failed=%ld written=%ld diagnostic=incomplete-graphics\n",
        index,physical,io[0],ok,InterlockedCompareExchange(&requests,0,0),InterlockedCompareExchange(&busy,0,0),
        InterlockedCompareExchange(&incomplete,0,0),InterlockedCompareExchange(&rejected,0,0),
        InterlockedCompareExchange(&failed,0,0),InterlockedCompareExchange(&written,0,0));
done:SetLastError(saved);
}

/* Selected AV buffer after a completed nonempty drain. This is not a flip,
 * presentation event or claim that the incomplete executor produced pixels. */
static unsigned av155_eligible,av155_last_version,av155_completed;
static volatile LONG av155_claimed,av155_failed,av155_rejected;
void driving_scanout155_leave(unsigned ready,unsigned submitted)
{
    DWORD saved=GetLastError();
    unsigned char *snapshot=NULL;
    uint32_t physical=0,put=0,version=0;
    unsigned eligible=0,index=0,report=0;
    complete=ready&&!primitive;
    if(depth==1 && complete && submitted && directory[0])report=!(++av155_completed%128);
    /* Called with the existing152 executor lock still held. */
    if(depth==1 && complete && submitted && directory[0] && av155_claimed<6){
        uint32_t io[2];
        if(driving_avbuffer155_bounds(av_physical,xbox_ContiguousAllocatedBytes(),av_mode,av_format,av_pitch)
            && read_guest(0xfd800040u,io,8) && io[0]==io[1]){
            eligible=++av155_eligible;
            /* First completed use of each AV selection, then every64 eligible
             * nonempty drains. No capture on arbitrary empty fence polls. */
            if(av155_last_version!=av_version || !(eligible%64)){
                snapshot=(unsigned char *)malloc(2560*480);
                if(snapshot && read_guest(0x80000000u+av_physical,snapshot,2560*480)){
                    physical=av_physical;put=io[0];version=av_version;
                    av155_last_version=av_version;index=(unsigned)InterlockedIncrement(&av155_claimed);
                }else{
                    free(snapshot);snapshot=NULL;InterlockedIncrement(&av155_failed);
                    /* Bound allocation/read failures too, avoid per-drain retry. */
                    av155_last_version=av_version;InterlockedIncrement(&av155_claimed);
                }
            }
        }else InterlockedIncrement(&av155_rejected);
    }
    depth--;LeaveCriticalSection(&lock);
    if(report)fprintf(stderr,"[AVBUFFER155] aggregate claimed=%ld failed=%ld rejected=%ld observation=AV-selected-buffer-not-present\n",
        InterlockedCompareExchange(&av155_claimed,0,0),InterlockedCompareExchange(&av155_failed,0,0),InterlockedCompareExchange(&av155_rejected,0,0));
    if(snapshot){
        char path[MAX_PATH];snprintf(path,sizeof(path),"%s/av-buffer155-%02u-%08X.bmp",directory,index,physical);
        int ok=write_bmp(path,snapshot);free(snapshot);
        if(!ok)InterlockedIncrement(&av155_failed);
        fprintf(stderr,"[AVBUFFER155] snapshot=%u physical=%08X pitch=2560 size=640x480 selection=%u completed_nonempty=%u put=%08X ok=%d failed=%ld rejected=%ld observation=AV-selected-buffer-not-present incomplete-graphics=1\n",
            index,physical,version,eligible,put,ok,InterlockedCompareExchange(&av155_failed,0,0),InterlockedCompareExchange(&av155_rejected,0,0));
    }
    SetLastError(saved);
}

/* This call occurs on the serialized consumer after a completed GPU resolve.
 * Only the original AV-selected buffer may be sent to the owned-image preview. */
extern void driving_present201(const uint8_t *,uint32_t);
extern int driving_flip204_enabled(void);
extern void driving_flip204_completed(uint32_t,const uint8_t *);
void driving_scanout201_frame(uint32_t physical,const uint8_t *pixels){
 if(driving_flip204_enabled()){
  if(depth==1&&!primitive&&driving_avbuffer155_bounds(physical,xbox_ContiguousAllocatedBytes(),av_mode,av_format,av_pitch))
   driving_flip204_completed(physical,pixels);
  return;
 }
 if(depth==1&&!primitive&&physical==av_physical&&driving_avbuffer155_bounds(physical,xbox_ContiguousAllocatedBytes(),av_mode,av_format,av_pitch))
  driving_present201(pixels,physical);
}
