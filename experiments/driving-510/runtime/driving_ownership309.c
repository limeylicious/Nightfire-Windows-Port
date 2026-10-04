/* Default-off source-entry observation. Called by generated chunk17 hooks.
 * No guest writes, completion changes, persistent guest pointers, or GPU calls.
 * Device serial is diagnostic only: NOT a host allocation generation. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fenv.h>
#include <xmmintrin.h>
#include "../src/recomp/gen/recomp_types.h"
#include "driving_ownership309.h"

typedef struct { DWORD error; int crt_error; unsigned mxcsr; fenv_t env; } OwnHost309;
typedef struct {
 uint64_t event,call;
 uint32_t function,kind,after,tid,caller,serial,valid,qualified;
 uint32_t device,args[6],result,post_stack,current_sequence,completed_sequence;
 uint32_t active[2],embedded[4],allocations[3],resource[2],words[2][6],locked[2];
} OwnEvent309;
static SRWLOCK own_lock309=SRWLOCK_INIT;
static volatile LONG own_enabled309=-1,own_dumped309;
static __declspec(thread) unsigned own_nested309;
static uint64_t own_calls309[OWN309_KINDS],own_targets309[OWN309_KINDS],own_returns309[OWN309_KINDS];
static uint64_t own_events309,own_nextcall309;
static uint32_t own_serial309;
static uint64_t own_invalid_stack309,own_invalid_device309,own_invalid_resource309,own_unwind309;
static __declspec(thread) uint32_t own_root_stack309;
/* Retain startup and recent observations, never unbounded per-event disk I/O. */
static OwnEvent309 own_first309[512],own_tail309[512];
static void own_save309(OwnHost309 *h){
 h->error=GetLastError();h->crt_error=errno;h->mxcsr=_mm_getcsr();fegetenv(&h->env);
}
static void own_restore309(const OwnHost309 *h){
 fesetenv(&h->env);_mm_setcsr(h->mxcsr);errno=h->crt_error;SetLastError(h->error);
}
void driving_ownership309_report(void);
static int own_enabled_read309(void){
 LONG v=InterlockedCompareExchange(&own_enabled309,-1,-1);
 if(v<0){char b[8];DWORD n=GetEnvironmentVariableA("DRIVING_OWNERSHIP309",b,sizeof b);
  v=n==1&&b[0]=='1';if(InterlockedCompareExchange(&own_enabled309,v,-1)==-1&&v)atexit(driving_ownership309_report);}
 return v!=0;
}
static int own_read309(uint32_t va,void *out,size_t bytes){
 uintptr_t base=(uintptr_t)g_xbox_mem_offset,a=base+va;SIZE_T got=0;
 if(!va||bytes>(uint64_t)UINT32_MAX+1-va||a<base||bytes>UINTPTR_MAX-a)return 0;
 return ReadProcessMemory(GetCurrentProcess(),(const void*)a,out,bytes,&got)&&got==bytes;
}
/* Values are diagnostic matches, never an allocation/ownership certificate.
 * 1 exact embedded pointer, 2 current linear interval, 3 child of embedded. */
static int own_target309(uint32_t resource,uint32_t device,unsigned *invalid){
 if(!resource||!device||device>UINT32_MAX-0x2244)return 0;
 if(resource>=device+0x21d0&&resource<device+0x2230&&((resource-device-0x21d0)%24)==0)return 1;
 uint32_t w[6],current[4][6];
 if(!own_read309(resource,w,sizeof w)||!own_read309(device+0x21d0,current,sizeof current)){(*invalid)++;return 0;}
 uint32_t type=w[0]&0x70000u;if(type!=0x40000&&type!=0x50000)return 0;
 if(type==0x50000&&w[5]>=device+0x21d0&&w[5]<device+0x2230&&((w[5]-device-0x21d0)%24)==0)return 3;
 uint32_t start=w[1]&0x0fffffffu;unsigned format=(w[3]>>8)&255;
 uint64_t end=(uint64_t)start+1;
 if(format==0x12||format==0x2e)end=(uint64_t)start+((uint64_t)(w[4]>>24)+1)*64*(((w[4]>>12)&4095)+1);
 for(unsigned i=0;i<4;i++){
  unsigned f=(current[i][3]>>8)&255;
  if((current[i][0]&0x70000)!=0x50000||(f!=0x12&&f!=0x2e))continue;
  uint32_t lo=current[i][1]&0x0fffffffu;
  uint64_t hi=(uint64_t)lo+((uint64_t)(current[i][4]>>24)+1)*64*(((current[i][4]>>12)&4095)+1);
  if(hi<=0x10000000ull&&end<=0x10000000ull&&(uint64_t)start<hi&&(uint64_t)lo<end)return 2;
 }
 return 0;
}
static void own_snapshot309(OwnEvent309 *e,const OwnScope309 *s,unsigned after){
 memset(e,0,sizeof *e);e->function=s->function;e->kind=s->kind;e->after=after;
 e->tid=s->tid;e->caller=s->caller;e->device=s->device;e->qualified=s->qualified;
 memcpy(e->args,s->args,sizeof e->args);e->call=s->call;e->result=g_eax;e->post_stack=g_esp;
 uint32_t d=s->device;
 if(d&&d<=UINT32_MAX-0x2244){
  uint32_t seq[2];if(own_read309(d+0x30,seq,sizeof seq)){e->current_sequence=seq[0];e->valid|=1;
   if(own_read309(seq[1],&e->completed_sequence,4))e->valid|=2;}
  if(own_read309(d+0x21b4,e->active,sizeof e->active))e->valid|=4;
  if(own_read309(d+0x21c0,e->embedded,sizeof e->embedded))e->valid|=8;
  if(own_read309(d+0x2230,e->allocations,sizeof e->allocations))e->valid|=16;
 }
 if(s->kind==OWN309_BIND){e->resource[0]=s->args[0];e->resource[1]=s->args[1];}
 else if(s->kind==OWN309_LOCK||s->kind==OWN309_RESOURCE_WAIT||s->kind==OWN309_VERTEX_WAIT)e->resource[0]=s->args[0];
 else {e->resource[0]=d<=UINT32_MAX-0x2218?d+0x21d0:0;e->resource[1]=d<=UINT32_MAX-0x2218?d+0x2218:0;}
 for(unsigned i=0;i<2;i++)if(own_read309(e->resource[i],e->words[i],sizeof e->words[i]))e->valid|=32u<<i;
 /* Common lock ABI: resource, face, level, output, rect, flags. */
 if(s->kind==OWN309_LOCK&&after&&own_read309(s->args[3],e->locked,sizeof e->locked))e->valid|=128;
}
OwnScope309 own_enter309(uint32_t function,unsigned kind){
 OwnScope309 s;memset(&s,0,sizeof s);
 /* Warm disabled path does not call CRT, read guest memory, or alter FP/LastError. */
 if(InterlockedCompareExchange(&own_enabled309,-1,-1)==0)return s;
 OwnHost309 h;own_save309(&h);
 if(!own_enabled_read309()){own_restore309(&h);return s;}
 s.entered=1;s.function=function;s.kind=kind;s.tid=GetCurrentThreadId();s.stack=g_esp;
 uint32_t stack[7]={0};if(own_read309(g_esp,stack,sizeof stack)){s.stack_valid=1;s.caller=stack[0];memcpy(s.args,stack+1,sizeof s.args);}
 s.device_valid=own_read309(0x175418,&s.device,4)&&s.device&&s.device<=UINT32_MAX-0x2244;
 if(kind==OWN309_CREATE||kind==OWN309_DESTROY){s.device=g_ecx;s.device_valid=s.device&&s.device<=UINT32_MAX-0x2244;}
 unsigned invalid_resource=0,unwound=0;
 if(own_nested309&&g_esp>=own_root_stack309){own_nested309=0;unwound=1;}
 s.nested_before=own_nested309;
 if((kind==OWN309_CREATE||kind==OWN309_DESTROY||kind==OWN309_IDLE)&&s.device_valid)s.qualified=1;
 else if(kind==OWN309_BIND){uint32_t old[2]={0};
  if(s.device&&s.device<=UINT32_MAX-0x21bc)own_read309(s.device+0x21b4,old,sizeof old);
  s.qualified=own_target309(s.args[0],s.device,&invalid_resource)||own_target309(s.args[1],s.device,&invalid_resource)||own_target309(old[0],s.device,&invalid_resource)||own_target309(old[1],s.device,&invalid_resource);}
 else if(kind==OWN309_FENCE)s.qualified=own_nested309?4:0;
 else s.qualified=own_target309(s.args[0],s.device,&invalid_resource);
 if(!s.stack_valid||!s.device_valid)s.qualified=0;
 if(s.qualified){if(!own_nested309)own_root_stack309=g_esp;own_nested309++;}
 OwnEvent309 e;if(s.qualified)own_snapshot309(&e,&s,0);
 AcquireSRWLockExclusive(&own_lock309);own_calls309[kind]++;
 own_invalid_stack309+=!s.stack_valid;own_invalid_device309+=!s.device_valid;own_invalid_resource309+=invalid_resource;own_unwind309+=unwound;
 if(s.qualified){own_targets309[kind]++;if(kind==OWN309_CREATE)own_serial309++;
  s.call=e.call=++own_nextcall309;e.event=++own_events309;e.serial=own_serial309;
  if(e.event<=512)own_first309[e.event-1]=e;own_tail309[(e.event-1)&511]=e;}
 ReleaseSRWLockExclusive(&own_lock309);own_restore309(&h);return s;
}
void own_leave309(OwnScope309 *s){
 if(!s->entered)return;
 OwnHost309 h;own_save309(&h);OwnEvent309 e;
 if(s->qualified)own_snapshot309(&e,s,1);
 own_nested309=s->nested_before;
 AcquireSRWLockExclusive(&own_lock309);own_returns309[s->kind]++;
 if(s->qualified){e.event=++own_events309;e.serial=own_serial309;
  if(e.event<=512)own_first309[e.event-1]=e;own_tail309[(e.event-1)&511]=e;}
 ReleaseSRWLockExclusive(&own_lock309);own_restore309(&h);
}
void driving_ownership309_report(void){
 if(InterlockedCompareExchange(&own_enabled309,-1,-1)!=1)return;
 OwnHost309 h;own_save309(&h);
 if(InterlockedCompareExchange(&own_dumped309,1,0)){own_restore309(&h);return;}
 /* Watchdog must not deadlock behind a stopped hook. No I/O while lock held. */
 static OwnEvent309 first[512],tail[512];uint64_t calls[OWN309_KINDS],targets[OWN309_KINDS],returns[OWN309_KINDS],events,invalid[4];uint32_t serial;
 if(!TryAcquireSRWLockExclusive(&own_lock309)){fprintf(stderr,"[OWNERSHIP309] snapshot_busy=1; no ownership inference\n");own_restore309(&h);return;}
 memcpy(first,own_first309,sizeof first);memcpy(tail,own_tail309,sizeof tail);
 memcpy(calls,own_calls309,sizeof calls);memcpy(targets,own_targets309,sizeof targets);memcpy(returns,own_returns309,sizeof returns);
 invalid[0]=own_invalid_stack309;invalid[1]=own_invalid_device309;invalid[2]=own_invalid_resource309;invalid[3]=own_unwind309;
 events=own_events309;serial=own_serial309;ReleaseSRWLockExclusive(&own_lock309);
 char dir[1536],path[1800];DWORD n=GetEnvironmentVariableA("DRIVING_CAPTURE_DIR",dir,sizeof dir);int saved=0;
 if(n&&n<sizeof dir&&snprintf(path,sizeof path,"%s/ownership309.bin",dir)>0){FILE*f=fopen(path,"wb");if(f){
  uint64_t head[8]={0x3930334e574full,sizeof(OwnEvent309),events,serial,events<512?events:512,events<512?events:512,OWN309_KINDS,2};
  saved=fwrite(head,sizeof head,1,f)==1&&fwrite(calls,sizeof calls,1,f)==1&&fwrite(targets,sizeof targets,1,f)==1&&fwrite(returns,sizeof returns,1,f)==1&&fwrite(invalid,sizeof invalid,1,f)==1&&fwrite(first,sizeof first,1,f)==1&&fwrite(tail,sizeof tail,1,f)==1;
  if(fclose(f))saved=0;}}
 fprintf(stderr,"[OWNERSHIP309] events=%llu retained_first=512 retained_tail=512 device_creation_attempt_serial=%u allocation_generation=UNTRACKED saved=%d\n",(unsigned long long)events,serial,saved);
 fprintf(stderr,"[OWNERSHIP309] evicted_middle=%llu invalid_stack=%llu invalid_device=%llu invalid_resource_reads=%llu observed_stack_unwinds=%llu\n",(unsigned long long)(events>1024?events-1024:0),(unsigned long long)invalid[0],(unsigned long long)invalid[1],(unsigned long long)invalid[2],(unsigned long long)invalid[3]);
 for(unsigned i=0;i<OWN309_KINDS;i++)fprintf(stderr,"[OWNERSHIP309] kind=%u calls=%llu target_or_context=%llu non_target_or_unqualified=%llu returns=%llu unpaired=%llu\n",i,(unsigned long long)calls[i],(unsigned long long)targets[i],(unsigned long long)(calls[i]-targets[i]),(unsigned long long)returns[i],(unsigned long long)(calls[i]>=returns[i]?calls[i]-returns[i]:0));
 fflush(stderr);own_restore309(&h);
}
