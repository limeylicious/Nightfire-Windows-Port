/*399 Diagnostic only. CPU elapsed within one original pair transaction.
 * No queries, copies, waits, renderer decisions or publication changes.
 * Serialized by the existing caller. Not an ownership certificate. */
#ifndef NIGHTFIRE_PAIR_COST399_H
#define NIGHTFIRE_PAIR_COST399_H
#include <errno.h>
#include <fenv.h>
#include <xmmintrin.h>
enum {PC399_PREFLIGHT,PC399_SELECT,PC399_BEGIN,PC399_SHADER,PC399_TEXTURE,
 PC399_IMPORT,PC399_COLOR,PC399_SCAN,PC399_DEPTH_SELECT,PC399_WRITE_MAP,
 PC399_UNPACK,PC399_UPLOAD,PC399_STATE,PC399_BIND,PC399_BEGIN_CLEANUP,
 PC399_DRAW,PC399_COPY_FLUSH,PC399_READ0,PC399_READ1,PC399_READ2,PC399_READ3,
 PC399_ROUND,PC399_PACK,PC399_CLEANUP,PC399_COUNT};
static const char*pc_names399[PC399_COUNT]={"preflight","select","begin_validation","shader","textures",
 "import_prepare","color_upload","depth_scan","depth_select","depth_write_map","depth_unpack","depth_upload",
 "state","bind","begin_cleanup","draw","copies_flush","read_map0","read_map1","read_map2","read_map3",
 "round_check","pack","cleanup"};
typedef struct{DWORD error;int crt;fenv_t env;unsigned csr;} PCGuard399;
static void pc_save399(PCGuard399*g){g->error=GetLastError();g->crt=errno;g->csr=_mm_getcsr();fegetenv(&g->env);}
static void pc_restore399(const PCGuard399*g){fesetenv(&g->env);_mm_setcsr(g->csr);errno=g->crt;SetLastError(g->error);}
static struct {uint64_t ticks[2][PC399_COUNT],count[2],failed[2],invalid,reads,hz,reported,report_ticks;
 uint64_t start,last,local[PC399_COUNT];unsigned active,valid,phase,profile,selected;} pc399;
static int pc_setting399=-1;
#ifndef PC399_CLOCK
static uint64_t pc_clock399(void){LARGE_INTEGER t;pc399.reads++;return QueryPerformanceCounter(&t)&&t.QuadPart>0?(uint64_t)t.QuadPart:0;}
#define PC399_CLOCK() pc_clock399()
#endif
static int pc_enabled399(void){
 if(pc_setting399<0){PCGuard399 g;pc_save399(&g);const char*v=getenv("DRIVING_SPRITE_COST399");pc_setting399=v&&!strcmp(v,"1");
  LARGE_INTEGER hz;if(pc_setting399&&QueryPerformanceFrequency(&hz)&&hz.QuadPart>0)pc399.hz=hz.QuadPart;
  pc_restore399(&g);
 }return pc_setting399&&pc399.hz;
}
void nf_hw_pair_profile399(unsigned profile){pc399.selected=profile;}
static void pc_enter399(void){
 unsigned selected=pc399.selected;pc399.selected=0;pc399.active=pc399.valid=0;
 if((selected!=25&&selected!=26)||!pc_enabled399())return;
 PCGuard399 g;pc_save399(&g);memset(pc399.local,0,sizeof pc399.local);
 pc399.profile=selected-25;pc399.phase=PC399_PREFLIGHT;
 pc399.start=pc399.last=PC399_CLOCK();pc399.valid=pc399.start!=0;pc399.active=1;
 if(!pc399.valid)pc399.invalid++;pc_restore399(&g);
}
static void pc_mark399(unsigned phase){
 if(!pc399.active||!pc399.valid)return;PCGuard399 g;pc_save399(&g);uint64_t now=PC399_CLOCK();
 if(!now||now<pc399.last||pc399.phase>=PC399_COUNT||phase>PC399_COUNT){pc399.valid=0;pc399.invalid++;}
 else{pc399.local[pc399.phase]+=now-pc399.last;pc399.last=now;pc399.phase=phase;}
 pc_restore399(&g);
}
static void pc_leave399(int result){
 if(pc399.active&&pc399.valid){pc_mark399(PC399_COUNT);if(pc399.valid){
  unsigned k=pc399.profile;pc399.count[k]++;pc399.failed[k]+=result!=1;
  for(unsigned i=0;i<PC399_COUNT;i++)pc399.ticks[k][i]+=pc399.local[i];
 }}pc399.active=pc399.valid=0;
}
void nf_hw_pair_report399(void){
 if(!pc_enabled399())return;
 uint64_t total=pc399.count[0]+pc399.count[1];if(total-pc399.reported<128)return;
 PCGuard399 g;pc_save399(&g);uint64_t start=PC399_CLOCK();
 for(unsigned k=0;k<2;k++)if(pc399.count[k]){
  fprintf(stderr,"[SPRITECOST399] profile=%u count=%llu failed=%llu invalid=%llu reads=%llu hz=%llu previous_report_ticks=%llu",k+25,
   (unsigned long long)pc399.count[k],(unsigned long long)pc399.failed[k],(unsigned long long)pc399.invalid,
   (unsigned long long)pc399.reads,(unsigned long long)pc399.hz,(unsigned long long)pc399.report_ticks);
  for(unsigned i=0;i<PC399_COUNT;i++)fprintf(stderr," %s=%llu",pc_names399[i],(unsigned long long)pc399.ticks[k][i]);
  fputs(" cpu_elapsed_not_gpu=1 original_pair=1\n",stderr);
 }
 pc399.reported=total;uint64_t end=PC399_CLOCK();if(start&&end>=start)pc399.report_ticks+=end-start;pc_restore399(&g);
}
#endif
