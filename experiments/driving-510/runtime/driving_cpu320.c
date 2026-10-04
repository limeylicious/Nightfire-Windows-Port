#include "driving_cpu320.h"
#ifdef DRIVING_ACCESS321
#include "driving_access321.h"
#endif
#include <intrin.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int driving_cpu320_enabled;
volatile LONG driving_cpu320_apertures;
static SRWLOCK cpu320_lock=SRWLOCK_INIT;
static DrivingCpu320Snapshot cpu320;
typedef struct Cpu320HostState {
 __declspec(align(16)) unsigned char fp[512];
 DWORD error;
 int crt_error;
} Cpu320HostState;
static void cpu320_save(Cpu320HostState *s){
 _fxsave64(s->fp);s->error=GetLastError();s->crt_error=errno;
}
static void cpu320_restore(const Cpu320HostState *s){
 _fxrstor64((unsigned char*)s->fp);errno=s->crt_error;SetLastError(s->error);
}
static void cpu320_inc(uint64_t *n){if(*n!=UINT64_MAX)++*n;}
static int cpu320_span(uint32_t address,uint32_t bytes){return bytes && (uint64_t)address+bytes<=UINT64_C(0x100000000);}

int driving_cpu320_init(int enabled){
 Cpu320HostState saved;cpu320_save(&saved);int ok=0;
 AcquireSRWLockExclusive(&cpu320_lock);
 if(driving_cpu320_enabled)cpu320_inc(&cpu320.init_refused);
 else{
  memset(&cpu320,0,sizeof cpu320);InterlockedExchange(&driving_cpu320_apertures,0);
  driving_cpu320_enabled=enabled!=0;ok=1;
 }
 ReleaseSRWLockExclusive(&cpu320_lock);cpu320_restore(&saved);return ok;
}
void driving_cpu320_publish(const uint32_t addresses[2],const uint32_t bytes[2]){
 Cpu320HostState saved;cpu320_save(&saved);
 if(!driving_cpu320_enabled){cpu320_restore(&saved);return;}
 AcquireSRWLockExclusive(&cpu320_lock);
 cpu320.valid=0;memset(cpu320.address,0,sizeof cpu320.address);memset(cpu320.bytes,0,sizeof cpu320.bytes);
 if(cpu320.epoch==UINT64_MAX || !addresses || !bytes || !cpu320_span(addresses[0],bytes[0]) || !cpu320_span(addresses[1],bytes[1])){
  cpu320_inc(&cpu320.invalid_publications);
 }else{
  ++cpu320.epoch;cpu320_inc(&cpu320.publications);
  unsigned hint=0;
  for(unsigned i=0;i<2;i++){
   cpu320.address[i]=addresses[i];cpu320.bytes[i]=bytes[i];
   unsigned end=(uint32_t)((uint64_t)addresses[i]+bytes[i]-1)>>28;
   for(unsigned at=addresses[i]>>28;at<=end;at++)hint|=1u<<at;
  }
  InterlockedOr(&driving_cpu320_apertures,(LONG)hint);cpu320.valid=1;
 }
 ReleaseSRWLockExclusive(&cpu320_lock);cpu320_restore(&saved);
}
void driving_cpu320_clear(void){
 Cpu320HostState saved;cpu320_save(&saved);
 if(!driving_cpu320_enabled){cpu320_restore(&saved);return;}
 AcquireSRWLockExclusive(&cpu320_lock);
 cpu320.valid=0;memset(cpu320.address,0,sizeof cpu320.address);memset(cpu320.bytes,0,sizeof cpu320.bytes);cpu320_inc(&cpu320.clears);
 ReleaseSRWLockExclusive(&cpu320_lock);cpu320_restore(&saved);
}
__declspec(noinline) void driving_cpu320_note(uint32_t address,unsigned bytes,uintptr_t site){
 Cpu320HostState saved;cpu320_save(&saved);
 if(!driving_cpu320_enabled){cpu320_restore(&saved);return;}
 if(!site)site=(uintptr_t)_ReturnAddress();
 AcquireSRWLockExclusive(&cpu320_lock);cpu320_inc(&cpu320.notes);
#ifdef DRIVING_ACCESS321
 unsigned category=site>=0x321000u&&site<0x321020u?(unsigned)(site-0x321000u):0;
 cpu320_inc(&cpu320.category_notes[category]);
#endif
 if(!cpu320_span(address,bytes))cpu320_inc(&cpu320.invalid_notes);
 else if(cpu320.valid)for(unsigned i=0;i<2;i++){
  if((uint64_t)address+bytes<=cpu320.address[i] || (uint64_t)cpu320.address[i]+cpu320.bytes[i]<=address)continue;
  cpu320_inc(&cpu320.hits);
#ifdef DRIVING_ACCESS321
  cpu320_inc(&cpu320.category_hits[category]);
#endif
  DrivingCpu320Event e={cpu320.hits,cpu320.epoch,site,GetCurrentThreadId(),address,bytes,i,cpu320.address[i],cpu320.bytes[i]};
  if(cpu320.first_count<DRIVING_CPU320_FIRST)cpu320.first[cpu320.first_count++]=e;
  if(cpu320.recent_count<DRIVING_CPU320_RECENT)++cpu320.recent_count;else cpu320_inc(&cpu320.overwritten);
  cpu320.recent[cpu320.recent_next]=e;cpu320.recent_next=(cpu320.recent_next+1)%DRIVING_CPU320_RECENT;
 }
 ReleaseSRWLockExclusive(&cpu320_lock);cpu320_restore(&saved);
}
int driving_cpu320_snapshot(DrivingCpu320Snapshot *out){
 Cpu320HostState saved;cpu320_save(&saved);int ok=out!=NULL;
 if(ok){AcquireSRWLockShared(&cpu320_lock);memcpy(out,&cpu320,sizeof cpu320);ReleaseSRWLockShared(&cpu320_lock);}
 cpu320_restore(&saved);return ok;
}
void driving_cpu320_report(void){
 Cpu320HostState saved;cpu320_save(&saved);DrivingCpu320Snapshot s;driving_cpu320_snapshot(&s);
#ifdef DRIVING_ACCESS321
 driving_access321_report();
 for(unsigned k=0;k<32;k++)if(s.category_notes[k])fprintf(stderr,"[CATEGORY321] kind=%u notes=%llu overlaps=%llu\n",k,(unsigned long long)s.category_notes[k],(unsigned long long)s.category_hits[k]);
#endif
 fprintf(stderr,"[CPU320] enabled=%d epoch=%llu publications=%llu clears=%llu invalid_publications=%llu notes=%llu invalid_notes=%llu overlaps=%llu first=%u recent=%u overwritten=%llu init_refused=%llu exact_guest_spans=1 publication_epoch_not_allocation_generation=1 native_site_not_exact_guest_pc=1 unobserved=bulk,atomic,kernel,escaped_pointers,other_aliases\n",
 driving_cpu320_enabled,(unsigned long long)s.epoch,(unsigned long long)s.publications,(unsigned long long)s.clears,(unsigned long long)s.invalid_publications,(unsigned long long)s.notes,(unsigned long long)s.invalid_notes,(unsigned long long)s.hits,s.first_count,s.recent_count,(unsigned long long)s.overwritten,(unsigned long long)s.init_refused);
 for(unsigned set=0;set<2;set++)for(unsigned j=0;j<(set?s.recent_count:s.first_count);j++){
  unsigned at=set?(s.recent_next+DRIVING_CPU320_RECENT-s.recent_count+j)%DRIVING_CPU320_RECENT:j;
  const DrivingCpu320Event *e=set?&s.recent[at]:&s.first[at];
  fprintf(stderr,"[CPU320-%s] sequence=%llu epoch=%llu thread=%u site=%p va=%08X bytes=%u span=%u watched=%08X+%u\n",set?"recent":"first",(unsigned long long)e->sequence,(unsigned long long)e->epoch,e->thread,(void*)e->site,e->address,e->bytes,e->span_index,e->span_address,e->span_bytes);
 }
 cpu320_restore(&saved);
}
