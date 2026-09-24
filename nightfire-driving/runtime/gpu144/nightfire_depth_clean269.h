/* Private248 lane-local proof only; never changes public boundary admission. */
#ifndef NIGHTFIRE_DEPTH_CLEAN269_H
#define NIGHTFIRE_DEPTH_CLEAN269_H
#ifdef NF_DEPTH_SRV256_AVAILABLE
#define NF_DEPTH_CLEAN269_AVAILABLE 1
typedef struct NFDepthClean269 {
 ID3D11Texture2D *target;unsigned canonical,in_draw;
 uint64_t expected_draws,expected_clears,expected_transfers;
} NFDepthClean269;
static NFDepthClean269 *depth269_current;
static uint64_t depth269_skipped,depth269_quantized,depth269_imports,depth269_invalidations;
static int depth269_enabled(void){static int on=-1;if(on<0){DWORD error=GetLastError();const char*v=getenv("DRIVING_DEPTH_CLEAN269");on=v&&!strcmp(v,"1");SetLastError(error);}return on;}
static void depth269_snapshot(NFDepthClean269*p){p->expected_draws=draws;p->expected_clears=gpu_depth_clears;p->expected_transfers=transfers;}
static void depth269_lane(NFDepthClean269*p,ID3D11Texture2D*t){memset(p,0,sizeof*p);p->target=t;depth269_snapshot(p);}
static void depth269_observe(void){
 NFDepthClean269*p=depth269_current;if(!p)return;
 if(p->target!=depth||p->expected_draws!=draws||p->expected_clears!=gpu_depth_clears||p->expected_transfers!=transfers){
  if(p->canonical)depth269_invalidations++;p->canonical=0;
 }
}
static void depth269_imported(ID3D11Texture2D*t,int canonical){
 NFDepthClean269*p=depth269_current;if(!p||p->target!=t)return;
 p->canonical=!!canonical;p->in_draw=0;depth269_snapshot(p);depth269_imports++;
}
static void depth269_before_draw(void){
 NFDepthClean269*p=depth269_current;if(!p)return;depth269_observe();
 /* This is the actual state selected by begin, not a later packet's flags. */
 if(active.depth_enable&&active.depth_write)p->canonical=0;
 p->in_draw=1;depth269_snapshot(p);
}
static void depth269_after_draw(void){
 NFDepthClean269*p=depth269_current;if(!p)return;
 if(!p->in_draw||p->target!=depth||draws!=p->expected_draws+1||p->expected_clears!=gpu_depth_clears||p->expected_transfers!=transfers){
  if(p->canonical)depth269_invalidations++;p->canonical=0;
 }
 p->in_draw=0;depth269_snapshot(p);
}
static int depth269_skip(ID3D11Texture2D*t){
 NFDepthClean269*p=depth269_current;if(!p)return 0;depth269_observe();
 if(p->target!=t||!p->canonical||p->in_draw)return 0;
 depth269_skipped++;return 1;
}
static void depth269_quantized_now(ID3D11Texture2D*t){
 NFDepthClean269*p=depth269_current;if(!p||p->target!=t)return;
 p->canonical=1;p->in_draw=0;depth269_snapshot(p);depth269_quantized++;
}
static void depth269_report(void){
 static uint64_t reports;if(++reports!=1&&reports%120)return;
 DWORD error=GetLastError();fprintf(stderr,"[DEPTH-CLEAN269] calls=%llu skipped=%llu quantized=%llu imports=%llu invalidations=%llu private_lane_proof=1 boundary_checks_retained=1\n",(unsigned long long)reports,(unsigned long long)depth269_skipped,(unsigned long long)depth269_quantized,(unsigned long long)depth269_imports,(unsigned long long)depth269_invalidations);SetLastError(error);
}
#else
static int depth269_skip(ID3D11Texture2D*t){(void)t;return 0;}
static void depth269_quantized_now(ID3D11Texture2D*t){(void)t;}
#endif
#endif
