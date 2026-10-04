#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <dbghelp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fenv.h>
#include <xmmintrin.h>
#include "driving_diag510.h"
#define RECORDS510 262144u
#define ATTEMPTS510 1048576u
_Static_assert(sizeof(D510Record)==512,"510 record ABI");
_Static_assert(sizeof(D510Draw)<=448&&sizeof(D510Glyph)<=448,"510 payload bound");
int driving_text510_on,driving_stall510_on;
static D510Record records510[RECORDS510];
static volatile LONG reserved510,lost510,done510;
static volatile LONG sealed510,producing510;
static unsigned written510;static volatile LONG writing510;static HANDLE text_file510=INVALID_HANDLE_VALUE;
static LARGE_INTEGER hz510;static volatile LONG64 last_present510,start510;
static uint64_t epoch510=1,command510,attempt510,attempt_command510;
static uint64_t attempt_commands510[ATTEMPTS510];
static struct {uint32_t count,primitive;struct {uint64_t command;uint32_t method,value;} items[27];} immediate510;
static __declspec(thread) uint64_t override510,selected_epoch510;
static __declspec(thread) unsigned selected_version510;
static uint64_t atlas_hashes510[1024];static unsigned atlases510;
static char directory510[1800];
static uint64_t now510(void){LARGE_INTEGER x;QueryPerformanceCounter(&x);return (uint64_t)x.QuadPart;}
void driving_diag510_emit(unsigned type,const void *payload,size_t size){
 if(!driving_text510_on)return;DWORD e=GetLastError();int ce=errno;
 if(InterlockedCompareExchange(&sealed510,0,0)){errno=ce;SetLastError(e);return;}InterlockedIncrement(&producing510);
 if(InterlockedCompareExchange(&sealed510,0,0)){InterlockedDecrement(&producing510);errno=ce;SetLastError(e);return;}
 LONG i=InterlockedIncrement(&reserved510)-1;
 if(i<0||(unsigned)i-written510>=RECORDS510||size>448){InterlockedIncrement(&lost510);InterlockedDecrement(&producing510);errno=ce;SetLastError(e);return;}
 D510Record*r=&records510[(unsigned)i%RECORDS510];r->type=type;r->tid=GetCurrentThreadId();r->bytes=(uint32_t)size;r->qpc=now510();
 r->epoch=epoch510;r->command=command510;r->attempt=override510?override510:attempt510;r->serial=(uint64_t)i+1;r->reserved=r->attempt<ATTEMPTS510?attempt_commands510[r->attempt]:0;
 if(size)memcpy(r->payload,payload,size);MemoryBarrier();InterlockedExchange(&r->ready,1);InterlockedDecrement(&producing510);errno=ce;SetLastError(e);
}
void driving_diag510_command(unsigned method,unsigned value){if(!driving_text510_on)return;
 if(method==0x17fc&&value&&immediate510.count){driving_diag510_emit(D510_IMMEDIATE,&immediate510,8+16*immediate510.count);immediate510.count=0;}
 command510++;if(method==0x17fc&&value){attempt510++;attempt_command510=command510;if(attempt510<ATTEMPTS510)attempt_commands510[attempt510]=command510;else InterlockedIncrement(&lost510);immediate510.primitive=value;}
 if(immediate510.primitive==8){unsigned i=immediate510.count++;immediate510.items[i].command=command510;immediate510.items[i].method=method;immediate510.items[i].value=value;
  if(immediate510.count==27||(method==0x17fc&&!value)){driving_diag510_emit(D510_IMMEDIATE,&immediate510,8+16*immediate510.count);immediate510.count=0;}}
 if(method==0x17fc&&!value)immediate510.primitive=0;
}
uint64_t driving_diag510_attempt(void){return driving_text510_on?attempt510:0;}
uint64_t driving_diag510_override(uint64_t id){uint64_t old=override510;override510=id;return old;}
void driving_diag510_route(unsigned path,unsigned stage,unsigned value,unsigned count){uint32_t p[4]={path,stage,value,count};driving_diag510_emit(D510_ROUTE,p,sizeof p);}
void driving_diag510_raw_attempt(unsigned primitive,const uint32_t*s){if(!driving_text510_on)return;
 uint32_t p[32]={primitive,s[0x210/4],s[0x208/4],s[0x1b00/4],s[0x1b04/4],s[0x300/4],s[0x304/4],s[0x30c/4],s[0x340/4],s[0x344/4],s[0x348/4],s[0x358/4]};
 for(unsigned k=0;k<4;k++){p[12+k*4]=s[(0x1b00+64*k)/4];p[13+k*4]=s[(0x1b04+64*k)/4];p[14+k*4]=s[(0x1b14+64*k)/4];p[15+k*4]=s[(0x1b08+64*k)/4];}
 driving_diag510_emit(D510_ATTEMPT,p,sizeof p);
}
void driving_diag510_clear(unsigned path,uint32_t target,const uint32_t*s,unsigned flags){if(!driving_text510_on)return;uint32_t p[8]={path,target,flags,s[0x1d98/4],s[0x1d9c/4],s[0x1d90/4],s[0x1d8c/4],s[0x208/4]};driving_diag510_emit(D510_CLEAR,p,sizeof p);}
uint64_t driving_diag510_atlas(const unsigned char*p,size_t n){if(!driving_text510_on)return 0;
 uint64_t h=14695981039346656037ull;for(size_t i=0;i<n;i++){h^=p[i];h*=1099511628211ull;}
 for(unsigned i=0;i<atlases510;i++)if(atlas_hashes510[i]==h)return h;
 if(atlases510>=1024){InterlockedIncrement(&lost510);return 0;}atlas_hashes510[atlases510++]=h;
 for(size_t at=0;at<n;at+=432){D510Atlas a={0};a.hash=h;a.offset=(uint32_t)at;a.total=(uint32_t)n;size_t b=n-at<432?n-at:432;memcpy(a.data,p+at,b);driving_diag510_emit(D510_ATLAS,&a,16+b);}return h;
}
uint64_t driving_diag510_epoch(void){return driving_text510_on?epoch510:0;}
uint64_t driving_diag510_completed(void){return selected_epoch510;}
uint64_t driving_diag510_resolve(uint32_t src,uint32_t dst,unsigned family,const float*v,unsigned count){if(!driving_text510_on)return 0;
 D510Resolve r={0};r.source=src;r.destination=dst;r.family=family;r.count=count;
 /* Pos/colour/UV0/UV3 from original input, three resolve vertices. */
 for(unsigned i=0;i<count&&i<3;i++){memcpy(r.attributes[i],v+i*64,16);memcpy(r.attributes[i]+4,v+i*64+12,16);memcpy(r.attributes[i]+8,v+i*64+36,16);memcpy(r.attributes[i]+12,v+i*64+48,16);}
 driving_diag510_emit(D510_RESOLVE,&r,sizeof r);return epoch510++;
}
void driving_diag510_cache(uint32_t physical,unsigned version,uint64_t ep){D510Link p={ep,physical,version,0,0};driving_diag510_emit(D510_CACHE,&p,sizeof p);}
void driving_diag510_select(uint64_t ep,unsigned version){selected_epoch510=ep;selected_version510=version;}
void driving_diag510_present(unsigned index,uint32_t physical){
 if(driving_stall510_on)InterlockedExchange64(&last_present510,(LONG64)now510());
 D510Link p={selected_epoch510,physical,selected_version510,index,0};driving_diag510_emit(D510_PRESENT,&p,sizeof p);driving_diag510_beat("present");
}
static void flush_text510(void){if(text_file510==INVALID_HANDLE_VALUE||InterlockedCompareExchange(&writing510,1,0))return;
 unsigned n=(unsigned)InterlockedCompareExchange(&reserved510,0,0);
 while(written510<n&&InterlockedCompareExchange(&records510[written510%RECORDS510].ready,0,0)&&records510[written510%RECORDS510].serial==(uint64_t)written510+1){
  unsigned end=written510+1;while(end<n&&end-written510<128&&end%RECORDS510&&InterlockedCompareExchange(&records510[end%RECORDS510].ready,0,0)&&records510[end%RECORDS510].serial==(uint64_t)end+1)end++;
  DWORD b=0,want=(end-written510)*sizeof(D510Record);if(!WriteFile(text_file510,&records510[written510%RECORDS510],want,&b,NULL)||b!=want){InterlockedIncrement(&lost510);break;}
  for(unsigned k=written510;k<end;k++)InterlockedExchange(&records510[k%RECORDS510].ready,0);MemoryBarrier();written510=end;
 }InterlockedExchange(&writing510,0);
}
#include "driving_stall510_impl.h"
void driving_diag510_terminal(const char*reason){
 if(!driving_text510_on&&!driving_stall510_on)return;
 DWORD e=GetLastError();int ce=errno;
 InterlockedExchange(&sealed510,1);uint64_t end510=now510()+(uint64_t)hz510.QuadPart;
 while(InterlockedCompareExchange(&producing510,0,0)&&now510()<end510)Sleep(1);
 do{flush_text510();if(written510==(unsigned)reserved510&&!writing510)break;Sleep(1);}while(now510()<end510);
 terminal_stall510(reason);
 /* Only the diagnostic writer flushes shared records. The footer states the exact committed prefix. */
 errno=ce;SetLastError(e);
}
static void normal_exit510(void){driving_diag510_terminal("atexit");}
static DWORD WINAPI diagnostic_worker510(void*p){(void)p;driving_diag510_register("diagnostic510");
 if(driving_stall510_on)symbols_init510();
 uint64_t limit=(uint64_t)deadline510*hz510.QuadPart+start510,next=now510();int deadline_written=0;
 while(!done510){uint64_t t=now510();flush_text510();if(driving_stall510_on)sample_stall510(t);
  if(t>=limit&&!deadline_written){flush_text510();terminal_stall510("locator-own-deadline");deadline_written=1;}
  uint64_t period=(uint64_t)hz510.QuadPart/(driving_stall510_on?4:2);next+=period;t=now510();if(next<t){next=t+period;missed_periods510++;}
  uint64_t remain=next-t;Sleep((DWORD)(remain*1000/hz510.QuadPart));}
 flush_text510();return 0;
}
void driving_diag510_init(void){DWORD e=GetLastError();int ce=errno;
 const char*t=getenv("DRIVING_TEXT510"),*s=getenv("DRIVING_STALL510");driving_text510_on=t&&!strcmp(t,"1");driving_stall510_on=s&&!strcmp(s,"1");
 if(!driving_text510_on&&!driving_stall510_on){errno=ce;SetLastError(e);return;}
 QueryPerformanceFrequency(&hz510);start510=last_present510=(LONG64)now510();
 const char*dir=getenv("DRIVING_CAPTURE_DIR");if(!dir||strlen(dir)>=sizeof(directory510)){fprintf(stderr,"[DIAG510] STOP invalid private diagnostic directory\n");ExitProcess(2);}
 strcpy_s(directory510,sizeof directory510,dir);const char*d=getenv("DRIVING_STALL510_DEADLINE_SECS");if(d&&atoi(d)>0)deadline510=(unsigned)atoi(d);
 char path[2048];snprintf(path,sizeof path,"%s/text510.bin",directory510);
 if(driving_text510_on){text_file510=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(text_file510==INVALID_HANDLE_VALUE){fprintf(stderr,"[DIAG510] STOP cannot create text recorder\n");ExitProcess(2);}}
 initialize_stall510();driving_diag510_register("guest-main");atexit(normal_exit510);
 HANDLE h=CreateThread(NULL,0,diagnostic_worker510,NULL,0,NULL);if(!h){fprintf(stderr,"[DIAG510] STOP cannot start diagnostic writer\n");ExitProcess(2);}CloseHandle(h);
 fprintf(stderr,"[DIAG510] text=%d stall=%d record_size=%zu capacity=%u qpc_hz=%lld deadline=%u fixed-storage=1 no-render-change=1\n",driving_text510_on,driving_stall510_on,sizeof(D510Record),RECORDS510,hz510.QuadPart,deadline510);
 errno=ce;SetLastError(e);
}
