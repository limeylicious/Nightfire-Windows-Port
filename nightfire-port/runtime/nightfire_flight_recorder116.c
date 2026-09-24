#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <float.h>
#include "nightfire_flight_recorder116.h"

typedef char nf_record116_is_128_bytes[(sizeof(nf_flight116_record)==128)?1:-1];
#define NF_PATH116 512u
static SRWLOCK ring_lock116=SRWLOCK_INIT, flush_lock116=SRWLOCK_INIT;
static volatile LONG enabled116;
static nf_flight116_record ring116[NF_FLIGHT116_CAPACITY];
/* Separate fixed buffer means no allocation or ring lock during file writing.
 * Combined record storage is exactly1MiB; metadata/path/scratch add under8KiB. */
static nf_flight116_record dump116[NF_FLIGHT116_CAPACITY];
static wchar_t directory116[NF_PATH116];
static uint64_t total116, overwritten116, flush_serial116;
static uint32_t head116, count116;
static _locale_t locale116; /* One small C-locale object allocated only at enable. */

static int active116(void) { return InterlockedCompareExchange(&enabled116,0,0)!=0; }
static void info116(nf_flight116_info *info) {
    info->total_records=total116; info->overwritten=overwritten116;
    info->count=count116; info->copied=0; info->capacity=NF_FLIGHT116_CAPACITY;
    info->enabled=(uint32_t)active116();
}
static void copy116(nf_flight116_record *out,size_t capacity,nf_flight116_info *info) {
    uint32_t amount=(uint32_t)(capacity<count116?capacity:count116);
    uint32_t first=(head116+NF_FLIGHT116_CAPACITY-amount)%NF_FLIGHT116_CAPACITY;
    uint32_t chunk=amount<NF_FLIGHT116_CAPACITY-first?amount:NF_FLIGHT116_CAPACITY-first;
    info116(info); info->copied=amount;
    if(chunk) memcpy(out,ring116+first,(size_t)chunk*sizeof *out);
    if(amount>chunk) memcpy(out+chunk,ring116,(size_t)(amount-chunk)*sizeof *out);
}

int nf_flight116_enable(const wchar_t *output_directory) {
    wchar_t resolved[NF_PATH116]; DWORD length,attributes;
    if(!output_directory || !((output_directory[0]>=L'A'&&output_directory[0]<=L'Z') ||
        (output_directory[0]>=L'a'&&output_directory[0]<=L'z')) || output_directory[1]!=L':' ||
        (output_directory[2]!=L'\\'&&output_directory[2]!=L'/')) return NF_FLIGHT116_INVALID;
    length=GetFullPathNameW(output_directory,NF_PATH116,resolved,NULL);
    if(!length || length>=NF_PATH116-96) return NF_FLIGHT116_INVALID;
    attributes=GetFileAttributesW(resolved);
    if(attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY)) return NF_FLIGHT116_INVALID;
    AcquireSRWLockExclusive(&flush_lock116);
    if(!locale116) locale116=_create_locale(LC_NUMERIC,"C");
    if(!locale116) { ReleaseSRWLockExclusive(&flush_lock116); return NF_FLIGHT116_INVALID; }
    AcquireSRWLockExclusive(&ring_lock116);
    InterlockedExchange(&enabled116,0);
    memcpy(directory116,resolved,((size_t)length+1)*sizeof(wchar_t));
    head116=count116=0; total116=overwritten116=0;
    InterlockedExchange(&enabled116,1);
    ReleaseSRWLockExclusive(&ring_lock116);
    ReleaseSRWLockExclusive(&flush_lock116);
    return NF_FLIGHT116_OK;
}
void nf_flight116_disable(void) {
    AcquireSRWLockExclusive(&ring_lock116);
    InterlockedExchange(&enabled116,0);
    ReleaseSRWLockExclusive(&ring_lock116);
}
int nf_flight116_append(const nf_flight116_record *record) {
    if(!active116()) return NF_FLIGHT116_DISABLED;
    if(!record) return NF_FLIGHT116_INVALID;
    AcquireSRWLockExclusive(&ring_lock116);
    if(!active116()) { ReleaseSRWLockExclusive(&ring_lock116); return NF_FLIGHT116_DISABLED; }
    ring116[head116]=*record;
    ring116[head116].sequence=++total116;
    ring116[head116].marker[NF_FLIGHT116_MARKER_BYTES-1]=0;
    head116=(head116+1)%NF_FLIGHT116_CAPACITY;
    if(count116<NF_FLIGHT116_CAPACITY) ++count116; else ++overwritten116;
    ReleaseSRWLockExclusive(&ring_lock116);
    return NF_FLIGHT116_OK;
}
int nf_flight116_snapshot(nf_flight116_record *out,size_t capacity,nf_flight116_info *info) {
    if(!info || (!out&&capacity)) return NF_FLIGHT116_INVALID;
    memset(info,0,sizeof *info); info->capacity=NF_FLIGHT116_CAPACITY;
    if(!active116()) return NF_FLIGHT116_DISABLED;
    if(!TryAcquireSRWLockShared(&ring_lock116)) return NF_FLIGHT116_BUSY;
    if(!active116()) { ReleaseSRWLockShared(&ring_lock116); return NF_FLIGHT116_DISABLED; }
    copy116(out,capacity,info);
    ReleaseSRWLockShared(&ring_lock116);
    return NF_FLIGHT116_OK;
}

/* Escape arbitrary bounded bytes, including non-ASCII, as valid JSON. This
 * preserves input bytes as U+00xx; marker/reason callers should prefer ASCII. */
static void escape116(char *out,const char *text,size_t bound) {
    static const char hex[]="0123456789abcdef"; size_t i=0,at=0;
    if(text) for(i=0;i<bound && text[i];++i) {
        unsigned char c=(unsigned char)text[i];
        if(c>=32 && c<127 && c!='"' && c!='\\') out[at++]=(char)c;
        else {out[at++]='\\';out[at++]='u';out[at++]='0';out[at++]='0';out[at++]=hex[c>>4];out[at++]=hex[c&15];}
    }
    out[at]=0;
}
static void number116(char out[48],float value,int valid) {
    if(!valid || !_finite((double)value)) {memcpy(out,"null",5);return;}
    if(_snprintf_l(out,48,"%.9g",locale116,(double)value)<0) memcpy(out,"null",5);
}
static int write116(HANDLE file,const char *bytes,size_t count) {
    while(count) {
        DWORD written=0;
        if(!WriteFile(file,bytes,(DWORD)count,&written,NULL) || !written) return 0;
        bytes+=written;count-=written;
    }
    return 1;
}
int nf_flight116_flush(const char *reason) {
    nf_flight116_info info; wchar_t path[NF_PATH116];
    char line[1536],escaped_reason[6*96+1],marker[6*NF_FLIGHT116_MARKER_BYTES+1];
    HANDLE file=INVALID_HANDLE_VALUE; int result=NF_FLIGHT116_IO_ERROR,n; uint32_t i;
    if(!active116()) return NF_FLIGHT116_DISABLED;
    if(!TryAcquireSRWLockExclusive(&flush_lock116)) return NF_FLIGHT116_BUSY;
    if(!TryAcquireSRWLockShared(&ring_lock116)) {ReleaseSRWLockExclusive(&flush_lock116);return NF_FLIGHT116_BUSY;}
    if(!active116()) {ReleaseSRWLockShared(&ring_lock116);ReleaseSRWLockExclusive(&flush_lock116);return NF_FLIGHT116_DISABLED;}
    copy116(dump116,NF_FLIGHT116_CAPACITY,&info);
    n=_snwprintf(path,NF_PATH116,L"%ls\\nightfire-history116-%lu-%llu.jsonl",directory116,
        GetCurrentProcessId(),(unsigned long long)++flush_serial116);
    ReleaseSRWLockShared(&ring_lock116);
    if(n<0 || n>=(int)NF_PATH116) goto finished;
    file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) goto finished;
    escape116(escaped_reason,reason,96);
    n=_snprintf_l(line,sizeof line,
        "{\"type\":\"header\",\"format\":\"nightfire-flight116\",\"version\":1,\"pid\":%lu,\"capacity\":%u,\"count\":%u,\"total_records\":%llu,\"overwritten\":%llu,\"reason\":\"%s\"}\n",
        locale116,GetCurrentProcessId(),info.capacity,info.count,(unsigned long long)info.total_records,
        (unsigned long long)info.overwritten,escaped_reason);
    if(n<0 || n>=(int)sizeof line || !write116(file,line,(size_t)n)) goto finished;
    for(i=0;i<info.copied;++i) {
        const nf_flight116_record *r=dump116+i;
        char x[48],y[48],z[48],yaw[48],health[48],level[24],tick[32];
        number116(x,r->position[0],r->metadata&NF_FLIGHT116_POSITION);
        number116(y,r->position[1],r->metadata&NF_FLIGHT116_POSITION);
        number116(z,r->position[2],r->metadata&NF_FLIGHT116_POSITION);
        number116(yaw,r->yaw,r->metadata&NF_FLIGHT116_YAW);
        number116(health,r->health,r->metadata&NF_FLIGHT116_HEALTH);
        if(r->metadata&NF_FLIGHT116_LEVEL) _snprintf(level,sizeof level,"%u",r->level); else memcpy(level,"null",5);
        if(r->metadata&NF_FLIGHT116_GAME_TICK) _snprintf(tick,sizeof tick,"%llu",(unsigned long long)r->game_tick); else memcpy(tick,"null",5);
        escape116(marker,r->marker,NF_FLIGHT116_MARKER_BYTES);
        n=_snprintf_l(line,sizeof line,
            "{\"type\":\"record\",\"sequence\":%llu,\"frame\":%llu,\"time_ms\":%llu,\"game_tick\":%s,\"event_kind\":%u,\"buttons\":%u,\"axes\":[%d,%d,%d,%d],\"triggers\":[%u,%u],\"analog_buttons\":[%u,%u,%u,%u,%u,%u],\"metadata\":%u,\"level\":%s,\"position\":[%s,%s,%s],\"yaw\":%s,\"health\":%s,\"marker\":\"%s\"}\n",
            locale116,(unsigned long long)r->sequence,(unsigned long long)r->frame,(unsigned long long)r->time_ms,tick,
            r->event_kind,r->buttons,r->axes[0],r->axes[1],r->axes[2],r->axes[3],r->triggers[0],r->triggers[1],
            r->analog_buttons[0],r->analog_buttons[1],r->analog_buttons[2],r->analog_buttons[3],r->analog_buttons[4],r->analog_buttons[5],
            r->metadata,level,x,y,z,yaw,health,marker);
        if(n<0 || n>=(int)sizeof line || !write116(file,line,(size_t)n)) goto finished;
    }
    /* End row distinguishes a complete snapshot from an interrupted file. No
     * FlushFileBuffers/durability claim; CloseHandle is checked below. */
    n=_snprintf(line,sizeof line,"{\"type\":\"end\",\"count\":%u,\"last_sequence\":%llu}\n",info.copied,
        (unsigned long long)(info.copied?dump116[info.copied-1].sequence:0));
    if(n>=0 && n<(int)sizeof line && write116(file,line,(size_t)n)) result=NF_FLIGHT116_OK;
finished:
    if(file!=INVALID_HANDLE_VALUE && !CloseHandle(file)) result=NF_FLIGHT116_IO_ERROR;
    ReleaseSRWLockExclusive(&flush_lock116);
    return result;
}

#ifdef NIGHTFIRE_FLIGHT116_TEST
void nf_flight116_test_hold_ring(void) {AcquireSRWLockExclusive(&ring_lock116);}
void nf_flight116_test_release_ring(void) {ReleaseSRWLockExclusive(&ring_lock116);}
void nf_flight116_test_hold_flush(void) {AcquireSRWLockExclusive(&flush_lock116);}
void nf_flight116_test_release_flush(void) {ReleaseSRWLockExclusive(&flush_lock116);}
#endif
