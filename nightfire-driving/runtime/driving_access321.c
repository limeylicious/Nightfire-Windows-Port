#include "driving_access321.h"
#include "driving_cpu320.h"
#include <intrin.h>
#include <errno.h>
#include <stdio.h>

extern int xbox_ContiguousSnapshot311(uint32_t,uint32_t,uint32_t*,uint32_t*,uint64_t*);
static uintptr_t offset321;
static int ready321;
static volatile LONG64 rejected321;
typedef struct Host321 {__declspec(align(16)) unsigned char fp[512];DWORD error;int crt;} Host321;
static void save321(Host321*s){_fxsave64(s->fp);s->error=GetLastError();s->crt=errno;}
static void restore321(Host321*s){_fxrstor64(s->fp);errno=s->crt;SetLastError(s->error);}
typedef struct Identity321 {
 uint32_t va,bytes,base,size,type,state;uint64_t generation;
 uintptr_t native,allocation_base;unsigned snapshot_ok,native_ok;
} Identity321;
static SRWLOCK identity_lock321=SRWLOCK_INIT;
static Identity321 identities321[32];
static unsigned identity_count321;
static uint64_t publications321,identity_overflow321,identity_failures321;
void driving_access321_init(uintptr_t offset){offset321=offset;ready321=1;}
void driving_access321_note(const void *p,size_t bytes,unsigned kind){
 if(!ready321||!driving_cpu320_enabled||!bytes)return;
 uintptr_t host=(uintptr_t)p;
 /* A host pointer outside the guest VA extent is not a guest access. Never
  * mask it into a physical address. Low/F remain distinct from contiguous. */
 if(host<offset321 || host-offset321>UINT32_MAX)return;
 uint32_t va=(uint32_t)(host-offset321);
 if(bytes>UINT32_MAX || bytes>UINT64_C(0x100000000)-va){InterlockedIncrement64(&rejected321);return;}
 driving_cpu320_note_if_possible(va,(unsigned)bytes,0x321000u+(kind&31u));
}
void *driving_access321_memcpy(void*d,const void*s,size_t n,unsigned k){
 driving_access321_note(s,n,k);driving_access321_note(d,n,k+1);return memcpy(d,s,n);
}
void *driving_access321_memmove(void*d,const void*s,size_t n,unsigned k){
 driving_access321_note(s,n,k);driving_access321_note(d,n,k+1);return memmove(d,s,n);
}
void *driving_access321_memset(void*d,int v,size_t n,unsigned k){
 driving_access321_note(d,n,k);return memset(d,v,n);
}
BOOL driving_access321_readfile(HANDLE h,void*p,DWORD n,DWORD*done,OVERLAPPED*ov){
 /* Requested span before the OS call. This does not claim successful I/O or
  * synchronously completed writes; the host call is unchanged. */
 driving_access321_note(p,n,DA321_FILE_READ_DEST);return ReadFile(h,p,n,done,ov);
}
BOOL driving_access321_writefile(HANDLE h,const void*p,DWORD n,DWORD*done,OVERLAPPED*ov){
 driving_access321_note(p,n,DA321_FILE_WRITE_SOURCE);return WriteFile(h,p,n,done,ov);
}
BOOL driving_access321_rpm(HANDLE h,const void*p,void*out,SIZE_T n,SIZE_T*done){
 /* Selected call sites read this process. Do not reinterpret a foreign
  * process address as local storage. */
 if(h==GetCurrentProcess())driving_access321_note(p,n,DA321_RPM_SOURCE);
 driving_access321_note(out,n,DA321_RPM_DEST);return ReadProcessMemory(h,p,out,n,done);
}
void driving_access321_publish(const uint32_t va[2],const uint32_t bytes[2],void *const native[2]){
 if(!ready321||!driving_cpu320_enabled)return;
 Host321 saved;save321(&saved);
 for(unsigned i=0;i<2;i++){
  Identity321 d={0};d.va=va[i];d.bytes=bytes[i];d.native=(uintptr_t)native[i];
  d.snapshot_ok=xbox_ContiguousSnapshot311(d.va,d.bytes,&d.base,&d.size,&d.generation);
  MEMORY_BASIC_INFORMATION m={0};
  if(VirtualQuery(native[i],&m,sizeof m)==sizeof m){
   d.allocation_base=(uintptr_t)m.AllocationBase;d.type=m.Type;d.state=m.State;
   d.native_ok=d.native==offset321+d.va && m.Type==MEM_PRIVATE && m.State==MEM_COMMIT &&
    (uintptr_t)m.BaseAddress<=d.native && m.RegionSize>=d.bytes &&
    d.native-(uintptr_t)m.BaseAddress<=m.RegionSize-d.bytes;
  }
  AcquireSRWLockExclusive(&identity_lock321);
  publications321++;if(!d.snapshot_ok||!d.native_ok)identity_failures321++;
  unsigned j;for(j=0;j<identity_count321;j++)if(!memcmp(&d,&identities321[j],sizeof d))break;
  if(j==identity_count321){if(j<32)identities321[identity_count321++]=d;else identity_overflow321++;}
  ReleaseSRWLockExclusive(&identity_lock321);
 }
 restore321(&saved);
}
/* Before guest execution: reversible byte probe of the actual mapped windows.
 * This samples backing identity, not allocation ownership or alias completeness. */
int driving_access321_backing(uintptr_t offset){
 Host321 saved;save321(&saved);
 uint32_t va[3]={0x1000,0xf0001000,0x80001000};
 uint32_t before[3]={0},after[3]={0};void*p[3];MEMORY_BASIC_INFORMATION m[3]={{0}};int ok=1;
 for(unsigned i=0;i<3;i++){
  p[i]=(void*)(offset+va[i]);SIZE_T got=0;
  if(VirtualQuery(p[i],&m[i],sizeof m[i])!=sizeof m[i]||m[i].State!=MEM_COMMIT||
    (m[i].Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(m[i].Protect&(PAGE_READWRITE|PAGE_EXECUTE_READWRITE))||
    !ReadProcessMemory(GetCurrentProcess(),p[i],before+i,4,&got)||got!=4)ok=0;
 }
 unsigned alias=0,separate=0,restored=0;
 if(ok){
  uint32_t a=before[0]^0x6b38a791u,b=before[2]^0x19dc52e7u;
  *(volatile uint32_t*)p[0]=a;
  alias=*(volatile uint32_t*)p[1]==a;
  separate=*(volatile uint32_t*)p[2]==before[2];
  *(volatile uint32_t*)p[2]=b;
  separate=separate&&*(volatile uint32_t*)p[0]==a&&*(volatile uint32_t*)p[1]==a;
  for(unsigned i=0;i<3;i++)*(volatile uint32_t*)p[i]=before[i];
  restored=1;for(unsigned i=0;i<3;i++){after[i]=*(volatile uint32_t*)p[i];if(after[i]!=before[i])restored=0;}
  ok=alias&&separate&&restored&&m[0].Type==MEM_MAPPED&&m[1].Type==MEM_MAPPED&&m[2].Type==MEM_PRIVATE;
 }
 fprintf(stderr,"[BACKING321] checked=%d low_F_shared=%u contig_separate=%u bytes_restored=%u low_type=%lX tiled_type=%lX contig_type=%lX sampling_only=1\n",ok,alias,separate,restored,m[0].Type,m[1].Type,m[2].Type);
 restore321(&saved);return ok;
}
void driving_access321_report(void){
 Host321 saved;save321(&saved);Identity321 copy[32];unsigned count;uint64_t pubs,failures,overflow;
 AcquireSRWLockShared(&identity_lock321);memcpy(copy,identities321,sizeof copy);count=identity_count321;
 pubs=publications321;failures=identity_failures321;overflow=identity_overflow321;ReleaseSRWLockShared(&identity_lock321);
 fprintf(stderr,"[ACCESS321] ready=%d identity_samples=%llu unique=%u failures=%llu overflow=%llu rejected_spans=%lld snapshots_not_leases=1 pointers_not_revoked=1 coverage=selected_generated_bulk_atomic_kernel_file_GPU143_split_join_clear_rows unobserved=other_runtime_units,arbitrary_escaped_dereferences,mirror_accesses,other_renderer_memcpy_le8\n",ready321,(unsigned long long)pubs,count,(unsigned long long)failures,(unsigned long long)overflow,(long long)rejected321);
 for(unsigned i=0;i<count;i++){
  Identity321*d=copy+i;
  fprintf(stderr,"[IDENTITY321] va=%08X bytes=%u allocation=%08X+%u generation=%llu native=%p backing=%p type=%lX snapshot=%u mapped=%u\n",d->va,d->bytes,d->base,d->size,(unsigned long long)d->generation,(void*)d->native,(void*)d->allocation_base,d->type,d->snapshot_ok,d->native_ok);
 }
 restore321(&saved);
}
