#ifndef NIGHTFIRE_FRAGMENT_DEPTH291_H
#define NIGHTFIRE_FRAGMENT_DEPTH291_H
/* Private immutable-list scope only. No persistent GPU-content certificate.
 * floor(rasterized normalized Z * 16777215) BEFORE compare is an experiment.
 * Not original-windowZ flooring or Xbox proof; original VS and clipping stay. */
typedef struct {
 const NFHardwareState *expected;
 const NFHardwareMaterialVertex221 *vertices;
 unsigned count,imported,drawn,control;
 ID3D11Texture2D *texture;
 ID3D11DepthStencilView *view;
} NFFragmentScope291;
static NFFragmentScope291 *fragment291_current;
static struct {uint64_t considered,eligible,ineligible,config,shader,draws,boundaries,failed;} fragment291_counts;
static int fragment291_enabled(void){
 static int on=-1;if(on<0){DWORD error=GetLastError();const char*v=getenv("DRIVING_FRAGMENT_DEPTH291");on=v&&!strcmp(v,"1");SetLastError(error);}return on;
}
static int fragment291_mode(void){
#ifdef NF_DEPTH_SSE2
 unsigned m=_mm_getcsr();return (m&0x6000)==0&&(m&0x1f80)==0x1f80&&(m&0x20)!=0;
#else
 return 0;
#endif
}
static int fragment291_options(void){
 static int allowed=-1;if(allowed<0){DWORD saved=GetLastError();
  const char*names[]={"DRIVING_DEPTH_IMPORT268","DRIVING_DEPTH_CLEAN269","DRIVING_DEPTH_PING272","DRIVING_DEPTH_SEED278"};allowed=1;
  for(unsigned i=0;i<4;i++){const char*v=getenv(names[i]);if(v&&!strcmp(v,"1"))allowed=0;}SetLastError(saved);
 }return allowed;
}
static int fragment291_identity(void){
 NFFragmentScope291 *p=fragment291_current;
 return p&&fragment291_mode()&&(_mm_getcsr()&0xffc0u)==p->control&&p->texture==depth&&p->view==dsv&&width==640&&height==480;
}
static int fragment291_begin(const NFHardwareState *s){
 NFFragmentScope291 *p=fragment291_current;
 return !p||(fragment291_identity()&&s==p->expected&&s->material221&&s->material221->integer_depth291==291&&(!pending||p->imported));
}
static int fragment291_imported(const NFHardwareState *s){
 if(!fragment291_current)return 1;
 if(!fragment291_begin(s))return 0;
 fragment291_current->imported=1;return 1;
}
static int fragment291_draw(const NFHardwareMaterialVertex221 *v,unsigned n){
 NFFragmentScope291 *p=fragment291_current;
 return !p||(fragment291_identity()&&p->imported&&p->expected&&p->vertices==v&&p->count==n&&
  active.color==p->expected->color&&active.depth==p->expected->depth&&active.material221&&
  !memcmp(active.material221,p->expected->material221,sizeof *active.material221));
}
/* Called only after every ordinary boundary precondition. */
static int fragment291_boundary(const NFHardwareState *next){
 NFFragmentScope291 *p=fragment291_current;
 if(!p)return 0;
 if(!fragment291_identity()||!p->imported||!p->drawn||p->expected!=next)return -1;
 p->drawn=0;fragment291_counts.boundaries++;return 1;
}
static void fragment291_report(void){
 uint64_t n=fragment291_counts.considered;
 if(n&&(n==1||!(n%120)))fprintf(stderr,"[FRAGMENT291] batches=%llu eligible=%llu input_miss=%llu config_miss=%llu shader_miss=%llu draws=%llu omitted_boundaries=%llu failed=%llu\n",
 (unsigned long long)n,(unsigned long long)fragment291_counts.eligible,(unsigned long long)fragment291_counts.ineligible,
 (unsigned long long)fragment291_counts.config,(unsigned long long)fragment291_counts.shader,(unsigned long long)fragment291_counts.draws,
 (unsigned long long)fragment291_counts.boundaries,(unsigned long long)fragment291_counts.failed);
}
#endif
