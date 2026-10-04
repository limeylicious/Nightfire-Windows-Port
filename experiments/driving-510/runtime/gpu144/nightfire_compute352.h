/*352 SOURCE CANDIDATE, UNBUILT/UNTESTED. Private, synchronous vertex compute.
 * Included only by nightfire_hardware.c, after its normal implementation.
 * This never binds a guest target, reads guest pointers, calls begin/sync, or
 * publishes RAM. Caller supplies a complete owned input snapshot. Existing
 * render transactions must already be completed; refuse pending/resident work.
 */
#ifndef NIGHTFIRE_COMPUTE352_H
#define NIGHTFIRE_COMPUTE352_H
#include "nightfire_vertex_compute351.h"
#include <fenv.h>
#include <errno.h>
#include <xmmintrin.h>

enum {COMPUTE352_CACHE=16,COMPUTE352_MAX=8192};
typedef struct ComputeKey352 {
 uint32_t code[136][4],valid[136];unsigned start,mode;
} ComputeKey352;
typedef struct ComputeEntry352 {
 ComputeKey352 key;ID3D11ComputeShader *shader;
} ComputeEntry352;
typedef struct ComputeConstants352 {
 uint32_t words[192][4],valid[192],params[4];
} ComputeConstants352;
typedef char ComputeSize352[(sizeof(NFHardwareVertexResult352)==272)?1:-1];
typedef char ComputeConstantsSize352[(sizeof(ComputeConstants352)==3856)?1:-1];
static ComputeEntry352 compute_cache352[COMPUTE352_CACHE];
static unsigned compute_next352;
static uint64_t compute_calls352,compute_vertices352,compute_compiles352,compute_refused352,compute_failed352;

/* Failure injection is inert in an ordinary candidate; later independent
 * fixtures must compare output buffers and all bindings at every failure. */
#ifndef COMPUTE352_INJECT
#define COMPUTE352_INJECT(site) 0
#endif
typedef struct ComputeBindings352 {
 ID3D11ComputeShader *shader;
 ID3D11ClassInstance *classes[D3D11_SHADER_MAX_INTERFACES];
 UINT count;
 ID3D11Buffer *constant;
 ID3D11ShaderResourceView *input;
 ID3D11UnorderedAccessView *output;
} ComputeBindings352;
static void compute_save352(ComputeBindings352 *s)
{
 memset(s,0,sizeof *s);s->count=D3D11_SHADER_MAX_INTERFACES;
 ID3D11DeviceContext_CSGetShader(ctx,&s->shader,s->classes,&s->count);
 ID3D11DeviceContext_CSGetConstantBuffers(ctx,0,1,&s->constant);
 ID3D11DeviceContext_CSGetShaderResources(ctx,0,1,&s->input);
 ID3D11DeviceContext_CSGetUnorderedAccessViews(ctx,0,1,&s->output);
}
static void compute_restore352(ComputeBindings352 *s)
{
 ID3D11UnorderedAccessView *u=NULL;ID3D11ShaderResourceView *v=NULL;
 UINT keep=~0u;
 /* Our buffers are unique and never bound in graphics slots. Remove them
  * before restoring the old (possibly aliasing) CS pair. Counter values of
  * an existing append/consume UAV must not be reset by this restoration. */
 ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&u,&keep);
 ID3D11DeviceContext_CSSetShaderResources(ctx,0,1,&v);
 ID3D11DeviceContext_CSSetShader(ctx,s->shader,s->classes,s->count);
 ID3D11DeviceContext_CSSetConstantBuffers(ctx,0,1,&s->constant);
 ID3D11DeviceContext_CSSetShaderResources(ctx,0,1,&s->input);
 ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&s->output,&keep);
 RELEASE(s->shader);RELEASE(s->constant);RELEASE(s->input);RELEASE(s->output);
 for(unsigned i=0;i<s->count&&i<D3D11_SHADER_MAX_INTERFACES;i++)RELEASE(s->classes[i]);
}
static int compute_shader352(const NFVertexProgram *p,ID3D11ComputeShader **result,HRESULT *failure)
{
 ComputeKey352 key;memset(&key,0,sizeof key);
 memcpy(key.code,p->code,sizeof key.code);memcpy(key.valid,p->valid,sizeof key.valid);
 key.start=p->start;key.mode=p->mode;
 for(unsigned i=0;i<COMPUTE352_CACHE;i++)if(compute_cache352[i].shader&&
    !memcmp(&compute_cache352[i].key,&key,sizeof key)){
  *result=compute_cache352[i].shader;return 1;
 }
 NFComputeSource351 *source=malloc(sizeof *source);
 if(!source){*failure=E_OUTOFMEMORY;return -1;}
 if(!nf_vertex_compute351(source,p)){free(source);return 0;}
 ID3DBlob *blob=NULL,*errors=NULL;ID3D11ComputeShader *shader=NULL;
 HRESULT hr=COMPUTE352_INJECT(1)?E_FAIL:D3DCompile(source->text,source->length,
  "nightfire-vertex352",NULL,NULL,"transform","cs_5_0",
  D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_IEEE_STRICTNESS,0,&blob,&errors);
 free(source);
 if(SUCCEEDED(hr))hr=COMPUTE352_INJECT(2)?E_FAIL:ID3D11Device_CreateComputeShader(dev,
     blob->lpVtbl->GetBufferPointer(blob),blob->lpVtbl->GetBufferSize(blob),NULL,&shader);
 if(FAILED(hr)){
  fprintf(stderr,"[COMPUTE352] shader failure=%08lX %s\n",(unsigned long)hr,
      errors?(char*)errors->lpVtbl->GetBufferPointer(errors):"");
  RELEASE(blob);RELEASE(errors);RELEASE(shader);*failure=hr;return -1;
 }
 RELEASE(blob);RELEASE(errors);
 unsigned slot=compute_next352++%COMPUTE352_CACHE;
 RELEASE(compute_cache352[slot].shader);compute_cache352[slot].shader=shader;
 compute_cache352[slot].key=key;*result=shader;compute_compiles352++;return 1;
}
static int compute_run352(const NFVertexProgram *p,const NFHardwareInputVertex *vertices,
 unsigned count,NFHardwareVertexResult352 *output,HRESULT *failure)
{
 *failure=S_OK;
 if(!p||!vertices||!output||!count||count>COMPUTE352_MAX||resident313_blocked()||
    initialized<=0||pending||!dev||!ctx)return 0;
 /* Do not initialize another backend or perturb a pending conversion.10.x
  * remains supported by its CPU path; this optional kernel requires11.0. */
 if(ID3D11Device_GetFeatureLevel(dev)<D3D_FEATURE_LEVEL_11_0)return 0;
 HRESULT hr=ID3D11Device_GetDeviceRemovedReason(dev);if(FAILED(hr)){*failure=hr;return -1;}
 ID3D11ComputeShader *shader=NULL;
 int prepared=compute_shader352(p,&shader,failure);if(prepared!=1)return prepared;
 ID3D11Buffer *input=NULL,*result=NULL,*read=NULL,*constant=NULL;
 ID3D11ShaderResourceView *srv=NULL;ID3D11UnorderedAccessView *uav=NULL;
 ComputeBindings352 bindings;int changed=0,accepted=-1;
 const UINT in_bytes=count*(UINT)sizeof *vertices,out_bytes=count*(UINT)sizeof *output;
 D3D11_BUFFER_DESC d={0};D3D11_SUBRESOURCE_DATA init={0};
 d.ByteWidth=in_bytes;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 d.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;d.StructureByteStride=16;init.pSysMem=vertices;
 hr=COMPUTE352_INJECT(3)?E_FAIL:ID3D11Device_CreateBuffer(dev,&d,&init,&input);if(FAILED(hr))goto done;
 D3D11_SHADER_RESOURCE_VIEW_DESC sd={0};sd.Format=DXGI_FORMAT_UNKNOWN;
 sd.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;sd.Buffer.NumElements=count*16;
 hr=COMPUTE352_INJECT(4)?E_FAIL:ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)input,&sd,&srv);if(FAILED(hr))goto done;
 d.ByteWidth=out_bytes;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
 d.StructureByteStride=sizeof *output;
 hr=COMPUTE352_INJECT(5)?E_FAIL:ID3D11Device_CreateBuffer(dev,&d,NULL,&result);if(FAILED(hr))goto done;
 D3D11_UNORDERED_ACCESS_VIEW_DESC ud={0};ud.Format=DXGI_FORMAT_UNKNOWN;
 ud.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;ud.Buffer.NumElements=count;
 hr=COMPUTE352_INJECT(6)?E_FAIL:ID3D11Device_CreateUnorderedAccessView(dev,(ID3D11Resource*)result,&ud,&uav);if(FAILED(hr))goto done;
 d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 d.MiscFlags=0;d.StructureByteStride=0;
 hr=COMPUTE352_INJECT(7)?E_FAIL:ID3D11Device_CreateBuffer(dev,&d,NULL,&read);if(FAILED(hr))goto done;
 ComputeConstants352 constants;memcpy(constants.words,p->constant_words,sizeof constants.words);
 memcpy(constants.valid,p->constant_valid,sizeof constants.valid);memset(constants.params,0,sizeof constants.params);constants.params[0]=count;
 memset(&d,0,sizeof d);d.ByteWidth=sizeof constants;d.Usage=D3D11_USAGE_IMMUTABLE;
 d.BindFlags=D3D11_BIND_CONSTANT_BUFFER;init.pSysMem=&constants;
 hr=COMPUTE352_INJECT(8)?E_FAIL:ID3D11Device_CreateBuffer(dev,&d,&init,&constant);if(FAILED(hr))goto done;
 compute_save352(&bindings);changed=1;
 if(COMPUTE352_INJECT(9)){hr=E_FAIL;goto done;}
 UINT keep=~0u;
 ID3D11DeviceContext_CSSetShader(ctx,shader,NULL,0);
 ID3D11DeviceContext_CSSetConstantBuffers(ctx,0,1,&constant);
 ID3D11DeviceContext_CSSetShaderResources(ctx,0,1,&srv);
 ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&uav,&keep);
 ID3D11DeviceContext_Dispatch(ctx,(count+63)/64,1,1);
 /* The Map completes our private work inside this call. No target/alias or
  * guest fence is acknowledged by a Dispatch, and none survives this API. */
 {ID3D11UnorderedAccessView *none=NULL;ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx,0,1,&none,&keep);}
 ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)read,(ID3D11Resource*)result);
 D3D11_MAPPED_SUBRESOURCE mapped;
 hr=COMPUTE352_INJECT(10)?E_FAIL:ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)read,0,D3D11_MAP_READ,0,&mapped);
 if(FAILED(hr))goto done;
 hr=ID3D11Device_GetDeviceRemovedReason(dev);
 if(COMPUTE352_INJECT(11))hr=E_FAIL;
 if(SUCCEEDED(hr)&&mapped.pData){memcpy(output,mapped.pData,out_bytes);accepted=1;}
 else if(SUCCEEDED(hr))hr=E_FAIL;
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)read,0);
done:
 if(changed)compute_restore352(&bindings);
 RELEASE(constant);RELEASE(srv);RELEASE(uav);RELEASE(input);RELEASE(result);RELEASE(read);
 if(accepted<0)*failure=hr;
 return accepted;
}
int nf_hw_vertex_compute352(const NFVertexProgram *p,const NFHardwareInputVertex *vertices,
 unsigned count,NFHardwareVertexResult352 *output,uint32_t *failure)
{
 /* Restore caller environment even when D3DCompile clears sticky FP flags.
  * API status/failure are the explicit outcome; errno/LastError aren't it. */
 DWORD error=GetLastError();int crt=errno;fenv_t env;unsigned csr=_mm_getcsr();fegetenv(&env);
 HRESULT hr=S_OK;int result=compute_run352(p,vertices,count,output,&hr);
 compute_calls352++;if(result>0)compute_vertices352+=count;
 else if(!result)compute_refused352++;else compute_failed352++;
 if(failure)*failure=(uint32_t)hr;
 fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(error);return result;
}
static void compute_report352(void)
{
 if(!compute_calls352)return;
 fprintf(stderr,"[COMPUTE352] calls=%llu vertices=%llu compiled=%llu refused=%llu failed=%llu synchronous=1 unvalidated_candidate=1\n",
  (unsigned long long)compute_calls352,(unsigned long long)compute_vertices352,
  (unsigned long long)compute_compiles352,(unsigned long long)compute_refused352,(unsigned long long)compute_failed352);
}
#endif
