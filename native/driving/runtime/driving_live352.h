/*352 SOURCE CANDIDATE. A single transaction owns an original BEGIN..END.
 * Font348/movie201 retain priority.352 precedes350/214 only when explicitly
 * enabled; old paths remain untouched when disabled. No retained target. */
#ifndef DRIVING_LIVE352_H
#define DRIVING_LIVE352_H
#include "driving_input352.h"
#include "driving_submit352.h"
static InputPacket352 renderer_packet352;
static uint32_t renderer_state352[2048];
static unsigned char renderer_known352[2048];
static NFVertexProgram renderer_program352;
static unsigned renderer_draws352,renderer_replays352;
static void renderer_emit352(void *context,unsigned method,uint32_t value)
{(void)context;nv2a_pb_exec_method(0,method,value);}
static void renderer_barrier352(void)
{
 if(renderer_packet352.phase==INPUT352_IDLE)return;
 /* Execute original fallback outside candidate FP/error guard, preserving
  * its own effects. An incomplete prefix may contain no END: don't invent one. */
 residency_flush236("renderer352-replay");renderer_replays352++;
 if(renderer_replays352<=8){GeometryHost350 host=geometry_host350();
  fprintf(stderr,"[RENDERER352] replay=%u phase=%u reason=%u commands=%u kind=%u vertices=%u shader_pc=%u shader_constant=%u original_prefix=1\n",
   renderer_replays352,renderer_packet352.phase,renderer_packet352.reason,
   renderer_packet352.commands,renderer_packet352.kind,renderer_packet352.count,
   renderer_program352.error_pc,renderer_program352.error_constant);
  geometry_host_restore350(&host);}
 input_replay352(&renderer_packet352,renderer_emit352,NULL);
}
static void renderer_before352(unsigned method,uint32_t value)
{
 if(renderer_packet352.phase!=INPUT352_IDLE&&!input_payload352(method,value))
  renderer_barrier352();
}
static int renderer_intercept352(GPUObject143 *object,unsigned method,uint32_t value)
{
 if(!renderer_enabled352()||enabled201<=0)return 0;
 GeometryHost350 host=geometry_host350();
 if(renderer_packet352.phase==INPUT352_IDLE){
  unsigned profile;
  if(method!=0x17fc||!value||collector214.phase!=DC214_IDLE||
     geometry_collect350.phase!=GC350_IDLE||
     !renderer_plan352(object->state,object->written167,&live201.program,value,&profile)){
   geometry_host_restore350(&host);return 0;
  }
  geometry_host_restore350(&host);residency_flush236("renderer352-begin");
  host=geometry_host350();
  memcpy(renderer_state352,object->state,sizeof renderer_state352);
  memcpy(renderer_known352,object->written167,sizeof renderer_known352);
  renderer_program352=live201.program;
  int result=input_begin352(&renderer_packet352,value,renderer_state352,renderer_known352);
  geometry_host_restore350(&host);return result;
 }
 unsigned result=input_feed352(&renderer_packet352,method,value);
 if(result==INPUT352_HOLD){geometry_host_restore350(&host);return 1;}
 if(result==INPUT352_READY){
  int accepted=geometry_submit352_impl(renderer_state352,renderer_known352,&renderer_program352,&renderer_packet352);
  if(accepted){
   scene_accept240(4,renderer_state352[0x208/4]);
   unsigned n=++renderer_draws352;
   if(n<=8||!(n%120))fprintf(stderr,
    "[RENDERER352] draws=%u primitive=%u input_kind=%u vertices=%u replays=%u two_lane=1 cpu_reference=1 unvalidated_candidate=1\n",
    n,renderer_packet352.primitive,renderer_packet352.kind,renderer_packet352.count,renderer_replays352);
   input_reset352(&renderer_packet352);geometry_host_restore350(&host);return 1;
  }
  geometry_host_restore350(&host);renderer_barrier352();return 1;
 }
 geometry_host_restore350(&host);
 if(result==INPUT352_BEFORE)renderer_barrier352();
 return 0;
}
#endif
