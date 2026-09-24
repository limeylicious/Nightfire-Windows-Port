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
void nightfire_window_input_begin(unsigned layout,int direct) {
    AcquireSRWLockExclusive(&input_lock);nf_pc_sample140(&pc_controls,layout,&pc_packet,&mouse_pending122,direct,modern_controls140());ReleaseSRWLockExclusive(&input_lock);
}
void nightfire_window_mouse_take122(int *x,int *y) {
    AcquireSRWLockExclusive(&input_lock);
    nf_pc_delta122 d=nf_pc_take122(&mouse_pending122);
    if(!pc_controls.captured)d.x=d.y=0;
    ReleaseSRWLockExclusive(&input_lock);*x=d.x;*y=d.y;
}
void nightfire_window_pc_input(unsigned char state[18],unsigned *buttons) {
    AcquireSRWLockShared(&input_lock);nf_pc_merge(&pc_packet,state,buttons);ReleaseSRWLockShared(&input_lock);
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
    if(!raw_mouse_ready || GetForegroundWindow()!=h)return;
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
static LRESULT CALLBACK window_proc(HWND h,UINT m,WPARAM w,LPARAM l)
{
    if(m==WM_KILLFOCUS || (m==WM_ACTIVATEAPP && !w) || m==WM_ENTERSIZEMOVE) mouse_release();
    if(m==WM_CAPTURECHANGED && mouse_captured && (HWND)l!=h)mouse_release();
    if((m==WM_MOVE || m==WM_SIZE) && mouse_captured)mouse_clip(h);
    if(m==WM_SETCURSOR && mouse_captured) {SetCursor(NULL);return TRUE;}
    if(m==WM_KEYDOWN || m==WM_KEYUP) {
        if(m==WM_KEYDOWN && w==VK_ESCAPE) {mouse_release();return 0;}
        if(m==WM_KEYDOWN && w==VK_F1 && !(l&(1L<<30))) {if(mouse_captured)mouse_release();else mouse_acquire(h);return 0;}
        if(w<256) {AcquireSRWLockExclusive(&input_lock);pc_controls.keys[w]=(m==WM_KEYDOWN);ReleaseSRWLockExclusive(&input_lock);return 0;}
    }
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
           !(input.data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE) && mouse_captured && GetForegroundWindow()==h) {
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
    RegisterClassA(&wc);AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
    HWND window=CreateWindowExA(0,wc.lpszClassName,"Nightfire - guest framebuffer (diagnostic)",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,
        rect.right-rect.left,rect.bottom-rect.top,NULL,NULL,wc.hInstance,NULL);
    if(!window) {fprintf(stderr,"[FBWIN] window creation failed: %lu\n",GetLastError());return 0;}
    RAWINPUTDEVICE mouse={1,2,0,window};
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
        AcquireSRWLockShared(&pixels_lock);
        int ready=captured;
        unsigned completed=published_frames;
        if(ready) memcpy(snapshot,pixels,NF_FRAME_BYTES);
        ReleaseSRWLockShared(&pixels_lock);
        if(!ready) {Sleep(16);continue;}
        DWORD now=GetTickCount();
        if(now-rate_time>=1000) {
            char title[220];
            const char *preview=getenv("NIGHTFIRE_MENU_PREVIEW");
            snprintf(title,sizeof title,"Nightfire%s | %.1f FPS | Frame %u | WASD move | %s | Enter pause",
                     preview && !strcmp(preview,"1") ? " MENU PREVIEW (opening drive skipped)" : "",
                     (completed-rate_frames)*1000.0/(now-rate_time),completed,mouse_captured?"Mouse look / Esc release":"Click/F1 mouse look");
            SetWindowTextA(window,title);rate_frames=completed;rate_time=now;
        }
        StretchDIBits(dc,0,0,NF_FRAME_WIDTH,NF_FRAME_HEIGHT,0,0,NF_FRAME_WIDTH,NF_FRAME_HEIGHT,
                      snapshot,&bi,DIB_RGB_COLORS,SRCCOPY);
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
