/*334 diagnostic only: CPU elapsed phases inside existing private-lane begin.
 * No GPU query, wait, content certificate, retry or changed renderer decision. */
#ifndef NIGHTFIRE_BEGIN_PROBE334_H
#define NIGHTFIRE_BEGIN_PROBE334_H
#include <errno.h>
#include <fenv.h>
#include <xmmintrin.h>
enum {NB334_VALIDATE,NB334_SHADER,NB334_TEXTURE,NB334_IMPORT,NB334_STATE,NB334_BIND,NB334_CLEANUP,NB334_COUNT};
typedef struct{DWORD error;int crt;fenv_t env;unsigned csr;} NBGuard334;
static void nb334_save(NBGuard334*g){g->error=GetLastError();g->crt=errno;fegetenv(&g->env);g->csr=_mm_getcsr();}
static void nb334_restore(const NBGuard334*g){fesetenv(&g->env);_mm_setcsr(g->csr);errno=g->crt;SetLastError(g->error);}
typedef struct{uint64_t start,last,ticks[NB334_COUNT];unsigned valid,phase;} NBScope334;
static NBScope334*nb334_current;
static struct{uint64_t ticks[NB334_COUNT],begins,failed,invalid,reads,total,hz;} nb334;
static int nb334_setting=-1;
static int nb334_enabled(void){
 if(nb334_setting<0){NBGuard334 g;nb334_save(&g);const char*v=getenv("DRIVING_BEGIN_PROBE334");nb334_setting=v&&!strcmp(v,"1");
  LARGE_INTEGER hz;if(nb334_setting&&QueryPerformanceFrequency(&hz)&&hz.QuadPart>0)nb334.hz=hz.QuadPart;
  nb334_restore(&g);
 }return nb334_setting&&nb334.hz;
}
#ifndef NB334_CLOCK
static uint64_t nb334_clock(void){LARGE_INTEGER t;nb334.reads++;return QueryPerformanceCounter(&t)&&t.QuadPart>0?(uint64_t)t.QuadPart:0;}
#define NB334_CLOCK() nb334_clock()
#endif
static NBScope334*nb334_enter(NBScope334*s,int admitted){
 NBScope334*old=nb334_current;nb334_current=NULL;s->valid=0;
 if(admitted&&nb334_enabled()){
  NBGuard334 g;nb334_save(&g);memset(s,0,sizeof*s);s->start=s->last=NB334_CLOCK();s->valid=s->start!=0;
  if(!s->valid)nb334.invalid++;nb334_current=s;nb334_restore(&g);
 }return old;
}
static void nb334_mark(unsigned phase){
 NBScope334*s=nb334_current;if(!s||!s->valid)return;
 NBGuard334 g;nb334_save(&g);uint64_t now=NB334_CLOCK();
 if(!now||now<s->last||phase>NB334_COUNT||s->phase>=NB334_COUNT){s->valid=0;nb334.invalid++;}
 else{s->ticks[s->phase]+=now-s->last;s->last=now;s->phase=phase;}
 nb334_restore(&g);
}
static void nb334_leave(NBScope334*s,NBScope334*old,int ok){
 if(nb334_current==s&&s->valid){
  nb334_mark(NB334_COUNT);
  if(s->valid){for(unsigned i=0;i<NB334_COUNT;i++)nb334.ticks[i]+=s->ticks[i];nb334.total+=s->last-s->start;nb334.begins++;nb334.failed+=!ok;}
 }
 nb334_current=old;
 if(s->valid&&(nb334.begins==1||nb334.begins%2048==0)){
  NBGuard334 g;nb334_save(&g);
  fprintf(stderr,"[BEGIN334] begins=%llu failed=%llu invalid=%llu clock_reads=%llu qpc_hz=%llu total_ticks=%llu validation_ticks=%llu shader_ticks=%llu textures_ticks=%llu import_ticks=%llu state_ticks=%llu bind_ticks=%llu cleanup_ticks=%llu cpu_elapsed_not_gpu=1 private_lane_only=1\n",
   (unsigned long long)nb334.begins,(unsigned long long)nb334.failed,(unsigned long long)nb334.invalid,(unsigned long long)nb334.reads,(unsigned long long)nb334.hz,(unsigned long long)nb334.total,
   (unsigned long long)nb334.ticks[0],(unsigned long long)nb334.ticks[1],(unsigned long long)nb334.ticks[2],(unsigned long long)nb334.ticks[3],(unsigned long long)nb334.ticks[4],(unsigned long long)nb334.ticks[5],(unsigned long long)nb334.ticks[6]);
  nb334_restore(&g);
 }
}
#endif
