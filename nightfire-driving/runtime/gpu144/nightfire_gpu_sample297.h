/* One complete private batch, diagnostic only. No Flush, Map, waits or retries.
 * Serialized context ownership may move between producer and295 worker.
 * All retained metadata is copied; no producer/TLS/descriptor pointers persist. */
#ifndef NIGHTFIRE_GPU_SAMPLE297_H
#define NIGHTFIRE_GPU_SAMPLE297_H
enum {GT297_INIT=1,GT297_MATERIAL,GT297_BOUNDARY,GT297_DEPTH130,GT297_DEPTH256,
 GT297_DEPTH268,GT297_DEPTH272,GT297_PACK294,GT297_FINAL,GT297_KIND_COUNT};
#ifdef NIGHTFIRE_GPU_SAMPLE297
#include <errno.h>
#include <limits.h>
#include <fenv.h>
#include <xmmintrin.h>
enum {GT297_STAMPS=512,GT297_EVENTS=255,GT297_STACK=4,GT297_POLLS=8,
 GT297_OPEN=1,GT297_ISSUED,GT297_READY,GT297_INVALID,GT297_UNAVAILABLE};
typedef struct {DWORD error;int err;fenv_t env;unsigned csr;} GTGuard297;
static void gt297_save(GTGuard297 *g){g->error=GetLastError();g->err=errno;fegetenv(&g->env);g->csr=_mm_getcsr();}
static void gt297_restore(const GTGuard297 *g){fesetenv(&g->env);_mm_setcsr(g->csr);errno=g->err;SetLastError(g->error);}
typedef struct {unsigned kind,parent,lane,draw,vertices,flags,detail,begin,end;uint32_t program,key;} GTEvent297;
static struct {
 ID3D11Query *disjoint,*stamp[GT297_STAMPS];D3D11_QUERY_DATA_TIMESTAMP_DISJOINT clock;
 UINT64 gpu[GT297_STAMPS];GTEvent297 event[GT297_EVENTS];
 unsigned state,prepared,attempt,scope,open,next,events,depth,stack[GT297_STACK],polls,printed;
 unsigned count,minimum,features,thread,lane,draw,vertices,flags;uint32_t program,key;
 unsigned kinds[GT297_KIND_COUNT];uint64_t ordinal,requested,qpc_hz,cpu[11],qpc_calls;
 const char *cause;HRESULT error;
} gt297;
static int gt297_setting=-1;
static uint64_t gt297_qpc(void){LARGE_INTEGER t;QueryPerformanceCounter(&t);gt297.qpc_calls++;return t.QuadPart;}
static int gt297_enabled(void){
 if(gt297_setting<0){GTGuard297 g;gt297_save(&g);const char*v=getenv("DRIVING_GPU_SAMPLE297");gt297_setting=v&&!strcmp(v,"1");gt297.requested=1024;gt297.minimum=1;
  if(gt297_setting){const char*n=getenv("DRIVING_GPU_SAMPLE297_BATCH");if(n&&*n){char*end;errno=0;unsigned long x=strtoul(n,&end,10);if(!errno&&!*end&&n[0]>='0'&&n[0]<='9'&&x&&x<=1000000)gt297.requested=x;}}
  if(gt297_setting){const char*n=getenv("DRIVING_GPU_SAMPLE297_MIN_DRAWS");if(n&&*n){char*end;errno=0;unsigned long x=strtoul(n,&end,10);if(!errno&&!*end&&n[0]>='0'&&n[0]<='9'&&x&&x<=32)gt297.minimum=(unsigned)x;}}
  gt297_restore(&g);
 }return gt297_setting;
}
static void gt297_close(ID3D11DeviceContext*c){if(gt297.open){ID3D11DeviceContext_End(c,(ID3D11Asynchronous*)gt297.disjoint);gt297.open=0;}}
static void gt297_invalid(ID3D11DeviceContext*c,const char*cause,HRESULT hr){gt297_close(c);if(gt297.state!=GT297_INVALID){gt297.state=GT297_INVALID;gt297.cause=cause;gt297.error=hr;}}
static int gt297_prepare(ID3D11Device*d){
 if(gt297.prepared)return gt297.state!=GT297_UNAVAILABLE;
 gt297.prepared=1;const char*v=getenv("DRIVING_GPU_SAMPLE254");if(v&&!strcmp(v,"1")){gt297.state=GT297_UNAVAILABLE;gt297.cause="conflicting254";return 0;}
 LARGE_INTEGER hz;if(!QueryPerformanceFrequency(&hz)||!hz.QuadPart){gt297.state=GT297_UNAVAILABLE;gt297.cause="qpc_frequency";return 0;}gt297.qpc_hz=hz.QuadPart;
 D3D11_QUERY_DESC desc={D3D11_QUERY_TIMESTAMP_DISJOINT,0};HRESULT hr=ID3D11Device_CreateQuery(d,&desc,&gt297.disjoint);desc.Query=D3D11_QUERY_TIMESTAMP;
 for(unsigned i=0;SUCCEEDED(hr)&&i<GT297_STAMPS;i++)hr=ID3D11Device_CreateQuery(d,&desc,&gt297.stamp[i]);
 if(FAILED(hr)){for(unsigned i=0;i<GT297_STAMPS;i++)if(gt297.stamp[i]){ID3D11Query_Release(gt297.stamp[i]);gt297.stamp[i]=NULL;}
  if(gt297.disjoint){ID3D11Query_Release(gt297.disjoint);gt297.disjoint=NULL;}gt297.state=GT297_UNAVAILABLE;gt297.cause="create_failed";gt297.error=hr;return 0;}
 return 1;
}
static unsigned gt297_stamp(ID3D11DeviceContext*c){
 if(gt297.next>=GT297_STAMPS){gt297_invalid(c,"timestamp_capacity",E_FAIL);return 0;}
 unsigned at=gt297.next++;ID3D11DeviceContext_End(c,(ID3D11Asynchronous*)gt297.stamp[at]);return at;
}
/* features bit0=272, bit1=278 scope, bit2=291, bit3=294. Preparation occurs on
 * the first enabled private call, normally well before requested ordinal1024. */
static void gt297_begin(ID3D11Device*d,ID3D11DeviceContext*c,uint64_t completed,unsigned count,unsigned features){
 if(!gt297_enabled()||gt297.attempt)return;
 GTGuard297 g;gt297_save(&g);
 if(!gt297_prepare(d)){gt297.attempt=1;goto done;}
 if(completed+1<gt297.requested||count<gt297.minimum||count>32)goto done;
 gt297.attempt=1;gt297.ordinal=completed+1;gt297.count=count;gt297.features=features;gt297.thread=GetCurrentThreadId();gt297.cause="none";
 gt297.scope=1;gt297.state=GT297_OPEN;gt297.cpu[0]=gt297_qpc();ID3D11DeviceContext_Begin(c,(ID3D11Asynchronous*)gt297.disjoint);gt297.open=1;gt297_stamp(c);
done:gt297_restore(&g);
}
static void gt297_select(unsigned lane,unsigned draw,unsigned vertices,uint32_t program,unsigned flags,const void *key,size_t size){
 if(!gt297.scope||gt297.state!=GT297_OPEN)return;
 gt297.lane=lane;gt297.draw=draw;gt297.vertices=vertices;gt297.program=program;gt297.flags=flags;
 uint32_t hash=2166136261u;const unsigned char*p=key;for(size_t i=0;i<size;i++)hash=(hash^p[i])*16777619u;gt297.key=hash;
}
static void gt297_op(ID3D11DeviceContext*c,unsigned kind,unsigned end,unsigned detail){
 if(!gt297.scope||gt297.state!=GT297_OPEN)return;
 GTGuard297 g;gt297_save(&g);
 if(!kind||kind>=GT297_KIND_COUNT){gt297_invalid(c,"event_kind",E_FAIL);goto done;}
 if(!end){
  if(gt297.events>=GT297_EVENTS||gt297.depth>=GT297_STACK){gt297_invalid(c,"event_capacity",E_FAIL);goto done;}
  unsigned at=gt297.events++;GTEvent297*e=&gt297.event[at];e->kind=kind;e->parent=gt297.depth?gt297.stack[gt297.depth-1]:UINT_MAX;
  e->lane=gt297.lane;e->draw=gt297.draw;e->vertices=gt297.vertices;e->program=gt297.program;e->key=gt297.key;e->flags=gt297.flags;e->detail=detail;e->end=UINT_MAX;
  e->begin=gt297_stamp(c);gt297.stack[gt297.depth++]=at;gt297.kinds[kind]++;
 }else{
  if(!gt297.depth){gt297_invalid(c,"event_underflow",E_FAIL);goto done;}
  GTEvent297*e=&gt297.event[gt297.stack[gt297.depth-1]];
  if(e->kind!=kind){gt297_invalid(c,"event_order",E_FAIL);goto done;}
  e->end=gt297_stamp(c);gt297.depth--;
 }
done:gt297_restore(&g);
}
static void gt297_end(ID3D11DeviceContext*c){
 if(!gt297.scope||gt297.state!=GT297_OPEN)return;GTGuard297 g;gt297_save(&g);
 if(gt297.depth||gt297.kinds[GT297_MATERIAL]!=2*gt297.count||gt297.kinds[GT297_INIT]!=2||gt297.kinds[GT297_BOUNDARY]!=2*(gt297.count-1)||gt297.kinds[GT297_FINAL]!=2||gt297.kinds[GT297_PACK294]!=((gt297.features&8)?2u:0u)||((gt297.features&1)&&gt297.kinds[GT297_DEPTH272]<2*(gt297.count-1))){gt297_invalid(c,"incomplete_coverage",E_FAIL);goto done;}
 gt297_stamp(c);gt297_close(c);if(gt297.state==GT297_OPEN)gt297.state=GT297_ISSUED;gt297.cpu[1]=gt297_qpc();
done:gt297_restore(&g);
}
/* CPU index2+2*i=Map start,3+2*i=Map end;10=owned output copied.
 * Guest interleaving/publication occurs later in packets236, not here. */
static void gt297_cpu(unsigned index){if(!gt297.scope||gt297.state!=GT297_ISSUED||index<2||index>10)return;GTGuard297 g;gt297_save(&g);gt297.cpu[index]=gt297_qpc();gt297_restore(&g);}
static void gt297_finish(ID3D11DeviceContext*c,int ok){if(!gt297.scope)return;GTGuard297 g;gt297_save(&g);if(!ok||gt297.state==GT297_OPEN)gt297_invalid(c,ok?"missing_end":"render_aborted",E_FAIL);gt297.scope=0;gt297_restore(&g);}
static const char*gt297_name(unsigned kind){static const char*names[]={"none","initial","material","boundary","depth130","depth256","depth268","depth272","pack294","final"};return kind<GT297_KIND_COUNT?names[kind]:"invalid";}
static void gt297_report(ID3D11DeviceContext*c){
 if(!gt297.attempt||gt297.printed||gt297.scope)return;GTGuard297 guard;gt297_save(&guard);
 if(gt297.state==GT297_ISSUED){
  gt297.polls++;HRESULT hr=ID3D11DeviceContext_GetData(c,(ID3D11Asynchronous*)gt297.disjoint,&gt297.clock,sizeof gt297.clock,D3D11_ASYNC_GETDATA_DONOTFLUSH);
  if(hr==S_OK){if(gt297.clock.Disjoint||!gt297.clock.Frequency){gt297.state=GT297_INVALID;gt297.cause=gt297.clock.Disjoint?"disjoint":"zero_frequency";}
   else{for(unsigned i=0;i<gt297.next;i++){hr=ID3D11DeviceContext_GetData(c,(ID3D11Asynchronous*)gt297.stamp[i],&gt297.gpu[i],sizeof gt297.gpu[i],D3D11_ASYNC_GETDATA_DONOTFLUSH);if(hr!=S_OK)break;}
    if(hr==S_OK){gt297.state=GT297_READY;for(unsigned i=1;i<gt297.next;i++)if(gt297.gpu[i]<gt297.gpu[i-1]){gt297.state=GT297_INVALID;gt297.cause="timestamp_order";break;}}
   }
  }
  if(gt297.state==GT297_ISSUED){if(hr!=S_FALSE){gt297.state=GT297_INVALID;gt297.cause="getdata_failed";gt297.error=hr;}else if(gt297.polls>=GT297_POLLS){gt297.state=GT297_UNAVAILABLE;gt297.cause="poll_limit";}}
 }
 if(gt297.state==GT297_ISSUED)goto done;
 if(gt297.state!=GT297_READY&&gt297.state!=GT297_INVALID&&gt297.state!=GT297_UNAVAILABLE)goto done;
 fprintf(stderr,"[GPU297] status=%s cause=%s hr=%08lX requested=%llu requested_min_draws=%u batch=%llu thread=%u draws=%u features=%u events=%u stamps=%u polls=%u qpc_calls=%llu timeline_not_shader_only=1 no_extra_wait=1\n",gt297.state==GT297_READY?"valid":gt297.state==GT297_INVALID?"invalid":"unavailable",gt297.cause?gt297.cause:"none",(unsigned long)gt297.error,(unsigned long long)gt297.requested,gt297.minimum,(unsigned long long)gt297.ordinal,gt297.thread,gt297.count,gt297.features,gt297.events,gt297.next,gt297.polls,(unsigned long long)gt297.qpc_calls);
 if(gt297.state==GT297_READY){
  uint64_t sums[GT297_KIND_COUNT]={0},outer=0;double ms=1000.0/(double)gt297.clock.Frequency;
  for(unsigned i=0;i<gt297.events;i++){GTEvent297*e=&gt297.event[i];uint64_t ticks=gt297.gpu[e->end]-gt297.gpu[e->begin];sums[e->kind]+=ticks;if(e->parent==UINT_MAX)outer+=ticks;
   fprintf(stderr,"[GPU297-EVENT] batch=%llu id=%u kind=%s parent=%d lane=%u draw=%u vertices=%u program=%08X key=%08X flags=%u detail=%u begin=%llu end=%llu ms=%.6f\n",(unsigned long long)gt297.ordinal,i,gt297_name(e->kind),e->parent==UINT_MAX?-1:(int)e->parent,e->lane,e->draw,e->vertices,e->program,e->key,e->flags,e->detail,(unsigned long long)gt297.gpu[e->begin],(unsigned long long)gt297.gpu[e->end],ticks*ms);
  }
  uint64_t total=gt297.gpu[gt297.next-1]-gt297.gpu[0];fprintf(stderr,"[GPU297-SUM] batch=%llu gpu_hz=%llu total_ms=%.6f unclassified_ms=%.6f",(unsigned long long)gt297.ordinal,(unsigned long long)gt297.clock.Frequency,total*ms,(total>=outer?total-outer:0)*ms);
  for(unsigned i=1;i<GT297_KIND_COUNT;i++)fprintf(stderr," %s_ms=%.6f %s_count=%u",gt297_name(i),sums[i]*ms,gt297_name(i),gt297.kinds[i]);
  fprintf(stderr," kernels_are_nested_subsets=1 gaps_include_submission=1 qpc_hz=%llu cpu_raw=",(unsigned long long)gt297.qpc_hz);for(unsigned i=0;i<11;i++)fprintf(stderr,"%s%llu",i?"/":"",(unsigned long long)gt297.cpu[i]);fputc('\n',stderr);
 }
 gt297.printed=1;
done:gt297_restore(&guard);
}
#else
#define gt297_begin(d,c,n,k,f) ((void)0)
#define gt297_select(l,i,n,p,f,k,s) ((void)0)
#define gt297_op(c,k,e,d) ((void)0)
#define gt297_end(c) ((void)0)
#define gt297_cpu(i) ((void)0)
#define gt297_finish(c,ok) ((void)0)
#define gt297_report(c) ((void)0)
#endif
#endif
