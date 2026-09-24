/* GPU-only equivalent of nf_depth_unpack(nf_depth_pack(D32)).
 * Included privately by nightfire_hardware.c; no guest memory is touched.
 * Integer texture loads/copies preserve float bits, including denormals.
 * CPU oracle exhausts all q24 and every finite input in the nonzero range.
 * NaNs map to zero, matching SSE MAXPS(x,0) in the 640-wide host path.
 * Two scratch textures avoid changing the ordinary depth resource bind flags.
 */
#ifndef NIGHTFIRE_DEPTH_ROUNDTRIP130_H
#define NIGHTFIRE_DEPTH_ROUNDTRIP130_H
static ID3D11ComputeShader *quant130_shader;
static ID3D11Texture2D *quant130_input,*quant130_output;
static ID3D11ShaderResourceView *quant130_srv;
static ID3D11UnorderedAccessView *quant130_uav;
static unsigned quant130_width,quant130_height;
static const char quant130_code[]=
"Texture2D<uint> src:register(t0);RWTexture2D<uint> dst:register(u0);"
"uint packz(uint x){uint e=(x>>23)&255,m=(x&0x7fffff)|0x800000;"
"if((x&0x80000000)||((x&0x7fffffff)>0x7f800000))return 0;"
"if(x>=0x3f800000)return 0xffffff;if(e<102)return 0;"
"if(e==126)return m==0x800000?m:m-1;uint r=126-e;return (m+(1u<<(r-1))-1)>>r;}"
"uint unpackz(uint q){if(!q)return 0;uint k=firstbithigh(q);"
"return ((k+103)<<23)+((q<<(23-k))-0x800000)+1;}"
"[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;dst.GetDimensions(w,h);"
"if(id.x<w&&id.y<h)dst[id.xy]=unpackz(packz(src.Load(int3(id.xy,0))));}";
static int quant130_prepare(unsigned w,unsigned h){
    if(!dev || !ctx || ID3D11Device_GetFeatureLevel(dev)<D3D_FEATURE_LEVEL_11_0)return 0;
    if(!quant130_shader){
        ID3DBlob *code=NULL,*errors=NULL;
        HRESULT hr=D3DCompile(quant130_code,sizeof quant130_code-1,NULL,NULL,NULL,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
        if(SUCCEEDED(hr))hr=ID3D11Device_CreateComputeShader(dev,ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),NULL,&quant130_shader);
        RELEASE(code);RELEASE(errors);if(FAILED(hr))return 0;
    }
    if(quant130_width==w && quant130_height==h)return 1;
    ID3D11Texture2D *in=NULL,*out=NULL;ID3D11ShaderResourceView *srv=NULL;ID3D11UnorderedAccessView *uav=NULL;
    D3D11_TEXTURE2D_DESC d={0};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R32_UINT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr=ID3D11Device_CreateTexture2D(dev,&d,NULL,&in);
    if(SUCCEEDED(hr))hr=ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)in,NULL,&srv);
    d.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
    if(SUCCEEDED(hr))hr=ID3D11Device_CreateTexture2D(dev,&d,NULL,&out);
    if(SUCCEEDED(hr))hr=ID3D11Device_CreateUnorderedAccessView(dev,(ID3D11Resource*)out,NULL,&uav);
    if(FAILED(hr)){RELEASE(in);RELEASE(out);RELEASE(srv);RELEASE(uav);return 0;}
    RELEASE(quant130_srv);RELEASE(quant130_uav);RELEASE(quant130_input);RELEASE(quant130_output);
    quant130_input=in;quant130_output=out;quant130_srv=srv;quant130_uav=uav;quant130_width=w;quant130_height=h;return 1;
}
static void quant130_apply(ID3D11Texture2D *target){
    ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);bound.targets=0;
    ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)quant130_input,(ID3D11Resource*)target);
    ID3D11DeviceContext_CSSetShader(ctx,quant130_shader,NULL,0);
    ID3D11DeviceContext_CSSetShaderResources(ctx,0,1,&quant130_srv);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&quant130_uav,NULL);
    ID3D11DeviceContext_Dispatch(ctx,(quant130_width+7)/8,(quant130_height+7)/8,1);
    ID3D11ShaderResourceView *null_srv=NULL;ID3D11UnorderedAccessView *null_uav=NULL;
    ID3D11DeviceContext_CSSetShaderResources(ctx,0,1,&null_srv);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&null_uav,NULL);
    ID3D11DeviceContext_CSSetShader(ctx,NULL,NULL,0);
    ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)target,(ID3D11Resource*)quant130_output);
}
#endif
