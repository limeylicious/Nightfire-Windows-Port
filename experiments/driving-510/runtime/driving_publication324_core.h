#ifndef DRIVING_PUBLICATION324_CORE_H
#define DRIVING_PUBLICATION324_CORE_H
#include "driving_publication324.h"
#include <errno.h>
#include "driving_pc508_guards.h"
typedef struct {SRWLOCK lock;volatile LONG64 publications,reads,owner_reads;
 volatile LONG64 command_reads,command_owner_reads;} DP324;
/* Global lock intentionally serializes every admitted RPM span, including a
 * known alternate view, without pretending VirtualQuery enumerates aliases.
 * Lock order: existing allocation lease -> publication lock. Read-side holds
 * ONLY this lock around RPM; no allocator callbacks, flush or GPU work there. */
static int dp324_begin(DP324*c,DL322Lease*allocated,DL322Lease*requested,DL322Lease**owner,unsigned purpose){
 DWORD error=GetLastError();int crt=errno;
 if(!allocated||allocated!=requested||!requested->active||requested->thread!=GetCurrentThreadId()||*owner||purpose!=DL322_PUBLISH_EXISTING)return 0;
 AcquireSRWLockExclusive(&c->lock);PC508_DEPTH_ENTER(2);*owner=requested;
 errno=crt;SetLastError(error);return 1;
}
static int dp324_end(DP324*c,DL322Lease*requested,DL322Lease**owner){
 DWORD error=GetLastError();int crt=errno;
 if(!requested||*owner!=requested||requested->thread!=GetCurrentThreadId()||!requested->active)return 0;
 *owner=NULL;InterlockedIncrement64(&c->publications);ReleaseSRWLockExclusive(&c->lock);PC508_DEPTH_LEAVE(2);
 errno=crt;SetLastError(error);return 1;
}
static BOOL dp324_read(DP324*c,DL322Lease*owner,HANDLE process,LPCVOID source,LPVOID out,SIZE_T bytes,SIZE_T*got){
 DWORD error=GetLastError();int crt=errno;
 /* Reentrant owner access cannot wait on itself. Preserve its actual RPM
  * semantics and count it; this is NOT a coherent foreign read guarantee.
  * The existing two joins make no such calls. Native validation checks zero. */
 if(!owner){AcquireSRWLockShared(&c->lock);PC508_DEPTH_ENTER(2);}else InterlockedIncrement64(&c->owner_reads);
 InterlockedIncrement64(&c->reads);errno=crt;SetLastError(error);
 BOOL ok=ReadProcessMemory(process,source,out,bytes,got);
 error=GetLastError();crt=errno;
 if(!owner){ReleaseSRWLockShared(&c->lock);PC508_DEPTH_LEAVE(2);}
 errno=crt;SetLastError(error);return ok;
}
#ifdef DRIVING_COMMAND_PUBLISH325
/* Reader: allocator lookup has already finished, then permission shared ->
 * publication shared. Publisher: allocation shared -> publication exclusive;
 * its two joins must NOT acquire permission lock or call a reader. No flush,
 * callback, allocator query or permission mutation is allowed in this scope.
 * As with RPM324, reentrant owner reads retain real access/error semantics,
 * are separately counted, and do not establish a coherent foreign snapshot. */
static int dp325_command_begin(DP324*c,DL322Lease*owner){
 DWORD error=GetLastError();int crt=errno;int shared=owner==NULL;
 if(shared){AcquireSRWLockShared(&c->lock);PC508_DEPTH_ENTER(2);}else InterlockedIncrement64(&c->command_owner_reads);
 InterlockedIncrement64(&c->command_reads);errno=crt;SetLastError(error);return shared;
}
static void dp325_command_end(DP324*c,int shared){
 DWORD error=GetLastError();int crt=errno;
 if(shared){ReleaseSRWLockShared(&c->lock);PC508_DEPTH_LEAVE(2);}
 errno=crt;SetLastError(error);
}
#endif
#endif
