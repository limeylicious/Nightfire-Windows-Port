#ifndef NIGHTFIRE_SHARED339_H
#define NIGHTFIRE_SHARED339_H
#if defined(NF_PAIR276_AVAILABLE) && defined(NF_DEPTH_PING272_AVAILABLE) && defined(NIGHTFIRE_MATERIAL_RING246) && defined(NIGHTFIRE_PAIR_BATCH248) && defined(NIGHTFIRE_BATCH234)
/*339 isolated count-one transaction. Retains original uploads and CPU checks.
 * A scoped deferred Draw never survives the existing248 call. No guest pointer
 * or resource is retained beyond that call. Default OFF: DRIVING_SHARED_SUBMIT339=1 is required outside tests. */
static ID3D11Texture2D *tx339_color,*tx339_depth;
static ID3D11RenderTargetView *tx339_rtv;
static ID3D11DepthStencilView *tx339_dsv;
static ID3D11GeometryShader *tx339_gs;
static ID3D11Buffer *tx339_cb;
static int tx339_active;
static int transaction_plain411(void){return !tx339_active;}
#ifdef NF_PAIR234_TEST
static int tx339_enabled,tx339_fail_create,tx339_fail_resource;
static void (*tx339_test_hook)(unsigned);
static int tx339_option(void){return tx339_enabled;}
#define TX339_FAULT(stage) (tx339_fail_resource==(stage))
#define TX339_OBSERVE(stage) do{if(tx339_test_hook)tx339_test_hook(stage);}while(0)
#else
static int tx339_option(void){static int on=-1;if(on<0){NBGuard334 guard;nb334_save(&guard);const char*v=getenv("DRIVING_SHARED_SUBMIT339");on=v&&!strcmp(v,"1");nb334_restore(&guard);}return on;}
#define TX339_FAULT(stage) 0
#define TX339_OBSERVE(stage) ((void)0)
#endif
static unsigned tx339_n,tx339_start,tx339_seen,tx339_queued;
static uint64_t tx339_admitted,tx339_fused,tx339_refused,tx339_failed;
static ID3D11Texture2D *tx339_colors[2],*tx339_depths[2];
static ID3D11Resource *tx339_copy_dst[2],*tx339_copy_src[2];
static const char tx339_code[]=
"struct P{float4 p:SV_POSITION;float4 uv0:TEXCOORD0;float4 uv1:TEXCOORD1;float4 uv2:TEXCOORD2;float4 uv3:TEXCOORD3;float4 c:COLOR0;float4 spec:COLOR1;float fog:TEXCOORD4;};"
"struct O{float4 p:SV_POSITION;float4 uv0:TEXCOORD0;float4 uv1:TEXCOORD1;float4 uv2:TEXCOORD2;float4 uv3:TEXCOORD3;float4 c:COLOR0;float4 spec:COLOR1;float fog:TEXCOORD4;uint lane:SV_RenderTargetArrayIndex;};"
"cbuffer Route:register(b7){uint trianglesPerLane;uint unused0;uint unused1;uint unused2;}"
"[maxvertexcount(3)] void geometry(triangle P v[3],uint id:SV_PrimitiveID,inout TriangleStream<O> s){"
"uint lane=id/trianglesPerLane;[unroll]for(uint i=0;i<3;i++){O o;o.p=v[i].p;o.uv0=v[i].uv0;o.uv1=v[i].uv1;o.uv2=v[i].uv2;o.uv3=v[i].uv3;o.c=v[i].c;o.spec=v[i].spec;o.fog=v[i].fog;o.lane=lane;s.Append(o);}}";
static void tx339_release(void){RELEASE(tx339_rtv);RELEASE(tx339_dsv);RELEASE(tx339_color);RELEASE(tx339_depth);RELEASE(tx339_gs);RELEASE(tx339_cb);}
static int tx339_create(void){
#ifdef NF_PAIR234_TEST
 if(tx339_fail_create)return 0;
#endif
 if(tx339_cb)return 1;
 HRESULT hr=E_OUTOFMEMORY;ID3DBlob *code=NULL,*err=NULL;
 D3D11_TEXTURE2D_DESC t={0};t.Width=640;t.Height=480;t.MipLevels=1;t.ArraySize=2;t.SampleDesc.Count=1;
 t.Format=DXGI_FORMAT_B8G8R8A8_UNORM;t.BindFlags=D3D11_BIND_RENDER_TARGET;
 if(TX339_FAULT(1)||FAILED(hr=ID3D11Device_CreateTexture2D(dev,&t,NULL,&tx339_color)))goto fail;
 D3D11_RENDER_TARGET_VIEW_DESC rd={0};rd.Format=t.Format;rd.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;rd.Texture2DArray.ArraySize=2;
 hr=E_OUTOFMEMORY;if(TX339_FAULT(2)||FAILED(hr=ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)tx339_color,&rd,&tx339_rtv)))goto fail;
 t.Format=DXGI_FORMAT_R32_TYPELESS;t.BindFlags=D3D11_BIND_DEPTH_STENCIL;
 hr=E_OUTOFMEMORY;if(TX339_FAULT(3)||FAILED(hr=ID3D11Device_CreateTexture2D(dev,&t,NULL,&tx339_depth)))goto fail;
 D3D11_DEPTH_STENCIL_VIEW_DESC zd={0};zd.Format=DXGI_FORMAT_D32_FLOAT;zd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2DARRAY;zd.Texture2DArray.ArraySize=2;
 hr=E_OUTOFMEMORY;if(TX339_FAULT(4)||FAILED(hr=ID3D11Device_CreateDepthStencilView(dev,(ID3D11Resource*)tx339_depth,&zd,&tx339_dsv)))goto fail;
 hr=E_OUTOFMEMORY;if(TX339_FAULT(5)||FAILED(hr=D3DCompile(tx339_code,sizeof tx339_code-1,"shared339-gs",NULL,NULL,"geometry","gs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&err)))goto fail;
 hr=E_OUTOFMEMORY;if(TX339_FAULT(6)||FAILED(hr=ID3D11Device_CreateGeometryShader(dev,ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),NULL,&tx339_gs)))goto fail;
 D3D11_BUFFER_DESC b={0};b.ByteWidth=16;b.Usage=D3D11_USAGE_DEFAULT;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
 hr=E_OUTOFMEMORY;if(TX339_FAULT(7)||FAILED(hr=ID3D11Device_CreateBuffer(dev,&b,NULL,&tx339_cb)))goto fail;
 RELEASE(code);RELEASE(err);return 1;
fail:
 fprintf(stderr,"[TX339] extra resource preparation refused HRESULT=%08lx\n",(unsigned long)hr);
 RELEASE(code);RELEASE(err);tx339_release();return 0;
}
static void tx339_enter(const NFHardwareBatchDraw248*list,unsigned count,int config){
 if(!tx339_option())return;
 NBGuard334 guard;nb334_save(&guard);
 /* Full original preflight has ALREADY run. Padding inequality only refuses.
  * Same descriptor pointer plus serialized owned packet236 snapshots ensures
  * texture bytes cannot change between lanes within this existing call. */
 int admit=config&&count==1&&list[0].count&&list[0].count<=16384&&list[0].count%3==0&&!tx339_active&&(_mm_getcsr()&_MM_MASK_MASK)==_MM_MASK_MASK;
 /* Original optional first-lane Flush and instrumentation remain meaningful.
  * Do not admit a configuration which requests either of them. */
 const char*omit=getenv("DRIVING_NO_LANE_FLUSH275");if(!omit||strcmp(omit,"1"))admit=0;
 const char*probes[]={"DRIVING_GPU_SAMPLE254","DRIVING_GPU_SAMPLE297","DRIVING_BEGIN_PROBE334","DRIVING_IMPORT_PROBE335"};
 for(unsigned i=0;i<sizeof probes/sizeof*probes;i++){const char*v=getenv(probes[i]);if(v&&!strcmp(v,"1"))admit=0;}
 if(admit){NFHardwareState a=list[0].states[0],b=list[0].states[1];a.color=b.color=NULL;a.depth=b.depth=NULL;admit=!memcmp(&a,&b,sizeof a);}
 if(admit)admit=tx339_create();
 if(admit){tx339_active=1;tx339_n=list[0].count;tx339_seen=tx339_queued=0;tx339_admitted++;}
 else tx339_refused++;
 nb334_restore(&guard);
}
/* Called ONLY after original per-lane validation and original VB Map/Unmap.
 *0 ordinary draw;1 deferred/shared draw;-1 actual late failure, never fallback. */
static int tx339_draw(unsigned n,unsigned start){
 if(!tx339_active)return 0;
 NBGuard334 guard;nb334_save(&guard);int result=-1;
 if(n!=tx339_n||tx339_seen>1||bound.geometry||!bound.valid||bound.vertex!=material_vs221||width!=640||height!=480)goto done;
 if(!tx339_seen){
  /* Refuse before deferring any draw if the second original upload would wrap
   * or discard the first list. No new upload, offset calculation or FP math. */
  if(!material_ring246||material_capacity246!=MATERIAL_RING_CAPACITY246||start>material_capacity246-2*n){tx339_active=0;tx339_refused++;result=0;goto done;}
  tx339_start=start;tx339_colors[0]=color;tx339_depths[0]=depth;tx339_seen=1;result=1;goto done;
 }
 if(start!=tx339_start+n||tx339_queued!=2||color==tx339_colors[0]||depth==tx339_depths[0])goto done;
 tx339_colors[1]=color;tx339_depths[1]=depth;
 /* Keep original immediate-context state and cache coherent. The ordinary
  * slot0 VB remains bound. Only OM targets, GS and GS b7 are temporarily changed. */
 ID3D11Buffer *oldcb=NULL;ID3D11DeviceContext_GSGetConstantBuffers(ctx,7,1,&oldcb);
 ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);
 for(unsigned lane=0;lane<2;lane++){
  ID3D11DeviceContext_CopySubresourceRegion(ctx,(ID3D11Resource*)tx339_color,lane,0,0,0,(ID3D11Resource*)tx339_colors[lane],0,NULL);
  ID3D11DeviceContext_CopySubresourceRegion(ctx,(ID3D11Resource*)tx339_depth,lane,0,0,0,(ID3D11Resource*)tx339_depths[lane],0,NULL);
 }
 unsigned route[4]={n/3,0,0,0};ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)tx339_cb,0,NULL,route,0,0);
 ID3D11DeviceContext_GSSetConstantBuffers(ctx,7,1,&tx339_cb);ID3D11DeviceContext_GSSetShader(ctx,tx339_gs,NULL,0);
 ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&tx339_rtv,tx339_dsv);
 TX339_OBSERVE(1);
 ID3D11DeviceContext_Draw(ctx,2*n,tx339_start);tx339_fused++;
 ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);
 for(unsigned lane=0;lane<2;lane++){
  ID3D11DeviceContext_CopySubresourceRegion(ctx,(ID3D11Resource*)tx339_colors[lane],0,0,0,0,(ID3D11Resource*)tx339_color,lane,NULL);
  ID3D11DeviceContext_CopySubresourceRegion(ctx,(ID3D11Resource*)tx339_depths[lane],0,0,0,0,(ID3D11Resource*)tx339_depth,lane,NULL);
 }
 for(unsigned i=0;i<2;i++)ID3D11DeviceContext_CopyResource(ctx,tx339_copy_dst[i],tx339_copy_src[i]);
 ID3D11DeviceContext_GSSetShader(ctx,NULL,NULL,0);ID3D11DeviceContext_GSSetConstantBuffers(ctx,7,1,&oldcb);RELEASE(oldcb);
 ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&rtv,dsv);
 TX339_OBSERVE(2);
 tx339_seen=2;result=1;
done:
 nb334_restore(&guard);return result;
}
static void tx339_copy(ID3D11Resource*dst,ID3D11Resource*src){
 if(tx339_active&&tx339_seen==1&&tx339_queued<2){tx339_copy_dst[tx339_queued]=dst;tx339_copy_src[tx339_queued++]=src;return;}
 ID3D11DeviceContext_CopyResource(ctx,dst,src);
}
static void tx339_leave(int ok){if(!tx339_active)return;if(!ok)tx339_failed++;tx339_active=0;tx339_seen=tx339_queued=0;memset(tx339_colors,0,sizeof tx339_colors);memset(tx339_depths,0,sizeof tx339_depths);memset(tx339_copy_src,0,sizeof tx339_copy_src);memset(tx339_copy_dst,0,sizeof tx339_copy_dst);if(tx339_admitted==1||!(tx339_admitted%120)){NBGuard334 guard;nb334_save(&guard);fprintf(stderr,"[SHARED339] admitted=%llu fused_draws=%llu refused=%llu failed=%llu count_one_only=1 original_publication=1\n",(unsigned long long)tx339_admitted,(unsigned long long)tx339_fused,(unsigned long long)tx339_refused,(unsigned long long)tx339_failed);nb334_restore(&guard);}}

#else
static int transaction_plain411(void){return 1;}
static int tx339_draw(unsigned n,unsigned start){(void)n;(void)start;return 0;}
static void tx339_enter(const NFHardwareBatchDraw248*l,unsigned n,int config){(void)l;(void)n;(void)config;}
static void tx339_copy(ID3D11Resource*dst,ID3D11Resource*src){ID3D11DeviceContext_CopyResource(ctx,dst,src);}
static void tx339_leave(int ok){(void)ok;}
#endif
#endif
