/* Lean build: Direct3D presentation into the game window.
 * Included at the end of lean_d3d.c so it shares the device and the resident
 * surfaces. The scanout surface is copied GPU to GPU into a flip-model swap
 * chain; DXGI stretches it to the window. This replaces the 640x480 CPU
 * readback and the GDI repaint loop (which redrew at most every ~16 ms).
 * LEAN_SWAPCHAIN=0 restores the old readback + GDI path.
 * LEAN_PRESENT_VSYNC=1 waits for the monitor's vertical sync on Present. */

extern HWND driving_present201_window(void);
extern void driving_present201_direct(int on);

static IDXGISwapChain1 *swap;
static unsigned swap_w,swap_h;
static int swap_state;      /* 0 not tried, 1 active, -1 off or failed */
static int swap_vsync;

static int swap_create(HWND hwnd,unsigned w,unsigned h){
    IDXGIDevice1 *dxdev=NULL;IDXGIAdapter *adapter=NULL;IDXGIFactory2 *factory=NULL;
    HRESULT hr=ID3D11Device_QueryInterface(dev,&IID_IDXGIDevice1,(void**)&dxdev);
    if(SUCCEEDED(hr))hr=IDXGIDevice1_GetAdapter(dxdev,&adapter);
    if(SUCCEEDED(hr))hr=IDXGIAdapter_GetParent(adapter,&IID_IDXGIFactory2,(void**)&factory);
    if(SUCCEEDED(hr)){
        DXGI_SWAP_CHAIN_DESC1 d={0};
        d.Width=w;d.Height=h;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;
        d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.BufferCount=2;
        d.Scaling=DXGI_SCALING_STRETCH;d.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;d.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
        hr=IDXGIFactory2_CreateSwapChainForHwnd(factory,(IUnknown*)dev,hwnd,&d,NULL,NULL,&swap);
        if(FAILED(hr)){d.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;hr=IDXGIFactory2_CreateSwapChainForHwnd(factory,(IUnknown*)dev,hwnd,&d,NULL,NULL,&swap);}
        if(SUCCEEDED(hr)){
            IDXGIFactory2_MakeWindowAssociation(factory,hwnd,DXGI_MWA_NO_ALT_ENTER);
            /* Keep at most one frame queued so input-to-screen delay stays one frame. */
            IDXGIDevice1_SetMaximumFrameLatency(dxdev,1);
        }
    }
    if(factory)IDXGIFactory2_Release(factory);
    if(adapter)IDXGIAdapter_Release(adapter);
    if(dxdev)IDXGIDevice1_Release(dxdev);
    if(FAILED(hr)){LOG("[LEAN-PRESENT] swap chain creation failed %08lX; keeping the GDI window\n",hr);swap=NULL;return 0;}
    swap_w=w;swap_h=h;
    LOG("[LEAN-PRESENT] Direct3D swap chain %ux%u on the game window (vsync=%d)\n",w,h,swap_vsync);
    return 1;
}

/* Shows the frame whose scanout surface is at guest address va.
 * Returns 1 when shown, 0 when the swap chain is off or not ready yet (use the
 * old path), -1 when the swap chain is active but this frame needs CPU pixels
 * (pixels==NULL and the surface is not a resident 1:1 colour surface). */
int lean_d3d_present(uint32_t va,const uint32_t *pixels){
    if(!ready||swap_state<0)return 0;
    if(!swap_state){
        const char *v=getenv("LEAN_SWAPCHAIN");if(v&&*v=='0'){swap_state=-1;LOG("[LEAN-PRESENT] LEAN_SWAPCHAIN=0: readback + GDI window\n");return 0;}
        v=getenv("LEAN_PRESENT_VSYNC");swap_vsync=v&&*v=='1';
    }
    RT *r=NULL;for(unsigned i=0;i<MAX_RT;i++)if(rts[i].tex&&!rts[i].depth&&rts[i].addr==va&&rts[i].gpu_written){r=&rts[i];break;}
    /* 640-wide scanout copies straight across; anything else (e.g. a 1280-wide
     * AA surface) goes through the CPU-averaged readback like before. */
    int direct=r&&r->w==640&&r->h==480;
    if(!direct&&!pixels)return swap_state>0?-1:0;
    if(!swap){
        HWND hwnd=driving_present201_window();
        if(!hwnd)return 0;                       /* window thread not up yet */
        if(!swap_create(hwnd,640,480)){swap_state=-1;return 0;}
        swap_state=1;driving_present201_direct(1);
    }
    ID3D11Texture2D *back=NULL;
    if(FAILED(IDXGISwapChain1_GetBuffer(swap,0,&IID_ID3D11Texture2D,(void**)&back)))return -1;
    if(direct)ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)back,(ID3D11Resource*)r->tex);
    else ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)back,0,NULL,pixels,640*4,0);
    ID3D11Texture2D_Release(back);
    HRESULT hr=IDXGISwapChain1_Present(swap,swap_vsync?1:0,0);
    if(FAILED(hr)){
        static unsigned n;if(n++<4)LOG("[LEAN-PRESENT] Present failed %08lX\n",hr);
        if(hr==DXGI_ERROR_DEVICE_REMOVED||hr==DXGI_ERROR_DEVICE_RESET){swap_state=-1;driving_present201_direct(0);}
        return 0;
    }
    return 1;
}
