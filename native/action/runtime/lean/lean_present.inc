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
static HWND swap_hwnd;static UINT swap_flags;   /* for ResizeBuffers (NF_OVERLAY window-sized buffers) */
static int swap_state;      /* 0 not tried, 1 active, -1 off or failed */
static int swap_vsync;
/* LEAN_PRESENT_UNCAPPED=1 (test): no vsync and no display-thread pacing, with
 * tearing allowed where the system supports it, to show frame-rate headroom. */
static int swap_uncapped,swap_tear;
static int swap_vsync_launcher;   /* LEAN_PRESENT_VSYNC as the launcher set it (swap_vsync can follow the settings) */
static int interp_enabled(void);   /* lean_interp.inc */
/* Every frame actually presented to the window (direct and LEAN_INTERP
 * in-between frames); read by the LEAN_FPS_COUNTER window title. */
volatile LONG lean_screen_presents;

/* Test switches (screen-rate measurements, native-driving/widescreen-test/DPI-PERF.md;
 * both unset = unchanged):
 * NF_PRESENT_STATS=1 logs the adapters of the device and of the window's output,
 *   and every 5 s the time spent in Present, WAS_STILL_DRAWING retries and the
 *   presentation mode (composed, overlay/independent flip).
 * NF_SWAP_FIXED=WxH keeps the swap chain at that size whatever the window, and
 *   DXGI stretches it to the window (DXGI_SCALING_STRETCH). */
static int nf_pstats=-1;static unsigned nf_fixed_w,nf_fixed_h;
static void nf_present_switches(void){
    if(nf_pstats>=0)return;
    const char *v=getenv("NF_PRESENT_STATS");nf_pstats=v&&v[0]=='1';
    v=getenv("NF_SWAP_FIXED");if(v&&sscanf(v,"%ux%u",&nf_fixed_w,&nf_fixed_h)!=2)nf_fixed_w=nf_fixed_h=0;
    if(nf_fixed_w<16||nf_fixed_h<16)nf_fixed_w=nf_fixed_h=0;
}
static volatile LONG64 nf_present_ticks,nf_present_calls,nf_present_busy,nf_present_gaveup;
static LONG64 nf_present_max;
static void nf_present_note(uint64_t ticks,int busy_retries,int gave_up){
    InterlockedAdd64(&nf_present_ticks,(LONG64)ticks);InterlockedIncrement64(&nf_present_calls);
    InterlockedAdd64(&nf_present_busy,busy_retries);if(gave_up)InterlockedIncrement64(&nf_present_gaveup);
    if((LONG64)ticks>nf_present_max)nf_present_max=(LONG64)ticks;
}
static void nf_present_report(const char *who){
    static ULONGLONG next;ULONGLONG now=GetTickCount64();
    if(!nf_pstats||!swap||now<next)return;
    int first=!next;next=now+5000;if(first)return;
    LARGE_INTEGER f;QueryPerformanceFrequency(&f);
    LONG64 c=InterlockedExchange64(&nf_present_calls,0),t=InterlockedExchange64(&nf_present_ticks,0);
    LONG64 b=InterlockedExchange64(&nf_present_busy,0),g=InterlockedExchange64(&nf_present_gaveup,0),mx=nf_present_max;nf_present_max=0;
    const char *mode="unknown";
    {static const GUID iid_media={0xdd95b90b,0xf05f,0x4f6a,{0xbd,0x65,0x25,0xbf,0xb2,0x64,0xbd,0x84}};   /* IDXGISwapChainMedia */
     IDXGISwapChainMedia *m=NULL;
     if(SUCCEEDED(IDXGISwapChain1_QueryInterface(swap,&iid_media,(void**)&m))){
         DXGI_FRAME_STATISTICS_MEDIA s;memset(&s,0,sizeof s);
         HRESULT hr=IDXGISwapChainMedia_GetFrameStatisticsMedia(m,&s);
         if(SUCCEEDED(hr))mode=s.CompositionMode==DXGI_FRAME_PRESENTATION_MODE_COMPOSED?"composed":
                               s.CompositionMode==DXGI_FRAME_PRESENTATION_MODE_OVERLAY?"overlay (independent flip)":
                               s.CompositionMode==DXGI_FRAME_PRESENTATION_MODE_NONE?"none":"composition failure";
         else if(hr==DXGI_ERROR_FRAME_STATISTICS_DISJOINT)mode="disjoint";
         else mode="not available";
         IDXGISwapChainMedia_Release(m);}}
    LOG("[NF-PRESENT] 5s %s: %lld presents, %.2f ms average in Present (max %.2f), %lld WAS_STILL_DRAWING retries, %lld gave up; swap chain %ux%u; mode %s\n",
        who,(long long)c,c?(double)t*1000.0/f.QuadPart/c:0.0,(double)mx*1000.0/f.QuadPart,(long long)b,(long long)g,swap_w,swap_h,mode);
}
static void nf_log_adapter(const char *what,IDXGIAdapter *a){
    DXGI_ADAPTER_DESC d;if(!a||FAILED(IDXGIAdapter_GetDesc(a,&d)))return;
    char name[128];WideCharToMultiByte(CP_UTF8,0,d.Description,-1,name,sizeof name,NULL,NULL);
    LOG("[NF-PRESENT] %s: %s, LUID %08lX:%08lX, vendor %04X device %04X, %llu MB dedicated\n",what,name,
        (unsigned long)d.AdapterLuid.HighPart,(unsigned long)d.AdapterLuid.LowPart,d.VendorId,d.DeviceId,(unsigned long long)(d.DedicatedVideoMemory>>20));
}

static int swap_create(HWND hwnd,unsigned w,unsigned h){
    nf_present_switches();
    if(nf_fixed_w){LOG("[NF-PRESENT] NF_SWAP_FIXED: swap chain %ux%u (window %ux%u), stretched by DXGI\n",nf_fixed_w,nf_fixed_h,w,h);w=nf_fixed_w;h=nf_fixed_h;}
    IDXGIDevice1 *dxdev=NULL;IDXGIAdapter *adapter=NULL;IDXGIFactory2 *factory=NULL;
    HRESULT hr=ID3D11Device_QueryInterface(dev,&IID_IDXGIDevice1,(void**)&dxdev);
    if(SUCCEEDED(hr))hr=IDXGIDevice1_GetAdapter(dxdev,&adapter);
    if(SUCCEEDED(hr))hr=IDXGIAdapter_GetParent(adapter,&IID_IDXGIFactory2,(void**)&factory);
    if(SUCCEEDED(hr)){
        DXGI_SWAP_CHAIN_DESC1 d={0};
        d.Width=w;d.Height=h;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;
        d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.BufferCount=2;
        d.Scaling=DXGI_SCALING_STRETCH;d.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;d.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
        /* Tearing is allowed for uncapped and capped pictures; with the settings overlay
         * these are chosen while the game runs (smooth motion only), so ask for it then too. */
        if(swap_uncapped||(nfo_enabled()&&interp_enabled())){IDXGIFactory5 *f5=NULL;BOOL tear=FALSE;
            if(SUCCEEDED(IDXGIFactory2_QueryInterface(factory,&IID_IDXGIFactory5,(void**)&f5))){
                if(FAILED(IDXGIFactory5_CheckFeatureSupport(f5,DXGI_FEATURE_PRESENT_ALLOW_TEARING,&tear,sizeof tear)))tear=FALSE;
                IDXGIFactory5_Release(f5);}
            swap_tear=tear!=FALSE;if(swap_tear)d.Flags|=DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;}
        hr=IDXGIFactory2_CreateSwapChainForHwnd(factory,(IUnknown*)dev,hwnd,&d,NULL,NULL,&swap);
        if(FAILED(hr)){d.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;hr=IDXGIFactory2_CreateSwapChainForHwnd(factory,(IUnknown*)dev,hwnd,&d,NULL,NULL,&swap);}
        if(SUCCEEDED(hr)){
            IDXGIFactory2_MakeWindowAssociation(factory,hwnd,DXGI_MWA_NO_ALT_ENTER);
            /* Keep at most one frame queued so input-to-screen delay stays one frame. */
            IDXGIDevice1_SetMaximumFrameLatency(dxdev,1);
            if(nf_pstats){   /* NF_PRESENT_STATS: which GPU renders and which drives the window's monitor */
                nf_log_adapter("device adapter",adapter);
                IDXGIOutput *o=NULL;
                if(SUCCEEDED(IDXGISwapChain1_GetContainingOutput(swap,&o))){
                    DXGI_OUTPUT_DESC od;if(SUCCEEDED(IDXGIOutput_GetDesc(o,&od))){char n[64];WideCharToMultiByte(CP_UTF8,0,od.DeviceName,-1,n,sizeof n,NULL,NULL);
                        LOG("[NF-PRESENT] window's output %s, desktop %ldx%ld\n",n,od.DesktopCoordinates.right-od.DesktopCoordinates.left,od.DesktopCoordinates.bottom-od.DesktopCoordinates.top);}
                    IDXGIAdapter *oa=NULL;if(SUCCEEDED(IDXGIOutput_GetParent(o,&IID_IDXGIAdapter,(void**)&oa))){nf_log_adapter("output adapter",oa);IDXGIAdapter_Release(oa);}
                    IDXGIOutput_Release(o);
                }else LOG("[NF-PRESENT] window's output: not known yet\n");
            }
        }
    }
    if(factory)IDXGIFactory2_Release(factory);
    if(adapter)IDXGIAdapter_Release(adapter);
    if(dxdev)IDXGIDevice1_Release(dxdev);
    if(FAILED(hr)){LOG("[LEAN-PRESENT] swap chain creation failed %08lX; keeping the GDI window\n",hr);swap=NULL;return 0;}
    swap_w=w;swap_h=h;swap_hwnd=hwnd;
    {DXGI_SWAP_CHAIN_DESC1 cd;if(SUCCEEDED(IDXGISwapChain1_GetDesc1(swap,&cd)))swap_flags=cd.Flags;}
    LOG("[LEAN-PRESENT] Direct3D swap chain %ux%u on the game window (vsync=%d uncapped=%d tearing=%d)\n",w,h,swap_vsync,swap_uncapped,swap_tear);
    return 1;
}

/* NF_OVERLAY=1: the swap chain buffers follow the window's client size, so
 * the picture is drawn at the real window resolution (lean_overlay.inc) instead
 * of 640x480 stretched by DXGI. Called by whichever thread presents, with the
 * renderer lock held, before it takes the back buffer. */
static void swap_fit_window(void){
    static unsigned failed_w,failed_h;
    if(!swap||!swap_hwnd||!nfo_enabled()||nf_fixed_w)return;   /* NF_SWAP_FIXED: DXGI stretches */
    RECT r;if(!GetClientRect(swap_hwnd,&r)||r.right<=0||r.bottom<=0)return;   /* minimised */
    unsigned w=(unsigned)r.right,h=(unsigned)r.bottom;
    if((w==swap_w&&h==swap_h)||(w==failed_w&&h==failed_h))return;
    /* The back buffer is never left bound on ctx (presents unbind it; the display
     * thread binds it only in its deferred context), so the game's own bindings stay. */
    ID3D11DeviceContext_Flush(ctx);
    HRESULT hr=IDXGISwapChain1_ResizeBuffers(swap,0,w,h,DXGI_FORMAT_UNKNOWN,swap_flags);
    if(SUCCEEDED(hr)){LOG("[NF-OVERLAY] swap chain now %ux%u (window size)\n",w,h);swap_w=w;swap_h=h;failed_w=failed_h=0;}
    else{LOG("[NF-OVERLAY] ResizeBuffers %ux%u failed %08lX; keeping %ux%u\n",w,h,(unsigned long)hr,swap_w,swap_h);failed_w=w;failed_h=h;}
}

/* LEAN_DISPLAY_GAMMA=g (presentation option, off by default): show the frame
 * through out = in^g. The game sets no gamma ramp; this only approximates the
 * TV/video-capture tone curve of the owner's console reference, where g of about
 * 0.8 matched every compared region (2026-10-05). Captures are not affected. */
static float present_gamma=-1;
static ID3D11VertexShader *gamma_vs;static ID3D11PixelShader *gamma_ps;
static ID3D11RasterizerState *gamma_rs;static ID3D11Buffer *gamma_cb;
static void gamma_init(void){
    if(present_gamma<0){
        const char *v=getenv("LEAN_DISPLAY_GAMMA");present_gamma=v?(float)atof(v):0;
        if(present_gamma>0&&present_gamma!=1.0f){
            static const char hs[]=
                "cbuffer G:register(b0){float4 g;};Texture2D t:register(t0);\n"
                "float4 vmain(uint i:SV_VertexID):SV_Position{float2 p=float2((i<<1)&2,i&2);return float4(p*float2(2,-2)+float2(-1,1),0,1);}\n"
                "float4 pmain(float4 p:SV_Position):SV_Target{float4 c=t.Load(int3(p.xy,0));return float4(pow(saturate(c.rgb),g.x),1);}\n";
            ID3DBlob *c=NULL,*e=NULL;
            if(SUCCEEDED(D3DCompile(hs,sizeof hs-1,"lean-gamma",NULL,NULL,"vmain","vs_5_0",0,0,&c,&e))){ID3D11Device_CreateVertexShader(dev,ID3D10Blob_GetBufferPointer(c),ID3D10Blob_GetBufferSize(c),NULL,&gamma_vs);ID3D10Blob_Release(c);}
            if(e){ID3D10Blob_Release(e);e=NULL;}
            if(SUCCEEDED(D3DCompile(hs,sizeof hs-1,"lean-gamma",NULL,NULL,"pmain","ps_5_0",0,0,&c,&e))){ID3D11Device_CreatePixelShader(dev,ID3D10Blob_GetBufferPointer(c),ID3D10Blob_GetBufferSize(c),NULL,&gamma_ps);ID3D10Blob_Release(c);}
            if(e)ID3D10Blob_Release(e);
            D3D11_RASTERIZER_DESC rd={0};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
            ID3D11Device_CreateRasterizerState(dev,&rd,&gamma_rs);
            D3D11_BUFFER_DESC bd={0};bd.ByteWidth=16;bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
            float gv[4]={present_gamma,0,0,0};D3D11_SUBRESOURCE_DATA sd={gv,0,0};ID3D11Device_CreateBuffer(dev,&bd,&sd,&gamma_cb);
            LOG("[LEAN-PRESENT] display gamma %.3f %s\n",present_gamma,gamma_vs&&gamma_ps&&gamma_rs&&gamma_cb?"on":"unavailable");
        }
    }
}
static int present_gamma_pass(ID3D11Texture2D *back,ID3D11ShaderResourceView *src){
    gamma_init();
    if(!(present_gamma>0&&present_gamma!=1.0f)||!gamma_vs||!gamma_ps||!gamma_rs||!gamma_cb)return 0;
    ID3D11RenderTargetView *rtv=NULL;
    if(FAILED(ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)back,NULL,&rtv)))return 0;
    D3D11_VIEWPORT vp={0,0,640,480,0,1};ID3D11ShaderResourceView *none=NULL;
    ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&rtv,NULL);ID3D11DeviceContext_RSSetViewports(ctx,1,&vp);
    ID3D11DeviceContext_OMSetBlendState(ctx,NULL,NULL,0xffffffffu);ID3D11DeviceContext_OMSetDepthStencilState(ctx,NULL,0);
    ID3D11DeviceContext_RSSetState(ctx,gamma_rs);ID3D11DeviceContext_IASetInputLayout(ctx,NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(ctx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(ctx,gamma_vs,NULL,0);ID3D11DeviceContext_GSSetShader(ctx,NULL,NULL,0);
    ID3D11DeviceContext_PSSetShader(ctx,gamma_ps,NULL,0);ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&gamma_cb);
    ID3D11DeviceContext_PSSetShaderResources(ctx,0,1,&src);
    ID3D11DeviceContext_Draw(ctx,3,0);
    ID3D11DeviceContext_PSSetShaderResources(ctx,0,1,&none);
    ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);
    ID3D11RenderTargetView_Release(rtv);
    return 1;
}

/* Shows the frame whose scanout surface is at guest address va.
 * Returns 1 when shown, 0 when the swap chain is off or not ready yet (use the
 * old path), -1 when the swap chain is active but this frame needs CPU pixels
 * (pixels==NULL and the surface is not a resident 1:1 colour surface). */
static int lean_d3d_present_impl(uint32_t va,const uint32_t *pixels);
static int interp_pace_due;static void interp_game_pace(void);   /* smooth motion: the screen-rate wait, outside the lock */
int lean_d3d_present(uint32_t va,const uint32_t *pixels){GAME_LOCK();int r=lean_d3d_present_impl(va,pixels);CTX_UNLOCK();if(r>0)InterlockedIncrement(&nfo_game_frames);
    if(interp_pace_due){interp_pace_due=0;interp_game_pace();}return r;}
/* Waits until the QPC time `until` on a high-resolution waitable timer (accurate to
 * about 0.5 ms whatever the system timer resolution), then yields for the rest.
 * timer: from CreateWaitableTimerExW(.., CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, ..), or
 * NULL for plain Sleep. Used for display-only pacing (lean_interp.inc) and below. */
static void lean_wait_until(uint64_t until,HANDLE timer,int64_t qpf){
    for(;;){LARGE_INTEGER q;QueryPerformanceCounter(&q);int64_t left=(int64_t)(until-(uint64_t)q.QuadPart);if(left<=0)return;
        if(timer&&left*2000>qpf){   /* more than 0.5 ms left: sleep on the timer to 0.3 ms before */
            LARGE_INTEGER due;due.QuadPart=-(LONGLONG)((left*10000000/qpf)-3000);if(due.QuadPart>-1)due.QuadPart=-1;
            if(SetWaitableTimer(timer,&due,0,NULL,NULL,FALSE)){WaitForSingleObject(timer,100);continue;}}
        int64_t ms=left*1000/qpf;if(ms>1)Sleep((DWORD)(ms-1));else Sleep(0);}
}
/* Smooth motion chosen in the settings (NF_OVERLAY, Action): the game thread hands its
 * frame to the display thread and no longer waits in Present. Where the launcher asked
 * for vsync, Present(1) with a frame latency of 1 held the game thread to one frame per
 * screen refresh wherever the game's own pacer does not run (menus, movies, loading;
 * the 10-09 runs: about 3000 frames a second there without this). Keep that wait: at
 * most one game frame per refresh period of the window's screen, and no wait at all
 * when the game is slower than the screen (its 60 Hz pacer in play). Driving (smooth
 * motion set by its launcher, game paced by its own step timer) is left as it was. */
static void interp_game_pace(void){
    if(!swap_vsync_launcher||!nfo_enabled()||nfo_smooth_forced())return;
    static LARGE_INTEGER hz;static uint64_t free_at,period,checked;static HANDLE timer;
    LARGE_INTEGER q;QueryPerformanceCounter(&q);uint64_t now=(uint64_t)q.QuadPart;
    if(!hz.QuadPart){QueryPerformanceFrequency(&hz);timer=CreateWaitableTimerExW(NULL,NULL,2,TIMER_ALL_ACCESS);}
    if(!period||now-checked>(uint64_t)hz.QuadPart*2){   /* the window's screen rate, re-read every 2 s */
        DWORD f=0;HMONITOR mon=MonitorFromWindow(driving_present201_window(),MONITOR_DEFAULTTONEAREST);
        MONITORINFOEXW mi;memset(&mi,0,sizeof mi);mi.cbSize=sizeof mi;
        if(mon&&GetMonitorInfoW(mon,(MONITORINFO*)&mi)){DEVMODEW dm;memset(&dm,0,sizeof dm);dm.dmSize=sizeof dm;
            if(EnumDisplaySettingsW(mi.szDevice,ENUM_CURRENT_SETTINGS,&dm))f=dm.dmDisplayFrequency;}
        if(f<24||f>1000)f=60;
        uint64_t p=(uint64_t)hz.QuadPart/f;
        if(p!=period)LOG("[LEAN-PRESENT] smooth motion: game frames held to the screen's %lu Hz outside the game's own pacing (as vsync did)\n",(unsigned long)f);
        period=p;checked=now;}
    if(free_at>now){   /* the last frame would still be waiting for the screen: wait as Present(1) did */
        lean_wait_until(free_at,timer,hz.QuadPart);QueryPerformanceCounter(&q);now=(uint64_t)q.QuadPart;}
    free_at=now+period;
}
static int lean_d3d_present_impl(uint32_t va,const uint32_t *pixels){
    if(!ready||swap_state<0)return 0;
    if(!swap_state){
        const char *v=getenv("LEAN_SWAPCHAIN");if(v&&*v=='0'){swap_state=-1;LOG("[LEAN-PRESENT] LEAN_SWAPCHAIN=0: readback + GDI window\n");return 0;}
        v=getenv("LEAN_PRESENT_VSYNC");swap_vsync=v&&*v=='1';
        v=getenv("LEAN_PRESENT_UNCAPPED");swap_uncapped=v&&*v=='1';if(swap_uncapped)swap_vsync=0;
        swap_vsync_launcher=swap_vsync;
    }
    RT *r=NULL;for(unsigned i=0;i<MAX_RT;i++)if(rts[i].tex&&!rts[i].depth&&rts[i].addr==va&&rts[i].gpu_written){r=&rts[i];break;}
    /* 640-wide scanout copies straight across; anything else (e.g. a 1280-wide
     * AA surface) goes through the CPU-averaged readback like before. */
    int direct=r&&r->w==640&&r->h==480;
    if(!direct&&!pixels)return swap_state>0?-1:0;
    if(!swap){
        HWND hwnd=driving_present201_window();
        if(!hwnd)return 0;                       /* window thread not up yet */
        unsigned cw=640,ch=480;
        if(nfo_enabled()){RECT cr;if(GetClientRect(hwnd,&cr)&&cr.right>0&&cr.bottom>0){cw=(unsigned)cr.right;ch=(unsigned)cr.bottom;}}
        if(!swap_create(hwnd,cw,ch)){swap_state=-1;return 0;}
        swap_state=1;driving_present201_direct(1);
    }
    /* LEAN_INTERP: the display thread presents; hand it the finished frame. */
    if(interp_enabled()){
        gamma_init();
        if(direct){interp_end_frame(r->tex,r->srv);interp_pace_due=1;return 1;}
        if(!pixels)return -1;                     /* ask for CPU pixels */
        /* Not a plain 640x480 surface (movies, AA scanout): the display
         * thread shows these CPU pixels as the frame. */
        static ID3D11Texture2D *cpu_scan;
        if(!cpu_scan){D3D11_TEXTURE2D_DESC td={640,480,1,1,DXGI_FORMAT_B8G8R8A8_UNORM,{1,0},D3D11_USAGE_DEFAULT,D3D11_BIND_SHADER_RESOURCE,0,0};
            ID3D11Device_CreateTexture2D(dev,&td,NULL,&cpu_scan);}
        static ID3D11ShaderResourceView *cpu_scan_srv;
        if(cpu_scan&&!cpu_scan_srv)ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)cpu_scan,NULL,&cpu_scan_srv);
        if(cpu_scan){ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)cpu_scan,0,NULL,pixels,640*4,0);interp_end_frame(cpu_scan,cpu_scan_srv);interp_pace_due=1;return 1;}
    }
    swap_fit_window();
    ID3D11Texture2D *back=NULL;
    if(FAILED(IDXGISwapChain1_GetBuffer(swap,0,&IID_ID3D11Texture2D,(void**)&back)))return -1;
    if(nfo_enabled()){   /* NF_OVERLAY: drawn at window size with the overlay's settings */
        ID3D11ShaderResourceView *src=direct?r->srv:NULL;
        if(!direct){   /* CPU pixels (movies before the swap chain path, AA scanout) */
            static ID3D11Texture2D *nfo_cpu;static ID3D11ShaderResourceView *nfo_cpu_srv;
            if(!nfo_cpu){D3D11_TEXTURE2D_DESC td={640,480,1,1,DXGI_FORMAT_B8G8R8A8_UNORM,{1,0},D3D11_USAGE_DEFAULT,D3D11_BIND_SHADER_RESOURCE,0,0};
                if(SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&td,NULL,&nfo_cpu)))ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)nfo_cpu,NULL,&nfo_cpu_srv);}
            if(nfo_cpu){ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)nfo_cpu,0,NULL,pixels,640*4,0);src=nfo_cpu_srv;}
        }
        nfo_present(ctx,back,src);
    }
    else if(direct&&r->srv&&present_gamma_pass(back,r->srv)){}   /* LEAN_DISPLAY_GAMMA */
    else if(direct)ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)back,(ID3D11Resource*)r->tex);
    else ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)back,0,NULL,pixels,640*4,0);
    {   /* LEAN_DUMP_BACKBUFFER=N (diagnostic): save what the window shows on the
         * Nth presented frame, after any display gamma, as backbuffer-N.bmp. */
        static int want=-1;static unsigned presents;
        if(want<0){const char *v=getenv("LEAN_DUMP_BACKBUFFER");want=v?atoi(v):0;}
        const char *dir=getenv("DRIVING_CAPTURE_DIR");
        if(want>0&&++presents==(unsigned)want&&dir){
            D3D11_TEXTURE2D_DESC sd;ID3D11Texture2D_GetDesc(back,&sd);sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;sd.MiscFlags=0;
            ID3D11Texture2D *stg=NULL;D3D11_MAPPED_SUBRESOURCE m;
            if(SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&sd,NULL,&stg))){
                ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)stg,(ID3D11Resource*)back);
                if(SUCCEEDED(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)stg,0,D3D11_MAP_READ,0,&m))){
                    char path[MAX_PATH];snprintf(path,sizeof path,"%s/backbuffer-%d.bmp",dir,want);FILE *fo=fopen(path,"wb");
                    if(fo){uint32_t W=sd.Width,H=sd.Height,img=W*H*4;uint8_t hd[54]={'B','M'};
                        *(uint32_t*)(hd+2)=54+img;*(uint32_t*)(hd+10)=54;*(uint32_t*)(hd+14)=40;*(int32_t*)(hd+18)=(int32_t)W;*(int32_t*)(hd+22)=-(int32_t)H;
                        *(uint16_t*)(hd+26)=1;*(uint16_t*)(hd+28)=32;*(uint32_t*)(hd+34)=img;fwrite(hd,1,54,fo);
                        for(uint32_t y=0;y<H;y++)fwrite((uint8_t*)m.pData+(size_t)y*m.RowPitch,1,W*4,fo);fclose(fo);
                        LOG("[LEAN-PRESENT] saved %s\n",path);}
                    ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)stg,0);
                }
                ID3D11Texture2D_Release(stg);
            }
        }
    }
    ID3D11Texture2D_Release(back);
    LARGE_INTEGER pq0={0};if(nf_pstats)QueryPerformanceCounter(&pq0);
    HRESULT hr=IDXGISwapChain1_Present(swap,swap_vsync?1:0,(swap_tear&&!swap_vsync)?DXGI_PRESENT_ALLOW_TEARING:0);   /* tearing only without vsync */
    if(nf_pstats){LARGE_INTEGER pq1;QueryPerformanceCounter(&pq1);nf_present_note((uint64_t)(pq1.QuadPart-pq0.QuadPart),0,0);nf_present_report("game thread (smooth mode off)");}
    if(SUCCEEDED(hr))InterlockedIncrement(&lean_screen_presents);   /* LEAN_FPS_COUNTER */
    if(FAILED(hr)){
        static unsigned n;if(n++<4)LOG("[LEAN-PRESENT] Present failed %08lX\n",hr);
        if(hr==DXGI_ERROR_DEVICE_REMOVED||hr==DXGI_ERROR_DEVICE_RESET){swap_state=-1;driving_present201_direct(0);}
        return 0;
    }
    return 1;
}
