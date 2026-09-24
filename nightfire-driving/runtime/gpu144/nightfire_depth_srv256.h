/*256: private248 only. Borrow a lane-owned FLOAT depth SRV during its synchronous API. */
#ifndef NIGHTFIRE_DEPTH_SRV256_H
#define NIGHTFIRE_DEPTH_SRV256_H
#if defined(NIGHTFIRE_PAIR_BATCH248) && defined(NIGHTFIRE_BATCH234) && defined(NIGHTFIRE_PAIR234) && defined(NIGHTFIRE_MATERIAL221) && !defined(NIGHTFIRE_GPU_FALLBACK96) && !defined(NIGHTFIRE_RESIDENT_MAIN130) && !defined(NIGHTFIRE_DEFER_MAIN128) && !defined(NIGHTFIRE_GPU_TIMING_DIAGNOSTIC) && !defined(NIGHTFIRE_DEPTH_OBSERVE_DIAGNOSTIC) && !defined(NIGHTFIRE_SURFACE_PROBE_DIAGNOSTIC) && !defined(NIGHTFIRE_COLOR_REUSE110) && !defined(NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC)
#define NF_DEPTH_SRV256_AVAILABLE 1
static ID3D11ComputeShader *quant256_shader;
static ID3D11Texture2D *quant256_target;
static ID3D11ShaderResourceView *quant256_selected_srv;
static uint64_t quant256_boundaries;
static int quant256_enabled(void){static int on=-1;if(on<0){const char*v=getenv("DRIVING_DEPTH_SRV256");on=v&&!strcmp(v,"1");}return on;}
#ifdef NF_PAIR234_TEST
static unsigned quant256_test_failure;
static int quant256_failure(unsigned n){if(quant256_test_failure==n){quant256_test_failure=0;return 1;}return 0;}
#else
static int quant256_failure(unsigned n){(void)n;return 0;}
#endif
static const char quant256_code[]=
"Texture2D<float> src:register(t0);RWTexture2D<uint> dst:register(u0);"
"uint packz(uint x){uint e=(x>>23)&255,m=(x&0x7fffff)|0x800000;"
"if((x&0x80000000)||((x&0x7fffffff)>0x7f800000))return 0;"
"if(x>=0x3f800000)return 0xffffff;if(e<102)return 0;"
"if(e==126)return m==0x800000?m:m-1;uint r=126-e;return (m+(1u<<(r-1))-1)>>r;}"
"uint unpackz(uint q){if(!q)return 0;uint k=firstbithigh(q);"
"return ((k+103)<<23)+((q<<(23-k))-0x800000)+1;}"
"[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;dst.GetDimensions(w,h);"
"if(id.x<w&&id.y<h)dst[id.xy]=unpackz(packz(asuint(src.Load(int3(id.xy,0)))));}";

static int quant256_prepare(void){
 if(quant256_failure(1))return 0;
 if(quant256_shader)return 1;
 ID3DBlob *code=NULL,*errors=NULL;
 HRESULT hr=D3DCompile(quant256_code,sizeof quant256_code-1,NULL,NULL,NULL,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
 if(SUCCEEDED(hr))hr=ID3D11Device_CreateComputeShader(dev,ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),NULL,&quant256_shader);
 RELEASE(code);RELEASE(errors);return SUCCEEDED(hr);
}
static int quant256_apply(ID3D11Texture2D *target){
 if(target!=quant256_target||!quant256_selected_srv||!quant256_shader)return 0;
 ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);bound.targets=0;
 ID3D11DeviceContext_CSSetShader(ctx,quant256_shader,NULL,0);
 ID3D11DeviceContext_CSSetShaderResources(ctx,0,1,&quant256_selected_srv);
 ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&quant130_uav,NULL);
 gt297_op(ctx,GT297_DEPTH256,0,0);ID3D11DeviceContext_Dispatch(ctx,(quant130_width+7)/8,(quant130_height+7)/8,1);gt297_op(ctx,GT297_DEPTH256,1,0);
 ID3D11ShaderResourceView *null_srv=NULL;ID3D11UnorderedAccessView *null_uav=NULL;
 ID3D11DeviceContext_CSSetShaderResources(ctx,0,1,&null_srv);
 ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&null_uav,NULL);
 ID3D11DeviceContext_CSSetShader(ctx,NULL,NULL,0);
 ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)target,(ID3D11Resource*)quant130_output);
 quant256_boundaries++;
 if(quant256_boundaries==1||!(quant256_boundaries%4096))fprintf(stderr,"[DEPTH-SRV256] boundaries=%llu input_copies_omitted=%llu private248=1 integer_quantization=1 output_copy_preserved=1\n",(unsigned long long)quant256_boundaries,(unsigned long long)quant256_boundaries);
 return 1;
}
#endif
#endif
