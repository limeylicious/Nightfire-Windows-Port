/* Private248 immutable-input evidence, valid only during its synchronous call.
 * Public begin/draw always pass NULL. Never a persistent content/permission cache. */
#ifndef NIGHTFIRE_VALIDATION267_H
#define NIGHTFIRE_VALIDATION267_H
typedef struct NFValidation267 {
 const NFHardwareState *state;
 const NFHardwareMaterial221 *material;
 const NFHardwareMaterialVertex221 *vertices;
 unsigned count,control,status,begun;
} NFValidation267;
static struct {uint64_t batches,begin_reused,begin_fallback,draw_reused,draw_fallback,vertices_reused;} validation267;
static int validation267_enabled(void){
 static int value=-1;if(value<0){DWORD saved=GetLastError();const char*v=getenv("DRIVING_VALIDATION267");value=v&&!strcmp(v,"1");SetLastError(saved);}return value;
}
static unsigned validation267_control(void){
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
 /* All MXCSR controls, including rounding/FTZ/DAZ/exception masks; ignore flags. */
 return _mm_getcsr()&0xffc0u;
#else
 return UINT32_MAX;
#endif
}
static int validation267_begin(const NFValidation267*p,const NFHardwareState*s){
 return p&&p->state==s&&s&&p->material==s->material221&&p->vertices&&p->count&&
  p->control!=UINT32_MAX&&p->control==validation267_control()&&
  s->width==640&&s->height==480&&s->pitch==2560&&s->depth_pitch==2560;
}
static unsigned validation267_status(void){
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
 return _mm_getcsr()&0x3fu;
#else
 return 0;
#endif
}
static int validation267_draw(const NFValidation267*p,const NFHardwareMaterialVertex221*v,unsigned n){
 return p&&p->begun&&p->vertices==v&&p->count==n&&p->control!=UINT32_MAX&&
  p->control==validation267_control()&&(validation267_status()&p->status)==p->status&&width==640&&height==480&&
  active.material221&&p->material&&
  !memcmp(active.material221,p->material,sizeof*p->material);
}
static void validation267_report(void){
 validation267.batches++;
 if(validation267.batches!=1&&validation267.batches%512)return;
 DWORD saved=GetLastError();
 fprintf(stderr,"[VALIDATION267] batches=%llu begin_reused=%llu begin_full_fallback=%llu draw_reused=%llu draw_full_fallback=%llu lane_vertices_reused=%llu full_first_preflight=1 clip_arithmetic_retained=1 call_scoped=1\n",
  (unsigned long long)validation267.batches,(unsigned long long)validation267.begin_reused,(unsigned long long)validation267.begin_fallback,
  (unsigned long long)validation267.draw_reused,(unsigned long long)validation267.draw_fallback,(unsigned long long)validation267.vertices_reused);SetLastError(saved);
}
#ifdef NF_PAIR234_TEST
static void (*validation267_test_hook)(unsigned phase);
#define VALIDATION267_HOOK(phase) do{if(validation267_test_hook)validation267_test_hook(phase);}while(0)
#else
#define VALIDATION267_HOOK(phase) ((void)0)
#endif
#endif
