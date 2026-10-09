/* One-window play (native-driving/one-window): when NIGHTFIRE_HOST_HWND names a
 * live window owned by the launcher, the engine's game window is created as a
 * child filling that host window instead of its own top-level window. The host
 * keeps its position, size and fullscreen state across Action <-> Driving
 * hand-overs and forwards close (window X, Alt+F4) to the child as WM_CLOSE.
 * Without the variable nothing changes. NF_OVERLAY=1 also makes the process
 * DPI aware (nf_host_dpi_setup; NF_DPI_AWARE=0 turns that off). Same file in nightfire-port-native and
 * nightfire-driving-native. */
#ifndef NIGHTFIRE_HOST_WINDOW_H
#define NIGHTFIRE_HOST_WINDOW_H
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define NF_HOST_CHILD_READY (WM_APP+0x4E46)   /* posted to the host: lParam = child window */

static __inline HWND nf_host_parent(void)
{
    static volatile LONG parsed; static HWND host;
    if(!InterlockedCompareExchange(&parsed,1,1)) {
        const char *v=getenv("NIGHTFIRE_HOST_HWND");
        HWND h=v&&*v?(HWND)(uintptr_t)_strtoui64(v,NULL,0):NULL;
        host=h&&IsWindow(h)?h:NULL;
        if(v&&*v)fprintf(stderr,"[HOST-WINDOW] NIGHTFIRE_HOST_HWND=%s %s\n",v,host?"used: game draws inside the launcher window":"is not a window; using an own window");
        InterlockedExchange(&parsed,1);
    }
    return host;
}
/* F10 overlay (NF_OVERLAY=1) only, unless NF_DPI_AWARE=0 (as fast as the
 * Windows-scaled window, native-driving/widescreen-test/DPI-PERF.md): the process is made per-monitor DPI aware
 * before its window exists, so on a scaled display (e.g. 200%) the game sees the
 * panel's real pixels instead of Windows blurring a half-size picture up. The
 * window opens at the same size on screen as before (its size scaled by the
 * DPI) and follows WM_DPICHANGED. Without NF_OVERLAY nothing changes. Returns
 * the DPI the window opens at (96 = no scaling). */
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
static __inline UINT nf_host_dpi_setup(void)
{
    static int done; static UINT dpi=96;
    if(done) return dpi;
    done=1;
    {const char *v=getenv("NF_OVERLAY"),*d=getenv("NF_DPI_AWARE");   /* NF_DPI_AWARE=0 turns it off */
     if(!v||v[0]!='1'||(d&&d[0]=='0')) return dpi;}
    HMODULE u=GetModuleHandleA("user32.dll");
    typedef BOOL (WINAPI *SetCtxFn)(HANDLE); typedef UINT (WINAPI *SysDpiFn)(void);
    SetCtxFn set_ctx=u?(SetCtxFn)(void*)GetProcAddress(u,"SetProcessDpiAwarenessContext"):NULL;
    const char *how=NULL;
    if(set_ctx&&set_ctx((HANDLE)(intptr_t)-4)) how="per-monitor v2";            /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 */
    else {
        typedef HRESULT (WINAPI *SetAwFn)(int);
        HMODULE s=LoadLibraryA("shcore.dll");
        SetAwFn set_aw=s?(SetAwFn)(void*)GetProcAddress(s,"SetProcessDpiAwareness"):NULL;
        if(set_aw&&SUCCEEDED(set_aw(2))) how="per-monitor (Windows 8.1 call)";    /* PROCESS_PER_MONITOR_DPI_AWARE */
    }
    SysDpiFn sys_dpi=u?(SysDpiFn)(void*)GetProcAddress(u,"GetDpiForSystem"):NULL;
    if(how&&sys_dpi) dpi=sys_dpi();
    if(dpi<96) dpi=96;
    fprintf(stderr,"[NF-OVERLAY] DPI awareness %s; window opens at %u DPI (%.2fx)\n",how?how:"not available (Windows scales the window)",dpi,dpi/96.0);
    return dpi;
}
/* WM_DPICHANGED (window moved to a monitor with other scaling): take the size
 * Windows suggests. Returns 1 when handled. Top-level windows only. */
static __inline int nf_host_dpi_changed(HWND h,UINT m,WPARAM w,LPARAM l)
{
    if(m!=WM_DPICHANGED||!l||(GetWindowLongPtrA(h,GWL_STYLE)&WS_CHILD)) return 0;
    const RECT *r=(const RECT*)l;
    SetWindowPos(h,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
    fprintf(stderr,"[NF-OVERLAY] window now at %u DPI: %ldx%ld\n",(unsigned)HIWORD(w),r->right-r->left,r->bottom-r->top);
    return 1;
}
/* Creates the game window: a child filling the host when hosted, otherwise
 * exactly the original top-level window. */
static __inline HWND nf_host_create_window(const char *cls,const char *title,int client_w,int client_h,HINSTANCE inst)
{
    UINT dpi=nf_host_dpi_setup();
    HWND host=nf_host_parent();
    if(host) {
        RECT r; if(!GetClientRect(host,&r)||r.right<=0||r.bottom<=0){r.right=client_w;r.bottom=client_h;}
        HWND h=CreateWindowExA(WS_EX_NOPARENTNOTIFY,cls,title,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
                               0,0,r.right,r.bottom,host,NULL,inst,NULL);
        if(h) {
            fprintf(stderr,"[HOST-WINDOW] child window %p inside host %p (%ldx%ld)\n",(void*)h,(void*)host,r.right,r.bottom);
            PostMessageA(host,NF_HOST_CHILD_READY,(WPARAM)GetCurrentProcessId(),(LPARAM)h);
            SetFocus(h);
            return h;
        }
        fprintf(stderr,"[HOST-WINDOW] child window creation failed (%lu); using an own window\n",GetLastError());
    }
    RECT rect={0,0,MulDiv(client_w,(int)dpi,96),MulDiv(client_h,(int)dpi,96)};
    if(dpi!=96) {   /* DPI aware (NF_OVERLAY): the borders at this DPI too */
        typedef BOOL (WINAPI *AdjFn)(LPRECT,DWORD,BOOL,DWORD,UINT);
        HMODULE u=GetModuleHandleA("user32.dll");
        AdjFn adj=u?(AdjFn)(void*)GetProcAddress(u,"AdjustWindowRectExForDpi"):NULL;
        if(adj) adj(&rect,WS_OVERLAPPEDWINDOW,FALSE,0,dpi); else AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
    } else AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
    return CreateWindowExA(0,cls,title,WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,
                           rect.right-rect.left,rect.bottom-rect.top,NULL,NULL,inst,NULL);
}
static __inline int nf_host_is_child(HWND h) { return h && (GetWindowLongPtrA(h,GWL_STYLE)&WS_CHILD)!=0; }

/* Display mode (F10 overlay / Action's PC Graphics page, NF_OVERLAY=1): 0 windowed,
 * 1 fullscreen (borderless covering the monitor and kept on top), 2 borderless
 * fullscreen (not on top, Alt+Tab friendly). True exclusive fullscreen is not used:
 * the game draws into a child of the launcher's shared window and changes process
 * at every Action/Driving hand-over. Hosted: the launcher's window switches itself
 * (one_window_host.py, NF_HOST_SET_MODE); own window: switched here. */
#define NF_HOST_SET_MODE (WM_APP+0x4E48)   /* posted to the host: wParam = mode */
static __inline int nf_window_get_mode(HWND game)
{
    HWND top=game?GetAncestor(game,GA_ROOT):NULL;
    if(!top) return 0;
    if(!(GetWindowLongPtrA(top,GWL_STYLE)&WS_POPUP)) return 0;
    return (GetWindowLongPtrA(top,GWL_EXSTYLE)&WS_EX_TOPMOST)?1:2;
}
static __inline void nf_window_set_mode(HWND game,int mode)
{
    static RECT saved; static int have_saved;
    HWND top=game?GetAncestor(game,GA_ROOT):NULL;
    if(!top||mode<0||mode>2) return;
    if(top!=game){PostMessageA(top,NF_HOST_SET_MODE,(WPARAM)mode,0);return;}
    LONG_PTR st=GetWindowLongPtrA(top,GWL_STYLE);
    if(mode==0){
        if(!(st&WS_POPUP)) return;
        SetWindowLongPtrA(top,GWL_STYLE,(st&~(LONG_PTR)WS_POPUP)|WS_OVERLAPPEDWINDOW);
        if(have_saved) SetWindowPos(top,HWND_NOTOPMOST,saved.left,saved.top,saved.right-saved.left,saved.bottom-saved.top,SWP_FRAMECHANGED|SWP_SHOWWINDOW);
        else SetWindowPos(top,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED|SWP_SHOWWINDOW);
    } else {
        if(!(st&WS_POPUP)){GetWindowRect(top,&saved);have_saved=1;}
        MONITORINFO mi;mi.cbSize=sizeof mi;
        if(!GetMonitorInfoA(MonitorFromWindow(top,MONITOR_DEFAULTTONEAREST),&mi)) return;
        SetWindowLongPtrA(top,GWL_STYLE,(st&~(LONG_PTR)WS_OVERLAPPEDWINDOW)|WS_POPUP|WS_VISIBLE);
        SetWindowPos(top,mode==1?HWND_TOPMOST:HWND_NOTOPMOST,mi.rcMonitor.left,mi.rcMonitor.top,
                     mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED|SWP_SHOWWINDOW);
    }
    fprintf(stderr,"[NF-OVERLAY] display mode %d (own window)\n",mode);
}
/* The game window is "in front": its top-level window (the host when hosted) is foreground. */
static __inline int nf_host_foreground(HWND h)
{
    if(!h) return 0;
    HWND f=GetForegroundWindow();
    return nf_host_is_child(h) ? f==GetAncestor(h,GA_ROOT) : f==h;
}
/* Raw mouse: a child of another process's window needs INPUTSINK; callers
 * still only use the input while nf_host_foreground() holds. */
static __inline DWORD nf_host_raw_flags(HWND h) { return nf_host_is_child(h)?RIDEV_INPUTSINK:0; }
/* Window title: shown on the host when hosted (never blocks on a hung host). */
static __inline void nf_host_title(HWND h,const char *text)
{
    if(nf_host_is_child(h)) {DWORD_PTR r; SendMessageTimeoutA(GetAncestor(h,GA_ROOT),WM_SETTEXT,0,(LPARAM)text,SMTO_ABORTIFHUNG|SMTO_BLOCK,250,&r);}
    else SetWindowTextA(h,text);
}
#endif
