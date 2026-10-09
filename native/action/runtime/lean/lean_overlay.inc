/* Settings overlay (NF_OVERLAY=1, off by default): a first test of in-game
 * PC settings, shared by both engines (this renderer copy is the same file in
 * nightfire-driving-native and nightfire-port-native).
 *
 * F10 (or the ` key left of 1) opens or closes a small menu drawn over the picture. Up/Down picks a row,
 * Left/Right changes it, Esc or F10 closes. While it is open the game gets no
 * keyboard or mouse input. Settings apply at once and are saved, when the menu
 * closes, to %LOCALAPPDATA%\NightfirePC\overlay-settings.ini, which is read
 * again at the next start (and then wins over the launcher's LEAN_DISPLAY_GAMMA).
 *
 * With NF_OVERLAY=1 the swap chain also follows the window's real size, and the
 * frame is drawn into it here (brightness, 4:3 or 16:9 bars, smooth or sharp scaling)
 * instead of being copied 640x480 and stretched by DXGI. Picture shape
 * Widescreen 16:9 is the games' own 16:9 mode (the Xbox dashboard setting): the
 * kernel's video-flags answer gets the widescreen bit (nf_overlay_widescreen
 * below), the game widens its camera and HUD, and the picture is shown 16:9.
 * The games read that flag once at start-up, so a change needs a restart. Resolution draws the
 * game's full-screen surfaces 2, 3 or 4 times larger than the Xbox's 640x480, or
 * as large as the window shows them (rt_render_scale below; lean_d3d.c does the
 * scaling). Original keeps the Xbox size.
 *
 * Text is drawn with GDI into a small bitmap that is re-uploaded only when the
 * menu or the frame counter changes.
 *
 * The window code (driving_present201.c / fb_present.c) owns the two globals
 * below; this file installs the key handler into them. */
extern int (*volatile nf_overlay_key_hook)(HWND,UINT,WPARAM,LPARAM);
extern volatile LONG nf_overlay_menu_open;

static int nfo_state=-1;                /* -1 unread, 0 off, 1 on */
static int nfo_enabled(void){
    if(nfo_state<0){const char *v=getenv("NF_OVERLAY");nfo_state=v&&*v=='1';}
    return nfo_state;
}

/* ------------------------------------------------------------ settings */
enum {NFO_BRIGHT,NFO_SHAPE,NFO_FILTER,NFO_FPS,NFO_RES,NFO_DISPLAY,NFO_CLOSE,NFO_ROWS};
#include "../nightfire_host_window.h"   /* display mode: nf_window_get_mode / nf_window_set_mode */
static const char *const nfo_display_names[3]={"Windowed","Fullscreen","Borderless fullscreen"};
static volatile LONG nfo_display=-1;   /* -1: not chosen yet (the launcher's start mode stays) */
extern HWND driving_present201_window(void);
static const char *const nfo_bright_names[]={"Off","Low","Medium","High"};
static const float nfo_bright_gamma[]={1.0f,0.9f,0.8f,0.7f};
#define NFO_SHAPE_N 3
static const char *const nfo_shape_names[NFO_SHAPE_N]={"Stretch to window","Keep 4:3 (black bars)","Widescreen 16:9"};
static const char *const nfo_filter_names[]={"Smooth","Sharp"};
static const char *const nfo_onoff[]={"Off","On"};
#define NFO_RES_N 5
static const char *const nfo_res_names[NFO_RES_N]={"Original 640x480","2x 1280x960","3x 1920x1440","4x 2560x1920","Match window"};
static volatile LONG nfo_bright,nfo_shape,nfo_filter,nfo_fps,nfo_res;
static volatile LONG nfo_sel;
static volatile LONG nfo_save_due;
static volatile LONG nfo_loaded;static SRWLOCK nfo_load_lock=SRWLOCK_INIT;
static volatile float nfo_cur_k=1;      /* internal resolution in use (rt_render_scale), for the menu */
static volatile LONG nfo_game_frames;   /* game frames shown (lean_d3d_present), for the frame counter */

static void nfo_path(char *out,size_t n){
    const char *base=getenv("LOCALAPPDATA");
    snprintf(out,n,"%s\\NightfirePC",base&&*base?base:".");
    CreateDirectoryA(out,NULL);
    size_t l=strlen(out);snprintf(out+l,n-l,"\\overlay-settings.ini");
}
static void nfo_load(void){
    if(nfo_loaded)return;
    /* The game thread (rt_render_scale) and the display thread can both get here first. */
    AcquireSRWLockExclusive(&nfo_load_lock);
    if(nfo_loaded){ReleaseSRWLockExclusive(&nfo_load_lock);return;}
    /* Defaults: what the launcher set before the overlay existed. */
    const char *g=getenv("LEAN_DISPLAY_GAMMA");float gv=g?(float)atof(g):1.0f;
    LONG b=0;for(LONG i=1;i<4;i++)if(gv>0&&fabsf(gv-nfo_bright_gamma[i])<fabsf(gv-nfo_bright_gamma[b]))b=i;
    nfo_bright=b;nfo_shape=0;nfo_filter=0;nfo_fps=0;nfo_res=0;
    char path[MAX_PATH];nfo_path(path,sizeof path);
    FILE *f=fopen(path,"r");
    if(f){char line[128];
        while(fgets(line,sizeof line,f)){
            char *eq=strchr(line,'=');if(!eq)continue;*eq=0;long v=atol(eq+1);
            if(!strcmp(line,"brightness")&&v>=0&&v<4)nfo_bright=v;
            else if(!strcmp(line,"shape")&&v>=0&&v<NFO_SHAPE_N)nfo_shape=v;
            else if(!strcmp(line,"filter")&&v>=0&&v<2)nfo_filter=v;
            else if(!strcmp(line,"fps")&&v>=0&&v<2)nfo_fps=v;
            else if(!strcmp(line,"resolution")&&v>=0&&v<NFO_RES_N)nfo_res=v;
            else if(!strcmp(line,"display")&&v>=0&&v<3)nfo_display=v;
        }
        fclose(f);
    }
    LOG("[NF-OVERLAY] on (F10). brightness=%s shape=%s filter=%s fps=%s resolution=%s; settings %s %s\n",
        nfo_bright_names[nfo_bright],nfo_shape_names[nfo_shape],nfo_filter_names[nfo_filter],nfo_onoff[nfo_fps],nfo_res_names[nfo_res],
        f?"read from":"will be saved to",path);
    InterlockedExchange(&nfo_loaded,1);ReleaseSRWLockExclusive(&nfo_load_lock);
}
static void nfo_save(void){
    char path[MAX_PATH];nfo_path(path,sizeof path);
    FILE *f=fopen(path,"w");if(!f){LOG("[NF-OVERLAY] could not save %s\n",path);return;}
    fprintf(f,"brightness=%ld\nshape=%ld\nfilter=%ld\nfps=%ld\nresolution=%ld\n",nfo_bright,nfo_shape,nfo_filter,nfo_fps,nfo_res);
    if(nfo_display>=0)fprintf(f,"display=%ld\n",nfo_display);
    fclose(f);
}

/* Widescreen 16:9 (Picture shape 2): asked by the kernel's video-flags query
 * (ExQueryNonVolatileSetting index 8, kernel_bridge.c / kernel_xbox.c), which
 * adds the widescreen bit when this returns 1. The game reads it once at start,
 * so the answer given is kept for the menu's "restart to apply" hint. */
static volatile LONG nfo_ws_told;       /* what the running game was told (0 = 4:3) */
int nf_overlay_widescreen(void){
    if(!nfo_enabled())return 0;
    nfo_load();
    int ws=nfo_shape==2;
    InterlockedExchange(&nfo_ws_told,ws);
    return ws;
}

/* The same settings for the game's own "PC Graphics" menu page (Action,
 * runtime/native_action/pcg_menu.c): 0 picture shape, 1 resolution, 2 brightness,
 * 3 scaling. Changes apply at once and are saved to the same file as F10. */
int nf_overlay_get(int which){
    if(!nfo_enabled())return -1;
    nfo_load();
    switch(which){case 0:return nfo_shape;case 1:return nfo_res;case 2:return nfo_bright;case 3:return nfo_filter;
    case 4:return nfo_display>=0?nfo_display:nf_window_get_mode(driving_present201_window());}
    return -1;
}
static void nfo_apply_display(LONG v){   /* 4: display mode, switched at once */
    InterlockedExchange(&nfo_display,v);
    {HWND w=driving_present201_window();   /* switched on the window's own thread (nfo_key) */
     if(w)PostMessageA(w,NF_HOST_SET_MODE,(WPARAM)v,0);}
    LOG("[NF-OVERLAY] display mode %s\n",nfo_display_names[v]);
}
void nf_overlay_set(int which,int v){
    static const int count[5]={NFO_SHAPE_N,NFO_RES_N,4,2,3};
    if(!nfo_enabled()||which<0||which>4||v<0||v>=count[which])return;
    nfo_load();
    if(which==4){if(nfo_display!=v||nf_window_get_mode(driving_present201_window())!=v){nfo_apply_display(v);nfo_save();}return;}
    volatile LONG *p=which==0?&nfo_shape:which==1?&nfo_res:which==2?&nfo_bright:&nfo_filter;
    if(*p==v)return;
    InterlockedExchange(p,v);
    nfo_save();
    LOG("[NF-OVERLAY] PC Graphics page: setting %d = %d (saved)\n",which,v);
}
int nf_overlay_widescreen_told(void){return nfo_ws_told!=0;}   /* what the running game was told (restart hint) */

/* Mouse in the game's menus (Action, pcg_menu.c): a point in the window's client
 * area (w x h, real pixels) to the game's 640x480 picture, through the same
 * rectangle nfo_present draws the picture into (stretch, 4:3 or 16:9 bars).
 * Returns 0 outside the picture. */
int nf_overlay_client_to_game(int x,int y,int w,int h,int *gx,int *gy){
    if(w<=0||h<=0)return 0;
    float W=(float)w,H=(float)h,vx=0,vy=0,vw=W,vh=H;
    if(nfo_shape==1){if(W*3>H*4){vw=floorf(H*4/3);vx=floorf((W-vw)/2);}else{vh=floorf(W*3/4);vy=floorf((H-vh)/2);}}
    else if(nfo_shape==2){if(W*9>H*16){vw=floorf(H*16/9);vx=floorf((W-vw)/2);}else{vh=floorf(W*9/16);vy=floorf((H-vh)/2);}}
    float fx=(x-vx)*640.0f/vw,fy=(y-vy)*480.0f/vh;
    if(fx<0||fy<0||fx>=640||fy>=480)return 0;
    *gx=(int)fx;*gy=(int)fy;return 1;
}

/* Internal resolution (lean_d3d.c rt_get): how many times larger than the Xbox's
 * 640x480 the full-screen surfaces are drawn. Called on the game's rendering
 * thread; a change applies from the next game frame (at most every 250 ms, so
 * resizing the window with Match window does not rebuild the surfaces constantly). */
extern HWND driving_present201_window(void);
static float rt_render_scale(void){
    if(!nfo_enabled())return 1.0f;
    static float k=1;static LONG kframe;static int kread;static ULONGLONG next;
    LONG gf=nfo_game_frames;ULONGLONG now=GetTickCount64();
    if(kread&&(gf==kframe||now<next))return k;
    kread=1;kframe=gf;next=now+250;
    nfo_load();
    float nk=k;LONG r=nfo_res;
    if(r>=0&&r<=3)nk=(float)(r+1);
    else{HWND h=driving_present201_window();RECT c;   /* Match window: the picture's size in the window (minimised: as before) */
        if(h&&GetClientRect(h,&c)&&c.right>0&&c.bottom>0){
            float kx=c.right/640.0f,ky=c.bottom/480.0f;
            if(nfo_shape==2){float pw=c.bottom*16.0f/9.0f;nk=(c.right<pw?(float)c.right:pw)/640.0f;}   /* 16:9: the picture's width */
            else nk=(nfo_shape==1&&kx<ky)?kx:ky;   /* 4:3 bars in a tall window: its width */
            nk=floorf(nk*4)/4;                 /* quarter steps */
            if(nk<1)nk=1;if(nk>3)nk=3;}}       /* at most 3x: 4x+ drew too slowly on a laptop GPU (DPI-PERF.md); the 4x row stays */
    if(nk!=k){LOG("[NF-OVERLAY] internal resolution %.2fx: game draws %ux%u\n",nk,(unsigned)(640*nk+0.5f),(unsigned)(480*nk+0.5f));k=nk;}
    nfo_cur_k=k;return k;
}

/* ------------------------------------------------------------ keys */
/* Window thread. Returns 0 when the game should see the message, 1 when the
 * overlay used it, 2 when the menu just opened (the caller releases the mouse). */
static int nfo_key(HWND h,UINT m,WPARAM w,LPARAM l){
    if(m==NF_HOST_SET_MODE){nf_window_set_mode(h,(int)w);return 1;}   /* display mode (nfo_apply_display) */
    int open=nf_overlay_menu_open!=0;
    /* F10 arrives as WM_SYSKEYDOWN (it is the menu-bar key). The key left of 1
     * (` or the UK backtick key: scan code 0x29 on every layout) also works, for
     * laptops whose F keys need Fn. */
    int hot=(w==VK_F10&&(m==WM_KEYDOWN||m==WM_SYSKEYDOWN||m==WM_KEYUP||m==WM_SYSKEYUP))||
            (((l>>16)&0x1FF)==0x29&&(m==WM_KEYDOWN||m==WM_KEYUP));
    if(hot&&(m==WM_KEYDOWN||m==WM_SYSKEYDOWN)){
        if(l&(1<<30))return 1;   /* held down: auto-repeat */
        InterlockedExchange(&nf_overlay_menu_open,!open);
        if(open)InterlockedExchange(&nfo_save_due,1);
        return open?1:2;
    }
    if(hot)return 1;   /* the release too: no menu-bar activation, no stray key-up for the game */
    if(!open)return 0;
    if(m==WM_KEYDOWN){
        LONG s=nfo_sel;int d=0;
        switch(w){
        case VK_UP:s=(s+NFO_ROWS-1)%NFO_ROWS;break;
        case VK_DOWN:s=(s+1)%NFO_ROWS;break;
        case VK_LEFT:d=-1;break;
        case VK_RIGHT:case VK_RETURN:case VK_SPACE:d=1;break;
        case VK_ESCAPE:InterlockedExchange(&nf_overlay_menu_open,0);InterlockedExchange(&nfo_save_due,1);break;
        }
        InterlockedExchange(&nfo_sel,s);
        if(d){
            switch(s){
            case NFO_BRIGHT:InterlockedExchange(&nfo_bright,(nfo_bright+4+d)%4);break;
            case NFO_SHAPE:InterlockedExchange(&nfo_shape,(nfo_shape+NFO_SHAPE_N+d)%NFO_SHAPE_N);break;
            case NFO_FILTER:InterlockedExchange(&nfo_filter,!nfo_filter);break;
            case NFO_FPS:InterlockedExchange(&nfo_fps,!nfo_fps);break;
            case NFO_RES:InterlockedExchange(&nfo_res,(nfo_res+NFO_RES_N+d)%NFO_RES_N);break;
            case NFO_DISPLAY:{LONG cur=nfo_display>=0?nfo_display:nf_window_get_mode(driving_present201_window());
                nfo_apply_display((cur+3+d)%3);}break;
            case NFO_CLOSE:if(w==VK_RETURN||w==VK_SPACE){InterlockedExchange(&nf_overlay_menu_open,0);InterlockedExchange(&nfo_save_due,1);}break;
            }
        }
        return 1;
    }
    /* Everything else the game would read while the menu is up. */
    if(m==WM_KEYUP||m==WM_CHAR||m==WM_LBUTTONDOWN||m==WM_LBUTTONUP||m==WM_RBUTTONDOWN||m==WM_RBUTTONUP||
       m==WM_MBUTTONDOWN||m==WM_MBUTTONUP||m==WM_MOUSEWHEEL)return 1;   /* WM_INPUT is left alone: the mouse is released while the menu is open */
    return 0;
}

/* ------------------------------------------------------------ GPU objects */
static ID3D11VertexShader *nfo_blit_vs,*nfo_quad_vs;static ID3D11PixelShader *nfo_blit_ps,*nfo_quad_ps;
static ID3D11SamplerState *nfo_smp[2];static ID3D11BlendState *nfo_bs_off,*nfo_bs_alpha;
static ID3D11RasterizerState *nfo_rs;static ID3D11DepthStencilState *nfo_ds;static ID3D11Buffer *nfo_cb;
static int nfo_gpu;   /* 0 not tried, 1 ready, -1 failed */
static int nfo_init_gpu(void){
    if(nfo_gpu)return nfo_gpu>0;
    nfo_gpu=-1;
    static const char hs[]=
        "cbuffer C:register(b0){float4 rect;float4 p;};Texture2D t:register(t0);SamplerState s:register(s0);\n"
        "struct V{float4 pos:SV_Position;float2 uv:TEXCOORD0;};\n"
        "V vblit(uint i:SV_VertexID){V o;float2 q=float2((i<<1)&2,i&2);o.pos=float4(q*float2(2,-2)+float2(-1,1),0,1);o.uv=q;return o;}\n"
        "float4 pblit(V i):SV_Target{float3 c=t.Sample(s,i.uv).rgb;return float4(pow(saturate(c),p.x),1);}\n"
        "V vquad(uint i:SV_VertexID){V o;float2 q=float2(i&1,i>>1);o.pos=float4(lerp(rect.xy,rect.zw,q),0,1);o.uv=q;return o;}\n"
        "float4 pquad(V i):SV_Target{return float4(t.Sample(s,i.uv).rgb,p.y);}\n";
    struct {const char *entry,*target;void **out;int vs;} sh[]={{"vblit","vs_5_0",(void**)&nfo_blit_vs,1},{"pblit","ps_5_0",(void**)&nfo_blit_ps,0},
        {"vquad","vs_5_0",(void**)&nfo_quad_vs,1},{"pquad","ps_5_0",(void**)&nfo_quad_ps,0}};
    for(unsigned i=0;i<4;i++){
        ID3DBlob *c=NULL,*e=NULL;
        if(FAILED(D3DCompile(hs,sizeof hs-1,"nf-overlay",NULL,NULL,sh[i].entry,sh[i].target,0,0,&c,&e))){
            LOG("[NF-OVERLAY] shader %s failed: %s\n",sh[i].entry,e?(const char*)ID3D10Blob_GetBufferPointer(e):"?");if(e)ID3D10Blob_Release(e);return 0;}
        if(e)ID3D10Blob_Release(e);
        HRESULT hr=sh[i].vs?ID3D11Device_CreateVertexShader(dev,ID3D10Blob_GetBufferPointer(c),ID3D10Blob_GetBufferSize(c),NULL,(ID3D11VertexShader**)sh[i].out)
                          :ID3D11Device_CreatePixelShader(dev,ID3D10Blob_GetBufferPointer(c),ID3D10Blob_GetBufferSize(c),NULL,(ID3D11PixelShader**)sh[i].out);
        ID3D10Blob_Release(c);if(FAILED(hr))return 0;
    }
    for(unsigned i=0;i<2;i++){D3D11_SAMPLER_DESC d={0};d.Filter=i?D3D11_FILTER_MIN_MAG_MIP_POINT:D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        d.AddressU=d.AddressV=d.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;d.MaxLOD=D3D11_FLOAT32_MAX;d.ComparisonFunc=D3D11_COMPARISON_NEVER;
        if(FAILED(ID3D11Device_CreateSamplerState(dev,&d,&nfo_smp[i])))return 0;}
    {D3D11_BLEND_DESC d={0};d.RenderTarget[0].RenderTargetWriteMask=15;
     if(FAILED(ID3D11Device_CreateBlendState(dev,&d,&nfo_bs_off)))return 0;
     d.RenderTarget[0].BlendEnable=TRUE;d.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;d.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;d.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
     d.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;d.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ZERO;d.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
     if(FAILED(ID3D11Device_CreateBlendState(dev,&d,&nfo_bs_alpha)))return 0;}
    {D3D11_RASTERIZER_DESC d={0};d.FillMode=D3D11_FILL_SOLID;d.CullMode=D3D11_CULL_NONE;d.DepthClipEnable=TRUE;if(FAILED(ID3D11Device_CreateRasterizerState(dev,&d,&nfo_rs)))return 0;}
    {D3D11_DEPTH_STENCIL_DESC d={0};d.DepthEnable=FALSE;d.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;d.DepthFunc=D3D11_COMPARISON_ALWAYS;if(FAILED(ID3D11Device_CreateDepthStencilState(dev,&d,&nfo_ds)))return 0;}
    {D3D11_BUFFER_DESC d={0};d.ByteWidth=32;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_CONSTANT_BUFFER;if(FAILED(ID3D11Device_CreateBuffer(dev,&d,NULL,&nfo_cb)))return 0;}
    nfo_gpu=1;return 1;
}

/* ------------------------------------------------------------ text panels */
#define NFO_TAB 16
typedef struct {ID3D11Texture2D *tex;ID3D11ShaderResourceView *srv;unsigned w,h;char key[512];} NfoPanel;
static NfoPanel nfo_menu_panel,nfo_fps_panel;
/* Draws `lines` (\n separated; a line starting with '>' is highlighted, '~'
 * is greyed) into a BGRA bitmap with GDI and uploads it when it changed. */
static int nfo_panel(NfoPanel *p,const char *lines,unsigned font_px,unsigned min_w){
    char key[512];snprintf(key,sizeof key,"%u|%u|%s",font_px,min_w,lines);
    if(p->tex&&!strcmp(key,p->key))return 1;
    unsigned nl=1;for(const char *c=lines;*c;c++)if(*c=='\n')nl++;
    unsigned lh=font_px*3/2,pad=font_px*3/4,w=min_w,h=nl*lh+pad*2;
    HDC dc=CreateCompatibleDC(NULL);if(!dc)return 0;
    HFONT font=CreateFontA(-(int)font_px,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,"Segoe UI");
    HGDIOBJ oldf=SelectObject(dc,font);
    /* Width: the longest line, with tabs expanded the way DrawText does
     * (DT_TABSTOP: every NFO_TAB average character widths). */
    TEXTMETRICA tm;GetTextMetricsA(dc,&tm);int tabpx=NFO_TAB*tm.tmAveCharWidth;
    {const char *s=lines;while(*s){const char *e=strchr(s,'\n');size_t n=e?(size_t)(e-s):strlen(s);
        const char *t=s;if(n&&(*t=='>'||*t=='~')){t++;n--;}
        DWORD ext=GetTabbedTextExtentA(dc,t,(int)n,1,&tabpx);if((unsigned)LOWORD(ext)+pad*2>w)w=LOWORD(ext)+pad*2;
        s=e?e+1:s+strlen(s);}}
    BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof bi.bmiHeader;bi.bmiHeader.biWidth=(LONG)w;bi.bmiHeader.biHeight=-(LONG)h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void *bits=NULL;HBITMAP bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,NULL,0);
    if(!bmp||!bits){SelectObject(dc,oldf);DeleteObject(font);DeleteDC(dc);return 0;}
    HGDIOBJ oldb=SelectObject(dc,bmp);
    RECT all={0,0,(LONG)w,(LONG)h};HBRUSH bg=CreateSolidBrush(RGB(14,18,26));FillRect(dc,&all,bg);DeleteObject(bg);
    SetBkMode(dc,TRANSPARENT);
    unsigned row=0;const char *s=lines;
    while(*s){const char *e=strchr(s,'\n');size_t n=e?(size_t)(e-s):strlen(s);
        int hi=n&&*s=='>',grey=n&&*s=='~';const char *t=s+((hi||grey)?1:0);size_t tn=n-((hi||grey)?1:0);
        RECT r={0,(LONG)(pad+row*lh),(LONG)w,(LONG)(pad+row*lh+lh)};
        if(hi){HBRUSH b=CreateSolidBrush(RGB(48,72,112));FillRect(dc,&r,b);DeleteObject(b);}
        SetTextColor(dc,hi?RGB(255,224,120):grey?RGB(120,128,140):RGB(232,236,242));
        /* DT_TABSTOP takes bits 8-15 for the tab size, so no DT_NOPREFIX (no & in this text). */
        r.left=pad;r.right=w-pad;DrawTextA(dc,t,(int)tn,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_EXPANDTABS|DT_TABSTOP|(NFO_TAB<<8));
        row++;s=e?e+1:s+strlen(s);}
    GdiFlush();
    if(p->srv)ID3D11ShaderResourceView_Release(p->srv);if(p->tex)ID3D11Texture2D_Release(p->tex);p->srv=NULL;p->tex=NULL;
    D3D11_TEXTURE2D_DESC td={w,h,1,1,DXGI_FORMAT_B8G8R8A8_UNORM,{1,0},D3D11_USAGE_IMMUTABLE,D3D11_BIND_SHADER_RESOURCE,0,0};
    D3D11_SUBRESOURCE_DATA sd={bits,w*4,0};
    int ok=SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&td,&sd,&p->tex))&&SUCCEEDED(ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)p->tex,NULL,&p->srv));
    SelectObject(dc,oldb);DeleteObject(bmp);SelectObject(dc,oldf);DeleteObject(font);DeleteDC(dc);
    if(ok){p->w=w;p->h=h;strcpy(p->key,key);}else p->key[0]=0;
    return ok;
}
static void nfo_draw_panel(ID3D11DeviceContext *c,NfoPanel *p,float x,float y,float W,float H,float alpha){
    if(!p->srv)return;
    float cb[8]={x/W*2-1,1-y/H*2,(x+p->w)/W*2-1,1-(y+p->h)/H*2,1,alpha,0,0};
    ID3D11DeviceContext_UpdateSubresource(c,(ID3D11Resource*)nfo_cb,0,NULL,cb,0,0);
    ID3D11DeviceContext_IASetPrimitiveTopology(c,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D11DeviceContext_VSSetShader(c,nfo_quad_vs,NULL,0);ID3D11DeviceContext_PSSetShader(c,nfo_quad_ps,NULL,0);
    ID3D11DeviceContext_PSSetShaderResources(c,0,1,&p->srv);ID3D11DeviceContext_PSSetSamplers(c,0,1,&nfo_smp[1]);
    ID3D11DeviceContext_Draw(c,4,0);
}

/* ------------------------------------------------------------ present */
/* Draws the finished frame `src` (a 4:3 picture) into the swap chain buffer
 * `back` with the chosen brightness, shape and filter, then the menu and the
 * frame counter. `c` is the immediate context (direct present, under the
 * renderer lock) or the in-between-frames display thread's deferred context. */
static void nfo_present(ID3D11DeviceContext *c,ID3D11Texture2D *back,ID3D11ShaderResourceView *src){
    nfo_load();
    if(!nfo_init_gpu()||!src)return;
    if(nf_overlay_key_hook!=nfo_key)nf_overlay_key_hook=nfo_key;
    if(InterlockedExchange(&nfo_save_due,0))nfo_save();
    {   /* a saved display mode wins over the launcher's start mode (once, at start) */
        static int checked;
        if(!checked){HWND w=driving_present201_window();if(w){checked=1;
            if(nfo_display>=0&&nf_window_get_mode(w)!=nfo_display)nfo_apply_display(nfo_display);}}
    }
    D3D11_TEXTURE2D_DESC bd;ID3D11Texture2D_GetDesc(back,&bd);
    float W=(float)bd.Width,H=(float)bd.Height;
    ID3D11RenderTargetView *rtv=NULL;
    if(FAILED(ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)back,NULL,&rtv)))return;
    float black[4]={0,0,0,1};
    ID3D11DeviceContext_ClearRenderTargetView(c,rtv,black);
    ID3D11DeviceContext_OMSetRenderTargets(c,1,&rtv,NULL);
    D3D11_VIEWPORT vp={0,0,W,H,0,1};
    if(nfo_shape==1){   /* largest 4:3 rectangle, centred */
        if(W*3>H*4){vp.Width=floorf(H*4/3);vp.TopLeftX=floorf((W-vp.Width)/2);}
        else{vp.Height=floorf(W*3/4);vp.TopLeftY=floorf((H-vp.Height)/2);}
    }else if(nfo_shape==2){   /* largest 16:9 rectangle, centred (the game's 16:9 picture, squeezed into 640x480) */
        if(W*9>H*16){vp.Width=floorf(H*16/9);vp.TopLeftX=floorf((W-vp.Width)/2);}
        else{vp.Height=floorf(W*9/16);vp.TopLeftY=floorf((H-vp.Height)/2);}
    }
    ID3D11DeviceContext_RSSetViewports(c,1,&vp);ID3D11DeviceContext_RSSetState(c,nfo_rs);
    ID3D11DeviceContext_OMSetBlendState(c,nfo_bs_off,NULL,0xffffffffu);ID3D11DeviceContext_OMSetDepthStencilState(c,nfo_ds,0);
    ID3D11DeviceContext_IASetInputLayout(c,NULL);ID3D11DeviceContext_IASetPrimitiveTopology(c,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_GSSetShader(c,NULL,NULL,0);
    float cb[8]={0,0,0,0,nfo_bright_gamma[nfo_bright],1,0,0};
    ID3D11DeviceContext_UpdateSubresource(c,(ID3D11Resource*)nfo_cb,0,NULL,cb,0,0);
    ID3D11DeviceContext_VSSetShader(c,nfo_blit_vs,NULL,0);ID3D11DeviceContext_PSSetShader(c,nfo_blit_ps,NULL,0);
    ID3D11DeviceContext_VSSetConstantBuffers(c,0,1,&nfo_cb);ID3D11DeviceContext_PSSetConstantBuffers(c,0,1,&nfo_cb);
    ID3D11DeviceContext_PSSetShaderResources(c,0,1,&src);ID3D11DeviceContext_PSSetSamplers(c,0,1,&nfo_smp[nfo_filter?1:0]);
    ID3D11DeviceContext_Draw(c,3,0);
    /* Overlay: whole buffer, alpha blended. */
    D3D11_VIEWPORT full={0,0,W,H,0,1};ID3D11DeviceContext_RSSetViewports(c,1,&full);
    ID3D11DeviceContext_OMSetBlendState(c,nfo_bs_alpha,NULL,0xffffffffu);
    unsigned font=(unsigned)(H/32);if(font<13)font=13;
    {   /* game frames and screen frames per second, updated twice a second
         * (they differ in smooth mode, LEAN_INTERP, where in-between frames are added) */
        static ULONGLONG t0;static unsigned n;static LONG g0;static double fps,gfps;
        ULONGLONG now=GetTickCount64();n++;if(!t0){t0=now;g0=nfo_game_frames;}
        if(now-t0>=500){LONG g=nfo_game_frames;fps=n*1000.0/(double)(now-t0);gfps=(g-g0)*1000.0/(double)(now-t0);n=0;g0=g;t0=now;}
        if(nfo_fps){char s[64];snprintf(s,sizeof s,"Game %.0f FPS | Screen %.0f",gfps,fps);
            if(nfo_panel(&nfo_fps_panel,s,font*3/4,0))nfo_draw_panel(c,&nfo_fps_panel,(float)(font/2),(float)(font/2),W,H,0.75f);}
    }
    if(nf_overlay_menu_open){
        char m[480],res[48],shp[64];LONG s=nfo_sel,rv=nfo_res,sh=nfo_shape;
        if(sh<0||sh>=NFO_SHAPE_N)sh=0;
        snprintf(shp,sizeof shp,"%s%s",nfo_shape_names[sh],(sh==2)!=(nfo_ws_told!=0)?" (restart to apply)":"");   /* 16:9 is read by the game at start */
        if(rv==4){float k=nfo_cur_k;snprintf(res,sizeof res,"Match window (%ux%u)",(unsigned)(640*k+0.5f),(unsigned)(480*k+0.5f));}
        else snprintf(res,sizeof res,"%s",nfo_res_names[rv>=0&&rv<NFO_RES_N?rv:0]);
        #define SEL(i) (s==(i)?">":"")
        snprintf(m,sizeof m,
            "Nightfire PC settings\n"
            "%sBrightness\t%s\n%sPicture shape\t%s\n%sScaling\t%s\n%sFrame counter\t%s\n%sResolution\t%s\n%sDisplay mode\t%s\n"
            "%sClose\n"
            "~Up/Down choose, Left/Right change, F10, ` or Esc closes",
            SEL(NFO_BRIGHT),nfo_bright_names[nfo_bright],SEL(NFO_SHAPE),shp,
            SEL(NFO_FILTER),nfo_filter_names[nfo_filter],SEL(NFO_FPS),nfo_onoff[nfo_fps],
            SEL(NFO_RES),res,
            SEL(NFO_DISPLAY),nfo_display_names[nfo_display>=0?nfo_display:nf_window_get_mode(driving_present201_window())],
            SEL(NFO_CLOSE));
        #undef SEL
        if(nfo_panel(&nfo_menu_panel,m,font,font*20))
            nfo_draw_panel(c,&nfo_menu_panel,floorf((W-nfo_menu_panel.w)/2),floorf((H-nfo_menu_panel.h)/2),W,H,0.92f);
    }
    ID3D11ShaderResourceView *none=NULL;ID3D11DeviceContext_PSSetShaderResources(c,0,1,&none);
    ID3D11DeviceContext_OMSetRenderTargets(c,0,NULL,NULL);
    ID3D11RenderTargetView_Release(rtv);
}
