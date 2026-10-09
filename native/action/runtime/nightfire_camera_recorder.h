/* Read-only, explicitly compiled diagnostic. Keeps bounded history in host RAM.
 * PAL 25FA0 receives a camera index; index zero is recorded before/after its
 * updater and matrix assembly. Fields remain raw until verified against PAL.
 * F8 or a request file saves history and the latest published game picture.
 * Snapshot I/O and memory reads perturb timing: never benchmark this build. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NF_CAMERA_RECORDS 4096u
typedef struct {
    unsigned char input[0xa8], mapped[0x158], camera[0x220];
    unsigned char player[0x100], state[0x900], weapon[0x100];
} nf_camera_bytes;
typedef struct {
    uint64_t before_ms, after_ms;
    uint32_t sequence, update, camera, index;
    uint32_t player_before, state_before, weapon_before;
    uint32_t player_after, state_after, weapon_after;
    uint32_t valid_before, valid_after;
    nf_camera_bytes before, after;
} nf_camera_record;
static nf_camera_record *nf_camera_ring;
static unsigned nf_camera_count, nf_camera_cursor, nf_camera_sequence;
static unsigned nf_camera_update, nf_camera_pending, nf_camera_depth;
static DWORD nf_camera_thread;
static int nf_camera_enabled=-1;
static char nf_camera_directory[MAX_PATH];

static int nf_camera_read(uint32_t address, void *out, size_t size)
{
    SIZE_T got=0;
    memset(out,0,size);
    if (!((address>=0x1000 && (uint64_t)address+size<=0x04000000u) ||
          (address>=0x80000000u && (uint64_t)address+size<=0x84000000u))) return 0;
    return ReadProcessMemory(GetCurrentProcess(),
        (void *)((uintptr_t)g_xbox_mem_offset+address),out,size,&got) && got==size;
}
static uint32_t nf_camera_word(uint32_t address)
{
    uint32_t result=0; nf_camera_read(address,&result,sizeof result); return result;
}
static void nf_camera_sample(nf_camera_record *r, unsigned after)
{
    nf_camera_bytes *b=after?&r->after:&r->before;
    uint32_t player=nf_camera_word(0x1f6654), state=player?nf_camera_word(player+0xbc):0;
    uint32_t weapon=state?nf_camera_word(state+0x778):0, valid=0;
    if(nf_camera_read(0x2ff49c,b->input,sizeof b->input))valid|=1;
    if(nf_camera_read(0x1fe6d0,b->mapped,sizeof b->mapped))valid|=2;
    if(nf_camera_read(r->camera,b->camera,sizeof b->camera))valid|=4;
    if(nf_camera_read(player,b->player,sizeof b->player))valid|=8;
    if(nf_camera_read(state,b->state,sizeof b->state))valid|=16;
    if(nf_camera_read(weapon,b->weapon,sizeof b->weapon))valid|=32;
    if(after){r->player_after=player;r->state_after=state;r->weapon_after=weapon;r->valid_after=valid;r->after_ms=GetTickCount64();}
    else{r->player_before=player;r->state_before=state;r->weapon_before=weapon;r->valid_before=valid;r->before_ms=GetTickCount64();}
}
static int nf_camera_save(const char *reason)
{
    static unsigned file_index;
    char path[MAX_PATH], picture[MAX_PATH]; HANDLE handle=INVALID_HANDLE_VALUE;
    for(unsigned attempt=0;attempt<1000;attempt++,file_index++) {
        snprintf(path,sizeof path,"%s/camera-%u.bin",nf_camera_directory,file_index);
        handle=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(handle!=INVALID_HANDLE_VALUE)break;
        if(GetLastError()!=ERROR_FILE_EXISTS && GetLastError()!=ERROR_ALREADY_EXISTS)break;
    }
    if(handle==INVALID_HANDLE_VALUE){fprintf(stderr,"[CAMERA-RECORDER] save failed error=%lu\n",GetLastError());return 0;}
    uint32_t header[]={0x4e464352,1,sizeof(nf_camera_record),nf_camera_count,nf_camera_sequence,nf_camera_update};
    DWORD written=0;int ok=WriteFile(handle,header,sizeof header,&written,NULL) && written==sizeof header;
    unsigned first=(nf_camera_cursor+NF_CAMERA_RECORDS-nf_camera_count)%NF_CAMERA_RECORDS;
    for(unsigned i=0;ok && i<nf_camera_count;i++) {
        ok=WriteFile(handle,&nf_camera_ring[(first+i)%NF_CAMERA_RECORDS],sizeof(nf_camera_record),&written,NULL) && written==sizeof(nf_camera_record);
    }
    CloseHandle(handle);
    snprintf(picture,sizeof picture,"%s/camera-%u.bmp",nf_camera_directory,file_index++);
    extern int xbox_FramebufferDumpBmp(const char *);
    int image_result=xbox_FramebufferDumpBmp(picture);
    fprintf(stderr,"[CAMERA-RECORDER] saved=%d reason=%s records=%u latest=%u at_ms=%llu file=%s picture_result=%d\n",
        ok,reason,nf_camera_count,nf_camera_sequence,(unsigned long long)GetTickCount64(),path,image_result);
    fflush(stderr);return ok;
}
static void nf_camera_exit_save(void)
{
    /* A watchdog can call exit on another thread; do not race the live writer. */
    if(GetCurrentThreadId()==nf_camera_thread && !nf_camera_pending && nf_camera_ring && nf_camera_count)nf_camera_save("exit");
}
static void nightfire_camera_recorder(uint32_t va,unsigned after)
{
    if(va!=0x25fa0 && va!=0x6cf50)return;
    if(nf_camera_enabled<0){
        const char *directory=getenv("NIGHTFIRE_CAMERA_TRACE_DIR");
        nf_camera_enabled=directory && *directory && strlen(directory)<MAX_PATH-40;
        if(nf_camera_enabled){
            strcpy(nf_camera_directory,directory);
            nf_camera_ring=(nf_camera_record *)calloc(NF_CAMERA_RECORDS,sizeof(nf_camera_record));
            nf_camera_enabled=nf_camera_ring!=NULL;nf_camera_thread=GetCurrentThreadId();
            if(nf_camera_enabled){atexit(nf_camera_exit_save);fprintf(stderr,"[CAMERA-RECORDER] enabled records=%u bytes=%zu F8 marks event; timing diagnostic only\n",NF_CAMERA_RECORDS,NF_CAMERA_RECORDS*sizeof(nf_camera_record));}
        }
    }
    if(!nf_camera_enabled || GetCurrentThreadId()!=nf_camera_thread)return;
    if(va==0x6cf50){if(!after)nf_camera_update++;return;}
    if(after>1)return;
    if(!after){
        if(nf_camera_depth++)return;
        uint32_t index=nf_camera_word(g_esp+4)&0xffff;
        if(index!=0)return;
        uint32_t camera=nf_camera_word(0x1f661c);
        if(!camera)return;
        nf_camera_record *r=&nf_camera_ring[nf_camera_cursor];memset(r,0,sizeof *r);
        r->sequence=++nf_camera_sequence;r->update=nf_camera_update;r->camera=camera;r->index=index;
        nf_camera_sample(r,0);nf_camera_pending=1;
    }else{
        if(!nf_camera_depth || --nf_camera_depth || !nf_camera_pending)return;
        nf_camera_pending=0;nf_camera_sample(&nf_camera_ring[nf_camera_cursor],1);
        nf_camera_cursor=(nf_camera_cursor+1)%NF_CAMERA_RECORDS;
        if(nf_camera_count<NF_CAMERA_RECORDS)nf_camera_count++;
        static int held;DWORD foreground=0;
        {extern HWND nightfire_window_hwnd(void);HWND own=nightfire_window_hwnd();   /* one-window play: the host is in front */
         HWND front=GetForegroundWindow();
         if(own && (GetWindowLongPtrA(own,GWL_STYLE)&WS_CHILD) && front==GetAncestor(own,GA_ROOT))foreground=GetCurrentProcessId();
         else GetWindowThreadProcessId(front,&foreground);}
        int down=foreground==GetCurrentProcessId() && (GetAsyncKeyState(VK_F8)&0x8000);
        if(down && !held)nf_camera_save("F8");held=down;
        static ULONGLONG next_poll;ULONGLONG now=GetTickCount64();
        if(now>=next_poll){
            next_poll=now+250;char request[MAX_PATH];snprintf(request,sizeof request,"%s/capture.request",nf_camera_directory);
            if(GetFileAttributesA(request)!=INVALID_FILE_ATTRIBUTES && nf_camera_save("request"))DeleteFileA(request);
        }
    }
}
