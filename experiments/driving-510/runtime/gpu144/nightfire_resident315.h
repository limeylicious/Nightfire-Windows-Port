#if defined(NF_PAIR276_AVAILABLE) && defined(NF_DEPTH_PING272_AVAILABLE) && defined(NIGHTFIRE_PAIR_BATCH248)
/* Private backend experiment only. Caller owns and freezes all CPU buffers,
 * serializes the context, and retains snapshots through finish/abort.
 * No guest visibility, allocation lease, GET, or semaphore policy is provided. */
#ifndef RESIDENT313_H
#define RESIDENT313_H
typedef struct {
 NFPairSurface234 surface;
 NFDepthScope272 ping;
 NFHardwareState previous;
 NFHardwareMaterial221 material;
 unsigned drawn;
} R313Lane;
typedef struct {
 unsigned phase,internal,segments,count,control;
 DWORD thread;
 uint64_t cookie;
 unsigned char *out[4];
 R313Lane lane[2];
 ID3D11Query *event;
 ID3D11VertexShader *vs;
 ID3D11PixelShader *ps;
 ID3D11RasterizerState *rs;
 ID3D11DepthStencilState *ds;
 ID3D11Device *device;
 ID3D11DeviceContext *context;
} R313;
static R313 r313;
static int r313_unbounded450; /*450: set only while a PC-mode session is open*/
/* Only an automatic scope of the currently executing API entry may be used.
 * This pointer is cleared on every return and is never stored in a session. */
static NFHostScope261 *resident317_host;
static uint64_t r313_serial;
static uint64_t pool315_created,pool315_reused,pool315_recycled;
static int resident313_blocked(void){return r313.phase&&(!r313.internal||r313.thread!=GetCurrentThreadId());}
static unsigned r313_control(void){return _mm_getcsr()&~63u;}
static int r313_config(void){
#ifdef NIGHTFIRE_GPU_SAMPLE254
 if(gt254_enabled())return 0;
#endif
#ifdef NIGHTFIRE_GPU_SAMPLE297
 if(gt297_enabled())return 0;
#endif
 return batch_enabled248()&&depth272_enabled()&&!quant256_enabled()&&!depth268_enabled()&&!depth269_enabled()&&
 !nf_hw_color_seed276_enabled()&&!nf_hw_depth_seed278_enabled()&&!fragment291_enabled()&&!native302_enabled()&&
 !readback294_enabled()&&
 (!nf_host_current261||nf_host_current261==resident317_host)&&!depth272_current&&!depth269_current&&!depth278_current&&!color276_current&&
 !fragment291_current&&!native302_current&&!quant256_target&&!quant256_selected_srv&&!depth268_target&&
 batch_nearest236()&&(_mm_getcsr()&_MM_MASK_MASK)==_MM_MASK_MASK;
}
static int r313_identity(uint64_t cookie){
 return r313.phase==1&&!r313.internal&&cookie&&cookie==r313.cookie&&r313.thread==GetCurrentThreadId();
}
static int r313_kernels(void){return r313.device==dev&&r313.context==ctx&&r313.vs==depth272_vs&&r313.ps==depth272_ps&&r313.rs==depth272_raster&&r313.ds==depth272_state;}
static void r313_release(void){
 r313.internal=1;
 for(unsigned l=0;l<2;l++){
  depth272_release(&r313.lane[l].ping.slots[1]);
  pair_release234(&r313.lane[l].surface);
 }
 RELEASE(r313.event);RELEASE(r313.vs);RELEASE(r313.ps);RELEASE(r313.rs);RELEASE(r313.ds);
 RELEASE(r313.context);RELEASE(r313.device);
 memset(&r313,0,sizeof r313);
}
/* Completed storage reuse, not retained guest authority. Every next begin
 * imports fresh caller seeds and obtains a distinct nonwrapping cookie. */
static void r315_recycle(void){
 for(unsigned l=0;l<2;l++){
  R313Lane *p=&r313.lane[l];p->drawn=0;p->ping.at=0;
  memset(&p->previous,0,sizeof p->previous);memset(&p->material,0,sizeof p->material);
 }
 memset(r313.out,0,sizeof r313.out);r313.phase=r313.internal=r313.segments=r313.count=r313.control=0;
 r313.cookie=0;r313.thread=0;pool315_recycled++;
 nf_hw_color_seed276_revoke();nf_hw_depth_seed278_revoke();
}
static int r313_join(void){
 ID3D11DeviceContext *owner=r313.context;
 ID3D11DeviceContext_End(owner,(ID3D11Asynchronous*)r313.event);ID3D11DeviceContext_Flush(owner);
 ULONGLONG until=GetTickCount64()+5000;HRESULT h;
 while((h=ID3D11DeviceContext_GetData(owner,(ID3D11Asynchronous*)r313.event,NULL,0,0))==S_FALSE&&GetTickCount64()<until)Sleep(1);
 if(h==S_OK)return 1;
 r313.phase=2;r313.internal=0;pair_poisoned234=1;return 0;
}
static int r313_retire_failure(void){if(r313_join())r313_release();return -1;}
static int r313_alt(NFDepthSlot272 *s){
 D3D11_TEXTURE2D_DESC td={0};td.Width=640;td.Height=480;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
 td.Format=DXGI_FORMAT_R32_TYPELESS;td.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
 if(FAILED(ID3D11Device_CreateTexture2D(dev,&td,NULL,&s->texture)))return 0;
 D3D11_DEPTH_STENCIL_VIEW_DESC dd={0};dd.Format=DXGI_FORMAT_D32_FLOAT;dd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
 if(FAILED(ID3D11Device_CreateDepthStencilView(dev,(ID3D11Resource*)s->texture,&dd,&s->dsv)))return 0;
 D3D11_SHADER_RESOURCE_VIEW_DESC sd={0};sd.Format=DXGI_FORMAT_R32_FLOAT;sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sd.Texture2D.MipLevels=1;
 return SUCCEEDED(ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)s->texture,&sd,&s->srv));
}
static int r313_prepare(void){
 if(r313.event){
  if(r313_kernels()){
   int valid=1;
   for(unsigned l=0;l<2;l++){R313Lane*p=&r313.lane[l];valid&=p->surface.color&&p->surface.depth&&p->surface.color_read&&
    p->surface.depth_transfer&&p->surface.rtv&&p->surface.dsv&&p->surface.depth_srv256&&
    p->ping.slots[1].texture&&p->ping.slots[1].dsv&&p->ping.slots[1].srv&&
    p->ping.slots[0].texture==p->surface.depth&&p->ping.slots[0].dsv==p->surface.dsv&&
    p->ping.slots[0].srv==p->surface.depth_srv256&&!p->drawn&&!p->ping.at;}
   if(valid){pool315_reused++;return 1;}
  }
  r313_release();
 }
 for(unsigned l=0;l<2;l++){
  R313Lane *p=&r313.lane[l];
  if(!pair_create234(&p->surface)||!p->surface.depth_srv256||!r313_alt(&p->ping.slots[1]))return 0;
  p->ping.slots[0]=(NFDepthSlot272){p->surface.depth,p->surface.dsv,p->surface.depth_srv256};
 }
 D3D11_QUERY_DESC q={D3D11_QUERY_EVENT,0};
 if(FAILED(ID3D11Device_CreateQuery(dev,&q,&r313.event)))return 0;
 r313.vs=depth272_vs;ID3D11VertexShader_AddRef(r313.vs);
 r313.ps=depth272_ps;ID3D11PixelShader_AddRef(r313.ps);
 r313.rs=depth272_raster;ID3D11RasterizerState_AddRef(r313.rs);
 r313.ds=depth272_state;ID3D11DepthStencilState_AddRef(r313.ds);
 r313.device=dev;ID3D11Device_AddRef(r313.device);
 r313.context=ctx;ID3D11DeviceContext_AddRef(r313.context);
 pool315_created++;
 return 1;
}
/* Saves only the strict configuration's permitted state. No borrowed scope
 * pointer survives this entry. The guards above refuse every other scope. */
static int r313_segment(const NFHardwareBatchDraw248 *list,unsigned count,unsigned proof_control,unsigned proof_status){
 NFPairSurface234 saved={color,depth,color_read,depth_transfer,rtv,dsv};
 unsigned sw=width,sh=height,pw=prepared_width234,ph=prepared_height234;
 NFHardwareState previous=active;NFHardwareMaterial221 old_material=material_active221;
 int old_program=program_active;unsigned old_inputs=program_inputs,old_dynamic=program_dynamic;
 ID3D11InputLayout *old_layout=input_layout;
#ifdef NIGHTFIRE_DIRECT_VERTEX88
 int old_direct28=direct28_selected;
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
 unsigned old_direct90=direct90_selected;
#endif
 int old_pending=pending,old_clear=nf_hw_clear_pending,ok=0;
 r313.internal=1;prepared_width234=640;prepared_height234=480;
 for(unsigned l=0;l<2;l++){
  R313Lane *p=&r313.lane[l];NFPairSurface234 selected=p->surface;
  selected.depth=p->ping.slots[p->ping.at].texture;selected.dsv=p->ping.slots[p->ping.at].dsv;
  selected.depth_srv256=p->ping.slots[p->ping.at].srv;
  pair_select234(&selected,640,480);depth272_current=&p->ping;
  if(p->drawn){active=p->previous;material_active221=p->material;active.material221=&material_active221;pending=1;nf_hw_clear_pending=1;}
  for(unsigned i=0;i<count;i++){
   const NFHardwareState *s=&list[i].states[l];
   if(p->drawn&&!nf_hw_batch234_boundary(s))goto done;
   NFValidation267 proof={s,s->material221,list[i].vertices[l],list[i].count,proof_control,proof_status,0};
   int reuse=validation267_enabled();
   if(!(reuse?nf_hw_begin_checked267(s,&proof):nf_hw_begin(s)))goto done;
   proof.begun=1;VALIDATION267_HOOK(1);
   if(r313_control()!=r313.control)goto done;
   if(!(reuse?nf_hw_draw_material_checked267(list[i].vertices[l],list[i].count,&proof):nf_hw_draw_material221(list[i].vertices[l],list[i].count))||pair_failure234(nf_batch_failure260(l,i)))goto done;
   p->drawn=1;
  }
  if(p->ping.at>1||depth!=p->ping.slots[p->ping.at].texture||dsv!=p->ping.slots[p->ping.at].dsv||!active.material221)goto done;
  p->previous=active;p->material=*active.material221;p->previous.material221=&p->material;
 }
 if(r313_control()!=r313.control)goto done;
 ok=1;
done:
 depth272_current=NULL;pair_select234(&saved,sw,sh);material_active221=old_material;active=previous;
 pending=old_pending;nf_hw_clear_pending=old_clear;prepared_width234=pw;prepared_height234=ph;r313.internal=0;
 program_active=old_program;program_inputs=old_inputs;program_dynamic=old_dynamic;input_layout=old_layout;
#ifdef NIGHTFIRE_DIRECT_VERTEX88
 direct28_selected=old_direct28;
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
 direct90_selected=old_direct90;
#endif
 return ok;
}
static int r313_cookie_span(const NFHardwareBatchDraw248 *list,unsigned count,uint64_t *cookie){
 if(!pair_span234(cookie,sizeof*cookie,1)||range179(cookie,sizeof*cookie,list,count*sizeof*list))return 0;
 for(unsigned i=0;i<count;i++)for(unsigned l=0;l<2;l++){
  const NFHardwareState*s=&list[i].states[l];const NFHardwareMaterial221*m=s->material221;
  if(range179(cookie,8,s->color,640*480*4)||range179(cookie,8,s->depth,640*480*4)||range179(cookie,8,m,sizeof*m)||
     range179(cookie,8,list[i].vertices[l],list[i].count*sizeof*list[i].vertices[l]))return 0;
  for(unsigned k=0;k<4;k++)if((m->pixel.program>>(k*5)&31)==1){size_t bytes;
   if(!material_texture_layout221(&m->textures[k],&bytes)||range179(cookie,8,m->textures[k].data,bytes))return 0;
  }
 }return 1;
}
static int resident317_begin_impl(const NFHardwareBatchDraw248 *list,unsigned count,uint64_t *cookie){
 if(r313.phase||r313_serial==UINT64_MAX||!r313_config())return 0;
 unsigned proof_control=validation267_control();
 if(!batch_preflight248(list,count))return 0;
 unsigned proof_status=validation267_status();VALIDATION267_HOOK(0);
 if(proof_control!=r313_control()||!r313_config())return 0;
 if(!r313_cookie_span(list,count,cookie))return 0;
 unsigned pw=prepared_width234,ph=prepared_height234;
 int ready=nf_hw_batch234_prepare(640,480)&&depth272_prepare();
 prepared_width234=pw;prepared_height234=ph;
 if(!ready||proof_control!=r313_control()||!r313_config())return 0;
 if(!r313_prepare()){r313_release();return 0;}
 if(proof_control!=r313_control()||!r313_config()){r313_release();return 0;}
 r313.phase=1;r313.cookie=++r313_serial;r313.thread=GetCurrentThreadId();r313.control=proof_control;
 for(unsigned l=0;l<2;l++){r313.out[l*2]=list[0].states[l].color;r313.out[l*2+1]=list[0].states[l].depth;}
 if(!r313_segment(list,count,proof_control,proof_status))return r313_retire_failure();
 r313.count=count;r313.segments=1;*cookie=r313.cookie;return 1;
}
static int resident317_append_impl(uint64_t cookie,const NFHardwareBatchDraw248 *list,unsigned count){
 if(!r313_identity(cookie)||!r313_config()||!r313_kernels()||r313.control!=r313_control()||
    (!r313_unbounded450&&(r313.segments>=2||count>64-r313.count))||!count)return 0; /*450: PC-mode sessions span many flushes*/
 unsigned proof_control=validation267_control();
 if(!batch_preflight248(list,count))return 0;
 unsigned proof_status=validation267_status();VALIDATION267_HOOK(0);
 if(r313.control!=r313_control())return 0;
 for(unsigned l=0;l<2;l++)if(list[0].states[l].color!=r313.out[l*2]||list[0].states[l].depth!=r313.out[l*2+1])return 0;
 if(!r313_segment(list,count,proof_control,proof_status))return r313_retire_failure();
 r313.count+=count;r313.segments++;return 1;
}
static int resident313_abort(uint64_t cookie){
 if(!r313_identity(cookie))return 0;
 if(!r313_join())return -1;
 r315_recycle();return 1;
}
static int resident317_finish_impl(uint64_t cookie){
 if(!r313_identity(cookie))return 0;
 if(!r313_config()||!r313_kernels()||r313.control!=r313_control())return r313_retire_failure();
 for(unsigned i=0;i<4;i++)if(!pair_span234(r313.out[i],640*480*4,1))return r313_retire_failure();
 ID3D11Resource *resources[4]={0};D3D11_MAPPED_SUBRESOURCE maps[4]={{0}};unsigned mapped=0;int ok=0;
 ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);memset(&bound,0,sizeof bound);
 for(unsigned l=0;l<2;l++){
  R313Lane*p=&r313.lane[l];
  resources[l*2]=(ID3D11Resource*)p->surface.color_read;resources[l*2+1]=(ID3D11Resource*)p->surface.depth_transfer;
  ID3D11DeviceContext_CopyResource(ctx,resources[l*2],(ID3D11Resource*)p->surface.color);
  ID3D11DeviceContext_CopyResource(ctx,resources[l*2+1],(ID3D11Resource*)p->ping.slots[p->ping.at].texture);
  if(!l)ID3D11DeviceContext_Flush(ctx);
 }
 for(unsigned i=0;i<4;i++){
  if(pair_failure234(i+3)||FAILED(ID3D11DeviceContext_Map(ctx,resources[i],0,D3D11_MAP_READ,0,&maps[i])))goto done;
  mapped|=1u<<i;if(!maps[i].pData||maps[i].RowPitch<2560)goto done;
 }
 for(unsigned l=0;l<2;l++)for(unsigned y=0;y<480;y++){
  memcpy(r313.out[l*2]+y*2560,(const char*)maps[l*2].pData+(size_t)y*maps[l*2].RowPitch,2560);
  nf_depth_pack((uint32_t*)(r313.out[l*2+1]+y*2560),(const float*)((const char*)maps[l*2+1].pData+(size_t)y*maps[l*2+1].RowPitch),640);
 }
 transfers+=2;ok=1;
done:
 for(unsigned i=0;i<4;i++)if(mapped&(1u<<i))ID3D11DeviceContext_Unmap(ctx,resources[i],0);
 if(!ok)return r313_retire_failure();
 r315_recycle();return 1;
}
/* Permission evidence is fresh for each API, even within the composite caller.
 * Never accept a caller's borrowed scope or keep this automatic object alive. */
static int resident313_begin(const NFHardwareBatchDraw248*list,unsigned count,uint64_t*cookie){
 if(r313.phase||nf_host_current261||resident317_host)return 0;
 if(!nf_host_enabled261())return resident317_begin_impl(list,count,cookie);
 NFHostScope261 scope;nf_host_enter261(&scope);resident317_host=&scope;
 int result=resident317_begin_impl(list,count,cookie);
 resident317_host=NULL;nf_host_leave261(&scope,result);return result;
}
static int resident313_append(uint64_t cookie,const NFHardwareBatchDraw248*list,unsigned count){
 if(!r313_identity(cookie)||nf_host_current261||resident317_host)return 0;
 if(!nf_host_enabled261())return resident317_append_impl(cookie,list,count);
 NFHostScope261 scope;nf_host_enter261(&scope);resident317_host=&scope;
 int result=resident317_append_impl(cookie,list,count);
 resident317_host=NULL;nf_host_leave261(&scope,result);return result;
}
static int resident313_finish(uint64_t cookie){
 if(!r313_identity(cookie)||nf_host_current261||resident317_host)return 0;
 if(!nf_host_enabled261())return resident317_finish_impl(cookie);
 NFHostScope261 scope;nf_host_enter261(&scope);resident317_host=&scope;
 int result=resident317_finish_impl(cookie);
 resident317_host=NULL;nf_host_leave261(&scope,result);return result;
}
#endif

static int resident315_setting=-1;
int nf_hw_material_retained315(const NFHardwareBatchDraw248 *list,unsigned count){
 if(resident315_setting<0){DWORD e=GetLastError();const char*v=getenv("DRIVING_RESIDENT315");resident315_setting=v&&!strcmp(v,"1");SetLastError(e);}
 if(!resident315_setting||count<2||!r313_config())return nf_hw_material_batch248(list,count);
 /*319: begin checks the WHOLE original flush before submitting any work.
  * The split interface remains independently checked, but production does not
  * need an artificial split or a duplicate outer scan. Preserve lane order. */
 uint64_t cookie=0;
 int result=resident313_begin(list,count,&cookie);
 if(!result)return nf_hw_material_batch248(list,count);
 if(result<0)return -1;
 result=resident313_finish(cookie);
 if(result>0){static uint64_t completed;++completed;if(completed==1||!(completed%120))fprintf(stderr,"[RESIDENT315] sequences=%llu resource_creations=%llu resource_reuses=%llu within_original_flush=1\n",(unsigned long long)completed,(unsigned long long)pool315_created,(unsigned long long)pool315_reused);}
 return result;
}

#else
static int resident313_blocked(void){return 0;}
int nf_hw_material_retained315(const NFHardwareBatchDraw248 *list,unsigned count){return nf_hw_material_batch248(list,count);}
#endif
