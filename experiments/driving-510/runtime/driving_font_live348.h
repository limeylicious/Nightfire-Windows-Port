#include "driving_diag510.h" /*510 private default-OFF diagnostics*/
/*348 Default-OFF, original inline glyph draw. No synthetic text or UI state.
 * Held commands never cross another original method or object boundary. */
#ifndef FONT348_LIVE_TEST
#include "driving_font_submit348.h"
#endif
#include <fenv.h>
#include <errno.h>
#include <xmmintrin.h>
static Inline223 font_collect348;
static uint64_t font_attempt510;
static uint32_t font_state348[2048];
static unsigned char font_known_state348[2048];
static NFVertexProgram font_program348;
static int font_mode348=-1;
static unsigned font_draws348,font_refused348;
static int font_enabled348(void){
 if(font_mode348<0){DWORD error=GetLastError();int crt=errno;
  const char*v=getenv("DRIVING_FONT348");font_mode348=v&&!strcmp(v,"1");
  errno=crt;SetLastError(error);}
 return font_mode348;
}
static void font_barrier348(void){
 if(!font_collect348.active)return;
 residency_flush236("font348-replay");
 uint64_t old510=driving_diag510_override(font_attempt510);driving_diag510_route(P510_FONT,4,0,font_collect348.count);
 for(unsigned i=0;i<font_collect348.commands;i++)nv2a_pb_exec_method(0,font_collect348.prefix[i][0],font_collect348.prefix[i][1]);
 driving_diag510_override(old510);
 font_collect348.active=font_collect348.commands=font_collect348.count=0;
}
static void font_before348(unsigned method,uint32_t value){
 if(!font_collect348.active)return;
 if((method==0x17fc&&!value)||method==0x194c||method==0x18c8||method==0x18cc||
    (method>=0x1518&&method<=0x1524&&!(method&3)))return;
 font_barrier348();
}
static int font_intercept348(GPUObject143*object,unsigned method,uint32_t value){
 if(!font_enabled348()||enabled201<=0)return 0;
 if(!font_collect348.active){
  if(method!=0x17fc||value!=8)return 0;
  int plan510=font_plan348(object->state,object->written167,&live201.program);driving_diag510_route(P510_FONT,1,plan510,0);if(!plan510)return 0;
  font_attempt510=driving_diag510_attempt();
  memcpy(font_state348,object->state,sizeof font_state348);memcpy(font_known_state348,object->written167,sizeof font_known_state348);font_program348=live201.program;
  return inline223_begin(&font_collect348,live201.current.attributes,live201.masks);
 }
 fenv_t env;fegetenv(&env);unsigned csr=_mm_getcsr();DWORD error=GetLastError();int crt=errno;
 int result=inline223_feed(&font_collect348,method,value);
 fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(error);
 if(result==0)return 1;
 if(result==1){
  if(font_submit348(font_state348,font_known_state348,&font_program348,&font_collect348)){
   driving_diag510_route(P510_FONT,3,1,font_collect348.count);
   error=GetLastError();crt=errno;
   unsigned n=++font_draws348;if(n<=4||!(n%120))fprintf(stderr,"[FONT348] original_draws=%u vertices=%u target=%08X RGB_mask=%08X synchronous_lanes=2\n",n,font_collect348.count,font_state348[0x210/4],font_state348[0x358/4]);
   errno=crt;SetLastError(error);font_collect348.commands=font_collect348.count=0;return 1;
  }
  /* Collector includes END on a complete-but-refused batch. Replay it once. */
  driving_diag510_route(P510_FONT,5,1,font_collect348.count);
  font_collect348.active=1;font_refused348++;font_barrier348();return 1;
 }
 /* Rejected current method was never retained. Caller forwards it once. */
 driving_diag510_route(P510_FONT,5,2,font_collect348.count);
 font_refused348++;font_barrier348();return 0;
}
