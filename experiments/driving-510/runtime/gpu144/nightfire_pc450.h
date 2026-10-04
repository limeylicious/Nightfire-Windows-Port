/* Checkpoint450 "PC mode": drain-scoped GPU residency of the main colour/depth
 * pair, built on Astra's resident313/315/317 retained-session API.
 *
 * Normal (faithful) path: every batch flush imports the pair from guest RAM,
 * draws, reads the result back and publishes it to guest RAM (~55x per frame).
 * PC mode keeps ONE retained session open across the flushes of a drain that do
 * not need guest RAM (capacity, clears of the main pair, semaphore methods) and
 * publishes once when anything else needs RAM, and always at drain exit (the
 * game's own thread is inside the drain, so it sees current RAM when it resumes).
 * Visual parity target, not a proof of Xbox bit-exactness. Default OFF:
 * DRIVING_PC450=1 enables it (requires COLOR_SEED276=0, OWNED253=1, 272 on).
 * While a session is open every other hardware entry point refuses
 * (resident313_blocked), so a missed publication point stops loudly instead of
 * drawing from stale memory. Included by nightfire_hardware.c after resident315.h. */
#ifndef NIGHTFIRE_PC450_H
#define NIGHTFIRE_PC450_H
#if defined(NF_PAIR276_AVAILABLE) && defined(NF_DEPTH_PING272_AVAILABLE) && defined(NIGHTFIRE_PAIR_BATCH248)
static uint64_t pc450_cookie;
static struct {uint64_t begins,appends,finishes,clears,refused,failed,draws;} pc450_hw;
int nf_hw_pc450_open(void){return r313.phase==1&&pc450_cookie!=0;}
/* 1 = session opened and the list drawn into it; 0 = refused (nothing done,
 * caller uses the ordinary path); -1 = fatal (work may have been submitted). */
int nf_hw_pc450_begin(const NFHardwareBatchDraw248 *list,unsigned count){
 if(pc450_cookie||r313.phase){pc450_hw.refused++;return 0;}
 uint64_t cookie=0;r313_unbounded450=1;int r=resident313_begin(list,count,&cookie);
 if(r>0){pc450_cookie=cookie;pc450_hw.begins++;pc450_hw.draws+=count;return 1;}
 r313_unbounded450=0;
 if(r<0){pc450_hw.failed++;return -1;}
 pc450_hw.refused++;return 0;
}
int nf_hw_pc450_append(const NFHardwareBatchDraw248 *list,unsigned count){
 if(!pc450_cookie)return 0;
 int r=resident313_append(pc450_cookie,list,count);
 if(r>0){pc450_hw.appends++;pc450_hw.draws+=count;return 1;}
 if(r<0){pc450_cookie=0;r313_unbounded450=0;pc450_hw.failed++;return -1;}
 pc450_hw.refused++;return 0;
}
/* Reads the retained pair back into the caller lane buffers given at begin. */
int nf_hw_pc450_finish(void){
 if(!pc450_cookie)return 0;
 uint64_t cookie=pc450_cookie;pc450_cookie=0;
 int r=resident313_finish(cookie);r313_unbounded450=0;
 if(r>0){pc450_hw.finishes++;
  if(pc450_hw.finishes==1||!(pc450_hw.finishes%256))fprintf(stderr,"[PC450-HW] begins=%llu appends=%llu finishes=%llu gpu_clears=%llu draws=%llu refused=%llu failed=%llu\n",
   (unsigned long long)pc450_hw.begins,(unsigned long long)pc450_hw.appends,(unsigned long long)pc450_hw.finishes,(unsigned long long)pc450_hw.clears,
   (unsigned long long)pc450_hw.draws,(unsigned long long)pc450_hw.refused,(unsigned long long)pc450_hw.failed);
  return 1;}
 pc450_hw.failed++;return r<0?-1:0;
}

/* GPU clear of the retained pair inside the open session. Same rectangle, the
 * same per-channel colour mask and the same Z24 value (converted exactly as the
 * ordinary import converts guest depth) as the CPU clear it replaces. Stencil
 * stays guest-RAM owned: the caller applies the stencil part to RAM. */
static ID3D11VertexShader *pc450_vs;static ID3D11PixelShader *pc450_ps;static ID3D11Buffer *pc450_cb;
static ID3D11RasterizerState *pc450_rs;static ID3D11DepthStencilState *pc450_ds[2];static ID3D11BlendState *pc450_bs[16];
static const char pc450_code[]=
 "cbuffer C:register(b0){float4 color;float4 depth;};"
 "float4 vertex(uint id:SV_VertexID):SV_Position{float2 p=float2((id<<1)&2,id&2);return float4(p*float2(2,-2)+float2(-1,1),0,1);}"
 "void pixel(float4 p:SV_Position,out float4 c:SV_Target0,out float d:SV_Depth){c=color;d=depth.x;}";
static int pc450_prepare_clear(void){
 if(pc450_vs&&pc450_ps&&pc450_cb&&pc450_rs&&pc450_ds[0]&&pc450_ds[1]&&pc450_bs[0])return 1;
 ID3DBlob*v=NULL,*p=NULL,*e=NULL;HRESULT hr;
 hr=D3DCompile(pc450_code,sizeof pc450_code-1,"nightfire-pc450",NULL,NULL,"vertex","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&v,&e);RELEASE(e);if(FAILED(hr))goto fail;
 hr=D3DCompile(pc450_code,sizeof pc450_code-1,"nightfire-pc450",NULL,NULL,"pixel","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&p,&e);RELEASE(e);if(FAILED(hr))goto fail;
 hr=ID3D11Device_CreateVertexShader(dev,ID3D10Blob_GetBufferPointer(v),ID3D10Blob_GetBufferSize(v),NULL,&pc450_vs);if(FAILED(hr))goto fail;
 hr=ID3D11Device_CreatePixelShader(dev,ID3D10Blob_GetBufferPointer(p),ID3D10Blob_GetBufferSize(p),NULL,&pc450_ps);if(FAILED(hr))goto fail;
 {D3D11_BUFFER_DESC b={0};b.ByteWidth=32;b.Usage=D3D11_USAGE_DEFAULT;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
  hr=ID3D11Device_CreateBuffer(dev,&b,NULL,&pc450_cb);if(FAILED(hr))goto fail;}
 {D3D11_RASTERIZER_DESC r={0};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=TRUE;r.ScissorEnable=TRUE;
  hr=ID3D11Device_CreateRasterizerState(dev,&r,&pc450_rs);if(FAILED(hr))goto fail;}
 for(unsigned i=0;i<2;i++){D3D11_DEPTH_STENCIL_DESC z={0};z.DepthEnable=TRUE;z.DepthFunc=D3D11_COMPARISON_ALWAYS;
  z.DepthWriteMask=i?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
  hr=ID3D11Device_CreateDepthStencilState(dev,&z,&pc450_ds[i]);if(FAILED(hr))goto fail;}
 for(unsigned m=0;m<16;m++){D3D11_BLEND_DESC b={0};b.RenderTarget[0].BlendEnable=FALSE;b.RenderTarget[0].RenderTargetWriteMask=(UINT8)m;
  hr=ID3D11Device_CreateBlendState(dev,&b,&pc450_bs[m]);if(FAILED(hr))goto fail;}
 RELEASE(v);RELEASE(p);return 1;
fail:
 fprintf(stderr,"[PC450] clear shader/state preparation failed hr=%08lX\n",(unsigned long)hr);
 RELEASE(v);RELEASE(p);RELEASE(pc450_vs);RELEASE(pc450_ps);RELEASE(pc450_cb);RELEASE(pc450_rs);RELEASE(pc450_ds[0]);RELEASE(pc450_ds[1]);
 for(unsigned m=0;m<16;m++)RELEASE(pc450_bs[m]);return 0;
}
/* x0..x1, y0..y1 inclusive, in 640x480 lane space (both lanes, like the CPU
 * clear). flags: 1 depth, 2 stencil (ignored here), 0x10 R 0x20 G 0x40 B 0x80 A. */
int nf_hw_pc450_clear(unsigned x0,unsigned x1,unsigned y0,unsigned y1,unsigned flags,uint32_t argb,uint32_t zs){
 if(!pc450_cookie||r313.phase!=1||r313.internal||x0>x1||y0>y1||x1>=640||y1>=480)return 0;
 unsigned mask=((flags&0x10)?1u:0u)|((flags&0x20)?2u:0u)|((flags&0x40)?4u:0u)|((flags&0x80)?8u:0u);
 int write_depth=(flags&1)!=0;
 if(!mask&&!write_depth)return 1;
 if(!pc450_prepare_clear())return 0;
 /* Save every piece of pipeline state this draw touches; restore it exactly,
  * so the renderer's own cached bindings stay true. */
 ID3D11RenderTargetView*old_rtv=NULL;ID3D11DepthStencilView*old_dsv=NULL;
 ID3D11VertexShader*old_vs=NULL;ID3D11PixelShader*old_ps=NULL;ID3D11GeometryShader*old_gs=NULL;ID3D11InputLayout*old_il=NULL;D3D11_PRIMITIVE_TOPOLOGY old_topo;
 ID3D11Buffer*old_cb=NULL;ID3D11RasterizerState*old_rs=NULL;ID3D11DepthStencilState*old_ds=NULL;UINT old_ref=0;
 ID3D11BlendState*old_bs=NULL;FLOAT old_factor[4];UINT old_sample=0;
 D3D11_VIEWPORT old_vp[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];UINT nvp=D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
 D3D11_RECT old_sc[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];UINT nsc=D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
 ID3D11DeviceContext_OMGetRenderTargets(ctx,1,&old_rtv,&old_dsv);
 ID3D11DeviceContext_VSGetShader(ctx,&old_vs,NULL,NULL);ID3D11DeviceContext_PSGetShader(ctx,&old_ps,NULL,NULL);ID3D11DeviceContext_GSGetShader(ctx,&old_gs,NULL,NULL);
 ID3D11DeviceContext_IAGetInputLayout(ctx,&old_il);ID3D11DeviceContext_IAGetPrimitiveTopology(ctx,&old_topo);
 ID3D11DeviceContext_PSGetConstantBuffers(ctx,0,1,&old_cb);ID3D11DeviceContext_RSGetState(ctx,&old_rs);
 ID3D11DeviceContext_OMGetDepthStencilState(ctx,&old_ds,&old_ref);ID3D11DeviceContext_OMGetBlendState(ctx,&old_bs,old_factor,&old_sample);
 ID3D11DeviceContext_RSGetViewports(ctx,&nvp,old_vp);ID3D11DeviceContext_RSGetScissorRects(ctx,&nsc,old_sc);
 float data[8]={((argb>>16)&255)/255.0f,((argb>>8)&255)/255.0f,(argb&255)/255.0f,((argb>>24)&255)/255.0f,
                (float)((double)(zs>>8)/16777215.0),0,0,0};
 ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)pc450_cb,0,NULL,data,0,0);
 D3D11_VIEWPORT vp={0,0,640,480,0,1};D3D11_RECT sc={(LONG)x0,(LONG)y0,(LONG)x1+1,(LONG)y1+1};
 ID3D11DeviceContext_IASetInputLayout(ctx,NULL);ID3D11DeviceContext_IASetPrimitiveTopology(ctx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 ID3D11DeviceContext_VSSetShader(ctx,pc450_vs,NULL,0);ID3D11DeviceContext_PSSetShader(ctx,pc450_ps,NULL,0);ID3D11DeviceContext_GSSetShader(ctx,NULL,NULL,0);
 ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&pc450_cb);ID3D11DeviceContext_RSSetState(ctx,pc450_rs);
 ID3D11DeviceContext_RSSetViewports(ctx,1,&vp);ID3D11DeviceContext_RSSetScissorRects(ctx,1,&sc);
 ID3D11DeviceContext_OMSetDepthStencilState(ctx,pc450_ds[write_depth],0);
 {FLOAT f[4]={0,0,0,0};ID3D11DeviceContext_OMSetBlendState(ctx,pc450_bs[mask],f,0xffffffffu);}
 for(unsigned l=0;l<2;l++){
  R313Lane*p=&r313.lane[l];
  ID3D11RenderTargetView*rtv=p->surface.rtv;ID3D11DepthStencilView*dv=p->ping.slots[p->ping.at].dsv;
  ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&rtv,dv);
  ID3D11DeviceContext_Draw(ctx,3,0);
 }
 ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&old_rtv,old_dsv);
 ID3D11DeviceContext_VSSetShader(ctx,old_vs,NULL,0);ID3D11DeviceContext_PSSetShader(ctx,old_ps,NULL,0);ID3D11DeviceContext_GSSetShader(ctx,old_gs,NULL,0);
 ID3D11DeviceContext_IASetInputLayout(ctx,old_il);ID3D11DeviceContext_IASetPrimitiveTopology(ctx,old_topo);
 ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&old_cb);ID3D11DeviceContext_RSSetState(ctx,old_rs);
 ID3D11DeviceContext_OMSetDepthStencilState(ctx,old_ds,old_ref);ID3D11DeviceContext_OMSetBlendState(ctx,old_bs,old_factor,old_sample);
 if(nvp)ID3D11DeviceContext_RSSetViewports(ctx,nvp,old_vp);
 if(nsc)ID3D11DeviceContext_RSSetScissorRects(ctx,nsc,old_sc);
 RELEASE(old_rtv);RELEASE(old_dsv);RELEASE(old_vs);RELEASE(old_ps);RELEASE(old_gs);RELEASE(old_il);RELEASE(old_cb);RELEASE(old_rs);RELEASE(old_ds);RELEASE(old_bs);
 pc450_hw.clears++;return 1;
}
/*451: submit the session's queued draws now; the pair stays resident. */
void nf_hw_pc451_kick(void){if(pc450_cookie&&r313.phase==1&&!r313.internal)ID3D11DeviceContext_Flush(ctx);}
#else
int nf_hw_pc450_open(void){return 0;}
int nf_hw_pc450_begin(const NFHardwareBatchDraw248 *list,unsigned count){(void)list;(void)count;return 0;}
int nf_hw_pc450_append(const NFHardwareBatchDraw248 *list,unsigned count){(void)list;(void)count;return 0;}
int nf_hw_pc450_finish(void){return 0;}
int nf_hw_pc450_clear(unsigned x0,unsigned x1,unsigned y0,unsigned y1,unsigned flags,uint32_t argb,uint32_t zs){(void)x0;(void)x1;(void)y0;(void)y1;(void)flags;(void)argb;(void)zs;return 0;}
void nf_hw_pc451_kick(void){}
#endif
#endif
