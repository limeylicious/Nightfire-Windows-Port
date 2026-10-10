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
 * The games read that flag once at start-up, so a change needs a restart. Resolution lists
 * real sizes for the chosen aspect ratio (4:3 and Stretch: 640x480 to 2560x1920;
 * 16:9: 854x480 to 3840x2160) or Match window, and draws the game's full-screen
 * surfaces that many times larger than the Xbox's 640x480 (rt_render_scale below;
 * lean_d3d.c does the scaling). The 16:9 picture is the game's 4:3 one squeezed,
 * so a 16:9 size is drawn at its full width and a third taller, then scaled down.
 * Original keeps the Xbox size.
 *
 * Frame rate (display only; the games' own update rates are never touched):
 * Smooth motion turns on the in-between frames (lean_interp.inc; read at start,
 * so a change needs a restart; a launcher's LEAN_INTERP wins). While it is on,
 * the display thread shows pictures at the screen's rate (Match screen, vsync),
 * at a chosen cap (timer, no vsync) or uncapped (as fast as it can without
 * slowing the game: it backs off when the game waits on it). Without smooth
 * motion each game update shows one picture and these two rows do nothing.
 * The highlighted row's description is shown under the menu.
 *
 * Key binds (lean_binds.inc): the "Key binds" row opens the Key Binds screen
 * (also opened from Action's PC Options page); while the overlay is up, key and
 * button releases still reach the game so nothing stays held.
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
enum {NFO_BRIGHT,NFO_SHAPE,NFO_FILTER,NFO_FPS,NFO_RES,NFO_DISPLAY,NFO_SMOOTH,NFO_UNCAP,NFO_CAP,NFO_KEYS,NFO_CLOSE,NFO_ROWS};
#include "../nightfire_host_window.h"   /* display mode: nf_window_get_mode / nf_window_set_mode */
static const char *const nfo_display_names[3]={"Windowed","Fullscreen","Borderless fullscreen"};
static volatile LONG nfo_display=-1;   /* -1: not chosen yet (the launcher's start mode stays) */
extern HWND driving_present201_window(void);
static const char *const nfo_bright_names[]={"Off","Low","Medium","High"};
static const float nfo_bright_gamma[]={1.0f,0.9f,0.8f,0.7f};
#define NFO_SHAPE_N 3
static const char *const nfo_shape_names[NFO_SHAPE_N]={"Stretch to window","4:3","16:9"};
static const char *const nfo_filter_names[]={"Smooth","Sharp"};
static const char *const nfo_onoff[]={"Off","On"};
/* Frame rate cap: pictures shown per second (0 = match the screen, vsync). */
static const unsigned short nfo_caps[]={0,30,40,50,60,75,90,100,120,144,165,240,360};
#define NFO_CAP_N ((int)(sizeof nfo_caps/sizeof *nfo_caps))
static int nfo_cap_index(LONG hz){   /* nearest listed cap (ties: the larger) */
    int best=0;LONG bd=0x7FFFFFFF;
    if(hz<=0)return 0;
    for(int i=1;i<NFO_CAP_N;i++){LONG d=labs(hz-(LONG)nfo_caps[i]);if(d<=bd){bd=d;best=i;}}
    return best;
}
/* One line describing each F10 row, shown under the menu for the highlighted row. */
static const char *const nfo_row_help[NFO_ROWS]={
    "Makes the picture brighter.",
    "Picture shape: stretch, 4:3 or 16:9. 16:9 needs a restart.",
    "Smooth or sharp scaling of the picture.",
    "Shows game and screen frames per second in the corner.",
    "The size the game is drawn at. Larger is sharper, slower.",
    "Windowed, fullscreen or borderless fullscreen.",
    "Adds in-between frames. Restart to apply. Can slow the game.",
    "Shows as many frames as the PC can. Game speed is unchanged.",
    "Limits the frames shown each second. Game speed is unchanged.",
    "Choose the keys and mouse buttons for each action.",
    "Closes this menu.",
};
/* Resolution: the picture heights offered for each aspect ratio (shape 0 Stretch and
 * 1 4:3 share the 4:3 list), then Match window. Saved as res_h (0 = Match window). */
static const unsigned short nfo_h43[]={480,600,768,960,1080,1200,1440,1920};
static const unsigned short nfo_h169[]={480,720,900,1080,1440,2160};
static int nfo_res_list(LONG shape,const unsigned short **h){
    if(shape==2){*h=nfo_h169;return (int)(sizeof nfo_h169/sizeof *nfo_h169);}
    *h=nfo_h43;return (int)(sizeof nfo_h43/sizeof *nfo_h43);
}
static float nfo_res_k(LONG shape,unsigned h){   /* how many times 640x480 is drawn */
    if(shape==2)return h==480?1.0f:(float)h/360.0f;   /* 16:9: the 640-wide picture to the size's width */
    return (float)h/480.0f;
}
static unsigned nfo_res_w(LONG shape,unsigned h){return shape==2?(h==480?854:(h*16+4)/9):h*4/3;}
static int nfo_res_index(LONG shape,LONG resh){   /* index in the list; the list's length = Match window */
    const unsigned short *h;int n=nfo_res_list(shape,&h);
    if(resh<=0)return n;
    for(int i=0;i<n;i++)if(h[i]==resh)return i;
    return 0;
}
/* A height valid for `to`: the same height when that list has it, else the size
 * drawn closest to `from`'s (ties: the larger). Match window stays. */
static LONG nfo_res_fit(LONG from,LONG to,LONG resh){
    if(resh<=0)return 0;
    const unsigned short *h;int n=nfo_res_list(to,&h);
    for(int i=0;i<n;i++)if(h[i]==resh)return resh;
    float k=nfo_res_k(from,(unsigned)resh),bd=1e9f;LONG best=h[0];
    for(int i=0;i<n;i++){float d=fabsf(nfo_res_k(to,h[i])-k);if(d<=bd){bd=d;best=h[i];}}
    return best;
}
static volatile LONG nfo_bright,nfo_shape,nfo_filter,nfo_fps,nfo_resh=480;
static volatile LONG nfo_smooth,nfo_uncap,nfo_cap=0;   /* smooth motion, uncapped, frame rate cap in Hz (0 = match screen) */
static volatile LONG nfo_smooth_told=-1;               /* smooth motion in the running game (interp_enabled), -1 unknown */
static int nfo_smooth_forced(void){const char *v=getenv("LEAN_INTERP");return v&&*v;}   /* the launcher decides (Driving) */
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
    nfo_bright=b;nfo_shape=0;nfo_filter=0;nfo_fps=0;nfo_resh=480;
    /* Smooth motion starts Off: Driving's launcher forces it on (LEAN_INTERP) and that
     * must not reach Action through the shared file. */
    {const char *v;nfo_smooth=0;
     v=getenv("LEAN_PRESENT_UNCAPPED");nfo_uncap=v&&*v=='1';
     v=getenv("LEAN_PRESENT_VSYNC");const char *hz=getenv("LEAN_INTERP_HZ");   /* no vsync: the launcher's timer rate (default 120) */
     nfo_cap=(v&&*v=='1')?0:nfo_caps[nfo_cap_index(hz?atol(hz):120)];}
    long old_res=-1,resh=-1;   /* resolution= is the older 1x/2x/3x/4x/Match setting */
    char path[MAX_PATH];nfo_path(path,sizeof path);
    FILE *f=fopen(path,"r");
    if(f){char line[128];
        while(fgets(line,sizeof line,f)){
            char *eq=strchr(line,'=');if(!eq)continue;*eq=0;long v=atol(eq+1);
            if(!strcmp(line,"brightness")&&v>=0&&v<4)nfo_bright=v;
            else if(!strcmp(line,"shape")&&v>=0&&v<NFO_SHAPE_N)nfo_shape=v;
            else if(!strcmp(line,"filter")&&v>=0&&v<2)nfo_filter=v;
            else if(!strcmp(line,"fps")&&v>=0&&v<2)nfo_fps=v;
            else if(!strcmp(line,"res_h")&&v>=0&&v<=4320)resh=v;
            else if(!strcmp(line,"resolution")&&v>=0&&v<5)old_res=v;
            else if(!strcmp(line,"display")&&v>=0&&v<3)nfo_display=v;
            else if(!strcmp(line,"smooth")&&v>=0&&v<2)nfo_smooth=v;
            else if(!strcmp(line,"fps_uncapped")&&v>=0&&v<2)nfo_uncap=v;
            else if(!strcmp(line,"fps_cap")&&v>=0&&v<=1000)nfo_cap=nfo_caps[nfo_cap_index(v)];
        }
        fclose(f);
    }
    if(resh<0&&old_res>=0)   /* older file: Nx becomes the size drawn N times larger */
        resh=old_res==4?0:nfo_shape==2?(old_res==0?480:360*(old_res+1)):480*(old_res+1);
    if(resh>=0)nfo_resh=nfo_res_fit(nfo_shape,nfo_shape,resh);
    {const unsigned short *h;int n=nfo_res_list(nfo_shape,&h);int i=nfo_res_index(nfo_shape,nfo_resh);
     char rn[32];if(i>=n)snprintf(rn,sizeof rn,"Match window");else snprintf(rn,sizeof rn,"%ux%u",nfo_res_w(nfo_shape,h[i]),(unsigned)h[i]);
     LOG("[NF-OVERLAY] on (F10). brightness=%s aspect=%s filter=%s fps=%s resolution=%s; settings %s %s\n",
        nfo_bright_names[nfo_bright],nfo_shape_names[nfo_shape],nfo_filter_names[nfo_filter],nfo_onoff[nfo_fps],rn,
        f?"read from":"will be saved to",path);
     char cn[16];if(nfo_cap)snprintf(cn,sizeof cn,"%ld",nfo_cap);else snprintf(cn,sizeof cn,"match screen");
     LOG("[NF-OVERLAY] smooth motion=%s%s uncapped=%s frame rate cap=%s (display only)\n",nfo_onoff[nfo_smooth!=0],
        nfo_smooth_forced()?" (launcher)":"",nfo_onoff[nfo_uncap!=0],cn);}
    InterlockedExchange(&nfo_loaded,1);ReleaseSRWLockExclusive(&nfo_load_lock);
}
static void nfo_save(void){
    char path[MAX_PATH];nfo_path(path,sizeof path);
    FILE *f=fopen(path,"w");if(!f){LOG("[NF-OVERLAY] could not save %s\n",path);return;}
    fprintf(f,"brightness=%ld\nshape=%ld\nfilter=%ld\nfps=%ld\nres_h=%ld\n",nfo_bright,nfo_shape,nfo_filter,nfo_fps,nfo_resh);
    if(nfo_display>=0)fprintf(f,"display=%ld\n",nfo_display);
    fprintf(f,"smooth=%ld\nfps_uncapped=%ld\nfps_cap=%ld\n",nfo_smooth,nfo_uncap,nfo_cap);
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
 * runtime/native_action/pcg_menu.c): 0 aspect ratio (picture shape), 1 resolution
 * (index in the current aspect ratio's list, nf_overlay_res_count / _res_name),
 * 2 brightness, 3 scaling, 4 display mode, 5 smooth motion (next start), 6 uncapped
 * frame rate, 7 frame rate cap (index in nfo_caps, nf_overlay_cap_name). Changes
 * apply at once and are saved to the same file as F10. */
int nf_overlay_get(int which){
    if(!nfo_enabled())return -1;
    nfo_load();
    switch(which){case 0:return nfo_shape;case 1:return nfo_res_index(nfo_shape,nfo_resh);case 2:return nfo_bright;case 3:return nfo_filter;
    case 4:return nfo_display>=0?nfo_display:nf_window_get_mode(driving_present201_window());
    case 5:return nfo_smooth_forced()?nfo_smooth_told>0:nfo_smooth!=0;case 6:return nfo_uncap!=0;case 7:return nfo_cap_index(nfo_cap);}
    return -1;
}
int nf_overlay_cap_name(int i,char *out,int n){   /* "Match Screen", "60 FPS", ... */
    if(!out||n<=0||i<0||i>=NFO_CAP_N)return 0;
    if(!nfo_caps[i])snprintf(out,(size_t)n,"Match Screen");else snprintf(out,(size_t)n,"%u FPS",(unsigned)nfo_caps[i]);
    return 1;
}
/* Smooth motion: what the running game uses (1 on, 0 off, -1 not decided yet), for the restart hint. */
int nf_overlay_smooth_told(void){return nfo_smooth_told;}
/* Frame rate for the in-between-frames display thread (lean_interp.inc), read every
 * frame: -1 no overlay (the launcher's switches), 0 match the screen (vsync),
 * 1 cap at *hz, 2 uncapped. Display only: the games' update rates are not touched. */
static int nfo_frame_mode(int *hz){
    if(!nfo_enabled())return -1;
    nfo_load();
    if(nfo_uncap)return 2;
    LONG c=nfo_cap;if(c<=0)return 0;
    *hz=(int)c;return 1;
}
int nf_overlay_res_count(void){   /* the current aspect ratio's sizes + Match window */
    const unsigned short *h;
    if(!nfo_enabled())return 0;
    nfo_load();
    return nfo_res_list(nfo_shape,&h)+1;
}
/* Name of entry i, e.g. "1920x1080"; Match window shows the size it draws while chosen. */
int nf_overlay_res_name(int i,char *out,int n){
    const unsigned short *h;LONG sh=nfo_shape;int cnt=nfo_res_list(sh,&h);
    if(!out||n<=0||i<0||i>cnt)return 0;
    if(i<cnt)snprintf(out,(size_t)n,"%ux%u%s",nfo_res_w(sh,h[i]),(unsigned)h[i],h[i]==480?" (original)":"");
    else if(nfo_resh>0)snprintf(out,(size_t)n,"Match Window");
    else{float k=nfo_cur_k;snprintf(out,(size_t)n,"Match Window (%ux%u)",(unsigned)(640*k+0.5f),(unsigned)((sh==2?360:480)*k+0.5f));}
    return 1;
}
static void nfo_apply_display(LONG v){   /* 4: display mode, switched at once */
    InterlockedExchange(&nfo_display,v);
    {HWND w=driving_present201_window();   /* switched on the window's own thread (nfo_key) */
     if(w)PostMessageA(w,NF_HOST_SET_MODE,(WPARAM)v,0);}
    LOG("[NF-OVERLAY] display mode %s\n",nfo_display_names[v]);
}
static void nfo_set_shape(LONG v){   /* aspect ratio; the resolution moves to the new ratio's list */
    LONG old=nfo_shape;
    InterlockedExchange(&nfo_resh,nfo_res_fit(old,v,nfo_resh));
    InterlockedExchange(&nfo_shape,v);
}
static void nfo_set_res(int i){   /* index in the current aspect ratio's list (its length = Match window) */
    const unsigned short *h;int n=nfo_res_list(nfo_shape,&h);
    if(i<0||i>n)return;
    InterlockedExchange(&nfo_resh,i==n?0:h[i]);
}
void nf_overlay_set(int which,int v){
    static const int count[8]={NFO_SHAPE_N,0,4,2,3,2,2,NFO_CAP_N};
    if(!nfo_enabled()||which<0||which>7||v<0)return;
    nfo_load();
    if(v>=(which==1?nf_overlay_res_count():count[which]))return;
    if(which==4){if(nfo_display!=v||nf_window_get_mode(driving_present201_window())!=v){nfo_apply_display(v);nfo_save();}return;}
    if(v==nf_overlay_get(which))return;
    if(which==0)nfo_set_shape(v);
    else if(which==1)nfo_set_res(v);
    else if(which==5){if(nfo_smooth_forced())return;InterlockedExchange(&nfo_smooth,v);}   /* the launcher decides */
    else if(which==6)InterlockedExchange(&nfo_uncap,v);
    else if(which==7)InterlockedExchange(&nfo_cap,nfo_caps[v]);
    else InterlockedExchange(which==2?&nfo_bright:&nfo_filter,v);
    nfo_save();
    LOG("[NF-OVERLAY] PC Graphics page: setting %d = %d (saved; resolution height %ld)\n",which,v,nfo_resh);
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
    float nk=k;LONG r=nfo_resh;
    if(r>0)nk=nfo_res_k(nfo_shape,(unsigned)r);   /* a listed size */
    else{HWND h=driving_present201_window();RECT c;   /* Match window: the picture's size in the window (minimised: as before) */
        if(h&&GetClientRect(h,&c)&&c.right>0&&c.bottom>0){
            float kx=c.right/640.0f,ky=c.bottom/480.0f;
            if(nfo_shape==2){float pw=c.bottom*16.0f/9.0f;nk=(c.right<pw?(float)c.right:pw)/640.0f;}   /* 16:9: the picture's width */
            else nk=(nfo_shape==1&&kx<ky)?kx:ky;   /* 4:3 bars in a tall window: its width */
            nk=floorf(nk*4)/4;                 /* quarter steps */
            if(nk<1)nk=1;if(nk>3)nk=3;}}       /* at most 3x: 4x+ drew too slowly on a laptop GPU (DPI-PERF.md); larger listed sizes stay */
    if(nk!=k){LOG("[NF-OVERLAY] internal resolution %.2fx: game draws %ux%u\n",nk,(unsigned)(640*nk+0.5f),(unsigned)(480*nk+0.5f));k=nk;}
    nfo_cur_k=k;return k;
}

/* ------------------------------------------------------------ keys */
static volatile LONG nfo_page;   /* while open: 0 the F10 list, 1 the Key Binds screen (lean_binds.inc) */
static int nfb_key(HWND h,UINT m,WPARAM w,LPARAM l);
static void nfb_open(int from_menu);
static int nfb_listening(void);
static void nfb_bind(unsigned code);
static unsigned nfb_grave_vk(void);
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
        if(open&&nfo_page==1&&nfb_listening()){nfb_bind(w==VK_F10?VK_F10:nfb_grave_vk());return 1;}   /* says it is kept */
        InterlockedExchange(&nf_overlay_menu_open,!open);
        InterlockedExchange(&nfo_page,0);   /* F10 always opens on its own list */
        if(open)InterlockedExchange(&nfo_save_due,1);
        return open?1:2;
    }
    if(hot)return 1;   /* the release too: no menu-bar activation, no stray key-up for the game */
    if(!open)return 0;
    if(nfo_page==1)return nfb_key(h,m,w,l);
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
            case NFO_SHAPE:nfo_set_shape((nfo_shape+NFO_SHAPE_N+d)%NFO_SHAPE_N);break;
            case NFO_FILTER:InterlockedExchange(&nfo_filter,!nfo_filter);break;
            case NFO_FPS:InterlockedExchange(&nfo_fps,!nfo_fps);break;
            case NFO_RES:{int n=nf_overlay_res_count();nfo_set_res((nf_overlay_get(1)+n+d)%n);}break;
            case NFO_DISPLAY:{LONG cur=nfo_display>=0?nfo_display:nf_window_get_mode(driving_present201_window());
                nfo_apply_display((cur+3+d)%3);}break;
            case NFO_SMOOTH:if(!nfo_smooth_forced())InterlockedExchange(&nfo_smooth,!nfo_smooth);break;
            case NFO_UNCAP:InterlockedExchange(&nfo_uncap,!nfo_uncap);break;
            case NFO_CAP:InterlockedExchange(&nfo_cap,nfo_caps[(nfo_cap_index(nfo_cap)+NFO_CAP_N+d)%NFO_CAP_N]);break;
            case NFO_KEYS:if(d>0)nfb_open(0);break;
            case NFO_CLOSE:if(w==VK_RETURN||w==VK_SPACE){InterlockedExchange(&nf_overlay_menu_open,0);InterlockedExchange(&nfo_save_due,1);}break;
            }
        }
        return 1;
    }
    /* Everything else the game would read while the menu is up, except releases
     * (WM_KEYUP, button ups): those reach the game, so no key stays held there. */
    if(m==WM_SYSKEYDOWN||m==WM_SYSKEYUP||m==WM_CHAR||m==WM_SYSCHAR||m==WM_LBUTTONDOWN||m==WM_RBUTTONDOWN||m==WM_MBUTTONDOWN||
       m==WM_XBUTTONDOWN||m==WM_MOUSEWHEEL||m==WM_MOUSEMOVE)return 1;   /* WM_INPUT is left alone: the mouse is released while the menu is open */
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
typedef struct {ID3D11Texture2D *tex;ID3D11ShaderResourceView *srv;unsigned w,h;char key[1200];} NfoPanel;
static NfoPanel nfo_menu_panel,nfo_fps_panel;
/* Draws `lines` (\n separated; a line starting with '>' is highlighted, '~'
 * is greyed) into a BGRA bitmap with GDI and uploads it when it changed. */
static int nfo_panel(NfoPanel *p,const char *lines,unsigned font_px,unsigned min_w){
    char key[1200];snprintf(key,sizeof key,"%u|%u|%s",font_px,min_w,lines);
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

/* ------------------------------------------------------------ key binds */
#include "lean_binds.inc"

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
    if(nf_overlay_menu_open&&nfo_page==1)nfb_draw(c,W,H);   /* Key Binds screen */
    else if(nf_overlay_menu_open){
        char m[1100],res[48],shp[64],smo[64],cap[32];LONG s=nfo_sel,sh=nfo_shape;
        if(sh<0||sh>=NFO_SHAPE_N)sh=0;
        snprintf(shp,sizeof shp,"%s%s",nfo_shape_names[sh],(sh==2)!=(nfo_ws_told!=0)?" (restart to apply)":"");   /* 16:9 is read by the game at start */
        if(!nf_overlay_res_name(nf_overlay_get(1),res,sizeof res))snprintf(res,sizeof res,"?");
        if(nfo_smooth_forced())snprintf(smo,sizeof smo,"%s (set by the launcher)",nfo_onoff[nfo_smooth_told>0]);
        else snprintf(smo,sizeof smo,"%s%s",nfo_onoff[nfo_smooth!=0],nfo_smooth_told>=0&&(nfo_smooth!=0)!=(nfo_smooth_told!=0)?" (restart to apply)":"");
        nf_overlay_cap_name(nfo_cap_index(nfo_cap),cap,sizeof cap);
        int smooth_now=nfo_smooth_told>0;   /* the frame rate rows act only with smooth motion running */
        #define SEL(i) (s==(i)?">":"")
        snprintf(m,sizeof m,
            "Nightfire PC settings\n"
            "%sBrightness\t%s\n%sAspect ratio\t%s\n%sScaling\t%s\n%sFrame counter\t%s\n%sResolution\t%s\n%sDisplay mode\t%s\n"
            "%sSmooth motion\t%s\n%sUncapped frame rate\t%s%s\n%sFrame rate cap\t%s%s\n"
            "%sKey binds\tOpen (Enter)\n"
            "%sClose\n"
            "~%s\n"
            "~Up/Down choose, Left/Right change, F10, ` or Esc closes",
            SEL(NFO_BRIGHT),nfo_bright_names[nfo_bright],SEL(NFO_SHAPE),shp,
            SEL(NFO_FILTER),nfo_filter_names[nfo_filter],SEL(NFO_FPS),nfo_onoff[nfo_fps],
            SEL(NFO_RES),res,
            SEL(NFO_DISPLAY),nfo_display_names[nfo_display>=0?nfo_display:nf_window_get_mode(driving_present201_window())],
            SEL(NFO_SMOOTH),smo,SEL(NFO_UNCAP),nfo_onoff[nfo_uncap!=0],smooth_now?"":" (needs smooth motion)",
            SEL(NFO_CAP),cap,smooth_now?(nfo_uncap?" (uncapped is on)":""):" (needs smooth motion)",
            SEL(NFO_KEYS),SEL(NFO_CLOSE),nfo_row_help[s>=0&&s<NFO_ROWS?s:0]);
        #undef SEL
        if(nfo_panel(&nfo_menu_panel,m,font,font*20))
            nfo_draw_panel(c,&nfo_menu_panel,floorf((W-nfo_menu_panel.w)/2),floorf((H-nfo_menu_panel.h)/2),W,H,0.92f);
    }
    ID3D11ShaderResourceView *none=NULL;ID3D11DeviceContext_PSSetShaderResources(c,0,1,&none);
    ID3D11DeviceContext_OMSetRenderTargets(c,0,NULL,NULL);
    ID3D11RenderTargetView_Release(rtv);
}
