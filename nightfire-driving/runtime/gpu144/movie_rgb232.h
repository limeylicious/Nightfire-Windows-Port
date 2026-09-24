/* Analysis candidate. Dedicated movie-only mutable RGB565 resources.
 * Caller owns serialized immediate-context access and completes previous work
 * before each get. Never use for immutable material views or retain an older
 * pixel generation through this view across a later get. Guest mappings stay
 * readable/stable during the call; only owned snapshots survive it. */
#ifndef MOVIE_RGB232_H
#define MOVIE_RGB232_H
#include "rgb565148.h"
typedef struct MovieRGB232Entry {
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    ID3D11Texture2D *texture;
    ID3D11ShaderResourceView *view;
    const void *source;
    RGB565148Layout layout;
    unsigned char *snapshot;
    uint64_t hits,uploads,creates,failures;
} MovieRGB232Entry;
static void movie_rgb232_release(MovieRGB232Entry *e)
{
    if(!e)return;
    if(e->view)ID3D11ShaderResourceView_Release(e->view);
    if(e->texture)ID3D11Texture2D_Release(e->texture);
    if(e->context)ID3D11DeviceContext_Release(e->context);
    if(e->device)ID3D11Device_Release(e->device);
    free(e->snapshot);memset(e,0,sizeof *e);
}
static ID3D11ShaderResourceView *movie_rgb232_get(MovieRGB232Entry *e,
    ID3D11Device *device,ID3D11DeviceContext *context,const void *source,
    size_t available,unsigned width,unsigned height,unsigned pitch)
{
    RGB565148Layout s;unsigned char *snapshot=NULL;
    ID3D11Texture2D *texture=NULL;ID3D11ShaderResourceView *view=NULL;
    if(!e)return NULL;
    if(!device||!context||!source||
       !rgb565148_layout(&s,0x11,width,height,pitch,1,available)||
       (uintptr_t)source+s.span<(uintptr_t)source||
       (e->device&&e->device!=device)||(e->context&&e->context!=context))goto fail;
    if(!e->device){
        ID3D11Device *owner=NULL;
        if(ID3D11DeviceContext_GetType(context)!=D3D11_DEVICE_CONTEXT_IMMEDIATE||
           !rgb565148_supported(device))goto fail;
        ID3D11DeviceContext_GetDevice(context,&owner);
        int matches=owner==device;if(owner)ID3D11Device_Release(owner);
        if(!matches)goto fail;
    }
    if(e->view&&e->source==source&&e->layout.width==width&&
       e->layout.height==height&&e->layout.pitch==pitch){
        int same=1;
        for(unsigned y=0;y<height;y++)
            if(memcmp(e->snapshot+y*s.row,(const unsigned char*)source+(size_t)y*pitch,s.row)){same=0;break;}
        if(same){e->hits++;return e->view;}
        for(unsigned y=0;y<height;y++)
            memcpy(e->snapshot+y*s.row,(const unsigned char*)source+(size_t)y*pitch,s.row);
        ID3D11DeviceContext_UpdateSubresource(context,(ID3D11Resource*)e->texture,
            0,NULL,e->snapshot,(UINT)s.row,0);
        /* UpdateSubresource has no HRESULT. Do not cache a successful generation
         * on a removed device; the caller receives failure, never a stale view. */
        if(FAILED(ID3D11Device_GetDeviceRemovedReason(device))){
            ID3D11ShaderResourceView_Release(e->view);e->view=NULL;goto fail;}
        e->uploads++;return e->view;
    }
    snapshot=(unsigned char*)malloc(s.bytes);if(!snapshot)goto fail;
    for(unsigned y=0;y<height;y++)
        memcpy(snapshot+y*s.row,(const unsigned char*)source+(size_t)y*pitch,s.row);
    D3D11_TEXTURE2D_DESC d={0};D3D11_SUBRESOURCE_DATA init={0};
    d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_B5G6R5_UNORM;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    init.pSysMem=snapshot;init.SysMemPitch=(UINT)s.row;
    if(FAILED(ID3D11Device_CreateTexture2D(device,&d,&init,&texture))||
       FAILED(ID3D11Device_CreateShaderResourceView(device,(ID3D11Resource*)texture,NULL,&view)))goto fail;
    /* Identity/layout changes commit only when all new resources are ready.
     * Retained old views continue to own their distinct resource. */
    if(e->view)ID3D11ShaderResourceView_Release(e->view);
    if(e->texture)ID3D11Texture2D_Release(e->texture);
    free(e->snapshot);
    if(!e->device){e->device=device;ID3D11Device_AddRef(device);
        e->context=context;ID3D11DeviceContext_AddRef(context);}
    e->source=source;e->layout=s;e->snapshot=snapshot;e->texture=texture;e->view=view;
    e->uploads++;e->creates++;return view;
fail:
    if(view)ID3D11ShaderResourceView_Release(view);
    if(texture)ID3D11Texture2D_Release(texture);
    free(snapshot);e->failures++;return NULL;
}
#endif
