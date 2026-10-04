/* Explicit linear A8R8G8B8 native12. Caller finishes source GPU aliases first.
 * No guest DMA resolution, convolution filter or AA behavior is implemented. */
#ifndef LINEAR_RGBA189_H
#define LINEAR_RGBA189_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef struct LinearLayout189 {unsigned width,height,pitch;size_t row,bytes,span;} LinearLayout189;
typedef struct LinearEntry189 {ID3D11Device *device;ID3D11ShaderResourceView *view;const void *source;LinearLayout189 layout;uint8_t *snapshot;uint64_t hits,uploads,failures;} LinearEntry189;
#ifdef NF_LINEAR189_TEST
static unsigned linear189_failure;
#define FAIL189(n) (linear189_failure==(n))
#else
#define FAIL189(n) 0
#endif
static int linear189_layout(LinearLayout189 *out,unsigned width,unsigned height,unsigned pitch,unsigned levels,size_t available){
 LinearLayout189 s;if(!out||!width||!height||width>2048||height>2048||levels!=1)return 0;
 s.width=width;s.height=height;s.pitch=pitch;s.row=(size_t)width*4;s.bytes=s.row*height;
 if(pitch<s.row||pitch>65536||(pitch&3))return 0;s.span=(size_t)(height-1)*pitch+s.row;
 if(s.span>available)return 0;*out=s;return 1;
}
static void linear189_release(LinearEntry189 *e){if(e->view)ID3D11ShaderResourceView_Release(e->view);if(e->device)ID3D11Device_Release(e->device);free(e->snapshot);memset(e,0,sizeof *e);}
static ID3D11ShaderResourceView *linear189_get(LinearEntry189 *e,ID3D11Device *device,const void *source,const LinearLayout189 *layout){
 uint8_t *snapshot=NULL;ID3D11Texture2D *texture=NULL;ID3D11ShaderResourceView *view=NULL;UINT support=0;
 if(!e||!device||!source||!layout)return NULL;
 if(e->device&&e->device!=device)goto fail;
 if(e->view&&e->source==source&&e->layout.width==layout->width&&e->layout.height==layout->height&&e->layout.pitch==layout->pitch){
  int same=1;for(unsigned y=0;y<layout->height;y++)if(memcmp(e->snapshot+y*layout->row,(const uint8_t*)source+(size_t)y*layout->pitch,layout->row)){same=0;break;}
  if(same){e->hits++;return e->view;}
 }
 if(!e->device){UINT need=D3D11_FORMAT_SUPPORT_TEXTURE2D|D3D11_FORMAT_SUPPORT_SHADER_SAMPLE;
  if(FAILED(ID3D11Device_CheckFormatSupport(device,DXGI_FORMAT_B8G8R8A8_UNORM,&support))||(support&need)!=need)goto fail;}
 if(FAIL189(1))goto fail;snapshot=malloc(layout->bytes);if(!snapshot)goto fail;
 for(unsigned y=0;y<layout->height;y++)memcpy(snapshot+y*layout->row,(const uint8_t*)source+(size_t)y*layout->pitch,layout->row);
 D3D11_TEXTURE2D_DESC d={0};D3D11_SUBRESOURCE_DATA init={0};d.Width=layout->width;d.Height=layout->height;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;init.pSysMem=snapshot;init.SysMemPitch=(UINT)layout->row;
 if(FAIL189(2)||FAILED(ID3D11Device_CreateTexture2D(device,&d,&init,&texture)))goto fail;
 if(FAIL189(3)||FAILED(ID3D11Device_CreateShaderResourceView(device,(ID3D11Resource*)texture,NULL,&view)))goto fail;
 /* Replace atomically only after allocation and both D3D operations succeed.
  * Failed misses retain their victim and never return its stale view. */
 if(e->view)ID3D11ShaderResourceView_Release(e->view);free(e->snapshot);if(!e->device){e->device=device;ID3D11Device_AddRef(device);}
 e->source=source;e->layout=*layout;e->snapshot=snapshot;e->view=view;e->uploads++;ID3D11Texture2D_Release(texture);return view;
fail:
 if(view)ID3D11ShaderResourceView_Release(view);if(texture)ID3D11Texture2D_Release(texture);free(snapshot);e->failures++;return NULL;
}
#undef FAIL189
#endif
