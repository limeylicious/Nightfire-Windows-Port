/* Analysis candidate: PAL Driving XPP transport replacement, opt-in only.
 * Reports go through original 108490/51B40; no gameplay/camera memory edits. */
#include "driving_input224.h"
#include "nightfire_host_window.h"   /* NIGHTFIRE_HOST_HWND one-window play */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern __declspec(thread) uint32_t g_eax,g_esp;
extern ptrdiff_t g_xbox_mem_offset;
#define PAD_TYPE224 0x183AECu
#define PAD_HANDLE224 0x22400001u
static SRWLOCK lock224=SRWLOCK_INIT;
static HWND window224;
static LONG enabled224=-1;
static int focused224,opened224,enumerated224,hidden224,captured224;
static unsigned char blocked224[256],previous224[18];
static int mouse_x224,mouse_y224;
static uint32_t packet224;
static ULONGLONG mouse_time224;
static int16_t mouse_axes224[2];
static unsigned polls224;
#ifdef DRIVING_INPUT224_TEST
extern void input224_test_sample(unsigned char out[18]);
#endif
static int active224(void){
 LONG state=InterlockedCompareExchange(&enabled224,-1,-1);
 if(state<0){const char *p=getenv("DRIVING_INPUT224");LONG wanted=p&&!strcmp(p,"1");InterlockedCompareExchange(&enabled224,wanted,-1);state=InterlockedCompareExchange(&enabled224,-1,-1);}
 return state!=0;
}
static void *memory224(uint32_t a,size_t n){
 if(a<0x10000u||a>0x04000000u||n>0x04000000u-a)return NULL;
 return (void *)(g_xbox_mem_offset+a);
}
static uint32_t word224(uint32_t a){uint32_t v=0;void *p=memory224(a,4);if(p)memcpy(&v,p,4);return v;}
static int put224(uint32_t a,const void *v,size_t n){void *p=memory224(a,n);if(!p)return 0;memcpy(p,v,n);return 1;}
static int finish224(uint32_t result,unsigned args){g_eax=result;g_esp+=4+4*args;return 1;}
/* Raw displacement is an approximate stick velocity, not native camera motion.
 * The nonzero floor clears the original title's stick deadzone. */
static int16_t mouse_axis224(int delta,ULONGLONG ms){
 if(!delta)return 0;
 int64_t n=delta<0?-(int64_t)delta:delta;
 n=14000+n*900*16/(int64_t)ms;if(n>32767)n=32767;
 return (int16_t)(delta<0?-n:n);
}
static int key224(unsigned key,const unsigned char *keys){return keys[key]&&!blocked224[key];}
/* Pure sample conversion is separately exercised without reading live input. */
static void map224(unsigned char out[18],const unsigned char keys[256],int focus,int captured,
                   int dx,int dy,ULONGLONG now){
 memset(out,0,18);
 if(!focus){focused224=0;memset(blocked224,1,sizeof blocked224);mouse_axes224[0]=mouse_axes224[1]=0;mouse_time224=now;return;}
 if(!focused224){memcpy(blocked224,keys,sizeof blocked224);focused224=1;mouse_axes224[0]=mouse_axes224[1]=0;mouse_time224=now;dx=dy=0;}
 for(unsigned i=0;i<256;i++)if(!keys[i])blocked224[i]=0;
 #define K(k) key224((k),keys)
 unsigned digital=(K(VK_UP)?1:0)|(K(VK_DOWN)?2:0)|(K(VK_LEFT)?4:0)|(K(VK_RIGHT)?8:0)|
  (K(VK_RETURN)?16:0)|(K(VK_TAB)?32:0)|(K(VK_SHIFT)?64:0)|(K('V')?128:0);
 out[0]=(unsigned char)digital;
 out[2]=(K('Z')||K('E')||K('R'))?255:0;out[3]=(K('X')||K('Q'))?255:0;
 out[4]=(K(VK_CONTROL)||K('C'))?255:0;out[5]=K(VK_SPACE)?255:0;
 out[6]=K('G')?255:0;out[7]=K('F')?255:0;
 out[8]=(captured&&(K(VK_RBUTTON)||K('T')))?255:0;out[9]=(captured&&K(VK_LBUTTON))?255:0;
 int16_t axes[4]={(int16_t)((K('D')-K('A'))*32767),(int16_t)((K('W')-K('S'))*32767),0,0};
 ULONGLONG elapsed=now-mouse_time224;
 if(!captured){mouse_axes224[0]=mouse_axes224[1]=0;mouse_time224=now;dx=dy=0;elapsed=0;}
 /* Repeated reads retain a sample for 8 ms; an idle subsequent sample is zero. */
 if(elapsed>=8){if(elapsed>50)elapsed=50;mouse_axes224[0]=mouse_axis224(dx,elapsed);mouse_axes224[1]=mouse_axis224(-dy,elapsed);mouse_time224=now;}
 axes[2]=mouse_axes224[0];axes[3]=mouse_axes224[1];memcpy(out+10,axes,8);
 #undef K
}
static void sample224(unsigned char out[18]){
 unsigned char keys[256]={0};ULONGLONG now=GetTickCount64();
 AcquireSRWLockShared(&lock224);HWND window=window224;ReleaseSRWLockShared(&lock224);
 extern volatile LONG nf_overlay_menu_open;   /* driving_present201.c: NF_OVERLAY menu is up */
 int focus=window&&nf_host_foreground(window)&&!nf_overlay_menu_open;
 if(focus)for(unsigned i=0;i<256;i++)keys[i]=(GetAsyncKeyState((int)i)&0x8000)!=0;
 AcquireSRWLockExclusive(&lock224);
 focus=focus&&window224==window&&nf_host_foreground(window)&&!nf_overlay_menu_open;
 int dx=mouse_x224,dy=mouse_y224;
 if(!focus||!focused224||now-mouse_time224>=8)mouse_x224=mouse_y224=0;
 map224(out,keys,focus,captured224,dx,dy,now);
 ReleaseSRWLockExclusive(&lock224);
}
static void clip224(HWND h,int enabled){
 if(enabled){RECT r;GetClientRect(h,&r);POINT a={r.left,r.top},b={r.right,r.bottom};ClientToScreen(h,&a);ClientToScreen(h,&b);r.left=a.x;r.top=a.y;r.right=b.x;r.bottom=b.y;ClipCursor(&r);if(!hidden224){ShowCursor(FALSE);hidden224=1;}}
 else{ClipCursor(NULL);if(hidden224){ShowCursor(TRUE);hidden224=0;}}
}
static void capture_state224(int capture,ULONGLONG now){
 captured224=capture;mouse_x224=mouse_y224=0;mouse_axes224[0]=mouse_axes224[1]=0;mouse_time224=now;blocked224[VK_LBUTTON]=blocked224[VK_RBUTTON]=blocked224['T']=1;
}
static void capture224(HWND h,int capture){
 AcquireSRWLockExclusive(&lock224);capture_state224(capture,GetTickCount64());ReleaseSRWLockExclusive(&lock224);
 clip224(h,capture);
}
/* NF_OVERLAY menu opened: let the mouse go, as Esc does. */
void driving_input224_release(HWND h){if(active224())capture224(h,0);}
const char *driving_input224_hint(void){
 if(!active224())return "";AcquireSRWLockShared(&lock224);int captured=captured224;ReleaseSRWLockShared(&lock224);
 return captured?"Mouse captured: Esc releases | Enter pauses":"Click or F1 captures mouse | Enter pauses";
}
void driving_input224_window(HWND h,UINT m,WPARAM w,LPARAM l){
 if(!active224())return;
 if(m==WM_CREATE){AcquireSRWLockExclusive(&lock224);window224=h;ReleaseSRWLockExclusive(&lock224);RAWINPUTDEVICE d={1,2,nf_host_raw_flags(h),h};if(!RegisterRawInputDevices(&d,1,sizeof d))fprintf(stderr,"[INPUT224] raw mouse registration failed=%lu\n",GetLastError());}
 AcquireSRWLockShared(&lock224);int captured=captured224;ReleaseSRWLockShared(&lock224);
 if(nf_host_foreground(h)){
  if(m==WM_KEYDOWN&&w==VK_ESCAPE)capture224(h,0);
  if(m==WM_KEYDOWN&&w==VK_F1&&!(l&(1LL<<30)))capture224(h,!captured);
  if(m==WM_LBUTTONDOWN&&!captured)capture224(h,1);
  if((m==WM_SIZE||m==WM_MOVE)&&captured)clip224(h,1);
 }
 if(m==WM_KILLFOCUS||m==WM_DESTROY){capture224(h,0);AcquireSRWLockExclusive(&lock224);focused224=0;memset(blocked224,1,sizeof blocked224);if(m==WM_DESTROY)window224=NULL;ReleaseSRWLockExclusive(&lock224);}
 if(m==WM_INPUT&&captured&&nf_host_foreground(h)){RAWINPUT r;UINT n=sizeof r;
  if(GetRawInputData((HRAWINPUT)l,RID_INPUT,&r,&n,sizeof(RAWINPUTHEADER))!=UINT_MAX&&r.header.dwType==RIM_TYPEMOUSE&&!(r.data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE)){
   AcquireSRWLockExclusive(&lock224);int64_t x=(int64_t)mouse_x224+r.data.mouse.lLastX,y=(int64_t)mouse_y224+r.data.mouse.lLastY;
   mouse_x224=(int)(x>100000?100000:x< -100000?-100000:x);mouse_y224=(int)(y>100000?100000:y< -100000?-100000:y);ReleaseSRWLockExclusive(&lock224);
  }
 }
}
static int implementation224(uint32_t va){
 if(!active224())return 0;
 if(va!=0x184BB3u&&va!=0x184BD5u&&va!=0x1848ADu&&va!=0x184903u&&va!=0x18490Fu&&va!=0x184AEDu&&va!=0x184B59u)return 0;
 uint32_t a=word224(g_esp+4),b=word224(g_esp+8),c=word224(g_esp+12);
 if(va==0x184BB3u){if(a!=PAD_TYPE224)return 0;enumerated224=1;return finish224(1,1);}
 if(va==0x184BD5u){if(a!=PAD_TYPE224)return 0;uint32_t insert=enumerated224?0:1,remove=0;
  if(!memory224(b,4)||!memory224(c,4))return finish224(0,3);
  put224(b,&insert,4);put224(c,&remove,4);enumerated224=1;return finish224(insert!=0,3);}
 if(va==0x1848ADu){if(a!=PAD_TYPE224)return 0;if(b||c)return finish224(0,4);opened224=1;return finish224(PAD_HANDLE224,4);}
 /* Original handles continue through the original implementation. */
 if(a!=PAD_HANDLE224)return 0;
 if(va==0x184903u){opened224=0;return finish224(0,1);}
 if(va==0x18490Fu){unsigned char caps[25]={0};caps[0]=1;caps[3]=255;memset(caps+5,255,8);for(unsigned i=13;i<21;i+=2){caps[i]=255;caps[i+1]=127;}
  if(!opened224)return finish224(ERROR_DEVICE_NOT_CONNECTED,2);return finish224(put224(b,caps,sizeof caps)?0:ERROR_INVALID_PARAMETER,2);}
 if(va==0x184B59u){uint32_t status=ERROR_NOT_SUPPORTED;put224(b,&status,4);return finish224(status,2);}
 if(!memory224(b,22))return finish224(ERROR_INVALID_PARAMETER,2);
 unsigned char state[18]={0};
 #ifdef DRIVING_INPUT224_TEST
 input224_test_sample(state);
 #else
 if(opened224)sample224(state);
 #ifdef DRIVING_LEAN_RENDERER
 {extern void lean_fire_test_sample(unsigned char *);lean_fire_test_sample(state);} /* test-only, LEAN_FIRE_TEST */
 #endif
 #endif
 if(!opened224)memset(state,0,18);
 if(memcmp(state,previous224,18)){memcpy(previous224,state,18);packet224++;}
 put224(b,&packet224,4);put224(b+4,state,18);
 if(++polls224==1)fprintf(stderr,"[INPUT224] original XPP report bridge active; port0 KBM, focus release, no automatic input\n");
 return finish224(opened224?0:ERROR_DEVICE_NOT_CONNECTED,2);
}
int driving_input224(uint32_t va){DWORD error=GetLastError();int result=implementation224(va);SetLastError(error);return result;}
