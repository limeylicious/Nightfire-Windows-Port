/* Isolated Driving two-lane candidate. Included after material221 backend.
 * Caller serializes the immediate context and holds all CPU spans stable until
 * return. VirtualQuery is a permission check, not a lock against concurrent
 * unmapping/protection changes. Private sets own every resource independently. */
#if defined(NIGHTFIRE_PAIR234) && defined(NIGHTFIRE_MATERIAL221) && !defined(NIGHTFIRE_GPU_FALLBACK96) && !defined(NIGHTFIRE_RESIDENT_MAIN130) && !defined(NIGHTFIRE_DEFER_MAIN128) && !defined(NIGHTFIRE_GPU_TIMING_DIAGNOSTIC) && !defined(NIGHTFIRE_DEPTH_OBSERVE_DIAGNOSTIC) && !defined(NIGHTFIRE_SURFACE_PROBE_DIAGNOSTIC) && !defined(NIGHTFIRE_COLOR_REUSE110) && !defined(NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC)
#define NF_PAIR276_AVAILABLE 1
static int readback294_setting=-1;
static int readback294_enabled(void){if(readback294_setting<0){DWORD e=GetLastError();const char*v=getenv("DRIVING_READBACK294");readback294_setting=v&&!strcmp(v,"1");SetLastError(e);}return readback294_setting;}
typedef struct {
 ID3D11Texture2D *color,*depth,*color_read,*depth_transfer;
 ID3D11RenderTargetView *rtv;ID3D11DepthStencilView *dsv;
#ifdef NF_DEPTH_SRV256_AVAILABLE
 ID3D11ShaderResourceView *depth_srv256;
#endif
} NFPairSurface234;
static NFPairSurface234 pair_surfaces234[2];
static ID3D11Query *pair_event234;
static int pair_poisoned234;
static uint64_t pairs235,depth_omitted235,depth_normal235;
static int pair_nearest235(void){
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
 return (_mm_getcsr()&_MM_ROUND_MASK)==_MM_ROUND_NEAREST;
#else
 return 0;
#endif
}
#ifdef NF_PAIR234_TEST
static unsigned pair_round_enabled235,pair_round235;
void nf_hw_pair235_test_round(unsigned enabled,unsigned round){pair_round_enabled235=enabled;pair_round235=round;}
static void pair_round_hook235(void){if(pair_round_enabled235){_MM_SET_ROUNDING_MODE(pair_round235);pair_round_enabled235=0;}}
#else
static void pair_round_hook235(void){}
#endif
#ifdef NF_PAIR234_TEST
static unsigned pair_fail234;
static unsigned pair_flush_first234;
void nf_hw_pair234_test_flush(unsigned enabled){pair_flush_first234=!!enabled;}

void nf_hw_pair234_test_fail(unsigned stage){pair_fail234=stage;}
static int pair_failure234(unsigned stage){if(pair_fail234==stage){pair_fail234=0;return 1;}return 0;}
#else
static int pair_failure234(unsigned stage){(void)stage;return 0;}
#endif
static int pair_span234(const void *p,size_t n,int write){
 if(nf_host_current261)return nf_host_span261(p,n,write);
 uintptr_t a=(uintptr_t)p;if(!a||!n||n>UINTPTR_MAX-a)return 0;uintptr_t end=a+n;
 while(a<end){MEMORY_BASIC_INFORMATION m;if(!VirtualQuery((void*)a,&m,sizeof m)||m.State!=MEM_COMMIT||m.Protect&(PAGE_GUARD|PAGE_NOACCESS))return 0;
  DWORD q=m.Protect&255;int rw=q==PAGE_READWRITE||q==PAGE_WRITECOPY||q==PAGE_EXECUTE_READWRITE||q==PAGE_EXECUTE_WRITECOPY;
  if(!rw&&(write||(q!=PAGE_READONLY&&q!=PAGE_EXECUTE_READ)))return 0;
  uintptr_t b=(uintptr_t)m.BaseAddress;if(m.RegionSize>UINTPTR_MAX-b||b+m.RegionSize<=a)return 0;a=b+m.RegionSize;
 }return 1;
}
static void pair_release234(NFPairSurface234 *s){if(resident313_blocked())return;
 nf_hw_color_seed276_revoke();nf_hw_depth_seed278_revoke();
#ifdef NF_DEPTH_SRV256_AVAILABLE
 RELEASE(s->depth_srv256);
#endif
RELEASE(s->rtv);RELEASE(s->dsv);RELEASE(s->color);RELEASE(s->depth);RELEASE(s->color_read);RELEASE(s->depth_transfer);}
static int pair_create234(NFPairSurface234 *s){
 if(s->depth_transfer)return 1;
 nf_hw_color_seed276_revoke();nf_hw_depth_seed278_revoke();
 D3D11_TEXTURE2D_DESC d={0};d.Width=640;d.Height=480;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
 d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;if(readback294_enabled())d.BindFlags|=D3D11_BIND_SHADER_RESOURCE;
 if(FAILED(ID3D11Device_CreateTexture2D(dev,&d,NULL,&s->color))||FAILED(ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)s->color,NULL,&s->rtv)))goto fail;
 d.Format=DXGI_FORMAT_R32_TYPELESS;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;if(readback294_enabled())d.BindFlags|=D3D11_BIND_SHADER_RESOURCE;
#ifdef NF_DEPTH_SRV256_AVAILABLE
 if(quant256_enabled()||depth272_enabled())d.BindFlags|=D3D11_BIND_SHADER_RESOURCE;
#endif
 if(FAILED(ID3D11Device_CreateTexture2D(dev,&d,NULL,&s->depth)))goto fail;
 D3D11_DEPTH_STENCIL_VIEW_DESC dd={0};dd.Format=DXGI_FORMAT_D32_FLOAT;dd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
 if(FAILED(ID3D11Device_CreateDepthStencilView(dev,(ID3D11Resource*)s->depth,&dd,&s->dsv)))goto fail;
#ifdef NF_DEPTH_SRV256_AVAILABLE
 if(quant256_enabled()||depth272_enabled()){
  D3D11_SHADER_RESOURCE_VIEW_DESC sd={0};sd.Format=DXGI_FORMAT_R32_FLOAT;sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sd.Texture2D.MipLevels=1;
  if(quant256_failure(2)||FAILED(ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)s->depth,&sd,&s->depth_srv256)))goto fail;
 }
#endif
 d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
 if(FAILED(ID3D11Device_CreateTexture2D(dev,&d,NULL,&s->color_read)))goto fail;
 d.Format=DXGI_FORMAT_R32_TYPELESS;d.CPUAccessFlags|=D3D11_CPU_ACCESS_WRITE;
 if(FAILED(ID3D11Device_CreateTexture2D(dev,&d,NULL,&s->depth_transfer)))goto fail;
 return 1;
fail:pair_release234(s);return 0;
}
static void pair_select234(const NFPairSurface234 *s,unsigned w,unsigned h){
 ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);
 color=s->color;depth=s->depth;color_read=s->color_read;depth_transfer=s->depth_transfer;rtv=s->rtv;dsv=s->dsv;width=w;height=h;
 memset(&bound,0,sizeof bound);pending=0;nf_hw_clear_pending=0;
}
static int pair_preflight234(const NFHardwareState states[2],const NFHardwareMaterialVertex221 *vertices[2],unsigned n){
 const size_t bytes=640u*480*4;
 if(pending||pair_poisoned234||!n||n>16384||n%3||!pair_span234(states,2*sizeof*states,0)||!pair_span234(vertices,2*sizeof*vertices,0))return 0;
 const void *out[4]={states[0].color,states[0].depth,states[1].color,states[1].depth};
 for(unsigned i=0;i<4;i++){
  if(!pair_span234(out[i],bytes,1))return 0;
  for(unsigned j=0;j<i;j++)if(range179(out[i],bytes,out[j],bytes))return 0;
  if(range179(out[i],bytes,states,2*sizeof*states)||range179(out[i],bytes,vertices,2*sizeof*vertices))return 0;
 }
 for(unsigned lane=0;lane<2;lane++){
  const NFHardwareState *s=&states[lane];const NFHardwareMaterialVertex221 *v=vertices[lane];
  if(s->width!=640||s->height!=480||s->pitch!=2560||s->depth_pitch!=2560||s->left||s->top||s->right!=640||s->bottom!=480||s->reverse_subtract97||s->depth_func<0x200||s->depth_func>0x207||
    (s->color_write_mask212&&(s->color_write_mask212<0x10||s->color_write_mask212>0x1f))||
    (s->blend_enable&&!((s->blend_src==0x302&&(s->blend_dst==0x303||s->blend_dst==1))||(s->blend_src==1&&(s->blend_dst==0||s->blend_dst==1)))))return 0;
  if(!pair_span234(s->material221,sizeof*s->material221,0)||!pair_span234(v,n*sizeof*v,0))return 0;
  for(unsigned j=0;j<4;j++)if(range179(out[j],bytes,v,n*sizeof*v)||range179(out[j],bytes,s->material221,sizeof*s->material221))return 0;
  if(!material_preflight221(s))return 0;
  for(unsigned stage=0;stage<4;stage++)if((s->material221->pixel.program>>(stage*5)&31)==1){size_t tbytes;
   if(!material_texture_layout221(&s->material221->textures[stage],&tbytes))return 0;
   for(unsigned j=0;j<4;j++)if(range179(out[j],bytes,s->material221->textures[stage].data,tbytes))return 0;
  }
  for(unsigned i=0;i<n;i++){
   const float *all=(const float*)&v[i];for(unsigned k=0;k<29;k++)if(!isfinite(all[k]))return 0;
   if(!v[i].position[3])return 0;
   for(unsigned k=0;k<4;k++)if(v[i].color[k]<0||v[i].color[k]>1||v[i].specular[k]<0||v[i].specular[k]>1)return 0;
   for(unsigned stage=0;stage<4;stage++){
    unsigned mode=s->material221->pixel.program>>(5*stage)&31;const float *uv=v[i].uv[stage];
    if(s->material221->pixel.white_stage2_242&&stage==2){if(!nf_pixel_zero_uv242(&s->material221->pixel,stage,uv))return 0;}
    else if(mode==1&&(!uv[3]||(uv[3]<0)!=(v[i-i%3].uv[stage][3]<0)||!isfinite(uv[0]/uv[3])||!isfinite(uv[1]/uv[3])))return 0;
    if(mode==4)for(unsigned k=0;k<4;k++)if(uv[k]<0||uv[k]>1)return 0;
   }
   if(!isfinite((v[i].position[0]*2/640.f-1)*v[i].position[3])||!isfinite((1-v[i].position[1]*2/480.f)*v[i].position[3])||!isfinite(v[i].position[2]/16777215.f*v[i].position[3]))return 0;
  }
 }return 1;
}
int nf_hw_material_pair234(const NFHardwareState states[2],const NFHardwareMaterialVertex221 *vertices[2],unsigned n){if(resident313_blocked())return 0;
 nf_hw_color_seed276_revoke();nf_hw_depth_seed278_revoke();
 if(!pair_preflight234(states,vertices,n)||!initialize()||!pair_create234(&pair_surfaces234[0])||!pair_create234(&pair_surfaces234[1]))return 0;
 if(!pair_event234){D3D11_QUERY_DESC q={D3D11_QUERY_EVENT,0};if(FAILED(ID3D11Device_CreateQuery(dev,&q,&pair_event234)))return 0;}
 NFPairSurface234 saved={color,depth,color_read,depth_transfer,rtv,dsv};unsigned sw=width,sh=height;NFHardwareState sa=active;
 D3D11_MAPPED_SUBRESOURCE maps[4]={{0}};ID3D11Resource *resources[4]={0};unsigned mapped=0;int ok=0;
 /* This API performs exactly one draw per lane; begin initializes its depth.
  * Skip only an identity roundtrip, with rounding checked again before output. */
 int preserve_depth235[2]={0,0};
 if(pair_nearest235())
  for(unsigned lane=0;lane<2;lane++)preserve_depth235[lane]=!states[lane].depth_enable||!states[lane].depth_write;
 for(unsigned lane=0;lane<2;lane++){
  pair_select234(&pair_surfaces234[lane],640,480);
  if(!nf_hw_begin(&states[lane])||!nf_hw_draw_material221(vertices[lane],n)||pair_failure234(lane+1))goto done;
  ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);bound.targets=0;
  ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)color_read,(ID3D11Resource*)color);
  if(!preserve_depth235[lane])ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)depth_transfer,(ID3D11Resource*)depth);
  /* The ordinary lane0 Map submitted its pending commands before the CPU
   * prepared lane1. Explicit submission may recover that overlap; it is not
   * completion and changes neither Map order nor publication boundaries. */
#ifdef NF_PAIR234_FLUSH_FIRST
  if(!lane)ID3D11DeviceContext_Flush(ctx);
#elif defined(NF_PAIR234_TEST)
  if(!lane && pair_flush_first234)ID3D11DeviceContext_Flush(ctx);
#endif
  resources[2*lane]=(ID3D11Resource*)color_read;
  if(!preserve_depth235[lane])resources[2*lane+1]=(ID3D11Resource*)depth_transfer;
  pending=0;nf_hw_clear_pending=0;
 }
 /* Acquire every readback before touching any caller output. */
 for(unsigned i=0;i<4;i++){
  if(!resources[i])continue;
  if(pair_failure234(i+3)||FAILED(ID3D11DeviceContext_Map(ctx,resources[i],0,D3D11_MAP_READ,0,&maps[i])))goto done;
  mapped|=1u<<i;
 }
 /* Guest callbacks are absent here; x64 callees preserve MXCSR control bits.
  * Still recover conservatively if the rounding mode changed after enqueue.
  * Color maps remain held; only independent depth resources are copied/mapped.
  * Every needed map succeeds before any caller-owned bytes are published. */
 pair_round_hook235();
 if(!pair_nearest235())for(unsigned lane=0;lane<2;lane++)if(preserve_depth235[lane]){
  unsigned i=2*lane+1;NFPairSurface234 *p=&pair_surfaces234[lane];
  ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)p->depth_transfer,(ID3D11Resource*)p->depth);
  resources[i]=(ID3D11Resource*)p->depth_transfer;
  if(pair_failure234(i+3)||FAILED(ID3D11DeviceContext_Map(ctx,resources[i],0,D3D11_MAP_READ,0,&maps[i])))goto done;
  mapped|=1u<<i;preserve_depth235[lane]=0;
 }
 for(unsigned lane=0;lane<2;lane++)for(unsigned y=0;y<480;y++){
  memcpy(states[lane].color+(size_t)y*2560,(const char*)maps[2*lane].pData+(size_t)y*maps[2*lane].RowPitch,2560);
  if(!preserve_depth235[lane])nf_depth_pack((uint32_t*)(states[lane].depth+(size_t)y*2560),(const float*)((const char*)maps[2*lane+1].pData+(size_t)y*maps[2*lane+1].RowPitch),640);
 }
 transfers+=2;ok=1;
 pairs235++;depth_omitted235+=(unsigned)preserve_depth235[0]+(unsigned)preserve_depth235[1];
 depth_normal235+=2u-(unsigned)preserve_depth235[0]-(unsigned)preserve_depth235[1];
 if(pairs235==1||!(pairs235%1024))fprintf(stderr,"[PAIR235] pairs=%llu depth-omitted-lanes=%llu depth-normal-lanes=%llu\n",
  (unsigned long long)pairs235,(unsigned long long)depth_omitted235,(unsigned long long)depth_normal235);
done:
 for(unsigned i=0;i<4;i++)if(mapped&(1u<<i))ID3D11DeviceContext_Unmap(ctx,resources[i],0);
 if(!ok){
  /* Failure never falls back or publishes. Drain queued work before releasing
   * the borrowed selection. A device failure/timeout poisons future pairs. */
  ID3D11DeviceContext_End(ctx,(ID3D11Asynchronous*)pair_event234);ID3D11DeviceContext_Flush(ctx);
  ULONGLONG limit=GetTickCount64()+5000;HRESULT hr;
  while((hr=ID3D11DeviceContext_GetData(ctx,(ID3D11Asynchronous*)pair_event234,NULL,0,0))==S_FALSE&&GetTickCount64()<limit)Sleep(1);
  if(hr!=S_OK)pair_poisoned234=1;
 }
 pair_select234(&saved,sw,sh);active=sa;return ok?1:-1;
}
#else
int nf_hw_material_pair234(const NFHardwareState states[2],const NFHardwareMaterialVertex221 *vertices[2],unsigned count){if(resident313_blocked())return 0;(void)states;(void)vertices;(void)count;return 0;}
#ifdef NF_PAIR234_TEST
void nf_hw_pair234_test_fail(unsigned stage){(void)stage;}
#endif
#endif
