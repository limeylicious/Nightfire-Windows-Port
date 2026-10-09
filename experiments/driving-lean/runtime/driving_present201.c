/* Owned completed image preview; no guest-memory polling thread. The201 path
 * submits AV-selected resolves; opt-in204 submits at original PCRTC writes.
 * Neither establishes accurate physical scanout timing. */
#include <windows.h>
#include "driving_input224.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static SRWLOCK pixels_lock=SRWLOCK_INIT;
static uint32_t latest[640*480];static unsigned count;static volatile LONG started,closed,repaint232=1;
/* Lean build: when a Direct3D swap chain owns the window, the GDI repaint stops. */
static HWND volatile window201;static volatile LONG direct201;
HWND driving_present201_window(void){return window201;}
void driving_present201_direct(int on){InterlockedExchange(&direct201,on!=0);if(!on)InterlockedExchange(&repaint232,1);}
static LRESULT CALLBACK procedure(HWND h,UINT m,WPARAM w,LPARAM l){
 driving_input224_window(h,m,w,l);
#ifdef DRIVING_LEAN_RENDERER
 if(m==WM_KEYDOWN&&w==VK_F9&&!(l&(1LL<<30))){extern void lean_session_mark(void);lean_session_mark();}   /* LEAN_SESSION_CAPTURE mark */
#endif
 if(m==WM_PAINT||m==WM_SIZE||m==WM_DISPLAYCHANGE)InterlockedExchange(&repaint232,1);
 if(m==WM_CLOSE){InterlockedExchange(&closed,1);
#ifdef DRIVING_LEAN_RENDERER
  {extern void lean_d3d_stop_presenter(void);lean_d3d_stop_presenter();}   /* no Present into a destroyed window */
#endif
  DestroyWindow(h);return 0;}
 if(m==WM_DESTROY)return 0;return DefWindowProcA(h,m,w,l);
}
static DWORD WINAPI display(void *unused){
 (void)unused;WNDCLASSA wc={0};wc.hInstance=GetModuleHandleA(NULL);wc.lpfnWndProc=procedure;wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.lpszClassName="NightfireDriving201";
 if(!RegisterClassA(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
 RECT rect={0,0,960,720};AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
 HWND h=CreateWindowA(wc.lpszClassName,"Nightfire Driving - experimental GPU preview",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,NULL,NULL,wc.hInstance,NULL);
 if(!h)return 0;HDC dc=GetDC(h);uint32_t *frame=malloc(sizeof latest);if(!frame){DestroyWindow(h);return 0;}
 window201=h;
 BITMAPINFO bi={0};bi.bmiHeader.biSize=40;bi.bmiHeader.biWidth=640;bi.bmiHeader.biHeight=-480;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
 const char *last_hint=NULL;unsigned shown=0,rate_count=0,copied232=~0u;ULONGLONG rate_time=GetTickCount64();double fps=0;
 fprintf(stderr,"[PRESENT201] owned-frame window open; experimental coverage, no audio/timing claim\n");
 while(!InterlockedCompareExchange(&closed,0,0)){
  MSG msg;while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}
  if(closed)break;
  AcquireSRWLockShared(&pixels_lock);unsigned current=count;int changed232=current!=copied232;
  if(changed232){memcpy(frame,latest,sizeof latest);copied232=current;}ReleaseSRWLockShared(&pixels_lock);
  /* Repaint a completed frame only when it changes or Windows requests it.
   * Keep input/message polling independent, including when the guest stalls. */
  int repaint=InterlockedExchange(&repaint232,0);
  if((changed232||repaint)&&!direct201){GetClientRect(h,&rect);if(rect.right>0&&rect.bottom>0)
   StretchDIBits(dc,0,0,rect.right,rect.bottom,0,0,640,480,frame,&bi,DIB_RGB_COLORS,SRCCOPY);}
  ULONGLONG now=GetTickCount64();int rate_update=now-rate_time>=1000;
  if(rate_update){fps=1000.0*(current-rate_count)/(now-rate_time);rate_count=current;rate_time=now;}
#ifdef DRIVING_LEAN_RENDERER
  {  /* LEAN_FPS_COUNTER=1: title shows frames presented to the screen (incl.
      * LEAN_INTERP in-between frames) and the game rate, twice a second. */
   static int fc=-1;static ULONGLONG ft;static LONG fp;static unsigned fg;static double sfps,gfps;
   extern volatile LONG lean_screen_presents;
   if(fc<0){const char *v=getenv("LEAN_FPS_COUNTER");fc=v&&v[0]=='1';ft=now;fp=lean_screen_presents;fg=current;}
   if(fc){
    if(now-ft>=500){LONG p=lean_screen_presents;sfps=1000.0*(p-fp)/(now-ft);gfps=1000.0*(current-fg)/(now-ft);fp=p;fg=current;ft=now;
     const char *hint=driving_input224_hint();char title[240];
     extern volatile ULONGLONG lean_mark_flash_until;extern volatile LONG lean_mark_count;
     if(GetTickCount64()<lean_mark_flash_until)snprintf(title,sizeof title,"Nightfire Driving | MARK %ld SAVED | %.0f FPS (game %.0f) | Frame %u",lean_mark_count,sfps,gfps,current);
     else snprintf(title,sizeof title,"Nightfire Driving | %.0f FPS (game %.0f) | Frame %u | %s",sfps,gfps,current,hint);
     SetWindowTextA(h,title);shown=current;last_hint=hint;}
    Sleep(16);continue;}
  }
#endif
  const char *hint=driving_input224_hint();if(current!=shown||hint!=last_hint||rate_update){char title[240];snprintf(title,sizeof title,"Nightfire Driving - experimental GPU preview | %.1f FPS | Frame %u | %s",fps,current,hint);SetWindowTextA(h,title);shown=current;last_hint=hint;}
  Sleep(16);
 }
 free(frame);ReleaseDC(h,dc);
 /* The preview is the application's window. Closing it must terminate the
  * experimental guest too, including an original busy loop that never draws.
  * Do not wait for another presented frame or the diagnostic watchdog. */
 fprintf(stderr,"[PRESENT227] window closed; exiting Driving preview\n");fflush(stderr);
 ExitProcess(0);return 0;
}
static void dump(const uint32_t *pixels,unsigned index,uint32_t physical){
 const char *dir=getenv("DRIVING_CAPTURE_DIR");if(!dir)return;
 char path[2048];int n=snprintf(path,sizeof path,"%s/gpu201-frame%u-%08X.bmp",dir,index,physical);if(n<0||(size_t)n>=sizeof path)return;
 unsigned char h[54]={0};uint32_t size=54+sizeof latest,off=54,dib=40,w=640;int height=-480;uint16_t planes=1,bits=32;
 memcpy(h,"BM",2);memcpy(h+2,&size,4);memcpy(h+10,&off,4);memcpy(h+14,&dib,4);memcpy(h+18,&w,4);memcpy(h+22,&height,4);memcpy(h+26,&planes,2);memcpy(h+28,&bits,2);
 FILE *f=fopen(path,"wb");if(!f)return;int ok=fwrite(h,1,54,f)==54&&fwrite(pixels,1,sizeof latest,f)==sizeof latest;if(fclose(f))ok=0;
 fprintf(stderr,"[PRESENT201] capture=%u physical=%08X ok=%d completed-game-resolve experimental-profile=1\n",index,physical,ok);
}
static int dump_index(unsigned index){
#ifdef DRIVING_LEAN_RENDERER
 {/* LEAN_DUMP_FRAMES=a-b/step: also save frames a..b every step (diagnostic). */
  static int parsed;static unsigned a,b,step;
  if(!parsed){const char *v=getenv("LEAN_DUMP_FRAMES");parsed=1;if(v)sscanf(v,"%u-%u/%u",&a,&b,&step);if(!step)step=1;}
  if(b&&index>=a&&index<=b&&!((index-a)%step))return 1;}
#endif
 return index==1||index==15||index==30||index==120||index==240||index==480||(index>=510&&index<=900&&index%30==0)||(index>=630&&index<=660);
}
/* Lean build: whether the next presented frame is one that gets saved, so
 * the Direct3D path reads pixels back only for those frames. */
int driving_present201_capture_due(void){
 if(!getenv("DRIVING_CAPTURE_DIR"))return 0;
 unsigned next=count+1;if(dump_index(next))return 1;
 const char *extra=getenv("DRIVING_CAPTURE_EXTRA453");return next==90&&extra&&!strcmp(extra,"1");
}
void driving_present201(const uint8_t *pixels,uint32_t physical){
 /* One producer: serialized consumer in201 or timer worker in204. Pixels are
  * an owned copy of a completed resolve; UI never touches guest memory.
  * Lean build: pixels is NULL when the frame was already shown by Direct3D. */
 AcquireSRWLockExclusive(&pixels_lock);if(pixels)memcpy(latest,pixels,sizeof latest);unsigned index=++count;ReleaseSRWLockExclusive(&pixels_lock);
 {static ULONGLONG begin,previous;ULONGLONG now=GetTickCount64();if(index==1)begin=previous=now;
  if(index%30==0){fprintf(stderr,"[PRESENT227] frame=%u elapsed_ms=%llu last30_ms=%llu\n",index,(unsigned long long)(now-begin),(unsigned long long)(now-previous));previous=now;}}
 if(pixels&&dump_index(index))dump(latest,index,physical);
 if(pixels&&index==90){const char *extra=getenv("DRIVING_CAPTURE_EXTRA453");if(extra&&!strcmp(extra,"1"))dump(latest,index,physical);}
 const char *v=getenv("DRIVING_MOVIE_WINDOW201");
 if(v&&!strcmp(v,"1")&&!InterlockedCompareExchange(&started,1,0)){
  HANDLE thread=CreateThread(NULL,0,display,NULL,0,NULL);if(thread)CloseHandle(thread);else InterlockedExchange(&started,0);
 }
}
