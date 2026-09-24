/* Private248 initial packed-depth import. No guest writes or new deferral. */
#ifndef NIGHTFIRE_DEPTH_IMPORT268_H
#define NIGHTFIRE_DEPTH_IMPORT268_H
#ifdef NF_DEPTH_SRV256_AVAILABLE
#define NF_DEPTH_IMPORT268_AVAILABLE 1
static ID3D11ComputeShader *depth268_shader;
static ID3D11Texture2D *depth268_target;
static uint64_t depth268_uploads,depth268_mode_fallbacks;
static int depth268_enabled(void){
 static int value=-1;if(value<0){DWORD error=GetLastError();const char*v=getenv("DRIVING_DEPTH_IMPORT268");value=v&&!strcmp(v,"1");SetLastError(error);}return value;
}
static int depth268_mode(void){
#ifdef NF_DEPTH_SSE2
 unsigned csr=_mm_getcsr();
 /* q24 /16777215 and float conversion have no nonzero subnormal operands or
  * results. FTZ/DAZ cannot affect the bits. Only precision can become sticky;
  * if it is clear, use the original CPU path rather than invent that status. */
 return (csr&_MM_ROUND_MASK)==_MM_ROUND_NEAREST &&
        (csr&_MM_MASK_MASK)==_MM_MASK_MASK && (csr&_MM_EXCEPT_INEXACT);
#else
 return 0;
#endif
}
#ifdef NF_PAIR234_TEST
static unsigned depth268_test_failure;
#endif
static const char depth268_code[]=
"Texture2D<uint> src:register(t0);RWTexture2D<uint> dst:register(u0);"
"uint unpackz(uint q){if(!q)return 0;uint k=firstbithigh(q);"
"return ((k+103)<<23)+((q<<(23-k))-0x800000)+1;}"
"[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;dst.GetDimensions(w,h);"
"if(id.x<w&&id.y<h)dst[id.xy]=unpackz(src.Load(int3(id.xy,0))>>8);}";
static int depth268_prepare(void){
#ifdef NF_PAIR234_TEST
 if(depth268_test_failure){depth268_test_failure=0;return 0;}
#endif
 if(!dev||!ctx||quant130_width!=640||quant130_height!=480||!quant130_input||!quant130_srv||!quant130_output||!quant130_uav)return 0;
 if(depth268_shader)return 1;
 ID3DBlob *code=NULL,*errors=NULL;ID3D11ComputeShader *shader=NULL;
 HRESULT hr=D3DCompile(depth268_code,sizeof depth268_code-1,NULL,NULL,NULL,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
 if(SUCCEEDED(hr))hr=ID3D11Device_CreateComputeShader(dev,ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),NULL,&shader);
 RELEASE(code);RELEASE(errors);if(FAILED(hr)){RELEASE(shader);return 0;}
 depth268_shader=shader;return 1;
}
static int depth268_apply(const NFHardwareState*s){
 if(!depth268_target||depth268_target!=depth||!depth268_shader||!s||!s->depth||
    s->color_only||s->color_layout||s->width!=640||s->height!=480||s->depth_pitch!=2560||
    width!=640||height!=480||quant130_width!=640||quant130_height!=480||
    !quant130_input||!quant130_srv||!quant130_output||!quant130_uav)return 0;
 if(!depth268_mode()){depth268_mode_fallbacks++;return 0;}
 /* UpdateSubresource snapshots source bytes. Integer output bits are copied
  * into the compatible typeless D32 resource; no FLOAT shader math is used. */
 ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);bound.targets=0;
 ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)quant130_input,0,NULL,s->depth,s->depth_pitch,0);
 ID3D11DeviceContext_CSSetShader(ctx,depth268_shader,NULL,0);
 ID3D11DeviceContext_CSSetShaderResources(ctx,0,1,&quant130_srv);
 ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&quant130_uav,NULL);
 gt297_op(ctx,GT297_DEPTH268,0,0);ID3D11DeviceContext_Dispatch(ctx,80,60,1);gt297_op(ctx,GT297_DEPTH268,1,0);
 ID3D11ShaderResourceView *null_srv=NULL;ID3D11UnorderedAccessView *null_uav=NULL;
 ID3D11DeviceContext_CSSetShaderResources(ctx,0,1,&null_srv);
 ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&null_uav,NULL);
 ID3D11DeviceContext_CSSetShader(ctx,NULL,NULL,0);
 ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)depth,(ID3D11Resource*)quant130_output);
 depth268_uploads++;
 if(depth268_uploads==1||!(depth268_uploads%1024)){
  DWORD error=GetLastError();fprintf(stderr,"[DEPTH-IMPORT268] uploads=%llu mode_fallbacks=%llu private248=1 cpu_unpack_omitted=1 staging_write_map_omitted=1 packed_source_fresh=1\n",(unsigned long long)depth268_uploads,(unsigned long long)depth268_mode_fallbacks);SetLastError(error);
 }
 return 1;
}
#endif
#endif
