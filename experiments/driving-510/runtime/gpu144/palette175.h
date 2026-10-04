/* Isolated one-entry proposal. No guest DMA decoding, synchronization, stage
 * selection, rendering, mip selection or process-global cache policy.
 * Caller completes GPU aliases and holds both source allocations stable. */
#ifndef DRIVING_PALETTE175_H
#define DRIVING_PALETTE175_H
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>
#include <d3d11.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "d3d8_swizzle.h"
typedef struct Palette175Entry {
 ID3D11Device *device; /* held reference makes identity stable */
 ID3D11ShaderResourceView *view;
 const void *source,*palette;
 unsigned width,height;
 unsigned char *indices,*colors;
 uint64_t hits,uploads,index_changes,palette_changes,failures;
 HRESULT last_error;
} Palette175Entry;
#ifdef PALETTE175_TEST
/* Deterministic fixture failure points, not a production selector. */
static unsigned palette175_fail_stage;
#define PALETTE175_FAIL(stage) (palette175_fail_stage==(stage))
#else
#define PALETTE175_FAIL(stage) 0
#endif
static int palette175_readable(const void *data,size_t bytes)
{
 uintptr_t at=(uintptr_t)data,end;
 if(!at||!bytes||bytes>UINTPTR_MAX-at)return 0;
 end=at+bytes;
 while(at<end){
  MEMORY_BASIC_INFORMATION m;uintptr_t next;DWORD p;
  if(!VirtualQuery((const void*)at,&m,sizeof m)||m.State!=MEM_COMMIT||
     (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return 0;
  p=m.Protect&255;
  if(p!=PAGE_READONLY&&p!=PAGE_READWRITE&&p!=PAGE_WRITECOPY&&
     p!=PAGE_EXECUTE_READ&&p!=PAGE_EXECUTE_READWRITE&&p!=PAGE_EXECUTE_WRITECOPY)return 0;
  if((uintptr_t)m.BaseAddress>UINTPTR_MAX-m.RegionSize)return 0;
  next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=at)return 0;
  at=next<end?next:end;
 }
 return 1;
}
static int palette175_supported(ID3D11Device *dev)
{
 UINT bits=0,need=D3D11_FORMAT_SUPPORT_TEXTURE2D|D3D11_FORMAT_SUPPORT_SHADER_LOAD|D3D11_FORMAT_SUPPORT_SHADER_SAMPLE;
 return dev&&SUCCEEDED(ID3D11Device_CheckFormatSupport(dev,DXGI_FORMAT_B8G8R8A8_UNORM,&bits))&&(bits&need)==need;
}
static void palette175_release(Palette175Entry *e)
{
 if(!e)return;
 if(e->view)ID3D11ShaderResourceView_Release(e->view);
 if(e->device)ID3D11Device_Release(e->device);
 free(e->indices);free(e->colors);memset(e,0,sizeof *e);
}
/* Returns a borrowed SRV, or NULL on every rejected/failed request. An existing
 * entry is preserved transactionally on failure, but is NEVER returned then.
 * Caller must propagate NULL and must not continue a draw using an old binding.
 * Required input spans are w*h index bytes and exactly256 BGRA palette words.
 * Full palette comparison includes currently unused entries, conservatively. */
static ID3D11ShaderResourceView *palette175_get(Palette175Entry *e,ID3D11Device *dev,
 const void *source,size_t source_available,const void *palette,size_t palette_available,
 unsigned format,unsigned width,unsigned height,unsigned levels)
{
 size_t bytes;unsigned char *indices=NULL,*colors=NULL,*linear=NULL,*decoded=NULL;
 ID3D11Texture2D *texture=NULL;ID3D11ShaderResourceView *view=NULL;
 D3D11_TEXTURE2D_DESC d={0};D3D11_SUBRESOURCE_DATA data={0};HRESULT error=E_INVALIDARG;
 int equal_indices=0,equal_palette=0,same_key=0;
 if(!e)return NULL;
 if(!dev||format!=0x0b||levels!=1||width!=64||(height!=32&&height!=64)||
    (e->device&&e->device!=dev))goto fail;
 bytes=(size_t)width*height;
 /* All validation precedes either source comparison/copy. VirtualQuery is a
  * bounds check, not a lifetime lock: caller still owns mapping/coherency. */
 if(source_available<bytes||palette_available<1024||
    !palette175_readable(source,bytes)||!palette175_readable(palette,1024))goto fail;
 if(!e->device&&!palette175_supported(dev)){error=DXGI_ERROR_UNSUPPORTED;goto fail;}
 same_key=e->view&&e->source==source&&e->palette==palette&&e->width==width&&e->height==height;
 if(same_key){
  equal_indices=!memcmp(e->indices,source,bytes);equal_palette=!memcmp(e->colors,palette,1024);
  if(equal_indices&&equal_palette){e->hits++;e->last_error=S_OK;return e->view;}
 }
 error=E_OUTOFMEMORY;
 if(PALETTE175_FAIL(1))goto fail;
 indices=(unsigned char*)malloc(bytes);colors=(unsigned char*)malloc(1024);
 linear=(unsigned char*)malloc(bytes);decoded=(unsigned char*)malloc(bytes*4);
 if(!indices||!colors||!linear||!decoded)goto fail;
 memcpy(indices,source,bytes);memcpy(colors,palette,1024);
 xbox_unswizzle_rect(linear,indices,width,height,1);
 for(size_t i=0;i<bytes;i++)memcpy(decoded+i*4,colors+(unsigned)linear[i]*4,4);
 d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=1;d.SampleDesc.Count=1;
 d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 data.pSysMem=decoded;data.SysMemPitch=width*4;
 error=PALETTE175_FAIL(2)?E_FAIL:ID3D11Device_CreateTexture2D(dev,&d,&data,&texture);
 if(FAILED(error))goto fail;
 error=PALETTE175_FAIL(3)?E_FAIL:ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)texture,NULL,&view);
 if(FAILED(error))goto fail;
 /* Commit last. No UpdateSubresource and no mutation of a prior resource.
  * Submitted draws hold D3D references; callers retaining a borrowed view past
  * the next get/release must explicitly AddRef it. */
 if(same_key){e->index_changes+=!equal_indices;e->palette_changes+=!equal_palette;}
 if(e->view)ID3D11ShaderResourceView_Release(e->view);
 free(e->indices);free(e->colors);
 if(!e->device){e->device=dev;ID3D11Device_AddRef(dev);}
 e->view=view;e->indices=indices;e->colors=colors;e->source=source;e->palette=palette;
 e->width=width;e->height=height;e->uploads++;e->last_error=S_OK;
 ID3D11Texture2D_Release(texture);free(linear);free(decoded);return view;
fail:
 if(view)ID3D11ShaderResourceView_Release(view);
 if(texture)ID3D11Texture2D_Release(texture);
 free(indices);free(colors);free(linear);free(decoded);e->failures++;e->last_error=error;return NULL;
}
#undef PALETTE175_FAIL
#endif
