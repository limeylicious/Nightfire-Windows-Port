#include "driving_diag510.h" /*510 private default-OFF diagnostics*/
/* Checkpoint450 "PC mode", driving side. See gpu144/nightfire_pc450.h.
 * Included twice from driving_packets236.h: first for state (before batch234),
 * then with DRIVING_PC450_IMPL defined (after batch234 and its helpers).
 * Default OFF: DRIVING_PC450=1. With it off nothing below changes behaviour. */
#ifndef DRIVING_PC450_IMPL
#ifndef DRIVING_PC450_STATE_H
#include <errno.h>
#include <fenv.h>
#include <xmmintrin.h>
#define DRIVING_PC450_STATE_H
#ifdef DRIVING_PC508
#include "driving_pc508_state.h"
#endif
enum {PC450_REASONS=24};
static struct {
#if defined(DRIVING_PC508) && defined(DRIVING_LEASE322)
 DL322Key keys508[2];
#endif
 int dirty;DrivingSpan183 spans[2];uint8_t *mapped[2],*targets;uint32_t target_fields[7];
 uint64_t sessions,deferred,published,gpu_clears,clear_fallbacks,refused,split_skipped;
 const char *reason[PC450_REASONS];uint64_t reason_deferred[PC450_REASONS],reason_published[PC450_REASONS];
} pc450;
static int pc450_setting=-1;
static int pc450_enabled(void){
 if(pc450_setting<0){DWORD e=GetLastError();int crt=errno;const char*v=getenv("DRIVING_PC450");pc450_setting=v&&!strcmp(v,"1");
  if(pc450_setting)fprintf(stderr,"[PC450] enabled=1 drain-scoped GPU residency of the main pair; visual-parity mode, not a bit-exact proof\n");
  errno=crt;SetLastError(e);}
 return pc450_setting;
}
static int pc450_dirty(void){
#ifdef DRIVING_PC508
 if(driving_pc508_active)return pc508_phase()!=PC508_CLEAN;
#endif
 return pc450.dirty;
}
static void pc450_publish(const char *reason);
/*451 cross-drain PC mode (default OFF, DRIVING_PC451=1 on top of DRIVING_PC450=1).
 * The open session also survives drain exit. Between drains the guest RAM of the
 * pair is made PAGE_NOACCESS, so the first CPU access from anywhere (game code,
 * runtime, kernel bridge) faults; on the consumer thread the fault publishes the
 * pair and the access then proceeds on current bytes. Any other thread stops with
 * a named fatal. Inside a drain the pages are ordinary read/write again and the
 * 450 publication rules apply unchanged. */
enum {PC451_SITES=32};
static struct {
 volatile LONG protected_flag;DWORD thread;unsigned control;fenv_t env;int env_valid;
 uint8_t *base[2];DWORD old[2];
 uint64_t kernel_ops[4];
 uint64_t protects,unprotects,faults,retries,refused_protect,lifetime_checks,symbolized,drains;
 uint64_t protect_ticks;
 struct {uintptr_t rip;uint64_t count;unsigned write;int printed;char module[64];uintptr_t rva;} site[PC451_SITES];unsigned sites;
} pc451;
static int pc451_setting=-1;
static int pc451_enabled(void){
 if(pc451_setting<0){DWORD e=GetLastError();int crt=errno;const char*v=getenv("DRIVING_PC451");pc451_setting=v&&!strcmp(v,"1")&&pc450_enabled();
  if(pc451_setting)fprintf(stderr,"[PC451] enabled=1 cross-drain residency of the main pair; guest RAM pages are no-access between drains and publish on first CPU touch\n");
  errno=crt;SetLastError(e);}
 return pc451_setting;
}
static int pc451_protected(void){
#ifdef DRIVING_PC508
 if(driving_pc508_active)return pc508_phase()!=PC508_CLEAN;
#endif
 return InterlockedCompareExchange(&pc451.protected_flag,0,0)!=0;
}
static int pc450_cpu_clear_setting=-1;
static int pc450_cpu_clear(void){ /* diagnostic: DRIVING_PC450_CPU_CLEAR=1 keeps clears on the CPU path */
 if(pc450_cpu_clear_setting<0){DWORD e=GetLastError();const char*v=getenv("DRIVING_PC450_CPU_CLEAR");pc450_cpu_clear_setting=v&&!strcmp(v,"1");SetLastError(e);}
 return pc450_cpu_clear_setting;
}
static void pc451_unprotect(void);
static void pc451_drain_enter(void);
static void pc451_drain_leave(void);
static void pc451_protect_pair(void);
#endif
#else
#ifndef DRIVING_PC450_IMPL_H
#define DRIVING_PC450_IMPL_H
static void pc450_count(const char *reason,int deferred){
 unsigned i=0;
 for(;i<PC450_REASONS&&pc450.reason[i];i++)if(!strcmp(pc450.reason[i],reason))break;
 if(i==PC450_REASONS)i=PC450_REASONS-1;else if(!pc450.reason[i])pc450.reason[i]=reason;
 if(deferred)pc450.reason_deferred[i]++;else pc450.reason_published[i]++;
}
static void pc450_report(void){
 uint64_t n=pc450.published;if(n!=1&&(n%256))return;
 DWORD e=GetLastError();int crt=errno;
 fprintf(stderr,"[PC450] sessions=%llu deferred_flushes=%llu publications=%llu gpu_clears=%llu clear_fallbacks=%llu refused=%llu split_skipped=%llu",
  (unsigned long long)pc450.sessions,(unsigned long long)pc450.deferred,(unsigned long long)pc450.published,(unsigned long long)pc450.gpu_clears,
  (unsigned long long)pc450.clear_fallbacks,(unsigned long long)pc450.refused,(unsigned long long)pc450.split_skipped);
 for(unsigned i=0;i<PC450_REASONS&&pc450.reason[i];i++)fprintf(stderr," %s=%llu/%llu",pc450.reason[i],(unsigned long long)pc450.reason_deferred[i],(unsigned long long)pc450.reason_published[i]);
 fputc('\n',stderr);errno=crt;SetLastError(e);
}
/* Only flushes that do not require guest RAM to be current may be deferred.
 * Everything else (drain exit, unadmitted/other draws, blits, object binds,
 * source/texture/vertex/descriptor aliases, replays, clears of other targets,
 * target changes, backend refusals) publishes first, exactly as before. */
static int pc450_deferrable(const char *reason){
 return
#ifdef DRIVING_PC508
  (driving_pc508_active&&!strcmp(reason,"semaphore-release"))||
#endif
  !strcmp(reason,"capacity")||!strcmp(reason,"drain-before-GET")||!strcmp(reason,"pc450-clear")||
  (pc451_enabled()&&!strcmp(reason,"drain-exit")); /*451: the session may outlive the drain (pages protected)*/
}
static int pc450_config(void){
 static int reported;
 int ok=!nf_hw_color_seed276_enabled()&&!nf_hw_depth_seed278_enabled()&&DRIVING_OWNED253_ENABLED()
#ifdef DRIVING_ASYNC295
  &&!driving_async295_enabled()
#endif
  ;
 if(!ok&&!reported){reported=1;fprintf(stderr,"[PC450] refused by configuration: needs DRIVING_COLOR_SEED276=0, DRIVING_DEPTH_SEED278=0, DRIVING_OWNED253=1, async295 off\n");}
 return ok;
}
static int pc450_same_target(const DrivingSpan183 *spans,uint8_t *const *mapped,const uint32_t *fields){
 if(memcmp(pc450.target_fields,fields,sizeof pc450.target_fields))return 0;
 for(unsigned i=0;i<2;i++)if(pc450.spans[i].address!=spans[i].address||pc450.spans[i].available!=spans[i].available||
   pc450.spans[i].handle!=spans[i].handle||pc450.spans[i].instance!=spans[i].instance||pc450.spans[i].offset!=spans[i].offset||
   pc450.mapped[i]!=mapped[i])return 0;
 return 1;
}
static int pc450_batch_matches(void){
 return batch234.targets==pc450.targets&&pc450_same_target(batch234.spans,batch234.mapped,batch234.target_fields);
}
static void pc450_sequence(NFHardwareBatchDraw248 *sequence){
 for(unsigned i=0;i<batch234.count;i++){
  DrivingPacket234 *p=&batch234.packets[i];sequence[i].count=p->dense;
  for(unsigned lane=0;lane<2;lane++){
   sequence[i].states[lane]=p->state;
   sequence[i].states[lane].color=batch234.targets+lane*DRIVING_LANE234;
   sequence[i].states[lane].depth=batch234.targets+(2+lane)*DRIVING_LANE234;
   sequence[i].vertices[lane]=p->vertices+lane*p->dense;
  }
 }
}
/* The same bookkeeping and release the ordinary flush epilogue performs. */
static void pc450_release_batch(uint64_t start){
 unsigned n=batch234.count;uint64_t done=driving_clock227();
 batch_time236.backend_ticks+=done-start;batch_time236.flushes++;batch_time236.draws+=n;
 if(n>batch_time236.maximum)batch_time236.maximum=n;
 for(unsigned i=0;i<n;i++){free(batch234.packets[i].vertices);free(batch234.packets[i].textures);}
 driving_release253(DS253_TARGETS,batch234.targets);memset(&batch234,0,sizeof batch234);
 batch_time236.flush_ticks+=driving_clock227()-start;
}
static void pc450_publish(const char *reason){
 driving_diag510_beat("publication-enter");
 if(!pc450_dirty())return;
#ifdef DRIVING_PC508
 if(driving_pc508_active)pc508_publication_enter();
#endif
 pc451_unprotect(); /*451: guest RAM must be writable before the lane split/join below*/
 uint64_t start=driving_clock227();
 /* Stencil is guest-RAM owned (clears apply it there); the pack keeps each lane
  * word's low byte, so refresh the depth lanes from RAM before reading back. */
 driving_split227(pc450.mapped[1],pc450.targets+2*DRIVING_LANE234,pc450.targets+3*DRIVING_LANE234,640*480);
 int r=nf_hw_pc450_finish();
 if(r<=0)fail143("pc450 publication failed",(uint32_t)r,0);
 driving_join227(pc450.mapped[0],pc450.targets,pc450.targets+DRIVING_LANE234,640*480);
 driving_join227(pc450.mapped[1],pc450.targets+2*DRIVING_LANE234,pc450.targets+3*DRIVING_LANE234,640*480);
#ifdef DRIVING_PC508
 if(driving_pc508_active)InterlockedExchange(&pc508.phase,PC508_CLEAN);else
#endif
 pc450.dirty=0;
 pc450.published++;driving_diag510_beat("publication-exit");pc450_count(reason,0);
 batch_time236.publish_ticks+=driving_clock227()-start;
 pc450_report();
#ifdef DRIVING_PC508
 if(driving_pc508_active)pc508_publication_leave();
#endif
}
/* Returns 1 when the flush was fully handled here, 0 to continue with the
 * ordinary flush (always with the session closed and guest RAM current). */
static int pc450_flush_hook(const char *reason){
 if(!pc450_enabled())return 0;
 uint64_t start=driving_clock227();
 if(pc450_deferrable(reason)){
  if(!batch234.count)return pc450_dirty();
  NFHardwareBatchDraw248 sequence[DRIVING_BATCH_COUNT234];
  if(!pc450_dirty()){
   if(!pc450_config()){pc450.refused++;return 0;}
   pc450_sequence(sequence);
   driving_batchmap264_reset();batch_epoch236++;
   int r=nf_hw_pc450_begin(sequence,batch234.count);
   if(r<0)fail143("pc450 session begin fatal",batch234.count,0);
   if(!r){pc450.refused++;return 0;}
#ifdef DRIVING_PC508
   if(!driving_pc508_active)
#endif
   pc450.dirty=1;
   pc450.sessions++;pc450.targets=batch234.targets;
   memcpy(pc450.spans,batch234.spans,sizeof pc450.spans);memcpy(pc450.mapped,batch234.mapped,sizeof pc450.mapped);
   memcpy(pc450.target_fields,batch234.target_fields,sizeof pc450.target_fields);
#ifdef DRIVING_PC508
   if(driving_pc508_active){
#ifdef DRIVING_LEASE322
    memcpy(pc450.keys508,batch234.keys322,sizeof pc450.keys508);
#endif
    pc508_open();
   }
#endif
  }else{
   if(!pc450_batch_matches()){pc450_publish("target-mismatch");return 0;}
   pc450_sequence(sequence);
   driving_batchmap264_reset();batch_epoch236++;
   int r=nf_hw_pc450_append(sequence,batch234.count);
   if(r<0)fail143("pc450 session append fatal",batch234.count,0);
   if(!r){pc450_publish("append-refused");return 0;}
  }
  pc450.deferred++;pc450_count(reason,1);pc450_release_batch(start);return 1;
 }
 if(!pc450_dirty())return 0;
 if(batch234.count&&pc450_batch_matches()){
  NFHardwareBatchDraw248 sequence[DRIVING_BATCH_COUNT234];pc450_sequence(sequence);
  driving_batchmap264_reset();batch_epoch236++;
  int r=nf_hw_pc450_append(sequence,batch234.count);
  if(r<0)fail143("pc450 session append fatal",batch234.count,1);
  if(r){pc450_publish(reason);pc450_release_batch(start);return 1;}
 }
 pc450_publish(reason);
 return batch234.count?0:1;
}
/* ---- 451 cross-drain residency -------------------------------------------------- */
#include <dbghelp.h>
#include "driving_protection452.h" /*452 tracked page mutations; opt-in*/
#include "driving_access453.h" /*453 bounded access/failure repair; opt-in*/
#pragma comment(lib,"dbghelp.lib")
#ifdef DRIVING_PC508
#include "driving_pc508_core.h"
#endif
static uint64_t pc451_ticks(void){LARGE_INTEGER t;QueryPerformanceCounter(&t);return (uint64_t)t.QuadPart;}
static double pc451_ms(uint64_t ticks){static LARGE_INTEGER f;if(!f.QuadPart)QueryPerformanceFrequency(&f);return 1000.0*(double)ticks/(double)f.QuadPart;}
static void pc451_report(int force){
 if(!force&&pc451.protects!=1&&(pc451.protects%256))return;
 DWORD e=GetLastError();int crt=errno;
 fprintf(stderr,"[PC451] drains=%llu protects=%llu unprotects=%llu cpu_faults=%llu retries=%llu refused_protect=%llu lifetime_checks=%llu protect_ms=%.1f sites=%u kernel_set_protect=%llu kernel_query_protect=%llu kernel_other=%llu\n",
  (unsigned long long)pc451.drains,(unsigned long long)pc451.protects,(unsigned long long)pc451.unprotects,(unsigned long long)pc451.faults,
  (unsigned long long)pc451.retries,(unsigned long long)pc451.refused_protect,(unsigned long long)pc451.lifetime_checks,pc451_ms(pc451.protect_ticks),pc451.sites,
  (unsigned long long)pc451.kernel_ops[0],(unsigned long long)pc451.kernel_ops[1],(unsigned long long)pc451.kernel_ops[2]);
 errno=crt;SetLastError(e);
}
/* Both spans become ordinary read/write again. Consumer thread only (the VEH
 * calls it on the consumer thread; other threads never get here). */
static void pc451_unprotect(void){
 if(!pc451_protected())return;
 uint64_t t=pc451_ticks();
 for(unsigned i=0;i<2;i++){DWORD old=0;
  if(!pc452_protect(pc451.base[i],DRIVING_PAIR234,pc451.old[i],&old))fail143("pc451 unprotect failed",(uint32_t)GetLastError(),i);
  if(old!=PAGE_NOACCESS)fail143("pc451 pair protection changed while unpublished",old,i);
 }
#ifdef DRIVING_PC508
 if(!driving_pc508_active)
#endif
 InterlockedExchange(&pc451.protected_flag,0);
 pc451.unprotects++;pc451.protect_ticks+=pc451_ticks()-t;
}
static void pc451_site(const EXCEPTION_RECORD *x,const CONTEXT *c){
 uintptr_t rip=
#if defined(_M_X64)
  (uintptr_t)c->Rip;
#else
  (uintptr_t)x->ExceptionAddress;
#endif
 for(unsigned i=0;i<pc451.sites;i++)if(pc451.site[i].rip==rip){pc451.site[i].count++;return;}
 if(pc451.sites==PC451_SITES)return;
 unsigned i=pc451.sites++;pc451.site[i].rip=rip;pc451.site[i].count=1;pc451.site[i].write=x->NumberParameters>0&&x->ExceptionInformation[0]==1;
 HMODULE m=NULL;
 if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)rip,&m)&&m){
  char path[MAX_PATH];DWORD n=GetModuleFileNameA(m,path,sizeof path);const char*b=path;
  for(DWORD k=0;k<n;k++)if(path[k]=='\\'||path[k]=='/')b=path+k+1;
  size_t len=n?strlen(b):0;if(len>sizeof pc451.site[i].module-1)len=sizeof pc451.site[i].module-1;
  memcpy(pc451.site[i].module,b,len);pc451.site[i].module[len]=0;pc451.site[i].rva=rip-(uintptr_t)m;
 }else{pc451.site[i].module[0]='?';pc451.site[i].module[1]=0;}
}
/* Symbol names are resolved later, on the consumer thread at drain entry, never
 * inside the exception handler. The PDB sits beside the EXE. */
static void pc451_symbolize(void){
 if(driving_stall510_on)return; /*510 sole DbgHelp owner; raw PC451 site data retained*/
 static int ready=-1;
 for(unsigned i=0;i<pc451.sites;i++){
  if(pc451.site[i].printed)continue;
  if(ready<0){SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_LOAD_LINES);ready=SymInitialize(GetCurrentProcess(),NULL,TRUE)?1:0;}
  char buffer[sizeof(SYMBOL_INFO)+256];SYMBOL_INFO*sym=(SYMBOL_INFO*)buffer;memset(buffer,0,sizeof buffer);
  sym->SizeOfStruct=sizeof(SYMBOL_INFO);sym->MaxNameLen=255;DWORD64 disp=0;
  const char*name=ready==1&&SymFromAddr(GetCurrentProcess(),(DWORD64)pc451.site[i].rip,&disp,sym)?sym->Name:"?";
  IMAGEHLP_LINE64 line;memset(&line,0,sizeof line);line.SizeOfStruct=sizeof line;DWORD ld=0;
  int has_line=ready==1&&SymGetLineFromAddr64(GetCurrentProcess(),(DWORD64)pc451.site[i].rip,&ld,&line);
  fprintf(stderr,"[PC451-SITE] n=%u count=%llu write=%u module=%s rva=%llX symbol=%s+0x%llX line=%s:%lu\n",i,(unsigned long long)pc451.site[i].count,
   pc451.site[i].write,pc451.site[i].module,(unsigned long long)pc451.site[i].rva,name,(unsigned long long)disp,
   has_line&&line.FileName?line.FileName:"?",has_line?(unsigned long)line.LineNumber:0ul);
  pc451.site[i].printed=1;pc451.symbolized++;
 }
}
static LONG CALLBACK pc451_veh(EXCEPTION_POINTERS *e){
 const EXCEPTION_RECORD *x=e->ExceptionRecord;
 if(x->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION||x->NumberParameters<2)return EXCEPTION_CONTINUE_SEARCH;
 uintptr_t a=(uintptr_t)x->ExceptionInformation[1];int inside=0;
 for(unsigned i=0;i<2;i++)if(pc451.base[i]&&a>=(uintptr_t)pc451.base[i]&&a-(uintptr_t)pc451.base[i]<(uintptr_t)DRIVING_PAIR234)inside=1;
 if(!inside)return EXCEPTION_CONTINUE_SEARCH;
 DWORD error=GetLastError();int crt=errno;
#ifdef DRIVING_PC508
 if(driving_pc508_active){
  /* A foreign fault in a remembered pair is a STOP even after flags change. */
  if(GetCurrentThreadId()!=pc451.thread)pc508_stop("pc508 foreign pair fault",(uint32_t)a,(uint32_t)GetCurrentThreadId());
  pc508_check_depth("pc508 owner fault under lock or trap");
  LONG phase=pc508_phase();
  if(phase==PC508_ARMING||phase==PC508_PUBLISHING)pc508_stop("pc508 recursive or transition fault",(uint32_t)a,(uint32_t)phase);
 }
#endif
 if(!pc451_protected()){
  /* A remembered address is not evidence that this handler owns the fault.
   * In particular, never retry a later legitimate NOACCESS/retired mapping. */
  if(pc453_enabled()){errno=crt;SetLastError(error);return EXCEPTION_CONTINUE_SEARCH;}
  pc451.retries++;errno=crt;SetLastError(error);return EXCEPTION_CONTINUE_EXECUTION;
 }
 if(GetCurrentThreadId()!=pc451.thread){
  fprintf(stderr,"[PC451] other-thread access tid=%lu consumer=%lu address=%p write=%u\n",(unsigned long)GetCurrentThreadId(),(unsigned long)pc451.thread,(void*)a,(unsigned)x->ExceptionInformation[0]);
  fail143("pc451 other-thread access to unpublished pair",(uint32_t)GetCurrentThreadId(),(uint32_t)(a&0xffffffffu));
 }
 if(pc453_enabled()){
  /* These allocations are data. Execute faults must not publish or become
   * executable as a side effect; leave them to the original exception chain. */
  if(x->ExceptionInformation[0]!=0&&x->ExceptionInformation[0]!=1){errno=crt;SetLastError(error);return EXCEPTION_CONTINUE_SEARCH;}
  if(pc453_publishing)fail143("pc453 recursive publication fault",(uint32_t)(a&0xffffffffu),(uint32_t)x->ExceptionInformation[0]);
  if(!pc450_dirty())fail143("pc453 protected pair without pending publication",(uint32_t)(a&0xffffffffu),0);
  pc453_publishing=1;
 }
 pc451.faults++;pc451_site(x,e->ContextRecord);
 /* The faulting code's FP environment is restored from the CONTEXT on return;
  * the publication runs under the environment recorded when the pages were
  * protected, the same one the session was built under. */
 fenv_t saved;fegetenv(&saved);unsigned csr=_mm_getcsr();
 if(pc451.env_valid)fesetenv(&pc451.env);
 _mm_setcsr(pc451.control);
 pc450_publish("cpu-fault");
 if(pc453_enabled()){
  if(pc450_dirty()||pc451_protected())
   fail143("pc453 publication did not release pair",pc450_dirty(),pc451_protected());
  pc453_publishing=0;
 }
 fesetenv(&saved);_mm_setcsr(csr);errno=crt;SetLastError(error);
 return EXCEPTION_CONTINUE_EXECUTION;
}
/* Called by the kernel bridge before an Xbox kernel memory service that reads or
 * changes the protection of guest pages (MmSetAddressProtect/MmQueryAddressProtect).
 * Such a call must never see or overwrite the no-access marking of an unpublished
 * pair: publish first (consumer thread), so the service sees ordinary pages. */
static void pc451_kernel_memory(const void *native,uint32_t bytes,unsigned kind){
#ifdef DRIVING_PC508
 if(driving_pc508_active){
  driving_pc508_guard_native(native,bytes,kind==0?"kernel-set-protect":kind==1?"kernel-query-protect":"kernel-memory");return;
 }
#endif
 if(!pc451_enabled()||!pc451_protected()||!native||!bytes)return;
 uintptr_t a=(uintptr_t)native;int overlap=0;
 for(unsigned i=0;i<2;i++)if(a<(uintptr_t)pc451.base[i]+(uintptr_t)DRIVING_PAIR234&&(uintptr_t)pc451.base[i]<a+bytes)overlap=1;
 if(!overlap)return;
 DWORD error=GetLastError();int crt=errno;
 if(GetCurrentThreadId()!=pc451.thread){
  fprintf(stderr,"[PC451] other-thread kernel memory service kind=%u tid=%lu consumer=%lu\n",kind,(unsigned long)GetCurrentThreadId(),(unsigned long)pc451.thread);
  fail143("pc451 other-thread kernel memory service on unpublished pair",kind,(uint32_t)GetCurrentThreadId());
 }
 pc451.kernel_ops[kind<3?kind:2]++;
 if(pc451.kernel_ops[kind<3?kind:2]<=4)fprintf(stderr,"[PC451] kernel memory service kind=%u address=%p bytes=%u publishes first\n",kind,native,bytes);
 fenv_t saved;fegetenv(&saved);unsigned csr=_mm_getcsr();
 if(pc451.env_valid)fesetenv(&pc451.env);
 _mm_setcsr(pc451.control);
 pc450_publish(kind==0?"kernel-set-protect":kind==1?"kernel-query-protect":"kernel-memory");
 fesetenv(&saved);_mm_setcsr(csr);errno=crt;SetLastError(error);
}
/*451b diagnostic: stop, naming the place, as soon as the pair is found no longer
 * no-access while the runtime believes it is (a protection reset without a fault). */
static void pc451_probe(const char *where,unsigned code){
 /*452: a background kernel-return probe may race the owner restoring the
  * pages. Only the consumer has a stable before/after boundary here. This
  * does not suppress the VEH/kernel-memory foreign-access STOP paths. */
 if(pc452_enabled()&&GetCurrentThreadId()!=pc451.thread)return;
 if(!pc451_enabled()||!pc451_protected())return;
 for(unsigned i=0;i<2;i++){MEMORY_BASIC_INFORMATION m={0};
  if(VirtualQuery(pc451.base[i],&m,sizeof m)==sizeof m&&m.Protect==PAGE_NOACCESS)continue;
  DWORD e=GetLastError();
  MEMORY_BASIC_INFORMATION actual={0};SIZE_T actual_n=pc452_real_query(pc451.base[i],&actual);
  fprintf(stderr,"[PC452] independent query at loss bytes=%llu real_protect=%lX real_region=%llu cached_protect=%lX generation=%llu fix=%d\n",(unsigned long long)actual_n,(unsigned long)actual.Protect,(unsigned long long)actual.RegionSize,(unsigned long)m.Protect,(unsigned long long)driving_permissions264_peek445(),pc452_enabled());
  fprintf(stderr,"[PC451] protection lost at %s code=%u span=%u protect=%lX region=%llu tid=%lu consumer=%lu\n",where,code,i,(unsigned long)m.Protect,(unsigned long long)m.RegionSize,(unsigned long)GetCurrentThreadId(),(unsigned long)pc451.thread);
  pc451_report(1);SetLastError(e);
  fail143("pc451 protection lost",code,i);
 }
}
/* Consumer thread, before anything else in the drain touches memory. */
static void pc451_drain_enter(void){
 if(!pc451_enabled())return;
#ifdef DRIVING_PC508
 if(driving_pc508_active){
  pc508_owner_enter("pc508 drain owner changed");
  if(pc508_drain_depth)pc508_stop("pc508 recursive drain",pc508_drain_depth,0);
  pc508_drain_depth=1;pc508.lifetime_pass=0;
 }
#endif
 pc451.drains++;pc451.thread=GetCurrentThreadId();
 if(pc451_protected()){
  /* Allocation lifetime: the spans must still be the same committed private
   * pages we protected; a free/decommit/remap in between is a named stop. */
  for(unsigned i=0;i<2;i++){MEMORY_BASIC_INFORMATION m={0};
   if(VirtualQuery(pc451.base[i],&m,sizeof m)!=sizeof m||m.State!=MEM_COMMIT||m.Protect!=PAGE_NOACCESS||m.Type!=MEM_PRIVATE||
      (uint8_t*)m.BaseAddress!=pc451.base[i]||m.RegionSize<(SIZE_T)DRIVING_PAIR234){
    pc451_report(1);
    fprintf(stderr,"[PC451] span=%u protect=%lX alloc_protect=%lX state=%lX type=%lX region=%llu\n",i,(unsigned long)m.Protect,(unsigned long)m.AllocationProtect,(unsigned long)m.State,(unsigned long)m.Type,(unsigned long long)m.RegionSize);
    fail143("pc451 pair allocation changed between drains",m.Protect,i);}
  }
  pc451.lifetime_checks++;
#ifdef DRIVING_PC508
  if(driving_pc508_active)pc508.lifetime_pass=1;else
#endif
  pc451_unprotect();
 }
 if(pc451.sites&&pc451.symbolized<pc451.sites)pc451_symbolize();
}
/* Consumer thread, after the drain-exit flush. With a session still open, the
 * pages are protected and the GPU is kicked; otherwise nothing to do. */
static void pc451_protect_pair(void){
 if(!pc451_enabled()||!pc450_dirty())return;
#ifdef DRIVING_PC508
 if(driving_pc508_active){if(pc508_phase()!=PC508_ARMING)pc508_stop("pc508 protect phase",pc508_phase(),0);}
 else
#endif
 if(InterlockedCompareExchange(&pc451.protected_flag,0,0))return;
 static PVOID handler;
 if(!handler){handler=AddVectoredExceptionHandler(1,pc451_veh);if(!handler){
  if(pc453_enabled())fail143("pc453 cannot install access handler",(uint32_t)GetLastError(),0);
  pc451.refused_protect++;pc450_publish("drain-exit-no-handler");return;
 }}
 uint64_t t=pc451_ticks();
 for(unsigned i=0;i<2;i++){
  uint8_t *b=pc450.mapped[i];
  if(!b||((uintptr_t)b&4095)||(DRIVING_PAIR234&4095)){
   if(pc453_enabled())fail143("pc453 invalid publication span",(uint32_t)(uintptr_t)b,i);
   pc451.refused_protect++;pc450_publish("drain-exit-unaligned");return;
  }
 }
 pc451.base[0]=pc450.mapped[0];pc451.base[1]=pc450.mapped[1];
 pc451.control=_mm_getcsr();pc451.env_valid=!fegetenv(&pc451.env);pc451.thread=GetCurrentThreadId();
 for(unsigned i=0;i<2;i++){DWORD old=0;
  /* The game may have set write-combine/no-cache on these pages (MmSetAddressProtect);
   * any read/write base protection is accepted and restored exactly on unprotect. */
  BOOL changed=pc452_protect(pc451.base[i],DRIVING_PAIR234,PAGE_NOACCESS,&old);
  if(!changed||(old&0xff)!=PAGE_READWRITE||(old&~(DWORD)(0xff|PAGE_NOCACHE|PAGE_WRITECOMBINE))){
   if(pc453_enabled()){
    DWORD failure=changed?ERROR_INVALID_ACCESS:GetLastError();
    /* Restore only transitions that actually succeeded, checking each one.
     * A failed protect may mean the allocation was decommitted/retired. Even
     * successful rollback cannot authorize publishing into those targets. */
    if(changed)pc453_rollback(i,old);
    for(unsigned j=0;j<i;j++)pc453_rollback(j,pc451.old[j]);
    pc451.refused_protect++;pc451.protect_ticks+=pc451_ticks()-t;
    fail143(changed?"pc453 target was not writable before protection":"pc453 target protection failed",failure,i);
    return;
   }
   if(old&&old!=PAGE_NOACCESS){DWORD back;pc452_protect(pc451.base[i],DRIVING_PAIR234,old,&back);}
   for(unsigned j=0;j<i;j++){DWORD back;pc452_protect(pc451.base[j],DRIVING_PAIR234,pc451.old[j],&back);}
   pc451.refused_protect++;pc451.protect_ticks+=pc451_ticks()-t;pc450_publish("drain-exit-unprotectable");return;
  }
  pc451.old[i]=old;
 }
#ifdef DRIVING_PC508
 if(driving_pc508_active){InterlockedExchange(&pc508.phase,PC508_RESIDENT);pc508.lifetime_pass=1;}else
#endif
 InterlockedExchange(&pc451.protected_flag,1);
 pc451.protects++;pc451.protect_ticks+=pc451_ticks()-t;
 if(pc452_enabled()&&pc451.protects<=4){
  for(unsigned i=0;i<2;i++){MEMORY_BASIC_INFORMATION actual={0},cached={0};
   SIZE_T a=pc452_real_query(pc451.base[i],&actual),b=VirtualQuery(pc451.base[i],&cached,sizeof cached);
   fprintf(stderr,"[PC452] protected span=%u real=%lX cache=%lX generation=%llu\n",i,(unsigned long)actual.Protect,(unsigned long)cached.Protect,(unsigned long long)driving_permissions264_peek445());
   if(a!=sizeof actual||b!=sizeof cached||actual.Protect!=PAGE_NOACCESS||cached.Protect!=actual.Protect||cached.BaseAddress!=actual.BaseAddress||cached.RegionSize!=actual.RegionSize)
    fail143("pc452 independent protection mismatch",i,actual.Protect);
  }
 }

#ifdef DRIVING_PC508
 if(!driving_pc508_active)
#endif
 nf_hw_pc451_kick();
 pc451_report(0);
}
static void pc451_drain_leave(void){
#ifdef DRIVING_PC508
 if(driving_pc508_active){if(pc450_dirty()){nf_hw_pc451_kick();pc451_report(0);}return;}
#endif
 pc451_protect_pair();
}
#endif
#endif
