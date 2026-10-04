/* One owned batch; no guest callbacks, command copies, or fabricated GET. */
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driving_async295.h"
#ifndef DRIVING_ASYNC295_WAIT_MS
#define DRIVING_ASYNC295_WAIT_MS 30000u
#endif
static INIT_ONCE once295=INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION gate295;
static HANDLE wake295,done295,thread295;
static volatile LONG setting295=-1,state295,stop295;
static DWORD worker295;
static unsigned retired295,worker_failed295;
static __declspec(thread) unsigned nesting295;
static struct {DrivingTail295 fn;void *ctx;DrivingFP295 fp;unsigned packets;size_t bytes;uint64_t armed,kicked,worker_ticks;} job295;
static struct {uint64_t armed,completed,joined,blocked,join_ticks,producer_ticks,worker_ticks,packets,bytes,fp_changes,rejected[12];} count295;
static uint64_t clock295(void){LARGE_INTEGER t;QueryPerformanceCounter(&t);return (uint64_t)t.QuadPart;}
static void fatal295(const char *why){fprintf(stderr,"[ASYNC295] FATAL %s state=%ld worker=%lu current=%lu; no further GET publication\n",why,InterlockedCompareExchange(&state295,0,0),worker295,GetCurrentThreadId());fflush(stderr);ExitProcess(142);}
int driving_async295_enabled(void){DWORD e=GetLastError();LONG on=InterlockedCompareExchange(&setting295,-1,-1);if(on<0){const char*v=getenv("DRIVING_ASYNC_TAIL295");LONG wanted=v&&!strcmp(v,"1"),old=InterlockedCompareExchange(&setting295,wanted,-1);on=old<0?wanted:old;}SetLastError(e);return on!=0;}
static BOOL CALLBACK init295(PINIT_ONCE o,PVOID p,PVOID*c){(void)o;(void)p;(void)c;InitializeCriticalSection(&gate295);return TRUE;}
static void report295(void){if(count295.joined!=1&&count295.joined%128)return;LARGE_INTEGER hz;QueryPerformanceFrequency(&hz);
 fprintf(stderr,"[ASYNC295] armed=%llu completed=%llu joins=%llu blocked=%llu join_ticks=%llu producer_before_join_ticks=%llu worker_ticks=%llu packets=%llu bytes=%llu fp_isolated_changes=%llu rejected=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu qpc_hz=%llu max_inflight=1 TIMING250_excludes_async_tail=1 producer_interval_not_useful_CPU_proof=1\n",
 (unsigned long long)count295.armed,(unsigned long long)count295.completed,(unsigned long long)count295.joined,(unsigned long long)count295.blocked,(unsigned long long)count295.join_ticks,(unsigned long long)count295.producer_ticks,(unsigned long long)count295.worker_ticks,(unsigned long long)count295.packets,(unsigned long long)count295.bytes,(unsigned long long)count295.fp_changes,
 (unsigned long long)count295.rejected[0],(unsigned long long)count295.rejected[1],(unsigned long long)count295.rejected[2],(unsigned long long)count295.rejected[3],(unsigned long long)count295.rejected[4],(unsigned long long)count295.rejected[5],(unsigned long long)count295.rejected[6],(unsigned long long)count295.rejected[7],(unsigned long long)count295.rejected[8],(unsigned long long)count295.rejected[9],(unsigned long long)count295.rejected[10],(unsigned long long)count295.rejected[11],(unsigned long long)hz.QuadPart);
}
static void join295(void){LONG state=InterlockedCompareExchange(&state295,0,0);if(!state)return;if(state==1)fatal295("join before kick");
 uint64_t start=clock295();count295.producer_ticks+=start-job295.kicked;count295.joined++;
 DWORD wait=WaitForSingleObject(done295,0);if(wait==WAIT_TIMEOUT){count295.blocked++;wait=WaitForSingleObject(done295,DRIVING_ASYNC295_WAIT_MS);}if(wait!=WAIT_OBJECT_0)fatal295("completion wait timeout/failure");
 count295.join_ticks+=clock295()-start;count295.worker_ticks+=job295.worker_ticks;
 if(InterlockedCompareExchange(&state295,0,0)!=3)fatal295("worker callback failed");
 count295.completed++;InterlockedExchange(&state295,0);memset(&job295,0,sizeof job295);report295();
}
void driving_async295_enter(void){if(!driving_async295_enabled())return;DWORD e=GetLastError();DrivingFP295 fp;driving_fp295_save(&fp);
 if(GetCurrentThreadId()==worker295&&worker295)fatal295("worker attempted producer/AV entry");
 InitOnceExecuteOnce(&once295,init295,NULL,NULL);EnterCriticalSection(&gate295);if(retired295)fatal295("consumer entry after shutdown");if(!nesting295)join295();nesting295++;driving_fp295_restore(&fp);SetLastError(e);
}
void driving_async295_leave(void){if(!driving_async295_enabled())return;DWORD e=GetLastError();if(!nesting295)fatal295("unbalanced API leave");if(nesting295==1&&InterlockedCompareExchange(&state295,0,0)==1)fatal295("armed job not kicked");nesting295--;LeaveCriticalSection(&gate295);SetLastError(e);}
static DWORD WINAPI run295(void *unused){(void)unused;for(;;){if(WaitForSingleObject(wake295,INFINITE)!=WAIT_OBJECT_0)fatal295("worker wake");if(InterlockedCompareExchange(&stop295,0,0))break;
 if(InterlockedCompareExchange(&state295,0,0)!=2)fatal295("unexpected worker state");DrivingFP295 prior;driving_fp295_save(&prior);driving_fp295_restore(&job295.fp);DWORD e=GetLastError();uint64_t start=clock295();
 int ok=job295.fn(job295.ctx);job295.worker_ticks=clock295()-start;driving_fp295_restore(&prior);SetLastError(e);
 InterlockedExchange(&state295,ok?3:4);if(!SetEvent(done295))fatal295("worker completion signal");
 }return 0;}
static int ensure295(void){if(thread295)return 1;if(worker_failed295||retired295)return 0;
 wake295=CreateEventA(NULL,FALSE,FALSE,NULL);done295=CreateEventA(NULL,TRUE,TRUE,NULL);
 if(wake295&&done295)thread295=CreateThread(NULL,0,run295,NULL,0,&worker295);
 if(thread295)return 1;if(wake295)CloseHandle(wake295);if(done295)CloseHandle(done295);wake295=done295=NULL;worker295=0;worker_failed295=1;return 0;
}
int driving_async295_arm(DrivingTail295 fn,void *ctx,unsigned packets,size_t bytes){if(!driving_async295_enabled())return 0;DWORD e=GetLastError();DrivingFP295 fp;driving_fp295_save(&fp);int ok=0;
 if(nesting295!=1||!fn||!packets||packets>32||bytes>32u*1024*1024||InterlockedCompareExchange(&state295,0,0)||!ensure295())goto done;
 if(!ResetEvent(done295))fatal295("reset completion");job295.fn=fn;job295.ctx=ctx;job295.fp=fp;job295.packets=packets;job295.bytes=bytes;job295.armed=clock295();
 count295.armed++;count295.packets+=packets;count295.bytes+=bytes;InterlockedExchange(&state295,1);ok=1;
 done:driving_fp295_restore(&fp);SetLastError(e);return ok;
}
void driving_async295_kick(void){DWORD e=GetLastError();if(!driving_async295_enabled()||nesting295!=1||InterlockedCompareExchange(&state295,0,0)!=1)fatal295("invalid kick");job295.kicked=clock295();InterlockedExchange(&state295,2);if(!SetEvent(wake295))fatal295("wake signal");SetLastError(e);}
void driving_async295_reject(unsigned reason){if(driving_async295_enabled()&&reason<12){DWORD e=GetLastError();static uint64_t refusals;count295.rejected[reason]++;refusals++;if(!count295.armed&&(refusals==1||!(refusals%512)))report295();SetLastError(e);}}
void driving_async295_fp_observed(const DrivingFP295 *before){if(!driving_async295_enabled())return;DrivingFP295 after;driving_fp295_save(&after);if(before->mxcsr!=after.mxcsr||memcmp(&before->env,&after.env,sizeof after.env))count295.fp_changes++;}
int driving_async295_pending(void){return driving_async295_enabled()&&InterlockedCompareExchange(&state295,0,0)!=0;}
void driving_async295_shutdown(void){if(!driving_async295_enabled())return;DWORD e=GetLastError();DrivingFP295 fp;driving_fp295_save(&fp);
 if(nesting295||(worker295&&GetCurrentThreadId()==worker295))fatal295("shutdown from active consumer");
 InitOnceExecuteOnce(&once295,init295,NULL,NULL);EnterCriticalSection(&gate295);
 if(retired295){LeaveCriticalSection(&gate295);driving_fp295_restore(&fp);SetLastError(e);return;}
 join295();retired295=1;
 if(thread295){InterlockedExchange(&stop295,1);if(!SetEvent(wake295)||WaitForSingleObject(thread295,DRIVING_ASYNC295_WAIT_MS)!=WAIT_OBJECT_0)fatal295("worker shutdown timeout/failure");CloseHandle(thread295);CloseHandle(wake295);CloseHandle(done295);thread295=wake295=done295=NULL;worker295=0;}
 LeaveCriticalSection(&gate295);driving_fp295_restore(&fp);SetLastError(e);
}
