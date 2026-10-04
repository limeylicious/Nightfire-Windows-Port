#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "driving_permissions264.h"
#include "driving_pc508_guards.h"
static SRWLOCK permission_lock264=SRWLOCK_INIT;
static uint64_t permission_generation264=1;
static unsigned permission_retired264;
static volatile LONG permission_setting264=-1;
int driving_permissions264_enabled(void){
 DWORD error=GetLastError();LONG setting=InterlockedCompareExchange(&permission_setting264,-1,-1);
 if(setting<0){const char*v=getenv("DRIVING_BATCH_MAP264");LONG desired=v&&!strcmp(v,"1");
  LONG previous=InterlockedCompareExchange(&permission_setting264,desired,-1);setting=previous<0?desired:previous;}
 SetLastError(error);return setting!=0;
}
uint64_t driving_permissions264_read_begin(void){
 DWORD error=GetLastError();AcquireSRWLockShared(&permission_lock264);PC508_DEPTH_ENTER(0);
 uint64_t result=permission_retired264?0:permission_generation264;SetLastError(error);return result;
}
void driving_permissions264_read_end(void){DWORD error=GetLastError();ReleaseSRWLockShared(&permission_lock264);PC508_DEPTH_LEAVE(0);SetLastError(error);}
unsigned driving_permissions264_write_begin(void){
 if(!driving_permissions264_enabled())return 0;
 DWORD error=GetLastError();AcquireSRWLockExclusive(&permission_lock264);PC508_DEPTH_ENTER(0);
 if(++permission_generation264==0)permission_retired264=1;SetLastError(error);return 1;
}
void driving_permissions264_write_end(unsigned token){
 if(!token)return;DWORD error=GetLastError();
 if(++permission_generation264==0)permission_retired264=1;
 ReleaseSRWLockExclusive(&permission_lock264);PC508_DEPTH_LEAVE(0);SetLastError(error);
}
void driving_permissions264_retire(void){
 if(!driving_permissions264_enabled())return;
 DWORD error=GetLastError();AcquireSRWLockExclusive(&permission_lock264);PC508_DEPTH_ENTER(0);
 permission_retired264=1;if(++permission_generation264==0)permission_generation264=1;
 ReleaseSRWLockExclusive(&permission_lock264);PC508_DEPTH_LEAVE(0);SetLastError(error);
}

/*445: lock-free generation snapshot for caches whose callers may already hold
 * the shared interval (batchmap264 wraps driving_map230 in read_begin/end); a
 * nested AcquireSRWLockShared can deadlock behind a queued exclusive writer.
 * Writers still change the generation only under the exclusive lock, before
 * and after each tracked mutation, so any change observed here invalidates
 * every entry stamped with the previous value. Returns0 = do not cache:
 * tracking disabled (DRIVING_BATCH_MAP264!=1) or permanently retired. */
uint64_t driving_permissions264_peek445(void){
 if(!driving_permissions264_enabled())return 0;
 uint64_t generation=(uint64_t)InterlockedCompareExchange64((volatile LONG64*)&permission_generation264,0,0);
 if(InterlockedCompareExchange((volatile LONG*)&permission_retired264,0,0))return 0;
 return generation;
}
