#ifndef NIGHTFIRE_SHARED341_H
#define NIGHTFIRE_SHARED341_H
#if defined(NF_PAIR276_AVAILABLE) && defined(NF_DEPTH_PING272_AVAILABLE) && defined(NIGHTFIRE_MATERIAL_RING246) && defined(NIGHTFIRE_PAIR_BATCH248) && defined(NIGHTFIRE_BATCH234)
/*341: optional count-one array backing within the original248 flush. Original
 * inputs, lane begins, VB uploads, staging Maps and caller publication remain.
 * Private cached COM resources outlive calls; NO guest pointer or authoritative
 * guest content does. Original pair resource identities remain untouched.
 * Thirteen optional preparation failures clean up and use the original path.
 * Count>1 and incompatible depth/seed modes never select this backing.
 * Default OFF: DRIVING_SLICE_BACKING341=1. No ownership or handoff extension. */
static ID3D11Texture2D *tx341_color,*tx341_depth;
static ID3D11RenderTargetView *tx341_rtv;
static ID3D11DepthStencilView *tx341_dsv;
static ID3D11GeometryShader *tx341_gs;
static ID3D11Buffer *tx341_cb;
static int tx341_active;
/* Separate scope: ring refusal can disable fusion after array imports. It must
 * retain the slice selection through the remaining original draws/readbacks. */
static int backing341_active;static unsigned backing341_lane;
static ID3D11RenderTargetView*backing341_rtv[2];
static ID3D11DepthStencilView*backing341_dsv[2];
static ID3D11ShaderResourceView*backing341_srv[2];
static uint64_t backing341_imports,backing341_depth_copies,backing341_reads;
static void backing341_upload(const NFHardwareState*s){
 ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)color,backing341_active?backing341_lane:0,NULL,s->color,s->pitch,0);
 backing341_imports+=backing341_active!=0;
}
static void backing341_depth_upload(void){
 if(backing341_active){ID3D11DeviceContext_CopySubresourceRegion(ctx,(ID3D11Resource*)depth,backing341_lane,0,0,0,(ID3D11Resource*)depth_transfer,0,NULL);backing341_depth_copies++;}
 else ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)depth,(ID3D11Resource*)depth_transfer);
}
static void backing341_read(ID3D11Resource*dst,ID3D11Resource*src,unsigned slice){
 if(backing341_active){ID3D11DeviceContext_CopySubresourceRegion(ctx,dst,0,0,0,0,src,slice,NULL);backing341_reads++;}
 else ID3D11DeviceContext_CopyResource(ctx,dst,src);
}
static void backing341_select(NFPairSurface234*s,unsigned lane){
 if(!backing341_active)return;
 backing341_lane=lane;s->color=tx341_color;s->depth=tx341_depth;
 s->rtv=backing341_rtv[lane];s->dsv=backing341_dsv[lane];s->depth_srv256=backing341_srv[lane];
}
#ifdef NF_PAIR234_TEST
static int tx341_enabled,tx341_fail_create,tx341_fail_resource;
static void (*tx341_test_hook)(unsigned);
static int tx341_option(void){return tx341_enabled;}
#define TX341_FAULT(stage) (tx341_fail_resource==(stage))
#define TX341_OBSERVE(stage) do{if(tx341_test_hook)tx341_test_hook(stage);}while(0)
#else
static int tx341_option(void){static int on=-1;if(on<0){NBGuard334 guard;nb334_save(&guard);const char*v=getenv("DRIVING_SLICE_BACKING341");on=v&&!strcmp(v,"1");nb334_restore(&guard);}return on;}
#define TX341_FAULT(stage) 0
#define TX341_OBSERVE(stage) ((void)0)
#endif
static unsigned tx341_n,tx341_start,tx341_seen,tx341_queued;
static uint64_t tx341_admitted,tx341_fused,tx341_refused,tx341_failed;
static ID3D11Texture2D *tx341_colors[2],*tx341_depths[2];
static ID3D11Resource *tx341_copy_dst[2],*tx341_copy_src[2];
static const char tx341_code[]=
"struct P{float4 p:SV_POSITION;float4 uv0:TEXCOORD0;float4 uv1:TEXCOORD1;float4 uv2:TEXCOORD2;float4 uv3:TEXCOORD3;float4 c:COLOR0;float4 spec:COLOR1;float fog:TEXCOORD4;};"
"struct O{float4 p:SV_POSITION;float4 uv0:TEXCOORD0;float4 uv1:TEXCOORD1;float4 uv2:TEXCOORD2;float4 uv3:TEXCOORD3;float4 c:COLOR0;float4 spec:COLOR1;float fog:TEXCOORD4;uint lane:SV_RenderTargetArrayIndex;};"
"cbuffer Route:register(b7){uint trianglesPerLane;uint unused0;uint unused1;uint unused2;}"
"[maxvertexcount(3)] void geometry(triangle P v[3],uint id:SV_PrimitiveID,inout TriangleStream<O> s){"
"uint lane=id/trianglesPerLane;[unroll]for(uint i=0;i<3;i++){O o;o.p=v[i].p;o.uv0=v[i].uv0;o.uv1=v[i].uv1;o.uv2=v[i].uv2;o.uv3=v[i].uv3;o.c=v[i].c;o.spec=v[i].spec;o.fog=v[i].fog;o.lane=lane;s.Append(o);}}";
static void tx341_release(void){for(unsigned l=0;l<2;l++){RELEASE(backing341_rtv[l]);RELEASE(backing341_dsv[l]);RELEASE(backing341_srv[l]);}RELEASE(tx341_rtv);RELEASE(tx341_dsv);RELEASE(tx341_color);RELEASE(tx341_depth);RELEASE(tx341_gs);RELEASE(tx341_cb);}
static int tx341_create(void){
#ifdef NF_PAIR234_TEST
 if(tx341_fail_create)return 0;
#endif
 if(tx341_cb&&backing341_srv[1])return 1;
 HRESULT hr=E_OUTOFMEMORY;ID3DBlob *code=NULL,*err=NULL;
 D3D11_TEXTURE2D_DESC t={0};t.Width=640;t.Height=480;t.MipLevels=1;t.ArraySize=2;t.SampleDesc.Count=1;
 t.Format=DXGI_FORMAT_B8G8R8A8_UNORM;t.BindFlags=D3D11_BIND_RENDER_TARGET;
 if(TX341_FAULT(1)||FAILED(hr=ID3D11Device_CreateTexture2D(dev,&t,NULL,&tx341_color)))goto fail;
 D3D11_RENDER_TARGET_VIEW_DESC rd={0};rd.Format=t.Format;rd.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;rd.Texture2DArray.ArraySize=2;
 hr=E_OUTOFMEMORY;if(TX341_FAULT(2)||FAILED(hr=ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)tx341_color,&rd,&tx341_rtv)))goto fail;
 t.Format=DXGI_FORMAT_R32_TYPELESS;t.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
 hr=E_OUTOFMEMORY;if(TX341_FAULT(3)||FAILED(hr=ID3D11Device_CreateTexture2D(dev,&t,NULL,&tx341_depth)))goto fail;
 D3D11_DEPTH_STENCIL_VIEW_DESC zd={0};zd.Format=DXGI_FORMAT_D32_FLOAT;zd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2DARRAY;zd.Texture2DArray.ArraySize=2;
 hr=E_OUTOFMEMORY;if(TX341_FAULT(4)||FAILED(hr=ID3D11Device_CreateDepthStencilView(dev,(ID3D11Resource*)tx341_depth,&zd,&tx341_dsv)))goto fail;
 hr=E_OUTOFMEMORY;if(TX341_FAULT(5)||FAILED(hr=D3DCompile(tx341_code,sizeof tx341_code-1,"shared341-gs",NULL,NULL,"geometry","gs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&err)))goto fail;
 hr=E_OUTOFMEMORY;if(TX341_FAULT(6)||FAILED(hr=ID3D11Device_CreateGeometryShader(dev,ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),NULL,&tx341_gs)))goto fail;
 D3D11_BUFFER_DESC b={0};b.ByteWidth=16;b.Usage=D3D11_USAGE_DEFAULT;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
 hr=E_OUTOFMEMORY;if(TX341_FAULT(7)||FAILED(hr=ID3D11Device_CreateBuffer(dev,&b,NULL,&tx341_cb)))goto fail;
 /* Per-lane import/clear views plus the full-array fused-draw view. */
 for(unsigned l=0;l<2;l++){
  rd.Texture2DArray.FirstArraySlice=l;rd.Texture2DArray.ArraySize=1;
  hr=E_OUTOFMEMORY;if(TX341_FAULT(8+3*l)||FAILED(hr=ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)tx341_color,&rd,&backing341_rtv[l])))goto fail;
  zd.Texture2DArray.FirstArraySlice=l;zd.Texture2DArray.ArraySize=1;
  hr=E_OUTOFMEMORY;if(TX341_FAULT(9+3*l)||FAILED(hr=ID3D11Device_CreateDepthStencilView(dev,(ID3D11Resource*)tx341_depth,&zd,&backing341_dsv[l])))goto fail;
  D3D11_SHADER_RESOURCE_VIEW_DESC sd={0};sd.Format=DXGI_FORMAT_R32_FLOAT;sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
  sd.Texture2DArray.MipLevels=1;sd.Texture2DArray.FirstArraySlice=l;sd.Texture2DArray.ArraySize=1;
  hr=E_OUTOFMEMORY;if(TX341_FAULT(10+3*l)||FAILED(hr=ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)tx341_depth,&sd,&backing341_srv[l])))goto fail;
 }
 RELEASE(code);RELEASE(err);return 1;
fail:
 fprintf(stderr,"[TX341] extra resource preparation refused HRESULT=%08lx\n",(unsigned long)hr);
 RELEASE(code);RELEASE(err);tx341_release();return 0;
}
static void tx341_enter(const NFHardwareBatchDraw248*list,unsigned count,int config){
 if(!tx341_option()||tx339_option())return;
 NBGuard334 guard;nb334_save(&guard);
 /* Full original preflight has ALREADY run. Padding inequality only refuses.
  * Same descriptor pointer plus serialized owned packet236 snapshots ensures
  * texture bytes cannot change between lanes within this existing call. */
 int admit=config&&count==1&&list[0].count&&list[0].count<=16384&&list[0].count%3==0&&!tx341_active&&(_mm_getcsr()&_MM_MASK_MASK)==_MM_MASK_MASK;
 /* Original optional first-lane Flush and instrumentation remain meaningful.
  * Do not admit a configuration which requests either of them. */
 const char*omit=getenv("DRIVING_NO_LANE_FLUSH275");if(!omit||strcmp(omit,"1"))admit=0;
 const char*probes[]={"DRIVING_GPU_SAMPLE254","DRIVING_GPU_SAMPLE297","DRIVING_BEGIN_PROBE334","DRIVING_IMPORT_PROBE335"};
 for(unsigned i=0;i<sizeof probes/sizeof*probes;i++){const char*v=getenv(probes[i]);if(v&&!strcmp(v,"1"))admit=0;}
 if(admit){NFHardwareState a=list[0].states[0],b=list[0].states[1];a.color=b.color=NULL;a.depth=b.depth=NULL;admit=!memcmp(&a,&b,sizeof a);}
 if(admit)admit=!list[0].states[0].color_layout&&!list[0].states[1].color_layout;
 if(admit)admit=tx341_create();
 if(admit){backing341_active=1;backing341_lane=0;tx341_active=1;tx341_n=list[0].count;tx341_seen=tx341_queued=0;tx341_admitted++;}
 else tx341_refused++;
 nb334_restore(&guard);
}
/* Called ONLY after original per-lane validation and original VB Map/Unmap.
 *0 ordinary draw;1 deferred/shared draw;-1 actual late failure, never fallback. */
static int tx341_draw(unsigned n,unsigned start){
 if(!tx341_active)return 0;
 NBGuard334 guard;nb334_save(&guard);int result=-1;
 if(n!=tx341_n||tx341_seen>1||bound.geometry||!bound.valid||bound.vertex!=material_vs221||width!=640||height!=480)goto done;
 if(!tx341_seen){
  /* Refuse before deferring any draw if the second original upload would wrap
   * or discard the first list. No new upload, offset calculation or FP math. */
  if(!material_ring246||material_capacity246!=MATERIAL_RING_CAPACITY246||start>material_capacity246-2*n){tx341_active=0;tx341_refused++;result=0;goto done;}
  tx341_start=start;tx341_colors[0]=color;tx341_depths[0]=depth;tx341_seen=1;result=1;goto done;
 }
 if(start!=tx341_start+n||tx341_queued!=2||!backing341_active||backing341_lane!=1||color!=tx341_color||depth!=tx341_depth||tx341_colors[0]!=tx341_color||tx341_depths[0]!=tx341_depth)goto done;
 tx341_colors[1]=color;tx341_depths[1]=depth;
 /* Keep original immediate-context state and cache coherent. The ordinary
  * slot0 VB remains bound. Only OM targets, GS and GS b7 are temporarily changed. */
 ID3D11Buffer *oldcb=NULL;ID3D11DeviceContext_GSGetConstantBuffers(ctx,7,1,&oldcb);
 ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);
 /* Both slices already contain their original imports. */
 unsigned route[4]={n/3,0,0,0};ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)tx341_cb,0,NULL,route,0,0);
 ID3D11DeviceContext_GSSetConstantBuffers(ctx,7,1,&tx341_cb);ID3D11DeviceContext_GSSetShader(ctx,tx341_gs,NULL,0);
 ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&tx341_rtv,tx341_dsv);
 TX341_OBSERVE(1);
 ID3D11DeviceContext_Draw(ctx,2*n,tx341_start);tx341_fused++;
 ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);
 for(unsigned i=0;i<2;i++)backing341_read(tx341_copy_dst[i],tx341_copy_src[i],0);
 ID3D11DeviceContext_GSSetShader(ctx,NULL,NULL,0);ID3D11DeviceContext_GSSetConstantBuffers(ctx,7,1,&oldcb);RELEASE(oldcb);
 ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&rtv,dsv);
 TX341_OBSERVE(2);
 tx341_seen=2;result=1;
done:
 nb334_restore(&guard);return result;
}
static void tx341_copy(ID3D11Resource*dst,ID3D11Resource*src){
 if(tx341_active&&tx341_seen==1&&tx341_queued<2){tx341_copy_dst[tx341_queued]=dst;tx341_copy_src[tx341_queued++]=src;return;}
 backing341_read(dst,src,backing341_lane);
}
static void tx341_leave(int ok){backing341_active=0;backing341_lane=0;if(!tx341_active)return;if(!ok)tx341_failed++;tx341_active=0;tx341_seen=tx341_queued=0;memset(tx341_colors,0,sizeof tx341_colors);memset(tx341_depths,0,sizeof tx341_depths);memset(tx341_copy_src,0,sizeof tx341_copy_src);memset(tx341_copy_dst,0,sizeof tx341_copy_dst);if(tx341_admitted==1||!(tx341_admitted%120)){NBGuard334 guard;nb334_save(&guard);fprintf(stderr,"[SHARED341] admitted=%llu fused_draws=%llu refused=%llu failed=%llu count_one_only=1 original_publication=1\n",(unsigned long long)tx341_admitted,(unsigned long long)tx341_fused,(unsigned long long)tx341_refused,(unsigned long long)tx341_failed);nb334_restore(&guard);}}

#else
static int backing341_active;
static void backing341_upload(const NFHardwareState*s){ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)color,0,NULL,s->color,s->pitch,0);}
static void backing341_depth_upload(void){ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)depth,(ID3D11Resource*)depth_transfer);}
#ifdef NF_PAIR276_AVAILABLE
static void backing341_select(NFPairSurface234*s,unsigned lane){(void)s;(void)lane;}
#endif
static int tx341_draw(unsigned n,unsigned start){(void)n;(void)start;return 0;}
static void tx341_enter(const NFHardwareBatchDraw248*l,unsigned n,int config){(void)l;(void)n;(void)config;}
static void tx341_copy(ID3D11Resource*dst,ID3D11Resource*src){ID3D11DeviceContext_CopyResource(ctx,dst,src);}
static void tx341_leave(int ok){(void)ok;}
#endif
static void backing341_copy(ID3D11Resource*dst,ID3D11Resource*src){if(backing341_active)tx341_copy(dst,src);else tx339_copy(dst,src);}
#endif
