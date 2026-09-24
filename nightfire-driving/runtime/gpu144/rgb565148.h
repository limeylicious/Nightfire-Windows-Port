/* Isolated proposal: no guest address decoding, synchronization or renderer hook.
 * Caller validates/finishes GPU aliases BEFORE calling. Returned SRV is borrowed.
 * All actual texel bits are compared; no RGBA8 expansion or color approximation. */
#ifndef DRIVING_RGB565148_H
#define DRIVING_RGB565148_H
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>
#include <d3d11.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "d3d8_swizzle.h"

typedef struct RGB565148Layout {
    unsigned format,width,height,pitch;
    size_t row,bytes,span;
} RGB565148Layout;
typedef struct RGB565148Entry {
    ID3D11Device *device; /* held reference makes the device identity stable */
    ID3D11ShaderResourceView *view;
    const void *source;
    RGB565148Layout layout;
    unsigned char *snapshot;
    uint64_t hits,uploads,changed,failures;
} RGB565148Entry;
static int rgb565148_layout(RGB565148Layout *out,unsigned format,unsigned width,
                            unsigned height,unsigned pitch,unsigned levels,size_t available)
{
    RGB565148Layout s;
    if(!out || levels!=1 || !width || !height || width>2048 || height>2048 ||
       (format!=5 && format!=0x11))return 0;
    s.format=format;s.width=width;s.height=height;s.row=(size_t)width*2;
    s.bytes=s.row*height;
    if(format==5){
        if((width&(width-1)) || (height&(height-1)) || (pitch && pitch!=s.row))return 0;
        s.pitch=(unsigned)s.row;s.span=s.bytes;
    }else{
        if(pitch<s.row || pitch>65536 || (pitch&1))return 0;
        s.pitch=pitch;s.span=(size_t)(height-1)*pitch+s.row;
    }
    if(s.span>available)return 0;
    *out=s;return 1;
}
static int rgb565148_supported(ID3D11Device *device)
{
    UINT support=0,required=D3D11_FORMAT_SUPPORT_TEXTURE2D|
        D3D11_FORMAT_SUPPORT_SHADER_LOAD|D3D11_FORMAT_SUPPORT_SHADER_SAMPLE;
    return device && SUCCEEDED(ID3D11Device_CheckFormatSupport(device,DXGI_FORMAT_B5G6R5_UNORM,&support)) &&
        (support&required)==required;
}
static void rgb565148_release(RGB565148Entry *e)
{
    if(!e)return;
    if(e->view)ID3D11ShaderResourceView_Release(e->view);
    if(e->device)ID3D11Device_Release(e->device);
    free(e->snapshot);memset(e,0,sizeof *e);
}
static ID3D11ShaderResourceView *rgb565148_get(RGB565148Entry *e,ID3D11Device *device,
    const void *source,size_t available,unsigned format,unsigned width,unsigned height,
    unsigned pitch,unsigned levels)
{
    RGB565148Layout s;unsigned char *snapshot=NULL,*decoded=NULL;
    ID3D11Texture2D *texture=NULL;ID3D11ShaderResourceView *view=NULL;
    D3D11_TEXTURE2D_DESC desc={0};D3D11_SUBRESOURCE_DATA initial={0};int same=0;
    if(!e)return NULL;
    if(!device || !source || !rgb565148_layout(&s,format,width,height,pitch,levels,available) ||
       (uintptr_t)source+s.span<(uintptr_t)source || (e->device && e->device!=device))goto fail;
    /* First admission only; unsupported devices never receive another format. */
    if(!e->device && !rgb565148_supported(device))goto fail;
    if(e->view && e->source==source && e->layout.format==s.format &&
       e->layout.width==width && e->layout.height==height && e->layout.pitch==s.pitch){
        same=1;
        if(format==5)same=!memcmp(e->snapshot,source,s.bytes);
        else for(unsigned y=0;y<height;y++)
            if(memcmp(e->snapshot+y*s.row,(const unsigned char*)source+(size_t)y*s.pitch,s.row)){same=0;break;}
        if(same){e->hits++;return e->view;}
    }
    snapshot=(unsigned char*)malloc(s.bytes);if(!snapshot)goto fail;
    if(format==5)memcpy(snapshot,source,s.bytes);
    else for(unsigned y=0;y<height;y++)memcpy(snapshot+y*s.row,(const unsigned char*)source+(size_t)y*s.pitch,s.row);
    initial.pSysMem=snapshot;initial.SysMemPitch=(UINT)s.row;
    if(format==5){
        decoded=(unsigned char*)malloc(s.bytes);if(!decoded)goto fail;
        xbox_unswizzle_rect(decoded,snapshot,width,height,2);initial.pSysMem=decoded;
    }
    desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_B5G6R5_UNORM;desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    if(FAILED(ID3D11Device_CreateTexture2D(device,&desc,&initial,&texture)))goto fail;
    if(FAILED(ID3D11Device_CreateShaderResourceView(device,(ID3D11Resource*)texture,NULL,&view)))goto fail;
    /* Commit only after the snapshot and new immutable resource both exist.
     * Previous queued draws keep their D3D context references. Never mutate an
     * older texture or retain a pointer to temporary caller/decoder storage. */
    if(e->view){e->changed++;ID3D11ShaderResourceView_Release(e->view);}
    free(e->snapshot);
    if(!e->device){e->device=device;ID3D11Device_AddRef(device);}
    e->view=view;e->source=source;e->layout=s;e->snapshot=snapshot;e->uploads++;
    ID3D11Texture2D_Release(texture);free(decoded);return view;
fail:
    if(view)ID3D11ShaderResourceView_Release(view);
    if(texture)ID3D11Texture2D_Release(texture);
    free(decoded);free(snapshot);e->failures++;return NULL;
}
#endif
