#include "driving_barrier242.h"
#include "driving_begin280.h"
#include <errno.h>
#include <fenv.h>
#include <xmmintrin.h>
#include "driving_observe454.h"
/* Experimental drain-local ownership contract in residency235-contract.md.
 * Included after live214 so collector/program state is visible. All APIs are
 * consumer-thread only; no context work is dispatched from another thread. */
static unsigned residency_depth236;
static int residency_setting236=-1;
static int residency_enabled236(void){
#ifdef DRIVING_BATCH236
 if(residency_setting236<0){const char *v=getenv("DRIVING_BATCH236");residency_setting236=v&&!strcmp(v,"1");}
 return residency_setting236&&residency_depth236==1;
#else
 return 0;
#endif
}
/*310: exact original software selector zero is consumed by flip204 with no
 * queue/callback operation. Keep the command and all state bookkeeping. */
extern int driving_flip204_enabled(void);
static int residency_setting310=-1;
static uint64_t residency_avoided310;
static int residency_nop310(unsigned method,uint32_t value){
 if(residency_setting310==0||method!=0x100||value||!batch234.count||!batch234.targets||
    batch234.spans[0].address!=0x82cbc000u||batch234.spans[1].address!=0x8316c000u||
    residency_depth236!=1||held201||collector214.phase!=DC214_IDLE||
    collector214.method_count||collector214.index_count||!residency_enabled236())return 0;
 DWORD saved=GetLastError();int saved_errno=errno;fenv_t env;unsigned mxcsr=_mm_getcsr();fegetenv(&env);
 if(residency_setting310<0){const char*v=getenv("DRIVING_NOP310");residency_setting310=v&&!strcmp(v,"1");}
 int accepted=residency_setting310&&driving_flip204_enabled();
 fesetenv(&env);_mm_setcsr(mxcsr);errno=saved_errno;SetLastError(saved);return accepted;
}
static void residency_note310(void){
 ++residency_avoided310;
 if(residency_avoided310!=1&&(residency_avoided310&255))return;
 DWORD saved=GetLastError();int saved_errno=errno;fenv_t env;unsigned mxcsr=_mm_getcsr();fegetenv(&env);
 fprintf(stderr,"[NOP310] avoided_precommand_barriers=%llu exact=0100:00000000 publication_barriers_unchanged=1\n",(unsigned long long)residency_avoided310);
 fesetenv(&env);_mm_setcsr(mxcsr);errno=saved_errno;SetLastError(saved);
}
static void residency_flush236(const char *reason){
 if(batch234.count||pc450_dirty())driving_batch234_flush(reason); /*450: an open session is also unpublished*/
}
static void residency_read236(uint32_t va,size_t bytes){
 if(!batch234.count&&!pc450_dirty())return;
 /* The command walker supplies physical offsets. These are mapped to the
  * actual contiguous view, just as read143 does, not to unrelated low RAM. */
 if(va<0x08000000u)va+=0x80000000u;
 if(driving_batch234_read_overlap(va,bytes))residency_flush236("source-alias");
}
static void residency_enter236(void){
 if(residency_depth236&&residency_enabled236())fail143("batch236 reentrant consumer",residency_depth236,0);
 residency_depth236++;
}
static void residency_leave236(void){
 residency_flush236("drain-exit");pc451_drain_leave();driving_batch236_report();if(residency_depth236)residency_depth236--; /*451: protect if still resident*/
}
/* Audited register payloads only. These exact ranges are arrays of combiner
 * words, transform program/constants, or vertex-array descriptors, not broad
 * numeric ranges of unknown methods. Surface/DMA target changes always flush.
 * Definitions: pinned xboxrecomp/src/nv2a/nv2a_regs.h; execution: nf_vp_method,
 * driving_immediate195 and nv2a_pb_exec_method. */
static int residency_pure236(unsigned m){
 if(m&3)return 0;
 if((m>=0x260&&m<0x280)||(m>=0xa60&&m<0xae0)||
    (m>=0xb00&&m<0xc00)||(m>=0x1720&&m<0x17a0)||
    (m>=0x1e40&&m<0x1e60))return 1;
 if(m>=0x1b00&&m<0x1c00){unsigned k=(m-0x1b00)&63;
  return k==0||k==4||k==8||k==12||k==16||k==20||k==28||k==32||k==36||k==40||k==44||k==48||k==52||k==56||k==60;
 }
 switch(m){
 case 0x288:case 0x28c:case 0x290:case 0x294:case 0x298:
 case 0x29c:case 0x2a0:case 0x2a4:case 0x2a8:
 case 0x300:case 0x304:case 0x308:case 0x30c:case 0x33c:case 0x340:
 case 0x344:case 0x348:case 0x34c:case 0x350:case 0x354:case 0x358:case 0x35c:
 /*237: named raster/lighting/combiner payloads; each admitted draw still
  * snapshots its exact state. Source audit: pure-state-audit237.md. */
 case 0x314:case 0x318:case 0x31c:case 0x37c:case 0x394:case 0x398:
 case 0x3b8:case 0x43c:case 0x17f8:
 case 0x181c:case 0x1820:case 0x1824:case 0x1e74:case 0x1e78:
 case 0x39c:case 0x3a0:case 0x9c0:case 0x9c4:case 0x9c8:
 case 0xa20:case 0xa24:case 0xa28:case 0xa2c:
 case 0xaf0:case 0xaf4:case 0xaf8:case 0xafc:
 case 0x1e20:case 0x1e24:case 0x1e60:case 0x1e6c:case 0x1e70:
 case 0x1e94:case 0x1e98:case 0x1e9c:case 0x1ea0:case 0x1ea4:return 1;
 default:return 0;
 }
}
/* PAL lighting producer00171160 writes17C4 from the two-sided state17585C.
 * Its zero payload is the only observed/admitted form; retain the barrier for
 * every other value. Vertex-buffer validation1710 is separately justified
 * by vertex-validation239/README.md: owned inputs, no completion side effect. */
static int residency_payload237(unsigned method,uint32_t value){
 return residency_pure236(method)||((method==0x17c4||method==0x1710)&&value==0)||residency_nop310(method,value);
}
/* Keep a second guard at the final generic executor call: an allowed collector
 * command can reach this point only after interceptors decline it. END and
 * draw-data commands must then publish the prefix before generic execution. */
static int pc450_method(unsigned method);
static void residency_generic237(unsigned method,uint32_t value){
 if(!residency_payload237(method,value)&&!pc450_method(method))residency_flush236("generic-executor"); /*450: clear parameters stay plain state*/
}
/*450 PC mode: clear parameters are plain state; the clear itself (1D94) is
 * handled by pc450_clear143 in driving_gpu143.c before any other interception;
 * semaphore methods do not read the pair, so they no longer publish it. */
static int pc450_method(unsigned method){
 return pc450_enabled()&&(method==0x1d8c||method==0x1d90||method==0x1d94||method==0x1d98||method==0x1d9c||
  method==0x1d6c||method==0x1d70||method==0x1a4);
}
static void residency_command236(GPUObject143 *object,unsigned method,uint32_t value){
 if(!batch234.count&&!pc450_dirty())return;
 if(object->cls!=0x97){residency_flush236("non-Kelvin");return;}
 if(method==0x17fc&&value){unsigned profile;
  if(!held201&&enabled221>0&&driving_plan221(object->state,object->written167,&live201.program,value,&profile))return;
  driving_begin280(object->state,object->written167,&live201.program,value,batch234.target_fields,batch234.spans,batch234.count,held201,enabled221);
  Token454 observed=observe_before454(object->state,object->written167,&live201.program,value,batch234.count,held201,enabled221);
  residency_flush236("unadmitted-BEGIN");observe_after454(observed);return;
 }
 if((method==0x17fc&&!value)||method==0x1800||method==0x1808||method==0x1810){
  if(collector214.phase==DC214_ACTIVE&&family214==4)return;
  residency_flush236("uncollected-draw");return;
 }
 if(object->cls==0x97&&pc450_method(method))return;
 if(!residency_payload237(method,value)){driving_barrier242(method,value);residency_flush236("method-barrier");}
 else if(method==0x100&&value==0)residency_note310();
}
static int submit_residency236(const uint32_t *state,const unsigned char *known,NFVertexProgram *program,unsigned primitive,const uint32_t *indices,unsigned count){
 if(!residency_enabled236()){residency_flush236("disabled");return submit221(state,known,program,primitive,indices,count);}
 int ok=submit234(state,known,program,primitive,indices,count);
#ifdef DRIVING_PRODUCER235
 /* Request only: actual original-producer observation occurs after the drain
  * returns. Preserve235's opt-in producer request without reading target RAM. */
 unsigned profile;if(ok&&count==512&&driving_plan221(state,known,program,primitive,&profile)&&profile==7)driving_producer235_request();
#endif
 return ok;
}
