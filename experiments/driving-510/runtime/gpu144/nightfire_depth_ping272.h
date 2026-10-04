/*272: whole-surface original-draw boundary conversion, private248 only.*/
#ifndef NIGHTFIRE_DEPTH_PING272_H
#define NIGHTFIRE_DEPTH_PING272_H
#ifdef NF_DEPTH_SRV256_AVAILABLE
#define NF_DEPTH_PING272_AVAILABLE 1
typedef struct{ID3D11Texture2D*texture;ID3D11DepthStencilView*dsv;ID3D11ShaderResourceView*srv;} NFDepthSlot272;
typedef struct{NFDepthSlot272 slots[2];unsigned at;
 /*331 proof exists only in the current private batch/lane. A skipped boundary
  * never selects the stale alternate. Only a full conversion certifies it. */
 unsigned private331,canonical331,in_draw331,plain421;ID3D11Texture2D*proof_target331;ID3D11DepthStencilView*proof_view331;
 uint64_t draws331,clears331,transfers331,uploads331,upload_clears331;
 unsigned partial332;D3D11_RECT rect332;
 /*333: after conversion, the two slots agree outside delta333. Before the
  * next boundary, only write333 can additionally differ. Unknown is NOT empty. */
 unsigned delta333_known,write333_known;D3D11_RECT delta333,write333;
} NFDepthScope272;
static NFDepthSlot272 depth272_alternate[2];static NFDepthScope272*depth272_current;
static ID3D11VertexShader*depth272_vs;static ID3D11PixelShader*depth272_ps;
static ID3D11RasterizerState*depth272_raster;static ID3D11DepthStencilState*depth272_state;
static uint64_t depth272_conversions,depth272_calls;
static uint64_t depth331_skipped,depth331_invalidated;
static uint64_t depth332_partial,depth332_pixels;
static uint64_t depth333_partial,depth333_pixels;
static void depth332_report(void);
static int depth333_enabled(void){static int on=-1;if(on<0){DWORD e=GetLastError();int crt=errno;const char*v=getenv("DRIVING_DEPTH_DELTA333");on=v&&!strcmp(v,"1");errno=crt;SetLastError(e);}return on;}
static int depth332_enabled(void){static int on=-1;if(on<0){DWORD e=GetLastError();int crt=errno;const char*v=getenv("DRIVING_DEPTH_RECT332");on=v&&!strcmp(v,"1");errno=crt;SetLastError(e);}return on;}
static int depth331_enabled(void){static int on=-1;if(on<0){DWORD e=GetLastError();int crt=errno;const char*v=getenv("DRIVING_DEPTH_CLEAN331");on=v&&!strcmp(v,"1");errno=crt;SetLastError(e);}return on;}
/*421 Isolated upper-half writer certificate. Only original248 scopes can
 * opt in after excluding other depth/submission experiments. Never certifies
 * imported guest content: the first full272 conversion is still required.
 * For every D32 bit pattern in (0.5,1], original U(P(x)) == x. Our ordinary
 * material VS/viewport/rasterizer writes stay far inside that interval when
 * every actual vertex passes these conservative, unclipped bounds. */
static uint64_t depth421_writers,depth421_refused,depth421_skipped;
static int depth421_enabled(void){static int on=-1;if(on<0){NBGuard334 g;nb334_save(&g);const char*v=getenv("DRIVING_DEPTH_RANGE421");on=v&&!strcmp(v,"1");nb334_restore(&g);}return on;}
/*423 Behind-camera no-coverage certificate. Ordinary material VS preserves W;
 * every point of an all-negative-W triangle lies outside the D3D clip volume.
 * Original Draw and all failure points remain. No GPU content/ownership claim. */
static uint64_t depth423_triangles;
static int depth423_enabled(void){static int on=-1;if(on<0){NBGuard334 g;nb334_save(&g);const char*v=getenv("DRIVING_DEPTH_CLIP423");on=v&&!strcmp(v,"1");nb334_restore(&g);}return on;}
static int depth423_negative(const NFHardwareMaterialVertex221*v){
 for(unsigned j=0;j<3;j++){
  uint32_t b[4];memcpy(b,v[j].position,sizeof b);
  if((b[0]&0x7fffffffu)>0x46800000u||(b[1]&0x7fffffffu)>0x46800000u||
     (b[2]&0x7fffffffu)>0x4c000000u||b[3]<0xbb800000u||b[3]>0xc3800000u)return 0;
 }
 return 1;
}
static int depth421_bounds(const NFHardwareMaterialVertex221*v,unsigned n){
 if(!v||!n||n%3||n>16384)return 0;
 const int clip423=depth423_enabled();
 for(unsigned i=0;i<n;i+=3){
  if(clip423&&depth423_negative(v+i)){depth423_triangles++;continue;}
  uint32_t q[3][4],outside=15;int interior=1;
  for(unsigned j=0;j<3;j++){
   memcpy(q[j],v[i+j].position,sizeof q[j]);const uint32_t*b=q[j];
   /* Bounded finite clip arithmetic and strictly positive ordinary W.
    * Integer comparisons avoid NaN/denormal FP exceptions. */
   if((b[0]&0x7fffffffu)>0x46800000u||(b[1]&0x7fffffffu)>0x46800000u||
      (b[2]&0x7fffffffu)>0x4b800000u||b[3]<0x3b800000u||b[3]>0x43800000u)return 0;
   uint32_t x=b[0]&0x80000000u?~b[0]:b[0]^0x80000000u;
   uint32_t y=b[1]&0x80000000u?~b[1]:b[1]^0x80000000u;
   /* All three vertices beyond one XY plane => original clipping emits no
    * covered pixel. Keep original Draw and failure points regardless. Two
    * pixels of separation dominate the bounded VS/viewport rounding error. */
   outside&=(x<0x3fffffffu?1u:0u)|(x>0xc4208000u?2u:0u)|
            (y<0x3fffffffu?4u:0u)|(y>0xc3f10000u?8u:0u);
   if(b[0]<0x40000000u||b[0]>0x441f8000u||b[1]<0x40000000u||b[1]>0x43ef0000u||
      b[2]<0x4b400000u||b[2]>0x4b7ffeffu)interior=0;
  }
  if(outside)continue;
  if(!interior)return 0;
  /* Visible candidate: at least64 square pixels of triangle area, plus
   * X[2,638],Y[2,478],Z[12582912,16776959]. Exclude ill-conditioned slivers.
   * Positive normal XY gives q8 shifts6..14; no floating math is performed. */
  int64_t xy[3][2];
  for(unsigned j=0;j<3;j++)for(unsigned axis=0;axis<2;axis++){
   uint32_t b=q[j][axis];unsigned shift=142-(b>>23);
   xy[j][axis]=(int64_t)(((b&0x7fffffu)|0x800000u)>>shift);
  }
  int64_t area=(xy[1][0]-xy[0][0])*(xy[2][1]-xy[0][1])-(xy[1][1]-xy[0][1])*(xy[2][0]-xy[0][0]);
  if(area<0)area=-area;if(area<128LL*65536)return 0;
 }
 return 1;
}
static int depth332_bounds(const NFHardwareMaterialVertex221*v,unsigned n,D3D11_RECT*r){
 /* Only unclipped interior triangle lists. Two-pixel padding exceeds the
  * bounded fp32 VS/divide/viewport error and 1/256 raster snapping. Unknown
  * W/clip-edge inputs keep the original whole-surface conversion. */
 if(!v||!n||n%3||n>16384)return 0;
 unsigned csr=_mm_getcsr();float lx=640,ly=480,hx=0,hy=0;int ok=0;
 for(unsigned i=0;i<n;i++){
  const float*p=v[i].position;
  if(!(p[0]>=2&&p[0]<=638&&p[1]>=2&&p[1]<=478&&p[2]>=256&&p[2]<=16776959&&p[3]>=0.00390625f&&p[3]<=256))goto done;
  if(p[0]<lx)lx=p[0];if(p[0]>hx)hx=p[0];if(p[1]<ly)ly=p[1];if(p[1]>hy)hy=p[1];
 }
 {D3D11_RECT q={(LONG)lx-2,(LONG)ly-2,(LONG)hx+3,(LONG)hy+3};
  if(q.right>640)q.right=640;if(q.bottom>480)q.bottom=480;
  if((q.right-q.left)*(q.bottom-q.top)>640*480/4)goto done;
  *r=q;ok=1;}
 done:_mm_setcsr(csr);return ok;
}
static void depth331_snapshot(NFDepthScope272*s){s->proof_target331=depth;s->proof_view331=dsv;s->draws331=draws;s->clears331=gpu_depth_clears;s->transfers331=transfers;s->uploads331=depth_upload_copies;s->upload_clears331=depth_upload_clears;}
static int depth331_same(NFDepthScope272*s){return s->proof_target331==depth&&s->proof_view331==dsv&&s->clears331==gpu_depth_clears&&s->transfers331==transfers&&s->uploads331==depth_upload_copies&&s->upload_clears331==depth_upload_clears;}
static void depth331_invalidate(NFDepthScope272*s){depth331_invalidated+=s->canonical331!=0;s->canonical331=0;s->partial332=0;s->delta333_known=s->write333_known=0;}
static void depth331_before_draw(const NFHardwareMaterialVertex221*v,unsigned n){
 NFDepthScope272*s=depth272_current;if(!s||!s->private331||!(depth331_enabled()||depth332_enabled()||depth333_enabled()||s->plain421))return;
 if(!depth331_same(s)||s->draws331!=draws||s->in_draw331||!bound.valid)depth331_invalidate(s);
 s->partial332=0;s->write333_known=0;
 int certified421=s->plain421&&s->canonical331&&active.depth_write&&depth421_bounds(v,n);
 if(s->plain421&&active.depth_write){if(certified421)depth421_writers++;else depth421_refused++;}
 if(active.depth_write&&!certified421){
  int bounded=s->canonical331&&(depth332_enabled()||depth333_enabled())&&depth332_bounds(v,n,&s->rect332);
  if(depth333_enabled()&&bounded){s->write333_known=1;s->write333=s->rect332;}
  /* A known draw invalidates source canonicality, not the previous slot
   * difference. Unexpected writers still invalidate both via the helper. */
  depth331_invalidated+=s->canonical331!=0;s->canonical331=0;
  s->partial332=bounded&&depth332_enabled();
 }else if(s->canonical331&&depth333_enabled()){
  s->write333_known=1;s->write333=(D3D11_RECT){0,0,0,0};
 }
 s->in_draw331=1;depth331_snapshot(s);
}
static void depth331_after_draw(void){
 NFDepthScope272*s=depth272_current;if(!s||!s->private331||!(depth331_enabled()||depth332_enabled()||depth333_enabled()||s->plain421))return;
 if(!s->in_draw331||!depth331_same(s)||draws!=s->draws331+1)depth331_invalidate(s);
 s->in_draw331=0;depth331_snapshot(s);
}
static int depth333_region(const NFDepthScope272*s,D3D11_RECT*out){
 if(!s->delta333_known||!s->write333_known)return 0;
 D3D11_RECT a=s->delta333,b=s->write333;
 if(a.right<=a.left||a.bottom<=a.top)a=b;
 else if(b.right>b.left&&b.bottom>b.top){if(b.left<a.left)a.left=b.left;if(b.top<a.top)a.top=b.top;if(b.right>a.right)a.right=b.right;if(b.bottom>a.bottom)a.bottom=b.bottom;}
 if(a.left<0||a.top<0||a.right>640||a.bottom>480||a.right<=a.left||a.bottom<=a.top)return 0;
 if((a.right-a.left)*(a.bottom-a.top)>640*480/4)return 0;
 *out=a;return 1;
}
static int depth330_enabled(void){static int on=-1;if(on<0){DWORD e=GetLastError();int crt=errno;const char*v=getenv("DRIVING_DEPTH_UNPACK330");on=v&&!strcmp(v,"1");errno=crt;SetLastError(e);}return on;}
static int depth272_enabled(void){static int on=-1;if(on<0){DWORD e=GetLastError();const char*v=getenv("DRIVING_DEPTH_PING272");on=v&&!strcmp(v,"1");SetLastError(e);}return on;}
#ifdef NF_PAIR234_TEST
static unsigned depth272_test_failure;
static int depth272_failure(unsigned at){if(depth272_test_failure==at){depth272_test_failure=0;return 1;}return 0;}
#else
static int depth272_failure(unsigned at){(void)at;return 0;}
#endif
static void depth272_release(NFDepthSlot272*s){if(resident313_blocked())return;nf_hw_depth_seed278_revoke();RELEASE(s->srv);RELEASE(s->dsv);RELEASE(s->texture);}
static const char depth272_code[]=
"Texture2D<float> src:register(t0);"
"uint packz(uint x){uint e=(x>>23)&255,m=(x&0x7fffff)|0x800000;"
"if((x&0x80000000)||((x&0x7fffffff)>0x7f800000))return 0;"
"if(x>=0x3f800000)return 0xffffff;if(e<102)return 0;"
"if(e==126)return m==0x800000?m:m-1;uint r=126-e;return (m+(1u<<(r-1))-1)>>r;}"
/* q is packz's0..FFFFFF result: conversion to float is exact. Subtracting24
 * exponent units scales it by2^-24, then the original one-ULP adjustment
 * gives exactly the old bit-scan/shift result. Zero retains its special case. */
"uint unpackz(uint q){\n#ifdef NF_DEPTH_UNPACK330\n"
"return q?asuint((float)q)-0x0c000000u+1u:0u;\n#else\n"
"if(!q)return 0;uint k=firstbithigh(q);"
"return ((k+103)<<23)+((q<<(23-k))-0x800000)+1;\n#endif\n}"
"float4 vertex(uint i:SV_VertexID):SV_Position{float2 p=float2((i<<1)&2,i&2);return float4(p*float2(2,-2)+float2(-1,1),0.5,1);}"
"float pixel(float4 p:SV_Position):SV_Depth{return asfloat(unpackz(packz(asuint(src.Load(int3(uint2(p.xy),0))))));}";
static int depth272_prepare(void){
 if(depth272_failure(1))return 0;
 if(depth272_vs&&depth272_ps&&depth272_raster&&depth272_state&&depth272_alternate[0].srv&&depth272_alternate[1].srv)return 1;
 ID3DBlob*v=NULL,*p=NULL,*e=NULL;ID3D11VertexShader*vs=NULL;ID3D11PixelShader*ps=NULL;ID3D11RasterizerState*rs=NULL;ID3D11DepthStencilState*ds=NULL;NFDepthSlot272 alt[2]={{0}};
 const D3D_SHADER_MACRO macros330[]={{"NF_DEPTH_UNPACK330","1"},{NULL,NULL}};
 HRESULT hr=D3DCompile(depth272_code,sizeof depth272_code-1,NULL,NULL,NULL,"vertex","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&v,&e);RELEASE(e);if(FAILED(hr))goto done;
 hr=D3DCompile(depth272_code,sizeof depth272_code-1,NULL,depth330_enabled()?macros330:NULL,NULL,"pixel","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&p,&e);RELEASE(e);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreateVertexShader(dev,ID3D10Blob_GetBufferPointer(v),ID3D10Blob_GetBufferSize(v),NULL,&vs);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreatePixelShader(dev,ID3D10Blob_GetBufferPointer(p),ID3D10Blob_GetBufferSize(p),NULL,&ps);if(FAILED(hr))goto done;
 D3D11_RASTERIZER_DESC rd={0};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;rd.ScissorEnable=TRUE;
 hr=ID3D11Device_CreateRasterizerState(dev,&rd,&rs);if(FAILED(hr))goto done;
 D3D11_DEPTH_STENCIL_DESC zd={0};zd.DepthEnable=TRUE;zd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;zd.DepthFunc=D3D11_COMPARISON_ALWAYS;
 hr=ID3D11Device_CreateDepthStencilState(dev,&zd,&ds);if(FAILED(hr))goto done;
 for(unsigned i=0;i<2;i++){
  D3D11_TEXTURE2D_DESC td={0};td.Width=640;td.Height=480;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R32_TYPELESS;td.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
  if(depth272_failure(i+2)){hr=E_FAIL;goto done;}
  hr=ID3D11Device_CreateTexture2D(dev,&td,NULL,&alt[i].texture);if(FAILED(hr))goto done;
  D3D11_DEPTH_STENCIL_VIEW_DESC dd={0};dd.Format=DXGI_FORMAT_D32_FLOAT;dd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
  hr=ID3D11Device_CreateDepthStencilView(dev,(ID3D11Resource*)alt[i].texture,&dd,&alt[i].dsv);if(FAILED(hr))goto done;
  D3D11_SHADER_RESOURCE_VIEW_DESC sd={0};sd.Format=DXGI_FORMAT_R32_FLOAT;sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sd.Texture2D.MipLevels=1;
  if(depth272_failure(i+4)){hr=E_FAIL;goto done;}
  hr=ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)alt[i].texture,&sd,&alt[i].srv);if(FAILED(hr))goto done;
 }
 RELEASE(depth272_vs);RELEASE(depth272_ps);RELEASE(depth272_raster);RELEASE(depth272_state);
 for(unsigned i=0;i<2;i++){depth272_release(&depth272_alternate[i]);depth272_alternate[i]=alt[i];memset(&alt[i],0,sizeof alt[i]);}
 depth272_vs=vs;vs=NULL;depth272_ps=ps;ps=NULL;depth272_raster=rs;rs=NULL;depth272_state=ds;ds=NULL;
 done:RELEASE(v);RELEASE(p);RELEASE(e);RELEASE(vs);RELEASE(ps);RELEASE(rs);RELEASE(ds);depth272_release(&alt[0]);depth272_release(&alt[1]);return SUCCEEDED(hr);
}
/*0 means ordinary path, -1 scoped-state refusal, 1 exact conversion scheduled.*/
static int depth272_apply(void){
 NFDepthScope272*s=depth272_current;if(!s)return 0;if(s->at>1)return -1;
 NFDepthSlot272*src=&s->slots[s->at],*dst=&s->slots[s->at^1];
 if(src->texture!=depth||src->dsv!=dsv||!src->srv||!dst->texture||!dst->dsv||!dst->srv||src->texture==dst->texture||!depth272_vs||!depth272_ps||!depth272_raster||!depth272_state||width!=640||height!=480)return -1;
 if(s->private331&&(depth331_enabled()||depth332_enabled()||depth333_enabled()||s->plain421)){
  if(!depth331_same(s)||s->draws331!=draws||s->in_draw331)depth331_invalidate(s);
  /* Identity/refusal checks above still run. P(U(q))=q was exhaustively
   * checked; with no depth writer, keeping this source is an exact boundary. */
  if(s->canonical331&&(depth331_enabled()||s->plain421)){depth331_skipped++;if(s->plain421)depth421_skipped++;return 1;}
 }
 ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);
 ID3D11ShaderResourceView*null_srv=NULL;ID3D11DeviceContext_PSSetShaderResources(ctx,0,1,&null_srv);
 D3D11_RECT rect333;int partial333=s->private331&&depth333_enabled()&&depth333_region(s,&rect333);
 int partial332=!partial333&&s->private331&&depth332_enabled()&&s->partial332;
 if(partial332){ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)dst->texture,(ID3D11Resource*)src->texture);depth332_partial++;depth332_pixels+=(uint64_t)(s->rect332.right-s->rect332.left)*(s->rect332.bottom-s->rect332.top);}
 ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,dst->dsv);
 ID3D11DeviceContext_OMSetDepthStencilState(ctx,depth272_state,0);ID3D11DeviceContext_OMSetBlendState(ctx,NULL,NULL,~0u);
 D3D11_VIEWPORT vp={0,0,640,480,0,1};D3D11_RECT sc={0,0,640,480};
 if(partial332)sc=s->rect332;
 if(partial333){sc=rect333;depth333_partial++;depth333_pixels+=(uint64_t)(sc.right-sc.left)*(sc.bottom-sc.top);}
 ID3D11DeviceContext_RSSetState(ctx,depth272_raster);ID3D11DeviceContext_RSSetViewports(ctx,1,&vp);ID3D11DeviceContext_RSSetScissorRects(ctx,1,&sc);
 ID3D11DeviceContext_IASetInputLayout(ctx,NULL);ID3D11DeviceContext_IASetPrimitiveTopology(ctx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 ID3D11DeviceContext_VSSetShader(ctx,depth272_vs,NULL,0);ID3D11DeviceContext_GSSetShader(ctx,NULL,NULL,0);
 ID3D11DeviceContext_HSSetShader(ctx,NULL,NULL,0);ID3D11DeviceContext_DSSetShader(ctx,NULL,NULL,0);
 ID3D11DeviceContext_PSSetShader(ctx,depth272_ps,NULL,0);ID3D11DeviceContext_PSSetShaderResources(ctx,0,1,&src->srv);
 gt297_op(ctx,GT297_DEPTH272,0,0);ID3D11DeviceContext_Draw(ctx,3,0);gt297_op(ctx,GT297_DEPTH272,1,0);
 ID3D11DeviceContext_PSSetShaderResources(ctx,0,1,&null_srv);ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);
 s->at^=1;depth=dst->texture;dsv=dst->dsv;memset(&bound,0,sizeof bound);
 quant256_target=quant256_enabled()?depth:NULL;quant256_selected_srv=quant256_enabled()?dst->srv:NULL;
 if(s->private331&&(depth331_enabled()||depth332_enabled()||depth333_enabled()||s->plain421)){
  /* New destination equals Q(source). With a canonical pre-draw source,
   * only this draw's admitted writes can differ from the old source slot. */
  s->delta333_known=s->write333_known;s->delta333=s->write333;s->write333_known=0;
  s->canonical331=1;s->in_draw331=0;s->partial332=0;depth331_snapshot(s);
 }
 depth272_conversions++;return 1;
}
static void depth272_report(void){if(++depth272_calls!=1&&depth272_calls%120)return;DWORD e=GetLastError();fprintf(stderr,"[DEPTH-PING272] calls=%llu boundary_conversions=%llu fullscreen_integer_depth=1 copyback_omitted=1\n",(unsigned long long)depth272_calls,(unsigned long long)depth272_conversions);if(depth331_enabled())fprintf(stderr,"[DEPTH-CLEAN331] skipped=%llu invalidated=%llu no_swap=1 private_flush_only=1\n",(unsigned long long)depth331_skipped,(unsigned long long)depth331_invalidated);if(depth421_enabled()){NBGuard334 g;nb334_save(&g);fprintf(stderr,"[DEPTH-RANGE421] writers=%llu refused=%llu skipped=%llu original_flush_only=1\n",(unsigned long long)depth421_writers,(unsigned long long)depth421_refused,(unsigned long long)depth421_skipped);if(depth423_enabled())fprintf(stderr,"[DEPTH-CLIP423] certified_negative_triangles=%llu original_Draw_retained=1\n",(unsigned long long)depth423_triangles);nb334_restore(&g);}SetLastError(e);}
#else
static int depth272_apply(void){return 0;}
#endif
#ifdef NF_DEPTH_SRV256_AVAILABLE
static void depth332_report(void){if(!depth332_enabled())return;static uint64_t calls;if(++calls!=1&&calls%120)return;DWORD e=GetLastError();fprintf(stderr,"[DEPTH-RECT332] partial=%llu shader_pixels=%llu full_copy_before_rect=1 private_flush_only=1\n",(unsigned long long)depth332_partial,(unsigned long long)depth332_pixels);SetLastError(e);}
static void depth333_report(void){if(!depth333_enabled())return;static uint64_t calls;if(++calls!=1&&calls%120)return;DWORD e=GetLastError();fprintf(stderr,"[DEPTH-DELTA333] partial=%llu shader_pixels=%llu extra_copy=0 private_flush_only=1\n",(unsigned long long)depth333_partial,(unsigned long long)depth333_pixels);SetLastError(e);}
#endif
#endif
