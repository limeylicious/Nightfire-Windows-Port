/*510 diagnostic implementation, included once by driving_diag510.c.
 * Suspend -> context and bounded stack copy -> resume -> DbgHelp/output.
 * The watchdog never holds a registry/CRT/renderer lock while a target is suspended. */
#define THREADS510 256
#define STACK510 65536
#define SAMPLES510 32768
#define FRAMES510 12
typedef struct T510 {DWORD tid,generation;HANDLE handle;char role[32];volatile LONG sequence;
 uint64_t heartbeat;char phase[40];unsigned wait_phase,wait_count,wait_type,has_timeout;int64_t timeout;
 uint32_t objects[64];uint64_t prior_cpu,prior_time;} T510;
typedef struct S510 {uint64_t time,gap,cpu_delta,wall_delta,rip,rsp,heartbeat;
 DWORD tid,generation,suspend_previous,context_error,resume_error,copy_error;
 unsigned copied,wait_phase,wait_count,wait_type,has_timeout,metadata_consistent,depth,truncated;
 int64_t timeout;char role[32],phase[40];uint32_t objects[64];uint64_t frames[FRAMES510];char names[FRAMES510][192];} S510;
static T510 threads510[THREADS510];static unsigned thread_count510;
static SRWLOCK registry510=SRWLOCK_INIT;static __declspec(thread) T510 *self510;
static S510 samples510[SAMPLES510];static unsigned sample_count510,completed_samples510,missed_periods510;
static unsigned thread_lost510,sample_lost510;static uint64_t max_gap510,first_present510;
static unsigned deadline510=355;static HANDLE stall_file510=INVALID_HANDLE_VALUE;
static unsigned char stack510[STACK510];static uintptr_t stack_base510;static size_t stack_bytes510;
static volatile LONG footer_count510;static DWORD sampler_tid510;static int symbol_ready510;
static void write_stall510(const char*p,size_t n){if(stall_file510!=INVALID_HANDLE_VALUE){DWORD wrote=0;if(!WriteFile(stall_file510,p,(DWORD)n,&wrote,NULL)||wrote!=n)sample_lost510++;}}
static T510 *lookup510(DWORD tid,const char*role){
 T510*out=NULL;AcquireSRWLockExclusive(&registry510);
 for(unsigned i=0;i<thread_count510;i++)if(threads510[i].tid==tid){DWORD code;
  if(WaitForSingleObject(threads510[i].handle,0)==WAIT_TIMEOUT){out=&threads510[i];break;}}
 if(!out){if(thread_count510<THREADS510){HANDLE h=OpenThread(SYNCHRONIZE|THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,tid);
  if(h){out=&threads510[thread_count510++];out->tid=tid;out->generation=thread_count510;out->handle=h;strcpy_s(out->role,sizeof out->role,"enumerated");}}
  else thread_lost510++;}
 if(out&&role)strcpy_s(out->role,sizeof out->role,role);ReleaseSRWLockExclusive(&registry510);return out;
}
void driving_diag510_register(const char*role){if(!driving_stall510_on)return;DWORD e=GetLastError();int ce=errno;self510=lookup510(GetCurrentThreadId(),role);errno=ce;SetLastError(e);}
void driving_diag510_beat(const char*phase){if(!driving_stall510_on)return;DWORD e=GetLastError();int ce=errno;
 if(!self510)self510=lookup510(GetCurrentThreadId(),NULL);T510*t=self510;if(t){InterlockedIncrement(&t->sequence);t->heartbeat=now510();strncpy_s(t->phase,sizeof t->phase,phase,_TRUNCATE);InterlockedIncrement(&t->sequence);}errno=ce;SetLastError(e);
}
void driving_diag510_wait(unsigned n,const uint32_t*objects,unsigned type,int timeout,int64_t value,unsigned phase){if(!driving_stall510_on)return;DWORD e=GetLastError();int ce=errno;
 if(!self510)self510=lookup510(GetCurrentThreadId(),NULL);T510*t=self510;if(t){InterlockedIncrement(&t->sequence);t->heartbeat=now510();t->wait_count=n>64?64:n;t->wait_type=type;t->has_timeout=timeout;t->timeout=value;t->wait_phase=phase;if(objects)memcpy(t->objects,objects,t->wait_count*4);InterlockedIncrement(&t->sequence);}errno=ce;SetLastError(e);
}
void driving_diag510_wait_phase(unsigned phase){if(!driving_stall510_on)return;DWORD e=GetLastError();int ce=errno;
 T510*t=self510;if(t){InterlockedIncrement(&t->sequence);t->wait_phase=phase;t->heartbeat=now510();InterlockedIncrement(&t->sequence);}errno=ce;SetLastError(e);
}
static void initialize_stall510(void){if(!driving_stall510_on)return;char path[2048];snprintf(path,sizeof path,"%s/stall510.jsonl",directory510);
 stall_file510=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 if(stall_file510==INVALID_HANDLE_VALUE){fprintf(stderr,"[DIAG510] STOP cannot create stall locator\n");ExitProcess(2);}}
static void symbols_init510(void){
 sampler_tid510=GetCurrentThreadId();char exe[MAX_PATH];DWORD n=GetModuleFileNameA(NULL,exe,sizeof exe);
 if(n&&n<sizeof exe){char*p=strrchr(exe,'\\');if(p)*p=0;}
 else strcpy_s(exe,sizeof exe,".");
 SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_LOAD_LINES|SYMOPT_FAIL_CRITICAL_ERRORS|SYMOPT_NO_PROMPTS);
 /* Local PDB and system module exports only. No symbol-server search path. */
 symbol_ready510=SymInitialize(GetCurrentProcess(),exe,TRUE)?1:0;
}
static BOOL CALLBACK read_stack510(HANDLE process,DWORD64 address,PVOID output,DWORD bytes,LPDWORD read){
 *read=0;if(address>=stack_base510&&address-stack_base510<=stack_bytes510&&bytes<=stack_bytes510-(size_t)(address-stack_base510)){
  memcpy(output,stack510+(size_t)(address-stack_base510),bytes);*read=bytes;return TRUE;}
 /* Unwind metadata is allowed only from immutable module-image pages. Never a resumed live stack or guest RAM. */
 MEMORY_BASIC_INFORMATION m;uintptr_t a=(uintptr_t)address;
 if(bytes&&VirtualQuery((void*)a,&m,sizeof m)&&m.Type==MEM_IMAGE&&m.State==MEM_COMMIT&&
  (m.Protect==PAGE_READONLY||m.Protect==PAGE_EXECUTE_READ)&&a>=(uintptr_t)m.BaseAddress&&
  a+bytes>=a&&a+bytes<=(uintptr_t)m.BaseAddress+m.RegionSize){SIZE_T copied=0;BOOL ok=ReadProcessMemory(process,(void*)a,output,bytes,&copied);*read=(DWORD)copied;return ok;}
 return FALSE;
}
static void symbolize_frame510(uint64_t address,char*out,size_t size){
 char buffer[sizeof(SYMBOL_INFO)+512];SYMBOL_INFO*symbol=(SYMBOL_INFO*)buffer;memset(buffer,0,sizeof buffer);symbol->SizeOfStruct=sizeof *symbol;symbol->MaxNameLen=511;DWORD64 displacement=0;
 if(symbol_ready510&&SymFromAddr(GetCurrentProcess(),address,&displacement,symbol))snprintf(out,size,"%s+0x%llX",symbol->Name,(unsigned long long)displacement);
 else {HMODULE m=NULL;char path[MAX_PATH]="unknown";
  if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)(uintptr_t)address,&m))GetModuleFileNameA(m,path,sizeof path);
  const char*p=strrchr(path,'\\');snprintf(out,size,"%s+0x%llX [no-local-symbol]",p?p+1:path,(unsigned long long)(address-(uint64_t)(uintptr_t)m));}
 /* PDB names are data, escape conservatively for JSON. */
 for(char*p=out;*p;p++)if(*p=='"'||*p=='\\'||(unsigned char)*p<32)*p='_';
}
static void capture_thread510(T510*t,uint64_t now,uint64_t gap){
 now=now510();gap=now-(uint64_t)InterlockedCompareExchange64(&last_present510,0,0);if(gap<3*(uint64_t)hz510.QuadPart)return;
 if(t->tid==GetCurrentThreadId())return;if(WaitForSingleObject(t->handle,0)!=WAIT_TIMEOUT)return;
 if(sample_count510>=SAMPLES510){sample_lost510++;return;}S510*s=&samples510[sample_count510++];s->time=now;s->gap=gap;s->tid=t->tid;s->generation=t->generation;memcpy(s->role,t->role,sizeof s->role);
 FILETIME create,exit,kernel,user;if(GetThreadTimes(t->handle,&create,&exit,&kernel,&user)){
  ULARGE_INTEGER k,u;k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;uint64_t cpu=k.QuadPart+u.QuadPart;
  if(t->prior_time){s->cpu_delta=cpu-t->prior_cpu;s->wall_delta=now-t->prior_time;}t->prior_cpu=cpu;t->prior_time=now;}
 CONTEXT c;memset(&c,0,sizeof c);c.ContextFlags=CONTEXT_FULL;
 s->suspend_previous=SuspendThread(t->handle);if(s->suspend_previous==(DWORD)-1){s->context_error=GetLastError();goto output;}
 if(GetThreadContext(t->handle,&c)){
  s->rip=c.Rip;s->rsp=c.Rsp;stack_base510=(uintptr_t)c.Rsp;stack_bytes510=0;
  MEMORY_BASIC_INFORMATION m;SIZE_T copied=0;
  if(VirtualQuery((void*)stack_base510,&m,sizeof m)&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))){
   size_t available=(uintptr_t)m.BaseAddress+m.RegionSize-stack_base510;size_t bytes=available<STACK510?available:STACK510;
   if(!ReadProcessMemory(GetCurrentProcess(),(void*)stack_base510,stack510,bytes,&copied))s->copy_error=GetLastError();stack_bytes510=copied;s->copied=(unsigned)copied;
  }
  LONG seq=InterlockedCompareExchange(&t->sequence,0,0);
  if(!(seq&1)){s->heartbeat=t->heartbeat;s->wait_phase=t->wait_phase;s->wait_count=t->wait_count;s->wait_type=t->wait_type;s->has_timeout=t->has_timeout;s->timeout=t->timeout;memcpy(s->objects,t->objects,sizeof s->objects);memcpy(s->phase,t->phase,sizeof s->phase);MemoryBarrier();s->metadata_consistent=seq==InterlockedCompareExchange(&t->sequence,0,0);}
 }else s->context_error=GetLastError();
 /* Every successful suspension is balanced before any DbgHelp, formatting, or I/O. */
 if(ResumeThread(t->handle)==(DWORD)-1){s->resume_error=GetLastError();goto output;}
 if(s->rip){STACKFRAME64 frame;memset(&frame,0,sizeof frame);frame.AddrPC.Offset=c.Rip;frame.AddrPC.Mode=AddrModeFlat;frame.AddrFrame.Offset=c.Rbp;frame.AddrFrame.Mode=AddrModeFlat;frame.AddrStack.Offset=c.Rsp;frame.AddrStack.Mode=AddrModeFlat;
  s->frames[s->depth++]=c.Rip;
  for(unsigned walk=0;s->depth<FRAMES510&&symbol_ready510&&walk<FRAMES510+2;walk++){if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,GetCurrentProcess(),t->handle,&frame,&c,read_stack510,SymFunctionTableAccess64,SymGetModuleBase64,NULL)||!frame.AddrPC.Offset){s->truncated=1;break;}
   if(frame.AddrPC.Offset!=s->frames[s->depth-1])s->frames[s->depth++]=frame.AddrPC.Offset;else if(walk){s->truncated=1;break;}}
  for(unsigned i=0;i<s->depth;i++)symbolize_frame510(s->frames[i],s->names[i],sizeof s->names[i]);
 }
output:;
 char line[8192];int at=snprintf(line,sizeof line,"{\"kind\":\"sample\",\"qpc\":%llu,\"gap_qpc\":%llu,\"tid\":%lu,\"generation\":%lu,\"role\":\"%s\",\"rip\":%llu,\"rsp\":%llu,\"cpu_100ns\":%llu,\"wall_qpc\":%llu,\"phase\":\"%s\",\"heartbeat\":%llu,\"wait_phase\":%u,\"wait_type\":%u,\"has_timeout\":%u,\"timeout\":%lld,\"metadata_consistent\":%u,\"suspend_previous\":%lu,\"context_error\":%lu,\"resume_error\":%lu,\"copy_error\":%lu,\"stack_bytes\":%u,\"truncated\":%u,\"objects\":[",(unsigned long long)s->time,(unsigned long long)s->gap,s->tid,s->generation,s->role,(unsigned long long)s->rip,(unsigned long long)s->rsp,(unsigned long long)s->cpu_delta,(unsigned long long)s->wall_delta,s->phase,(unsigned long long)s->heartbeat,s->wait_phase,s->wait_type,s->has_timeout,(long long)s->timeout,s->metadata_consistent,s->suspend_previous,s->context_error,s->resume_error,s->copy_error,s->copied,s->truncated);
 for(unsigned i=0;i<s->wait_count&&i<64;i++)at+=snprintf(line+at,sizeof line-at,"%s%u",i?",":"",s->objects[i]);at+=snprintf(line+at,sizeof line-at,"],\"frames\":[");
 for(unsigned i=0;i<s->depth;i++)at+=snprintf(line+at,sizeof line-at,"%s{\"address\":%llu,\"symbol\":\"%s\"}",i?",":"",(unsigned long long)s->frames[i],s->names[i]);at+=snprintf(line+at,sizeof line-at,"]}\n");write_stall510(line,(size_t)at);completed_samples510++;
}
static void sample_stall510(uint64_t now){
 uint64_t last=(uint64_t)InterlockedCompareExchange64(&last_present510,0,0);if(last!=(uint64_t)start510)first_present510=1;
 if(!first_present510)return;uint64_t gap=now-last;if(gap>max_gap510)max_gap510=gap;if(gap<3*(uint64_t)hz510.QuadPart)return;
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snapshot!=INVALID_HANDLE_VALUE){THREADENTRY32 e;e.dwSize=sizeof e;
  if(Thread32First(snapshot,&e))do{if(e.th32OwnerProcessID==GetCurrentProcessId())lookup510(e.th32ThreadID,NULL);}while(Thread32Next(snapshot,&e));CloseHandle(snapshot);}
 unsigned n=thread_count510;for(unsigned i=0;i<n;i++)capture_thread510(&threads510[i],now,gap);
}
static void terminal_stall510(const char*reason){
 LONG ordinal=InterlockedIncrement(&footer_count510);char path[2048],body[2048];uint64_t now=now510(),last=(uint64_t)InterlockedCompareExchange64(&last_present510,0,0);
 if(first_present510&&now-last>max_gap510)max_gap510=now-last;
 snprintf(path,sizeof path,"%s/diag510-footer-%ld.json",directory510,ordinal);
 int length=snprintf(body,sizeof body,"{\"checkpoint\":510,\"reason\":\"%s\",\"qpc_hz\":%lld,\"start_qpc\":%lld,\"terminal_qpc\":%llu,\"last_present_qpc\":%llu,\"has_present\":%llu,\"max_gap_qpc\":%llu,\"thread_count\":%u,\"thread_overflow\":%u,\"sample_count\":%u,\"completed_samples\":%u,\"missed_periods\":%u,\"sample_overflow_or_io_error\":%u,\"text_reserved\":%ld,\"text_written\":%u,\"text_overflow_or_io_error\":%ld,\"text_on\":%d,\"stall_on\":%d,\"symbols_initialized\":%d,\"stack_limit_bytes\":65536,\"frames_limit\":12}\n",reason,hz510.QuadPart,start510,(unsigned long long)now,(unsigned long long)last,(unsigned long long)first_present510,(unsigned long long)max_gap510,thread_count510,thread_lost510,sample_count510,completed_samples510,missed_periods510,sample_lost510,reserved510,written510,lost510,driving_text510_on,driving_stall510_on,symbol_ready510);
 HANDLE f=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(f!=INVALID_HANDLE_VALUE){DWORD n;WriteFile(f,body,(DWORD)length,&n,NULL);FlushFileBuffers(f);CloseHandle(f);}
}
