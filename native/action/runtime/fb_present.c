/* Read-only scanout observer for the PAL mode seen in Nightfire's logs.
 * Displays only guest memory; no substitute picture or synthetic test pattern.
 * This is not GPU command execution. */
#if defined(_WIN32)
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "nightfire_frame_pixels.h"
#include "nightfire_surface_probe83.h"
#include "nightfire_pc_input.h"
#include "nightfire_host_window.h"   /* NIGHTFIRE_HOST_HWND one-window play */
#include "lean/nf_binds.h"            /* key binds (NF_OVERLAY=1) */
extern ptrdiff_t xbox_GetMemoryOffset(void);
static SRWLOCK config_lock=SRWLOCK_INIT, pixels_lock=SRWLOCK_INIT;
static uint32_t framebuffer,pitch,captured_address;
static uint32_t pixels[NF_FRAME_WIDTH*NF_FRAME_HEIGHT];
static uint32_t staging[NF_FRAME_WIDTH*NF_FRAME_HEIGHT];
static int captured;
static unsigned published_frames;
static volatile LONG started, running;
static SRWLOCK input_lock=SRWLOCK_INIT;
static nf_pc_controls pc_controls;
static nf_pc_packet pc_packet;
static nf_pc_delta122 mouse_pending122;
static int mouse_captured,raw_mouse_ready;
static volatile LONG close_requested;
/* Native graphics (LEAN_NATIVE_D3D=1): the Direct3D renderer presents into this
 * window through its own swap chain. It asks for the window here and, once its
 * swap chain is up, switches the GDI repaint off (direct mode). Frames it shows
 * are counted with nightfire_window_frame_shown() for the title and the crash
 * capture's frame counter. */
static volatile HWND s_window;
static volatile LONG direct_mode;
HWND nightfire_window_hwnd(void) { return s_window; }
void nightfire_window_direct(int on) { InterlockedExchange(&direct_mode, on); }
void nightfire_window_show_pixels(const uint32_t *frame)
{
    AcquireSRWLockExclusive(&pixels_lock); memcpy(pixels, frame, NF_FRAME_BYTES); captured = 1; ReleaseSRWLockExclusive(&pixels_lock);
}
void nightfire_window_frame_shown(void)
{
    AcquireSRWLockExclusive(&pixels_lock); published_frames++; ReleaseSRWLockExclusive(&pixels_lock);
    {extern void nightfire_session242_frame(void);nightfire_session242_frame();}
}
static int modern_controls140(void) {
    const char *v=getenv("NIGHTFIRE_MODERN_CONTROLS140");
    return v && !strcmp(v,"1");
}
void nightfire_window_exit_if_closed(void)
{
    /* Acquire pairs with the UI's InterlockedExchange. This poll does not need
     * to write the cache line at every translated function boundary. */
    if(ReadAcquire(&close_requested)) {fprintf(stderr,"[WINDOW] closed by user\n");fflush(NULL);exit(0);}
}
unsigned nightfire_window_buttons(void) {
    /* Modern actions belong to the once-per-input-update packet. OR-ing the
     * old raw E/R/C/Space mapping here would press two different pad buttons. */
    if(modern_controls140())return 0;
    AcquireSRWLockShared(&input_lock);unsigned b=nf_pc_buttons(&pc_controls);ReleaseSRWLockShared(&input_lock);return b;
}
/* Key binds (NF_OVERLAY=1, runtime/lean/lean_binds.inc; the store lives with the
 * renderer, so builds without it link these stand-ins and keep the fixed keys).
 * With the modern keys, every action reads the keys chosen on the Key Binds page.
 * The defaults are the fixed keys above, so an untouched page plays as before,
 * except that a keyboard key on Fire or Aim also works while the mouse is free.
 * Mouse buttons and the wheel count only while the mouse is captured. */
int nf_binds_on_default(void) { return 0; }
void nf_binds_get_default(int game,int action,unsigned char out[2]) { (void)game;(void)action;out[0]=out[1]=0; }
void nf_binds_wheel_default(int notches) { (void)notches; }
int nf_binds_wheel_down_default(int code) { (void)code;return 0; }
#pragma comment(linker, "/alternatename:nf_binds_on=nf_binds_on_default")
#pragma comment(linker, "/alternatename:nf_binds_get=nf_binds_get_default")
#pragma comment(linker, "/alternatename:nf_binds_wheel=nf_binds_wheel_default")
#pragma comment(linker, "/alternatename:nf_binds_wheel_down=nf_binds_wheel_down_default")
static int bind_down(const nf_pc_controls *s,unsigned code) {
    if(!code)return 0;
    if(code==VK_LBUTTON)return s->left;
    if(code==VK_RBUTTON)return s->right;
    if(code==NFB_WHEEL_UP||code==NFB_WHEEL_DOWN)return s->captured&&nf_binds_wheel_down((int)code);
    return code<256&&s->keys[code];
}
static int bound(const nf_pc_controls *s,int action) {
    unsigned char c[2];nf_binds_get(NFB_ACTION,action,c);
    return bind_down(s,c[0])||bind_down(s,c[1]);
}
static void binds_apply(const nf_pc_controls *s,unsigned layout,nf_pc_packet *p) {
#define A(a) bound(s,(a))
    unsigned b=(A(NFA_PAUSE)?16:0)|(A(NFA_DUP)?1:0)|(A(NFA_DDOWN)?2:0)|(A(NFA_DLEFT)?4:0)|(A(NFA_DRIGHT)?8:0);
    if(A(NFA_SELECT))b|=0x10000;
    if(A(NFA_MENUBACK))b|=0x20000;
    if(A(NFA_OBJECTIVES))b|=0x20;
    p->buttons=b|nf_pc_actions140(layout,A(NFA_USE),A(NFA_JUMP),A(NFA_CROUCH),A(NFA_ALTFIRE),A(NFA_WEAPON),A(NFA_GADGET));
    /* Movement: the same axes nf_pc_sample gives W/A/S/D for this layout. */
    unsigned turn=(layout==2||layout==4||layout==5||layout==7)?0:2;
    unsigned strafe=2-turn,forward=layout==4?3:1;
    int side=A(NFA_RIGHT)-A(NFA_LEFT),ahead=A(NFA_FORWARD)-A(NFA_BACK);
    p->axis_mask&=~((1u<<strafe)|(1u<<forward));p->axes[strafe]=p->axes[forward]=0;
    if(side){p->axis_mask|=1u<<strafe;p->axes[strafe]=(int16_t)(side*32767);}
    if(ahead){p->axis_mask|=1u<<forward;p->axes[forward]=(int16_t)(ahead*32767);}
    p->lt=A(NFA_AIM)?255:0;p->rt=A(NFA_FIRE)?255:0;
#undef A
}
void nightfire_window_input_begin(unsigned layout,int direct) {
    int modern=modern_controls140(),binds=modern&&nf_binds_on();
    AcquireSRWLockExclusive(&input_lock);nf_pc_sample140(&pc_controls,layout,&pc_packet,&mouse_pending122,direct,modern);
    if(binds)binds_apply(&pc_controls,layout,&pc_packet);
    ReleaseSRWLockExclusive(&input_lock);
}
void nightfire_window_mouse_take122(int *x,int *y) {
    AcquireSRWLockExclusive(&input_lock);
    nf_pc_delta122 d=nf_pc_take122(&mouse_pending122);
    if(!pc_controls.captured)d.x=d.y=0;
    ReleaseSRWLockExclusive(&input_lock);*x=d.x;*y=d.y;
}
/* Mouse in the game's menus (NF_OVERLAY=1, runtime/native_action/pcg_menu.c;
 * native-driving/ingame-menu/BUILD.md). While a menu page is on screen
 * (nf_menu_active) the mouse is not captured for mouse-look: the window records
 * the pointer, clicks and wheel here, the menu code turns them into focus and
 * short pad presses (nightfire_menu_press), which are merged into the pad below. */
int nf_menu_active_default(void) { return 0; }
#pragma comment(linker, "/alternatename:nf_menu_active=nf_menu_active_default")
int nf_menu_active(void);
static struct { int x,y,moved,left,right,wheel; } menu_mouse;   /* under input_lock */
static struct { unsigned queue[16],head,tail,current; ULONGLONG until,gap; } menu_press;   /* under input_lock */
int nightfire_menu_mouse_take(int *x,int *y,int *w,int *h,int *left,int *right,int *wheel)
{
    RECT r={0};HWND win=s_window;if(win)GetClientRect(win,&r);
    AcquireSRWLockExclusive(&input_lock);
    int moved=menu_mouse.moved;*x=menu_mouse.x;*y=menu_mouse.y;*left=menu_mouse.left;*right=menu_mouse.right;*wheel=menu_mouse.wheel;
    menu_mouse.moved=menu_mouse.left=menu_mouse.right=menu_mouse.wheel=0;
    ReleaseSRWLockExclusive(&input_lock);
    *w=r.right;*h=r.bottom;return moved;
}
void nightfire_menu_press(unsigned buttons)   /* one pad press (about 80 ms), queued */
{
    AcquireSRWLockExclusive(&input_lock);
    if(menu_press.tail-menu_press.head<16)menu_press.queue[menu_press.tail++%16]=buttons;
    ReleaseSRWLockExclusive(&input_lock);
}
static unsigned menu_press_now(void)   /* under input_lock */
{
    ULONGLONG now=GetTickCount64();
    if(menu_press.current&&now<menu_press.until)return menu_press.current;
    if(menu_press.current){menu_press.current=0;menu_press.gap=now+50;}   /* released between presses */
    if(now<menu_press.gap||menu_press.head==menu_press.tail)return 0;
    menu_press.current=menu_press.queue[menu_press.head++%16];menu_press.until=now+80;
    return menu_press.current;
}
void nightfire_window_pc_input(unsigned char state[18],unsigned *buttons) {
    AcquireSRWLockExclusive(&input_lock);nf_pc_merge(&pc_packet,state,buttons);*buttons|=menu_press_now();ReleaseSRWLockExclusive(&input_lock);
}
static void mouse_release(void) {
    int was_captured=mouse_captured;mouse_captured=0;
    AcquireSRWLockExclusive(&input_lock);nf_pc_clear(&pc_controls);memset(&pc_packet,0,sizeof pc_packet);mouse_pending122.x=mouse_pending122.y=0;ReleaseSRWLockExclusive(&input_lock);
    if(was_captured) {ClipCursor(NULL);ReleaseCapture();SetCursor(LoadCursorA(NULL,IDC_ARROW));}
}
static void mouse_clip(HWND h) {
    RECT r;if(!GetClientRect(h,&r))return;
    MapWindowPoints(h,NULL,(POINT*)&r,2);ClipCursor(&r);
}
static void mouse_acquire(HWND h) {
    if(!raw_mouse_ready || !nf_host_foreground(h))return;
    AcquireSRWLockExclusive(&input_lock);pc_controls.captured=1;pc_controls.mouse_x=pc_controls.mouse_y=0;ReleaseSRWLockExclusive(&input_lock);
    mouse_captured=1;SetCapture(h);mouse_clip(h);SetCursor(NULL);
}
/* Called by the command executor at a flip boundary, on its writer thread.
 * The UI reads this owned copy, never a surface being cleared or rasterised. */
void xbox_FramebufferPublish(uint32_t address,uint32_t row)
{
    nf_surface_probe_end(1003); /* ReadProcessMemory can fail without a VEH. */
    SIZE_T read=0;
    if(row!=NF_FRAME_WIDTH*4 || !address ||
       (uint64_t)address+NF_FRAME_BYTES>0x100000000ULL) return;
    AcquireSRWLockExclusive(&pixels_lock);
    if(ReadProcessMemory(GetCurrentProcess(),(void *)((uintptr_t)address+xbox_GetMemoryOffset()),
                         staging,NF_FRAME_BYTES,&read) && read==NF_FRAME_BYTES) {
        memcpy(pixels,staging,NF_FRAME_BYTES);
        captured=1; captured_address=address;published_frames++;
        {extern void nightfire_session242_frame(void);nightfire_session242_frame();} /* checkpoint242 crash capture: frame counter only */
        if(published_frames%100==0) {
            const char *prefix=getenv("RECOMP_FB_DUMP");
            if(prefix) {char name[MAX_PATH];snprintf(name,sizeof name,"%s-flip-%u.bmp",prefix,published_frames);nf_frame_write_bmp(name,pixels);}
            fprintf(stderr,"[PRESENT] published=%u at_ms=%llu\n",published_frames,(unsigned long long)GetTickCount64());
        }
    }
    ReleaseSRWLockExclusive(&pixels_lock);
}
void xbox_FramebufferWindowSet(uint32_t address,uint32_t bytes_per_row)
{
    AcquireSRWLockExclusive(&config_lock);
    framebuffer=address; pitch=bytes_per_row;
    ReleaseSRWLockExclusive(&config_lock);
}
int xbox_FramebufferDumpBmp(const char *path)
{
    int result=-1;
    AcquireSRWLockShared(&pixels_lock);
    if(captured) {
        result=nf_frame_write_bmp(path,pixels);
        fprintf(stderr,"[FBWIN] capture %s: %s, guest=%08X\n",path,
                result==0?"saved":"failed",captured_address);
        fflush(stderr);
    }
    ReleaseSRWLockShared(&pixels_lock);
    return result;
}
/* NF_OVERLAY settings menu (runtime/lean/lean_overlay.inc installs the hook on its
 * first frame): it sees window messages first; 1 = used, 2 = used and the menu just
 * opened (release the mouse). nf_overlay_menu_open keeps game input off meanwhile. */
int (*volatile nf_overlay_key_hook)(HWND,UINT,WPARAM,LPARAM);
volatile LONG nf_overlay_menu_open;
static LRESULT CALLBACK window_proc(HWND h,UINT m,WPARAM w,LPARAM l)
{
    {int (*hook)(HWND,UINT,WPARAM,LPARAM)=nf_overlay_key_hook;
     if(hook){int r=hook(h,m,w,l);if(r){if(r==2)mouse_release();return 0;}}}
    if(nf_host_dpi_changed(h,m,w,l))return 0;   /* NF_OVERLAY DPI awareness: another monitor's scaling */
    if(m==WM_KILLFOCUS || (m==WM_ACTIVATEAPP && !w) || m==WM_ENTERSIZEMOVE) mouse_release();
    if(m==WM_CAPTURECHANGED && mouse_captured && (HWND)l!=h)mouse_release();
    if((m==WM_MOVE || m==WM_SIZE) && mouse_captured)mouse_clip(h);
    if(m==WM_SETCURSOR && mouse_captured) {SetCursor(NULL);return TRUE;}
    if(m==WM_KEYDOWN || m==WM_KEYUP) {
        if(m==WM_KEYDOWN && w==VK_ESCAPE) {mouse_release();return 0;}
        if(m==WM_KEYDOWN && w==VK_F1 && !(l&(1L<<30))) {if(mouse_captured)mouse_release();else mouse_acquire(h);return 0;}
        if(w<256) {AcquireSRWLockExclusive(&input_lock);pc_controls.keys[w]=(m==WM_KEYDOWN);ReleaseSRWLockExclusive(&input_lock);return 0;}
    }
    if(m==WM_MOUSEMOVE) {   /* menus: where the pointer is (client pixels) */
        AcquireSRWLockExclusive(&input_lock);
        menu_mouse.x=(short)LOWORD(l);menu_mouse.y=(short)HIWORD(l);menu_mouse.moved=1;
        ReleaseSRWLockExclusive(&input_lock);
    }
    if(m==WM_MOUSEWHEEL && !mouse_captured && nf_menu_active()) {
        AcquireSRWLockExclusive(&input_lock);menu_mouse.wheel+=GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA;ReleaseSRWLockExclusive(&input_lock);
        return 0;
    }
    if((m==WM_LBUTTONDOWN || m==WM_RBUTTONDOWN) && !mouse_captured && nf_menu_active()) {   /* menus: A / B, no capture */
        SetFocus(h);
        AcquireSRWLockExclusive(&input_lock);
        if(m==WM_LBUTTONDOWN)menu_mouse.left++;else menu_mouse.right++;
        menu_mouse.x=(short)LOWORD(l);menu_mouse.y=(short)HIWORD(l);menu_mouse.moved=1;
        ReleaseSRWLockExclusive(&input_lock);
        return 0;
    }
    if(mouse_captured && (m==WM_MBUTTONDOWN || m==WM_MBUTTONUP || m==WM_XBUTTONDOWN || m==WM_XBUTTONUP)) {   /* key binds: middle and side buttons */
        unsigned vk=(m==WM_MBUTTONDOWN||m==WM_MBUTTONUP)?VK_MBUTTON:GET_XBUTTON_WPARAM(w)==XBUTTON1?VK_XBUTTON1:VK_XBUTTON2;
        AcquireSRWLockExclusive(&input_lock);pc_controls.keys[vk]=(m==WM_MBUTTONDOWN||m==WM_XBUTTONDOWN);ReleaseSRWLockExclusive(&input_lock);
        return (m==WM_XBUTTONDOWN||m==WM_XBUTTONUP)?TRUE:0;
    }
    if(m==WM_MOUSEWHEEL && mouse_captured) {nf_binds_wheel(GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA);return 0;}   /* key binds: wheel up/down */
    if(m==WM_LBUTTONDOWN || m==WM_RBUTTONDOWN || m==WM_LBUTTONUP || m==WM_RBUTTONUP) {
        if(m==WM_LBUTTONDOWN && !mouse_captured) {SetFocus(h);mouse_acquire(h);return 0;}
        AcquireSRWLockExclusive(&input_lock);
        if(mouse_captured) {if(m==WM_LBUTTONDOWN || m==WM_LBUTTONUP)pc_controls.left=(m==WM_LBUTTONDOWN);else pc_controls.right=(m==WM_RBUTTONDOWN);}
        ReleaseSRWLockExclusive(&input_lock);return 0;
    }
    if(m==WM_INPUT) {
        RAWINPUT input={0};UINT size=sizeof input;
        UINT received=GetRawInputData((HRAWINPUT)l,RID_INPUT,&input,&size,sizeof(RAWINPUTHEADER));
        if(received!=(UINT)-1 && received>=sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE) &&
           received<=sizeof input && input.header.dwType==RIM_TYPEMOUSE &&
           !(input.data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE) && mouse_captured && nf_host_foreground(h)) {
            AcquireSRWLockExclusive(&input_lock);nf_pc_mouse(&pc_controls,input.data.mouse.lLastX,input.data.mouse.lLastY);ReleaseSRWLockExclusive(&input_lock);
        }
        /* DefWindowProc performs foreground raw-input cleanup. */
    }
    if(m==WM_CLOSE) {mouse_release();InterlockedExchange(&close_requested,1);InterlockedExchange(&running,0);return 0;}
    return DefWindowProcA(h,m,w,l);
}
static DWORD WINAPI frame_thread(LPVOID unused)
{
    (void)unused;
    WNDCLASSA wc={0};RECT rect={0,0,NF_FRAME_WIDTH,NF_FRAME_HEIGHT};
    wc.lpfnWndProc=window_proc;wc.hInstance=GetModuleHandleA(NULL);
    wc.hCursor=LoadCursorA(NULL,IDC_ARROW);wc.lpszClassName="NightfireFrameObserver";
    RegisterClassA(&wc);
    /* Own top-level window, or a child filling the launcher's window (NIGHTFIRE_HOST_HWND). */
    HWND window=nf_host_create_window(wc.lpszClassName,"Nightfire - guest framebuffer (diagnostic)",
        rect.right,rect.bottom,wc.hInstance);
    if(!window) {fprintf(stderr,"[FBWIN] window creation failed: %lu\n",GetLastError());return 0;}
    s_window=window;
    RAWINPUTDEVICE mouse={1,2,nf_host_raw_flags(window),window};
    raw_mouse_ready=RegisterRawInputDevices(&mouse,1,sizeof mouse)!=0;
    fprintf(stderr,"[PC-INPUT] WASD movement, click/F1 capture mouse, Esc release; raw mouse=%d\n",raw_mouse_ready);
    HDC dc=GetDC(window);BITMAPINFO bi={0};
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=NF_FRAME_WIDTH;
    bi.bmiHeader.biHeight=-(LONG)NF_FRAME_HEIGHT;bi.bmiHeader.biPlanes=1;
    bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    uint32_t *snapshot=malloc(NF_FRAME_BYTES);
    if(!snapshot || !dc) {free(snapshot);if(dc)ReleaseDC(window,dc);DestroyWindow(window);return 0;}
    unsigned frames=0,first_nonblack=0;
    DWORD rate_time=GetTickCount(); unsigned rate_frames=0;
    fprintf(stderr,"[FBWIN] 640x480 observer open; displays immutable flip snapshots\n");
    fflush(stderr);
    while(InterlockedCompareExchange(&running,1,1)) {
        MSG msg;
        while(PeekMessageA(&msg,NULL,0,0,PM_REMOVE)) {TranslateMessage(&msg);DispatchMessageA(&msg);}
        {   /* menus: free the pointer while a menu is on screen; take it back for mouse-look afterwards */
            static int freed_for_menu;
            int menu=nf_menu_active();
            if(menu && mouse_captured){mouse_release();freed_for_menu=1;}
            else if(!menu && freed_for_menu){freed_for_menu=0;mouse_acquire(window);}
        }
        int direct=ReadAcquire(&direct_mode)!=0;
        AcquireSRWLockShared(&pixels_lock);
        int ready=captured||direct;
        unsigned completed=published_frames;
        if(captured&&!direct) memcpy(snapshot,pixels,NF_FRAME_BYTES);
        ReleaseSRWLockShared(&pixels_lock);
        if(!ready) {Sleep(16);continue;}
        DWORD now=GetTickCount();
        if(now-rate_time>=1000) {
            char title[220];
            const char *preview=getenv("NIGHTFIRE_MENU_PREVIEW");
            snprintf(title,sizeof title,"Nightfire%s | %.1f FPS | Frame %u | WASD move | %s | Enter pause",
                     preview && !strcmp(preview,"1") ? " MENU PREVIEW (opening drive skipped)" : "",
                     (completed-rate_frames)*1000.0/(now-rate_time),completed,mouse_captured?"Mouse look / Esc release":"Click/F1 mouse look");
            nf_host_title(window,title);rate_frames=completed;rate_time=now;
        }
        if(direct) {Sleep(16);continue;}   /* the Direct3D swap chain owns the picture */
        {int dw=NF_FRAME_WIDTH,dh=NF_FRAME_HEIGHT;RECT cr;   /* hosted: fill the launcher window */
         if(nf_host_is_child(window)&&GetClientRect(window,&cr)&&cr.right>0&&cr.bottom>0){dw=cr.right;dh=cr.bottom;}
         StretchDIBits(dc,0,0,dw,dh,0,0,NF_FRAME_WIDTH,NF_FRAME_HEIGHT,
                      snapshot,&bi,DIB_RGB_COLORS,SRCCOPY);}
        frames++;
        unsigned nonblack=nf_frame_nonblack(snapshot);
        const char *prefix=getenv("RECOMP_FB_DUMP");
        if(prefix && (frames==1 || frames==60 || frames==180 || frames==600)) {
            char path[MAX_PATH];snprintf(path,sizeof(path),"%s-%u.bmp",prefix,frames);
            xbox_FramebufferDumpBmp(path);
            fprintf(stderr,"[FBWIN] snapshot=%u nonblack=%u/%u\n",frames,nonblack,NF_FRAME_WIDTH*NF_FRAME_HEIGHT);
        }
        if(nonblack && !first_nonblack) {
            first_nonblack=1;
            fprintf(stderr,"[FBWIN] first nonblack snapshot: %u pixels; image inspection required\n",nonblack);
            if(prefix) {char path[MAX_PATH];snprintf(path,sizeof(path),"%s-nonblack.bmp",prefix);xbox_FramebufferDumpBmp(path);}
        }
        Sleep(16);
    }
    mouse_release();free(snapshot);ReleaseDC(window,dc);DestroyWindow(window);return 0;
}
void xbox_FramebufferWindowStart(void)
{
    const char *enabled=getenv("RECOMP_FB_WINDOW");
    if(!enabled || !strcmp(enabled,"0") || InterlockedCompareExchange(&started,1,0))return;
    InterlockedExchange(&running,1);
    HANDLE thread=CreateThread(NULL,0,frame_thread,NULL,0,NULL);
    if(thread)CloseHandle(thread);
    else fprintf(stderr,"[FBWIN] thread creation failed: %lu\n",GetLastError());
}
#endif
