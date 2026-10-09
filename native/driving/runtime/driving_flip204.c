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
#ifdef DRIVING_LEAN_RENDERER
static HANDLE lean_wake_event(void){
 static HANDLE ev;
 if(!ev){HANDLE h=CreateEventA(NULL,FALSE,FALSE,NULL);
  if(InterlockedCompareExchangePointer((PVOID *)&ev,h,NULL))CloseHandle(h);}
 return ev;
}
/* Called by the lean APU copy when its interrupt line rises. */
void lean_kernel_wake_timer(void){SetEvent(lean_wake_event());}
/* LEAN_TIMER_PRECISE (kernel_bridge.c): the same wake event for its own wait. */
void driving_flip204_wake_handle(HANDLE *out){*out=lean_wake_event();}
#endif
void driving_flip204_wait(void){
 /* Wake the existing timer worker for a new command; do not add a second
  * caller of the original queue or change its vblank/timer due-time rules. */
#ifdef DRIVING_LEAN_RENDERER
 /* Lean: the APU interrupt also wakes this worker (lean_kernel_wake_timer). */
 /* LEAN_VBLANK_WAIT=1 (experimental, default off): also wake exactly when the next 50 Hz vblank
  * is due, using a high-resolution timer. The plain 10 ms wait rounds up to
  * the 15.6 ms system tick, so vblanks (and the movie frames they pace)
  * arrived 0-15 ms late and uneven. 0 restores the plain wait. */
 static int vw=-1;static HANDLE vt;static LARGE_INTEGER vhz;
 if(vw<0){const char *v=getenv("LEAN_VBLANK_WAIT");vw=v&&v[0]=='1';QueryPerformanceFrequency(&vhz);
  if(vw){vt=CreateWaitableTimerExW(NULL,NULL,0x00000002/*CREATE_WAITABLE_TIMER_HIGH_RESOLUTION*/,TIMER_ALL_ACCESS);if(!vt)vw=0;}
  fprintf(stderr,"[LEAN] vblank wait %s\n",vw?"high-resolution":"plain 10 ms");}
 HANDLE events[3];DWORD n=0;
 events[n++]=lean_wake_event();if(driving_flip204_enabled())events[n++]=wake204;
 if(vw){extern volatile long long lean_vblank_next;long long nx=lean_vblank_next;LARGE_INTEGER t;QueryPerformanceCounter(&t);
  long long left=nx?nx-t.QuadPart:0;
  if(left>0){if(left>vhz.QuadPart/100)left=vhz.QuadPart/100;
   LARGE_INTEGER d;d.QuadPart=-(left*10000000LL/vhz.QuadPart);if(!d.QuadPart)d.QuadPart=-1;
   if(SetWaitableTimer(vt,&d,0,NULL,NULL,FALSE))events[n++]=vt;}
  else return;}
 DWORD result=WaitForMultipleObjects(n,events,FALSE,10);
 if(result==WAIT_FAILED)stop204("timer wake wait");
#else
 if(driving_flip204_enabled()){
  DWORD result=WaitForSingleObject(wake204,10);
  if(result!=WAIT_OBJECT_0&&result!=WAIT_TIMEOUT)stop204("timer wake wait");
 }else Sleep(10);
#endif
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
void driving_flip204_completed(uint32_t physical,const uint8_t *pixels){
 AcquireSRWLockExclusive(&cache_lock204);
 unsigned slot=0;for(;slot<2;slot++)if(keys204[slot]==physical||!keys204[slot])break;
 if(slot==2){ReleaseSRWLockExclusive(&cache_lock204);stop204("more than two movie output buffers");}
 keys204[slot]=physical;memcpy(cache204[slot],pixels,sizeof cache204[slot]);versions204[slot]++;
 ReleaseSRWLockExclusive(&cache_lock204);
}
void driving_flip204_scanout(uint32_t physical){
#ifdef DRIVING_LEAN_RENDERER
 {static unsigned n;static ULONGLONG t0;if(!t0)t0=GetTickCount64();if(getenv("LEAN_LOG_SW_AFTER_MS")&&n<400){n++;fprintf(stderr,"[LEAN-PCRTC] %llu physical=%08X",GetTickCount64()-t0,physical);fputc(10,stderr);}}
 {extern void lean_scanout(uint32_t);lean_scanout(physical);}
#endif
 if(!driving_flip204_enabled())return;
 static unsigned presented,misses;
 int found=0;
 AcquireSRWLockExclusive(&cache_lock204);
 for(unsigned slot=0;slot<2;slot++)if(keys204[slot]==physical&&versions204[slot]!=shown204[slot]){
  driving_present201((const uint8_t*)cache204[slot],physical);shown204[slot]=versions204[slot];found=1;
  if(++presented<=6||!(presented%120))fprintf(stderr,"[FLIP204] presented=%u physical=%08X version=%u ms=%llu original-PCRTC-write=1\n",presented,physical,versions204[slot],(unsigned long long)GetTickCount64());
  break;
 }
 ReleaseSRWLockExclusive(&cache_lock204);
 if(!found&&++misses<=6)fprintf(stderr,"[FLIP204] scanout without new owned movie image physical=%08X (loading/unsupported/repeated surface)\n",physical);
}
