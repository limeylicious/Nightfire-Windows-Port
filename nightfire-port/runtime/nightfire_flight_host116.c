/* Optional game-only diagnostic history. Never writes guest input/state. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nightfire_flight_host116.h"
#include "nightfire_flight_recorder116.h"
extern ptrdiff_t g_xbox_mem_offset;
static volatile LONG enabled116,stopped116;
static int read116(uint32_t address,void *out,size_t bytes){
    SIZE_T got=0;
    if(!g_xbox_mem_offset || (uint64_t)address+bytes>0x100000000ull)return 0;
    return ReadProcessMemory(GetCurrentProcess(),(const void*)((uintptr_t)g_xbox_mem_offset+address),out,bytes,&got) && got==bytes;
}
static void exit116(void){
    if(ReadAcquire(&enabled116) && !ReadAcquire(&stopped116)){
        int status=nf_flight116_flush("process-exit");
        fprintf(stderr,"[HISTORY116] process-exit flush=%d\n",status);
    }
}
void nightfire_flight116_start(void){
    const wchar_t *directory=_wgetenv(L"NIGHTFIRE_HISTORY116_DIR");
    if(!directory || !*directory)return;
    int status=nf_flight116_enable(directory);
    if(status==NF_FLIGHT116_OK){
        InterlockedExchange(&enabled116,1);atexit(exit116);
        nf_flight116_record record={0};record.time_ms=GetTickCount64();
        record.event_kind=NF_FLIGHT116_MARKER;strcpy(record.marker,"startup");
        nf_flight116_append(&record);
    }
    fprintf(stderr,"[HISTORY116] enable-status=%d capacity=%u; game input/location history only\n",status,NF_FLIGHT116_CAPACITY);
}
void nightfire_flight116_stop(const char *function,const char *reason){
    if(!ReadAcquire(&enabled116))return;
    /* Do not append or acquire blocking locks from the stop path. The core
     * takes a best-effort snapshot; the original stop always remains in force. */
    InterlockedExchange(&stopped116,1);
    char description[96];snprintf(description,sizeof description,"%.40s: %.50s",function?function:"stop",reason?reason:"");
    int status=nf_flight116_flush(description);
    fprintf(stderr,"[HISTORY116] diagnostic-stop flush=%d\n",status);
}
void nightfire_flight116_pad(const unsigned char state[18],uint32_t frame,uint32_t tick){
    if(!ReadAcquire(&enabled116))return;
    nf_flight116_record record={0};record.time_ms=GetTickCount64();
    record.frame=frame;record.game_tick=tick;record.metadata=NF_FLIGHT116_GAME_TICK;
    record.event_kind=NF_FLIGHT116_INPUT;record.buttons=(unsigned)state[0]|((unsigned)state[1]<<8);
    memcpy(record.analog_buttons,state+2,6);memcpy(record.triggers,state+8,2);memcpy(record.axes,state+10,8);
    /* Sample position at most10Hz. Other records intentionally lack position
     * validity bits rather than presenting cached coordinates as fresh. */
    static ULONGLONG next_position;
    if(record.time_ms>=next_position){
        next_position=record.time_ms+100;
        uint32_t player=0;unsigned char data[0x20];
        if(read116(0x1f6654,&player,sizeof player) && player>=0x80000000u && player<0x83ffff00u &&
           read116(player+0x24,data,sizeof data)){
            float xyz[3],yaw;memcpy(xyz,data,sizeof xyz);memcpy(&yaw,data+0x1c,sizeof yaw);
            if(isfinite(xyz[0])&&isfinite(xyz[1])&&isfinite(xyz[2])){
                memcpy(record.position,xyz,sizeof xyz);record.metadata|=NF_FLIGHT116_POSITION;
            }
            if(isfinite(yaw)){record.yaw=yaw;record.metadata|=NF_FLIGHT116_YAW;}
        }
    }
    nf_flight116_append(&record);
}
void nightfire_flight116_load(uint32_t guest_sp){
    if(!ReadAcquire(&enabled116))return;
    uint32_t args[4];
    if(guest_sp<0x1000 || guest_sp>0x03fffff0 || !read116(guest_sp,args,sizeof args))return;
    nf_flight116_record record={0};record.time_ms=GetTickCount64();
    record.event_kind=NF_FLIGHT116_MARKER;record.level=args[1];record.metadata=NF_FLIGHT116_LEVEL;
    /* This is the REQUESTED level ID, not proof that loading completed. */
    snprintf(record.marker,sizeof record.marker,"load-request flags=%u skip=%u",args[2],args[3]);
    nf_flight116_append(&record);
}
