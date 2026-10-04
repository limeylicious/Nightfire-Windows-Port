/*335 diagnostic only: CPU elapsed phases inside existing private-lane first import.
 * No GPU query, wait, content certificate, retry or changed renderer decision. */
#ifndef NIGHTFIRE_IMPORT_PROBE335_H
#define NIGHTFIRE_IMPORT_PROBE335_H
#include <errno.h>
#include <fenv.h>
#include <xmmintrin.h>
enum {NI335_PREPARE,NI335_COLOR,NI335_SCAN,NI335_SELECT,NI335_MAP,NI335_UNPACK,NI335_COPY,NI335_TAIL,NI335_COUNT};
typedef struct{DWORD error;int crt;fenv_t env;unsigned csr;} NIGuard335;
static void ni335_save(NIGuard335*g){g->error=GetLastError();g->crt=errno;fegetenv(&g->env);g->csr=_mm_getcsr();}
static void ni335_restore(const NIGuard335*g){fesetenv(&g->env);_mm_setcsr(g->csr);errno=g->crt;SetLastError(g->error);}
typedef struct{uint64_t start,last,ticks[NI335_COUNT];unsigned valid,phase;} NIScope335;
static NIScope335*ni335_current;
static struct{uint64_t ticks[NI335_COUNT],begins,failed,invalid,reads,total,hz;} ni335;
static int ni335_setting=-1;
static int ni335_enabled(void){
 if(ni335_setting<0){NIGuard335 g;ni335_save(&g);const char*v=getenv("DRIVING_IMPORT_PROBE335");ni335_setting=v&&!strcmp(v,"1");
  LARGE_INTEGER hz;if(ni335_setting&&QueryPerformanceFrequency(&hz)&&hz.QuadPart>0)ni335.hz=hz.QuadPart;
  ni335_restore(&g);
 }return ni335_setting&&ni335.hz;
}
#ifndef NI335_CLOCK
static uint64_t ni335_clock(void){LARGE_INTEGER t;ni335.reads++;return QueryPerformanceCounter(&t)&&t.QuadPart>0?(uint64_t)t.QuadPart:0;}
#define NI335_CLOCK() ni335_clock()
#endif
static NIScope335*ni335_enter(NIScope335*s,int admitted){
 NIScope335*old=ni335_current;ni335_current=NULL;s->valid=0;
 if(admitted&&ni335_enabled()){
  NIGuard335 g;ni335_save(&g);memset(s,0,sizeof*s);s->start=s->last=NI335_CLOCK();s->valid=s->start!=0;
  if(!s->valid)ni335.invalid++;ni335_current=s;ni335_restore(&g);
 }return old;
}
static void ni335_mark(unsigned phase){
 NIScope335*s=ni335_current;if(!s||!s->valid)return;
 NIGuard335 g;ni335_save(&g);uint64_t now=NI335_CLOCK();
 if(!now||now<s->last||phase>NI335_COUNT||s->phase>=NI335_COUNT){s->valid=0;ni335.invalid++;}
 else{s->ticks[s->phase]+=now-s->last;s->last=now;s->phase=phase;}
 ni335_restore(&g);
}
static void ni335_leave(NIScope335*s,NIScope335*old,int ok){
 if(ni335_current==s&&s->valid){
  ni335_mark(NI335_COUNT);
  if(s->valid){for(unsigned i=0;i<NI335_COUNT;i++)ni335.ticks[i]+=s->ticks[i];ni335.total+=s->last-s->start;ni335.begins++;ni335.failed+=!ok;}
 }
 ni335_current=old;
 if(s->valid&&(ni335.begins==1||ni335.begins%2048==0)){
  NIGuard335 g;ni335_save(&g);
  fprintf(stderr,"[IMPORT335] begins=%llu failed=%llu invalid=%llu clock_reads=%llu qpc_hz=%llu total_ticks=%llu prepare_ticks=%llu color_ticks=%llu scan_ticks=%llu selection_ticks=%llu map_ticks=%llu unpack_ticks=%llu copy_ticks=%llu tail_ticks=%llu cpu_elapsed_not_gpu=1 private_lane_only=1\n",
   (unsigned long long)ni335.begins,(unsigned long long)ni335.failed,(unsigned long long)ni335.invalid,(unsigned long long)ni335.reads,(unsigned long long)ni335.hz,(unsigned long long)ni335.total,
   (unsigned long long)ni335.ticks[0],(unsigned long long)ni335.ticks[1],(unsigned long long)ni335.ticks[2],(unsigned long long)ni335.ticks[3],(unsigned long long)ni335.ticks[4],(unsigned long long)ni335.ticks[5],(unsigned long long)ni335.ticks[6],(unsigned long long)ni335.ticks[7]);
  ni335_restore(&g);
 }
}
#endif
