/*464: default-OFF query-miss diagnostic, not an optimization or permission proof.
 * Per-thread totals; no allocations and no guest memory access. Ticks are wall
 * time inside the real VirtualQuery, not CPU cycles. Only report at445 cadence. */
#ifndef DRIVING_QUERY464_H
#define DRIVING_QUERY464_H
#include <intrin.h>
#include <fenv.h>
#include <errno.h>
typedef struct Query464Row {
 uintptr_t caller; uint64_t calls,hits,misses,ticks,failed,outside,no_entries,
     existing_entries,same_end,backward,forward,uncovered,bad_store;
} Query464Row;
static __declspec(thread) Query464Row query464_rows[32];
static __declspec(thread) uint64_t query464_overflow;
static volatile LONG query464_setting=-1;
static int query464_enabled(void){
 LONG mode=InterlockedCompareExchange(&query464_setting,-1,-1);
 if(mode<0){DWORD e=GetLastError();int c=errno;fenv_t env;fegetenv(&env);unsigned csr=_mm_getcsr();
  const char*v=getenv("DRIVING_QUERY464");LONG want=v&&!strcmp(v,"1");
  LONG old=InterlockedCompareExchange(&query464_setting,want,-1);mode=old<0?want:old;
  fesetenv(&env);_mm_setcsr(csr);errno=c;SetLastError(e);
 }return mode!=0;
}
static Query464Row*query464_begin(uintptr_t caller,int hit,uint64_t generation,uintptr_t address,const DR445Cache*c){
 if(!query464_enabled())return NULL;
 Query464Row*r=NULL;
 for(unsigned i=0;i<32;i++)if(!query464_rows[i].caller||query464_rows[i].caller==caller){r=&query464_rows[i];r->caller=caller;break;}
 if(!r){query464_overflow++;return NULL;}
 r->calls++;if(hit){r->hits++;return NULL;}r->misses++;
 unsigned current=0;uintptr_t low=UINTPTR_MAX,high=0;
 for(unsigned i=0;i<DR445_ENTRIES;i++)if(generation&&c->entries[i].generation==generation){current++;if(c->entries[i].begin<low)low=c->entries[i].begin;if(c->entries[i].end>high)high=c->entries[i].end;}
 if(!current)r->no_entries++;else{r->existing_entries++;if(address<low)r->backward++;else if(address>=high)r->forward++;else r->uncovered++;}
 return r;
}
static uint64_t query464_tick(Query464Row*r){if(!r)return 0;DWORD e=GetLastError();LARGE_INTEGER t;QueryPerformanceCounter(&t);SetLastError(e);return(uint64_t)t.QuadPart;}
static void query464_end(Query464Row*r,uint64_t start,SIZE_T result,const MEMORY_BASIC_INFORMATION*m,uint64_t generation,const DR445Cache*c,uintptr_t address){
 if(!r)return;r->ticks+=query464_tick(r)-start;
 if(result!=sizeof *m){r->failed++;return;}
 uintptr_t lo=0,hi=0;int inside=0;
 for(int i=0;i<2;i++)if(xbox_Region445Window(i,&lo,&hi)&&address>=lo&&address<hi){inside=1;break;}
 if(!inside){r->outside++;return;}
 uintptr_t b=(uintptr_t)m->BaseAddress;
 if(m->State!=MEM_COMMIT||b<lo||m->RegionSize>UINTPTR_MAX-b||b+m->RegionSize>hi)r->bad_store++;
 for(unsigned i=0;i<DR445_ENTRIES;i++)if(generation&&c->entries[i].generation==generation&&c->entries[i].end==b+m->RegionSize){r->same_end++;break;}
}
static void query464_report(void){
 if(!query464_enabled())return;DWORD e=GetLastError();int crt=errno;fenv_t env;fegetenv(&env);unsigned csr=_mm_getcsr();
 LARGE_INTEGER f;QueryPerformanceFrequency(&f);
 for(unsigned i=0;i<32;i++){Query464Row*r=&query464_rows[i];if(!r->caller)continue;
  fprintf(stderr,"[QUERY464] tid=%lu caller=%p calls=%llu hits=%llu misses=%llu query_ticks=%llu qpc_frequency=%llu failed=%llu outside=%llu no_entries=%llu existing_entries=%llu backward=%llu forward=%llu uncovered=%llu same_end=%llu bad_store=%llu overflow=%llu\n",(unsigned long)GetCurrentThreadId(),(void*)r->caller,r->calls,r->hits,r->misses,r->ticks,(uint64_t)f.QuadPart,r->failed,r->outside,r->no_entries,r->existing_entries,r->backward,r->forward,r->uncovered,r->same_end,r->bad_store,query464_overflow);
 }
 fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(e);
}
#endif
