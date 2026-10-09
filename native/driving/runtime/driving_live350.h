/*350 UNTESTED integration. New commands are synchronous owned transactions.
 * Neither residency234 nor owned-tail295 is used for these replacement draws.
 * Every partial prefix drains before GET, object changes or producer return. */
#ifndef DRIVING_LIVE350_H
#define DRIVING_LIVE350_H
#include "driving_collect350.h"
#include "driving_submit350.h"
#include <fenv.h>
#include <errno.h>
#include <xmmintrin.h>
static GeometryCollector350 geometry_collect350;
static uint32_t geometry_state350[2048];
static unsigned char geometry_known350[2048];
static NFVertexProgram geometry_program350;
static unsigned geometry_draws350,geometry_refusals350,geometry_prefixes350;
typedef struct GeometryHost350 {fenv_t env;unsigned csr;DWORD error;int crt;} GeometryHost350;
static GeometryHost350 geometry_host350(void)
{
 GeometryHost350 s;s.error=GetLastError();s.crt=errno;s.csr=_mm_getcsr();fegetenv(&s.env);return s;
}
static void geometry_host_restore350(const GeometryHost350 *s)
{fesetenv(&s->env);_mm_setcsr(s->csr);errno=s->crt;SetLastError(s->error);}
/*403 Default-OFF, only provably idle no-op callbacks skip the host guard.
 * The first environment lookup remains guarded; no original command is skipped. */
static int idle403_setting=-1;
static int idle403_enabled(void){
 if(idle403_setting<0){GeometryHost350 host=geometry_host350();
  const char *v=getenv("DRIVING_IDLE_GUARD403");idle403_setting=v&&!strcmp(v,"1");
  geometry_host_restore350(&host);}
 return idle403_setting;
}
static void geometry_emit350(void *ctx,unsigned method,uint32_t value)
{(void)ctx;nv2a_pb_exec_method(0,method,value);}
static void geometry_barrier350(void)
{
 if(geometry_collect350.phase==GC350_IDLE)return;
 /* Replay executes OUTSIDE the adapter's FP/error guard. Its original effects
  * must remain visible, including meaningful baseline failures/diagnostics. */
 residency_flush236("geometry350-replay");geometry_prefixes350++;
 geometry_replay350(&geometry_collect350,geometry_emit350,NULL);
}
static void geometry_before350(unsigned method,uint32_t value)
{
 if(geometry_collect350.phase==GC350_IDLE||geometry_payload350(method,value))return;
 geometry_collect350.reason=GC350_BARRIER;geometry_barrier350();
}
static int geometry_intercept350(GPUObject143 *object,unsigned method,uint32_t value)
{
 if(!geometry_enabled350()||enabled201<=0)return 0;
 if(geometry_collect350.phase==GC350_IDLE&&(method!=0x17fc||!value)&&idle403_enabled())return 0;
 GeometryHost350 host=geometry_host350();
 if(geometry_collect350.phase==GC350_IDLE){
  unsigned profile;
  if(method!=0x17fc||!value||
     !geometry_plan350(object->state,object->written167,&live201.program,value,&profile)){
   geometry_host_restore350(&host);return 0;
  }
  /* Existing residency_command236 has already flushed an unadmitted BEGIN.
   * Keep an explicit barrier too; no new family inherits an older live batch. */
  geometry_host_restore350(&host);
  residency_flush236("geometry350-begin");
  host=geometry_host350();
  memcpy(geometry_state350,object->state,sizeof geometry_state350);
  memcpy(geometry_known350,object->written167,sizeof geometry_known350);
  geometry_program350=live201.program;
  int held=geometry_begin350(&geometry_collect350,value);
  geometry_host_restore350(&host);return held;
 }
 unsigned result=geometry_feed350(&geometry_collect350,method,value);
 if(result==GC350_HOLD){geometry_host_restore350(&host);return 1;}
 if(result==GC350_READY){
  DrivingTopology350 topology;
  int accepted=geometry_result350(&geometry_collect350,&topology)&&
   geometry_submit350_impl(geometry_state350,geometry_known350,&geometry_program350,
       geometry_collect350.primitive,geometry_collect350.indices,geometry_collect350.count);
  if(accepted){
   unsigned n=++geometry_draws350;
   if(n<=8||!(n%120))fprintf(stderr,
    "[GEOMETRY350] original_draws=%u primitive=%u vertices=%u triangles=%u target=%08X synchronous_lanes=2 unvalidated_candidate=1\n",
    n,topology.primitive,topology.count,topology.dense/3,geometry_state350[0x210/4]);
   geometry_reset350(&geometry_collect350);geometry_host_restore350(&host);return 1;
  }
  geometry_refusals350++;geometry_host_restore350(&host);
  /* END is retained on this path, so consume it after exact prefix replay. */
  geometry_barrier350();return 1;
 }
 if(result==GC350_BEFORE_CURRENT){
  geometry_refusals350++;geometry_host_restore350(&host);
  geometry_barrier350();return 0;
 }
 geometry_host_restore350(&host);return 0;
}
#endif
