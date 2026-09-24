/* Private synchronous readback experiment. No caller memory until Map succeeds.
 * The output buffer is plain typed DEFAULT/CPU_READ, never structured or raw.
 * Views retain every referenced texture until replaced after a completed call. */
#ifndef NIGHTFIRE_READBACK294_H
#define NIGHTFIRE_READBACK294_H
#include <d3d11_2.h>
enum {RB294_PIXELS=640*480,RB294_BYTES=640*480*4};
typedef struct {ID3D11Texture2D *texture;ID3D11ShaderResourceView *srv;} NFReadView294;
static NFReadView294 read_color294[2],read_depth294[4];
static ID3D11Buffer *read_buffer294;
static ID3D11UnorderedAccessView *read_uav294[2];
static ID3D11ComputeShader *read_shader294;
static ID3D11Device *read_cap_device294;static int read_cap294;
static uint64_t read_calls294,read_fallback294,read_failed294,read_alt294;
static int read_mode294(void){return (_mm_getcsr()&(_MM_ROUND_MASK|_MM_MASK_MASK|_MM_EXCEPT_INEXACT))==(_MM_MASK_MASK|_MM_EXCEPT_INEXACT);}
static void read_view_release294(NFReadView294 *v){RELEASE(v->srv);RELEASE(v->texture);}
static int read_view294(NFReadView294 *v,ID3D11Texture2D *texture,DXGI_FORMAT format){
 if(v->texture==texture&&v->srv)return 1;
 D3D11_SHADER_RESOURCE_VIEW_DESC d={0};d.Format=format;d.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;d.Texture2D.MipLevels=1;
 ID3D11ShaderResourceView *srv=NULL;
 if(!texture||FAILED(ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)texture,&d,&srv)))return 0;
 ID3D11Texture2D_AddRef(texture);read_view_release294(v);v->texture=texture;v->srv=srv;return 1;
}
static int read_prepare_impl294(int alternate){
 if(!readback294_enabled()||!read_mode294()||ID3D11Device_GetFeatureLevel(dev)<D3D_FEATURE_LEVEL_11_0||pair_failure234(200))return 0;
#ifdef NIGHTFIRE_GPU_SAMPLE254
 if(gt254_enabled())return 0; /* Existing timestamps mean CopyResource, not Dispatch. */
#endif
 if(read_cap_device294!=dev){D3D11_FEATURE_DATA_D3D11_OPTIONS1 options={0};
  int supported=SUCCEEDED(ID3D11Device_CheckFeatureSupport(dev,D3D11_FEATURE_D3D11_OPTIONS1,&options,sizeof options))&&options.MapOnDefaultBuffers;
  ID3D11Device_AddRef(dev);RELEASE(read_cap_device294);read_cap_device294=dev;read_cap294=supported;
 }if(!read_cap294)return 0;
 if(!read_shader294){
  /* Reuse the exact already-proven packz source, without another depth formula. */
  const char *start=strstr(quant130_code,"uint packz("),*end=strstr(quant130_code,"uint unpackz(");char code[4096];
  if(!start||!end||end<=start)return 0;
  int n=snprintf(code,sizeof code,"Texture2D<float4> color:register(t0);Texture2D<uint> z:register(t1);RWBuffer<uint> output:register(u0);%.*s"
   "[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){if(id.x>=640||id.y>=480)return;uint at=id.y*640+id.x;"
   "uint4 c=(uint4)(color.Load(int3(id.xy,0))*255.0+0.5);output[at]=c.b|(c.g<<8)|(c.r<<16)|(c.a<<24);"
   "output[307200+at]=packz(z.Load(int3(id.xy,0)))<<8;}",(int)(end-start),start);
  if(n<0||(size_t)n>=sizeof code)return 0;
  ID3DBlob *b=NULL,*e=NULL;HRESULT hr=D3DCompile(code,n,"readback294",NULL,NULL,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_IEEE_STRICTNESS,0,&b,&e);
  if(SUCCEEDED(hr))hr=ID3D11Device_CreateComputeShader(dev,ID3D10Blob_GetBufferPointer(b),ID3D10Blob_GetBufferSize(b),NULL,&read_shader294);
  RELEASE(b);RELEASE(e);if(FAILED(hr))return 0;
 }
 if(!read_buffer294){
  D3D11_BUFFER_DESC d={0};d.ByteWidth=4*RB294_BYTES;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_UNORDERED_ACCESS;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  if(FAILED(ID3D11Device_CreateBuffer(dev,&d,NULL,&read_buffer294)))return 0;
 }
 for(unsigned lane=0;lane<2;lane++){
  if(!read_uav294[lane]){D3D11_UNORDERED_ACCESS_VIEW_DESC d={0};d.Format=DXGI_FORMAT_R32_UINT;d.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;d.Buffer.FirstElement=lane*2*RB294_PIXELS;d.Buffer.NumElements=2*RB294_PIXELS;
   if(FAILED(ID3D11Device_CreateUnorderedAccessView(dev,(ID3D11Resource*)read_buffer294,&d,&read_uav294[lane])))return 0;
  }
  if(!read_view294(&read_color294[lane],pair_surfaces234[lane].color,DXGI_FORMAT_B8G8R8A8_UNORM)||!read_view294(&read_depth294[lane],pair_surfaces234[lane].depth,DXGI_FORMAT_R32_UINT))return 0;
#ifdef NF_DEPTH_PING272_AVAILABLE
  if(alternate&&!read_view294(&read_depth294[2+lane],depth272_alternate[lane].texture,DXGI_FORMAT_R32_UINT))return 0;
#else
  if(alternate)return 0;
#endif
 }
 return 1;
}
static int read_prepare294(int alternate){unsigned csr=_mm_getcsr();int ok=read_prepare_impl294(alternate);_mm_setcsr(csr);return ok;}
static int read_dispatch294(unsigned lane,ID3D11Texture2D *c,ID3D11Texture2D *z){
 if(lane>1||!read_mode294()||read_color294[lane].texture!=c||pair_failure234(201+lane))return 0;
 NFReadView294 *v=NULL;for(unsigned i=0;i<4;i++)if(read_depth294[i].texture==z&&read_depth294[i].srv){v=&read_depth294[i];if(i>=2)read_alt294++;break;}
 if(!v)return 0;
 ID3D11ShaderResourceView *views[2]={read_color294[lane].srv,v->srv};
 ID3D11DeviceContext_CSSetShader(ctx,read_shader294,NULL,0);ID3D11DeviceContext_CSSetShaderResources(ctx,0,2,views);
 ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&read_uav294[lane],NULL);gt297_op(ctx,GT297_PACK294,0,(unsigned)(v-read_depth294));ID3D11DeviceContext_Dispatch(ctx,80,60,1);gt297_op(ctx,GT297_PACK294,1,0);
 views[0]=views[1]=NULL;ID3D11UnorderedAccessView *u=NULL;
 ID3D11DeviceContext_CSSetShaderResources(ctx,0,2,views);ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&u,NULL);ID3D11DeviceContext_CSSetShader(ctx,NULL,NULL,0);
 return 1;
}
static void read_report294(void){if(read_calls294!=1&&read_calls294%120)return;DWORD e=GetLastError();fprintf(stderr,"[READBACK294] batches=%llu fallback=%llu failed=%llu alternate_lanes=%llu bytes_per_batch=%u maps_per_batch=1\n",(unsigned long long)read_calls294,(unsigned long long)read_fallback294,(unsigned long long)read_failed294,(unsigned long long)read_alt294,4*RB294_BYTES);SetLastError(e);}
#endif
