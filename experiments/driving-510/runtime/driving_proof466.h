/*466 packet-only finite-proof candidate. Original selectors, callbacks and
 * generated shader cores remain unchanged. Only a copied native247 wrapper
 * whose existing full-output scan succeeds may return private status2.
 * All dynamic interpreter/ARL/exception fallbacks retain status0/1 and need
 * the unchanged packet outer check. No content cache or ownership change. */
#ifndef DRIVING_PROOF466_H
#define DRIVING_PROOF466_H
#include <fenv.h>
#include <errno.h>
static volatile LONG proof466_setting=-1;
static int proof466_enabled(void){
 LONG setting=InterlockedCompareExchange(&proof466_setting,-1,-1);
 if(setting<0){DWORD error=GetLastError();int crt=errno;fenv_t env;fegetenv(&env);unsigned csr=_mm_getcsr();
  const char*v=getenv("DRIVING_PROOF466");LONG want=v&&!strcmp(v,"1");LONG old=InterlockedCompareExchange(&proof466_setting,want,-1);setting=old<0?want:old;
  fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(error);
 }return setting!=0;
}
static uint64_t proof466_calls,proof466_certified,proof466_replayed;
static int driving_run42_466(NFVertexProgram *s,const float in[16][4],float out[16][4]){
 int ok=driving_core42_247(s,in,out);
 if(ok&&!driving_finite328(out))ok=0; /*466 exact finite classification*/
 /* Precise compiler mode can choose different NaN payloads after constant propagation. Reexecute only exceptional results using the unchanged oracle. Static cohorts have no numeric early branch: cache writes follow the same instruction prefix and are idempotent. */
 if(!ok){driving_exception_replays247++;return nf_vp_run(s,in,out);}return 2; /*466 proves ALL outputs finite for this invocation*/
}
static int driving_run48_466(NFVertexProgram *s,const float in[16][4],float out[16][4]){
 int ok=driving_core48_247(s,in,out);
 if(ok&&!driving_finite328(out))ok=0; /*466 exact finite classification*/
 /* Precise compiler mode can choose different NaN payloads after constant propagation. Reexecute only exceptional results using the unchanged oracle. Static cohorts have no numeric early branch: cache writes follow the same instruction prefix and are idempotent. */
 if(!ok){driving_exception_replays247++;return nf_vp_run(s,in,out);}return 2; /*466 proves ALL outputs finite for this invocation*/
}
static int driving_run66_466(NFVertexProgram *s,const float in[16][4],float out[16][4]){
 int ok=driving_core66_247(s,in,out);
 if(ok&&!driving_finite328(out))ok=0; /*466 exact finite classification*/
 /* Precise compiler mode can choose different NaN payloads after constant propagation. Reexecute only exceptional results using the unchanged oracle. Static cohorts have no numeric early branch: cache writes follow the same instruction prefix and are idempotent. */
 if(!ok){driving_exception_replays247++;return nf_vp_run(s,in,out);}return 2; /*466 proves ALL outputs finite for this invocation*/
}
static int driving_run55_466(NFVertexProgram*s,const float in[16][4],float out[16][4]){
 uint32_t bits;memcpy(&bits,&in[6][0],4);uint32_t magnitude=bits&0x7fffffffu;
 /*Integer-only domain check: retain oracle ARL/FP ordering for nonfinite, out-of-range and subnormal sources before any state/cache write. Normal finite -256<=x<256 guarantees pc0 ARL succeeds; subsequent failures are source-validity/final-output failures with the same cache prefix.*/
 if((magnitude&&magnitude<0x00800000u)||magnitude>0x43800000u||(magnitude==0x43800000u&&!(bits>>31))){driving_arl_fallback277++;return nf_vp_run(s,in,out);}
 int ok=driving_core55_247(s,in,out);if(ok&&!driving_finite328(out))ok=0; /*466 exact finite classification*/
 if(!ok){driving_exception_replays247++;return nf_vp_run(s,in,out);}return 2; /*466 proves ALL outputs finite for this invocation*/}
static int driving_run52_466(NFVertexProgram*s,const float in[16][4],float out[16][4]){
 uint32_t bits;memcpy(&bits,&in[4][0],4);uint32_t magnitude=bits&0x7fffffffu;
 /*Integer-only domain check: retain oracle ARL/FP ordering for nonfinite, out-of-range and subnormal sources before any state/cache write. Normal finite -256<=x<256 guarantees pc0 ARL succeeds; subsequent failures are source-validity/final-output failures with the same cache prefix.*/
 if((magnitude&&magnitude<0x00800000u)||magnitude>0x43800000u||(magnitude==0x43800000u&&!(bits>>31))){driving_arl_fallback277++;return nf_vp_run(s,in,out);}
 int ok=driving_core52_247(s,in,out);if(ok&&!driving_finite328(out))ok=0; /*466 exact finite classification*/
 if(!ok){driving_exception_replays247++;return nf_vp_run(s,in,out);}return 2; /*466 proves ALL outputs finite for this invocation*/}
#define PROOF_WRAPPER466(N,I) \
static int driving_native##N##_call466(NFVertexProgram*s,const float in[16][4],float out[16][4]){ \
 driving_stats247.native_transforms++;driving_stats247.cohort_transforms[I]++; \
 int ok; \
 if(!driving_stats247.timing)ok=driving_run##N##_466(s,in,out); \
 else{LARGE_INTEGER a,b;QueryPerformanceCounter(&a);ok=driving_run##N##_466(s,in,out);QueryPerformanceCounter(&b);driving_stats247.native_ticks+=b.QuadPart-a.QuadPart;} \
 proof466_calls++;if(ok==2)proof466_certified++;else proof466_replayed++;return ok; \
}
PROOF_WRAPPER466(42,0)
PROOF_WRAPPER466(48,1)
PROOF_WRAPPER466(66,2)
PROOF_WRAPPER466(55,3)
PROOF_WRAPPER466(52,4)
#undef PROOF_WRAPPER466
static DrivingVertex247 driving_proof466_select(DrivingVertex247 f){
 /*Only the exact callbacks returned by the unchanged binder are recognized.
  * Profile-instrumented and other-family callbacks remain entirely original.*/
 if(f==driving_native42_call247)return driving_native42_call466;
 if(f==driving_native48_call247)return driving_native48_call466;
 if(f==driving_native66_call247)return driving_native66_call466;
 if(f==driving_native55_call247)return driving_native55_call466;
 if(f==driving_native52_call247)return driving_native52_call466;
 return f;
}
#endif
