#include "driving_diag510.h" /*510 private default-OFF diagnostics*/
/* Opt-in software-method/display-queue bridge. The existing timer worker runs
 * the original title handler; the command consumer waits before buffer reuse.
 * This is a bounded synchronous adapter, not a full PGRAPH interrupt model. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static INIT_ONCE once204=INIT_ONCE_STATIC_INIT;
static int enabled204;
static HANDLE delivered204,displayed204,wake204;
static volatile LONG phase204;
static uint32_t payload204,context204,ticket204;
extern int driving_flip204_dispatch(uint32_t,uint32_t *,uint32_t *);
extern int driving_flip204_consumed(uint32_t,uint32_t);
extern void driving_present201(const uint8_t *,uint32_t);
static void stop204(const char *why){fprintf(stderr,"[FLIP204] STOP %s payload=%08X phase=%ld\n",why,payload204,phase204);fflush(stderr);abort();}
static BOOL CALLBACK init204(PINIT_ONCE o,PVOID p,PVOID *c){
 (void)o;(void)p;(void)c;
 const char *v=getenv("DRIVING_FLIP204"),*gpu=getenv("DRIVING_MOVIE_GPU201");
 enabled204=v&&!strcmp(v,"1")&&gpu&&(!strcmp(gpu,"1")||!strcmp(gpu,"2"));
 if(enabled204){delivered204=CreateEventA(NULL,TRUE,FALSE,NULL);displayed204=CreateEventA(NULL,TRUE,FALSE,NULL);
  wake204=CreateEventA(NULL,FALSE,FALSE,NULL);
  if(!delivered204||!displayed204||!wake204)stop204("event creation");}
 return TRUE;
}
int driving_flip204_enabled(void){InitOnceExecuteOnce(&once204,init204,NULL,NULL);return enabled204;}
void driving_flip204_wait(void){
 /* Wake the existing timer worker for a new command; do not add a second
  * caller of the original queue or change its vblank/timer due-time rules. */
 if(driving_flip204_enabled()){
  DWORD result=WaitForSingleObject(wake204,10);
  if(result!=WAIT_OBJECT_0&&result!=WAIT_TIMEOUT)stop204("timer wake wait");
 }else Sleep(10);
}
int driving_flip204_method(unsigned method,uint32_t value){
 if(!driving_flip204_enabled())return 0;
 if(method==0x100){
  if(!value)return 1; /* Original selector zero exits without work. */
  if((value&31)!=1){
   /* This adapter implements display requests only. Preserve the existing
    * executor for other software methods, including device initialization. */
   static unsigned other;
   if(++other<=12)fprintf(stderr,"[FLIP204] baseline software method payload=%08X selector=%u (not implemented by display bridge)\n",value,value&31);
   return 0;
  }
  if(((value>>5)&15)!=1)stop204("unverified display interval/flags");
  if(InterlockedCompareExchange(&phase204,0,0))stop204("display request before previous stall");
  ResetEvent(delivered204);ResetEvent(displayed204);payload204=value;
  InterlockedExchange(&phase204,1);
  SetEvent(wake204);
  if(WaitForSingleObject(delivered204,5000)!=WAIT_OBJECT_0)stop204("original handler timeout");
  return 1;
 }
 if(method==0x130 && InterlockedCompareExchange(&phase204,0,0)){
  if(value)stop204("unverified stall argument");
  if(WaitForSingleObject(displayed204,5000)!=WAIT_OBJECT_0)stop204("original display queue timeout");
  InterlockedExchange(&phase204,0);
 }
 return 0; /* Preserve baseline bookkeeping for FLIP_INCREMENT/STALL. */
}
/* Only the existing kernel timer thread calls this, before/after its vblank ISR.
 * The producer never calls translated code or writes original queue counters. */
void driving_flip204_tick(void){
 if(!driving_flip204_enabled())return;
 LONG phase=InterlockedCompareExchange(&phase204,0,0);
 if(phase==1){
  if(!driving_flip204_dispatch(payload204,&context204,&ticket204))stop204("original handler/context unavailable");
  InterlockedExchange(&phase204,2);SetEvent(delivered204);phase=2;
 }
 if(phase==2 && driving_flip204_consumed(context204,ticket204)){
  static unsigned count;count++;
  if(count<=6||!(count%120))fprintf(stderr,"[FLIP204] consumed=%u physical=%08X ticket=%u ms=%llu original-queue=1\n",count,(payload204>>5)&~15u,ticket204,(unsigned long long)GetTickCount64());
  InterlockedExchange(&phase204,3);SetEvent(displayed204);
 }
}
/* Owned completed image copies bridge the consumer and original PCRTC callback.
 * The timer never waits for the consumer lock or reads actively reused guest RAM. */
static SRWLOCK cache_lock204=SRWLOCK_INIT;
static uint32_t keys204[2],versions204[2],shown204[2],cache204[2][640*480];
static uint64_t epochs510[2];
void driving_flip204_completed(uint32_t physical,const uint8_t *pixels){
 AcquireSRWLockExclusive(&cache_lock204);
 unsigned slot=0;for(;slot<2;slot++)if(keys204[slot]==physical||!keys204[slot])break;
 if(slot==2){ReleaseSRWLockExclusive(&cache_lock204);stop204("more than two movie output buffers");}
 keys204[slot]=physical;memcpy(cache204[slot],pixels,sizeof cache204[slot]);versions204[slot]++;epochs510[slot]=driving_diag510_completed();driving_diag510_cache(physical,versions204[slot],epochs510[slot]);
 ReleaseSRWLockExclusive(&cache_lock204);
}
void driving_flip204_scanout(uint32_t physical){
 if(!driving_flip204_enabled())return;
 static unsigned presented,misses;
 int found=0;
 AcquireSRWLockExclusive(&cache_lock204);
 for(unsigned slot=0;slot<2;slot++)if(keys204[slot]==physical&&versions204[slot]!=shown204[slot]){
  driving_diag510_select(epochs510[slot],versions204[slot]);
  driving_present201((const uint8_t*)cache204[slot],physical);driving_diag510_select(0,0);shown204[slot]=versions204[slot];found=1;
  if(++presented<=6||!(presented%120))fprintf(stderr,"[FLIP204] presented=%u physical=%08X version=%u ms=%llu original-PCRTC-write=1\n",presented,physical,versions204[slot],(unsigned long long)GetTickCount64());
  break;
 }
 ReleaseSRWLockExclusive(&cache_lock204);
 if(!found&&++misses<=6)fprintf(stderr,"[FLIP204] scanout without new owned movie image physical=%08X (loading/unsupported/repeated surface)\n",physical);
}
