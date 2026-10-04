#ifndef NIGHTFIRE_NATIVE_DEPTH302_H
#define NIGHTFIRE_NATIVE_DEPTH302_H
/* Native D24 experiment. Original VS/PS unchanged. Scope belongs only to the
 * synchronous private248 call; no token or GPU ownership escapes that call. */
typedef struct {
 const NFHardwareState *expected;
 const NFHardwareMaterialVertex221 *vertices;unsigned count,imported,drawn,control;
 ID3D11Texture2D *texture,*transfer;ID3D11DepthStencilView *view;
} NFNativeScope302;
static NFNativeScope302 *native302_current;
#ifdef NF_PAIR234_TEST
static unsigned native302_import_fail;
#endif
static struct {uint64_t considered,eligible,config,contract,resource,imports,draws,boundaries,failed;unsigned config_mask;} native302_counts;
static int native302_enabled(void){static int on=-1;if(on<0){DWORD e=GetLastError();const char*v=getenv("DRIVING_NATIVE_DEPTH302");on=v&&!strcmp(v,"1");SetLastError(e);}return on;}
static uint32_t native302_to_dxgi(uint32_t x){return (x>>8)|(x<<24);}
static uint32_t native302_from_dxgi(uint32_t x){return (x<<8)|(x>>24);}
static int native302_mode(void){
#ifdef NF_DEPTH_SSE2
 return (_mm_getcsr()&(_MM_ROUND_MASK|_MM_MASK_MASK|_MM_EXCEPT_INEXACT))==(_MM_MASK_MASK|_MM_EXCEPT_INEXACT);
#else
 return 0;
#endif
}
static unsigned native302_control(void){
#ifdef NF_DEPTH_SSE2
 return _mm_getcsr()&0xffc0u;
#else
 return UINT32_MAX;
#endif
}
static int native302_identity(void){
 NFNativeScope302*q=native302_current;
 return q&&native302_mode()&&q->control==native302_control()&&q->texture==depth&&q->view==dsv&&q->transfer==depth_transfer&&
  q->texture&&q->view&&q->transfer&&width==640&&height==480;
}
static int native302_state(const NFHardwareState*s){
 return s&&s->material221&&s->material221->integer_depth302==302&&
  s->depth_enable==1&&s->depth_write<=1&&(s->depth_func==0x203||s->depth_func==0x207)&&
  !s->color_only&&!s->color_layout&&s->width==640&&s->height==480&&s->pitch==2560&&s->depth_pitch==2560;
}
static int native302_begin(const NFHardwareState*s){
 return !native302_current||(native302_identity()&&native302_current->expected==s&&native302_state(s)&&
  (pending?native302_current->imported:!native302_current->imported));
}
/* 0 inactive, 1 imported, -1 operational failure. Caller has performed all
 * ordinary begin/span checks. Import all stencil bits, even though testing is
 * disabled. CPU publication below additionally preserves original low bytes. */
static int native302_import(const NFHardwareState*s){
 if(!native302_current)return 0;
 if(!native302_begin(s)||pending||native302_current->imported)return -1;
#ifdef NF_PAIR234_TEST
 if(native302_import_fail&&!--native302_import_fail)return -1;
#endif
 D3D11_MAPPED_SUBRESOURCE m={0};
 if(FAILED(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)depth_transfer,0,D3D11_MAP_WRITE,0,&m)))return -1;
 if(!m.pData||m.RowPitch<2560){ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)depth_transfer,0);return -1;}
 for(unsigned y=0;y<480;y++){
  uint32_t*dst=(uint32_t*)((char*)m.pData+(size_t)y*m.RowPitch);
  const uint32_t*src=(const uint32_t*)(s->depth+(size_t)y*s->depth_pitch);
  for(unsigned x=0;x<640;x++)dst[x]=native302_to_dxgi(src[x]);
 }
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)depth_transfer,0);
 ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)depth,(ID3D11Resource*)depth_transfer);
 native302_current->imported=1;native302_counts.imports++;return 1;
}
static int native302_boundary(const NFHardwareState*s){
 if(!native302_current)return 0;
 if(!pending||!native302_identity()||native302_current->expected!=s||!native302_state(s)||
  !native302_current->imported||!native302_current->drawn)return -1;
 native302_current->drawn=0;native302_counts.boundaries++;return 1;
}
static int native302_draw(const NFHardwareMaterialVertex221*v,unsigned n){
 return !native302_current||(pending&&native302_identity()&&native302_current->imported&&
  native302_current->expected&&active.material221&&
  active.color==native302_current->expected->color&&active.depth==native302_current->expected->depth&&
  !memcmp(active.material221,native302_current->expected->material221,sizeof *active.material221)&&
  native302_current->vertices==v&&native302_current->count==n&&!native302_current->drawn);
}
static void native302_report(void){
 DWORD e=GetLastError();if(native302_counts.considered==1||!(native302_counts.considered%120))
 fprintf(stderr,"[NATIVE-DEPTH302] considered=%llu eligible=%llu config=%llu contract=%llu resource=%llu imports=%llu lane_draws=%llu omitted_boundaries=%llu failed=%llu config_mask=%02X\n",
  (unsigned long long)native302_counts.considered,(unsigned long long)native302_counts.eligible,(unsigned long long)native302_counts.config,
  (unsigned long long)native302_counts.contract,(unsigned long long)native302_counts.resource,(unsigned long long)native302_counts.imports,
  (unsigned long long)native302_counts.draws,(unsigned long long)native302_counts.boundaries,(unsigned long long)native302_counts.failed,native302_counts.config_mask);
 SetLastError(e);
}
#endif
