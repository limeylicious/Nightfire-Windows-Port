/* Private, bounded immediate-context observation; no rendering/sync changes.
 * Include once in nightfire_hardware.c. All calls stay on its context owner.
 * begin: inside !pending AFTER texture/target/surface setup, before color upload.
 * stamp(1): after depth upload/initial clear; stamp(2): before readback copies;
 * stamp(3): after both copies. end closes disjoint BEFORE existing read Maps.
 * CPU stages remain active through those Maps; finish_cpu after pack/Unmap.
 * Call abandon on any begin/sync failure after a sample might have started.
 * report only at ordinary existing report boundaries; never add a Flush/Map.
 * Fixed lifetime query pool: <=20 attempts, one per frame at frame>=100,
 * rotating desired pending-interval ordinal 0/1/2. No ordinal substitution.
 * GPU spans include possible CPU-submission starvation, not pure GPU busy time.
 */
#ifndef NIGHTFIRE_GPU_TIMING84_H
#define NIGHTFIRE_GPU_TIMING84_H
#include <stdint.h>
#include <stdio.h>

enum NFGpuTime84Stage {
 NF_GPU_TIME84_COLOR_UPLOAD=0, NF_GPU_TIME84_DEPTH_SCAN,
 NF_GPU_TIME84_DEPTH_MAP_WRITE, NF_GPU_TIME84_DEPTH_UNPACK,
 NF_GPU_TIME84_DEPTH_UPLOAD_COPY, NF_GPU_TIME84_COLOR_MAP_READ,
 NF_GPU_TIME84_COLOR_ROW_COPY, NF_GPU_TIME84_DEPTH_MAP_READ,
 NF_GPU_TIME84_DEPTH_PACK, NF_GPU_TIME84_STAGE_COUNT
};

#ifdef NIGHTFIRE_GPU_TIMING_DIAGNOSTIC
#include <windows.h>
#include <d3d11.h>
#include <string.h>
#include <stdlib.h>
enum { NF84_LIMIT=20, NF84_POLL_LIMIT=8 };
enum { NF84_OPEN=1,NF84_ISSUED,NF84_READY,NF84_INVALID,NF84_UNAVAILABLE };
enum { NF84_OK=0,NF84_CREATE_FAILED,NF84_ABANDONED,NF84_STAMP_ORDER,
       NF84_GETDATA_FAILED,NF84_DISJOINT,NF84_BAD_FREQUENCY,NF84_BAD_ORDER,
       NF84_POLL_EXHAUSTED };
typedef struct NFGpuTime84Sample {
 ID3D11Query *disjoint,*stamp[4];
 D3D11_QUERY_DATA_TIMESTAMP_DISJOINT clock;
 UINT64 gpu[4];
 uint64_t cpu[NF_GPU_TIME84_STAGE_COUNT],cpu_start,cpu_end;
 unsigned frame,ordinal,state,cause,stamp_mask,query_open,cpu_done;
 unsigned polls,pending_reported,printed;
 HRESULT error;
} NFGpuTime84Sample;
static NFGpuTime84Sample nf84_samples[NF84_LIMIT];
static unsigned nf84_count,nf84_frame,nf84_ordinal,nf84_frame_seen;
static unsigned nf84_last_sample_frame,nf84_sample_frame_seen,nf84_disabled;
static int nf84_current=-1;
static uint64_t nf84_qpc_frequency;

static uint64_t nf84_clock(void){
 LARGE_INTEGER t;if(!QueryPerformanceCounter(&t) || t.QuadPart<=0)return 0;
 return (uint64_t)t.QuadPart;
}
static void nf84_close(ID3D11DeviceContext *ctx,NFGpuTime84Sample *s){
 if(s->query_open && ctx){ctx->lpVtbl->End(ctx,(ID3D11Asynchronous*)s->disjoint);s->query_open=0;}
}
static void nf_gpu_time84_abandon(ID3D11DeviceContext *ctx){
 if(nf84_current<0)return;
 NFGpuTime84Sample *s=&nf84_samples[nf84_current];nf84_close(ctx,s);
 s->state=NF84_INVALID;if(!s->cause)s->cause=NF84_ABANDONED;
 s->cpu_end=nf84_clock();s->cpu_done=1;nf84_current=-1;
}
/* CP103: optional one-shot gate trigger. Query count/lifetime and all GPU
 * boundaries remain unchanged; absent variable retains original frame100 rule. */
static int nf84_trigger_ready(unsigned frame){
 static int initialized,armed;static const char *path;
 static unsigned seen,last_frame;
 if(!initialized){initialized=1;path=getenv("NIGHTFIRE_GPU_TIMING_TRIGGER");if(!path)armed=1;}
 if(armed)return 1;
 if(seen && frame==last_frame)return 0;
 seen=1;last_frame=frame;
 unsigned token=0;FILE *f=fopen(path,"r");
 if(f){if(fscanf(f,"%u",&token)!=1)token=0;fclose(f);}
 if(token){armed=1;fprintf(stderr,"[GPU-TIME103] trigger_frame=%u token=%u\n",frame,token);}
 return armed;
}
static void nf_gpu_time84_begin(ID3D11Device *dev,ID3D11DeviceContext *ctx,unsigned frame){
 /* A duplicate begin is invalid instrumentation, never a nested query. */
 if(nf84_current>=0)nf_gpu_time84_abandon(ctx);
 unsigned ordinal;
 if(!nf84_frame_seen || frame!=nf84_frame){nf84_frame_seen=1;nf84_frame=frame;nf84_ordinal=0;}
 ordinal=nf84_ordinal++;
 if(!dev || !ctx || nf84_disabled || nf84_count>=NF84_LIMIT || frame<100 || !nf84_trigger_ready(frame) ||
    (nf84_sample_frame_seen && frame==nf84_last_sample_frame) || ordinal!=nf84_count%3)return;
 unsigned index=nf84_count++;NFGpuTime84Sample *s=&nf84_samples[index];
 memset(s,0,sizeof *s);s->frame=frame;s->ordinal=ordinal;s->state=NF84_OPEN;
 nf84_last_sample_frame=frame;nf84_sample_frame_seen=1;
 if(!nf84_qpc_frequency){LARGE_INTEGER f;if(QueryPerformanceFrequency(&f) && f.QuadPart>0)nf84_qpc_frequency=(uint64_t)f.QuadPart;}
 /* Allocation precedes T0 and all measured CPU upload stages. */
 D3D11_QUERY_DESC d={D3D11_QUERY_TIMESTAMP_DISJOINT,0};
 HRESULT hr=dev->lpVtbl->CreateQuery(dev,&d,&s->disjoint);
 for(unsigned i=0;SUCCEEDED(hr) && i<4;i++){
  d.Query=D3D11_QUERY_TIMESTAMP;hr=dev->lpVtbl->CreateQuery(dev,&d,&s->stamp[i]);
 }
 nf84_current=(int)index;s->cpu_start=nf84_clock();
 if(FAILED(hr)){
  s->state=NF84_INVALID;s->cause=NF84_CREATE_FAILED;s->error=hr;nf84_disabled=1;
  /* None has been issued, so release the incomplete allocation immediately. */
  for(unsigned i=0;i<4;i++)if(s->stamp[i]){s->stamp[i]->lpVtbl->Release(s->stamp[i]);s->stamp[i]=NULL;}
  if(s->disjoint){s->disjoint->lpVtbl->Release(s->disjoint);s->disjoint=NULL;}
  return;
 }
 ctx->lpVtbl->Begin(ctx,(ID3D11Asynchronous*)s->disjoint);s->query_open=1;
 ctx->lpVtbl->End(ctx,(ID3D11Asynchronous*)s->stamp[0]);s->stamp_mask=1;
}
static void nf_gpu_time84_stamp(ID3D11DeviceContext *ctx,unsigned point){
 if(nf84_current<0)return;NFGpuTime84Sample *s=&nf84_samples[nf84_current];
 if(s->state!=NF84_OPEN)return;
 if(!ctx || point<1 || point>3 || s->stamp_mask!=((1u<<point)-1)){
  nf84_close(ctx,s);s->state=NF84_INVALID;s->cause=NF84_STAMP_ORDER;return;
 }
 ctx->lpVtbl->End(ctx,(ID3D11Asynchronous*)s->stamp[point]);s->stamp_mask|=1u<<point;
}
static void nf_gpu_time84_end(ID3D11DeviceContext *ctx){
 if(nf84_current<0)return;NFGpuTime84Sample *s=&nf84_samples[nf84_current];
 if(s->state!=NF84_OPEN)return;
 nf84_close(ctx,s);
 if(!ctx || s->stamp_mask!=15){s->state=NF84_INVALID;s->cause=NF84_STAMP_ORDER;}
 else s->state=NF84_ISSUED;
}
static uint64_t nf_gpu_time84_start(void){return nf84_current>=0?nf84_clock():0;}
static void nf_gpu_time84_cpu(unsigned stage,uint64_t start){
 if(nf84_current<0 || stage>=NF_GPU_TIME84_STAGE_COUNT || !start)return;
 uint64_t end=nf84_clock();if(end>=start)nf84_samples[nf84_current].cpu[stage]+=end-start;
}
static void nf_gpu_time84_finish_cpu(void){
 if(nf84_current<0)return;NFGpuTime84Sample *s=&nf84_samples[nf84_current];
 /* Missing end must not strand a Begin; integration must abandon(ctx) first. */
 if(s->query_open)return;
 s->cpu_end=nf84_clock();s->cpu_done=1;nf84_current=-1;
}
static const char *nf84_cause(unsigned cause){
 static const char *names[]={"none","create_failed","abandoned","stamp_order",
  "getdata_failed","disjoint","bad_frequency","bad_timestamp_order","poll_exhausted"};
 return cause<sizeof names/sizeof names[0]?names[cause]:"unknown";
}
static void nf84_poll(ID3D11DeviceContext *ctx,NFGpuTime84Sample *s){
 if(s->state!=NF84_ISSUED || !s->cpu_done || !ctx)return;
 s->polls++;
 HRESULT hr=ctx->lpVtbl->GetData(ctx,(ID3D11Asynchronous*)s->disjoint,&s->clock,sizeof s->clock,D3D11_ASYNC_GETDATA_DONOTFLUSH);
 if(hr==S_OK){
  if(s->clock.Disjoint){s->state=NF84_INVALID;s->cause=NF84_DISJOINT;return;}
  if(!s->clock.Frequency){s->state=NF84_INVALID;s->cause=NF84_BAD_FREQUENCY;return;}
  for(unsigned i=0;i<4;i++){
   hr=ctx->lpVtbl->GetData(ctx,(ID3D11Asynchronous*)s->stamp[i],&s->gpu[i],sizeof s->gpu[i],D3D11_ASYNC_GETDATA_DONOTFLUSH);
   if(hr!=S_OK)break;
  }
  if(hr==S_OK){
   for(unsigned i=1;i<4;i++)if(s->gpu[i]<s->gpu[i-1]){
    s->state=NF84_INVALID;s->cause=NF84_BAD_ORDER;return;
   }
   s->state=NF84_READY;return;
  }
 }
 if(hr!=S_FALSE){s->state=NF84_INVALID;s->cause=NF84_GETDATA_FAILED;s->error=hr;}
 else if(s->polls>=NF84_POLL_LIMIT){s->state=NF84_UNAVAILABLE;s->cause=NF84_POLL_EXHAUSTED;s->error=hr;}
}
static void nf84_print(FILE *stream,unsigned index,NFGpuTime84Sample *s){
 static const char *stages[NF_GPU_TIME84_STAGE_COUNT]={"color_upload","depth_scan","depth_map_write","depth_unpack",
  "depth_upload_copy","color_map_read","color_row_copy","depth_map_read","depth_pack"};
 const char *state=s->state==NF84_READY?"valid":s->state==NF84_UNAVAILABLE?"unavailable":"invalid";
 fprintf(stream,"[GPU-TIMING84] id=%u frame=%u ordinal=%u status=%s cause=%s hr=%08lX stamps=%X polls=%u",
  index,s->frame,s->ordinal,state,nf84_cause(s->cause),(unsigned long)s->error,s->stamp_mask,s->polls);
 if(s->state==NF84_READY){
  double k=1000.0/(double)s->clock.Frequency;
  fprintf(stream," disjoint=0 gpu_hz=%llu gpu_upload_ms=%.6f gpu_render_span_ms=%.6f gpu_copies_ms=%.6f gpu_total_ms=%.6f gpu_ticks=%llu/%llu/%llu/%llu",
   (unsigned long long)s->clock.Frequency,(s->gpu[1]-s->gpu[0])*k,(s->gpu[2]-s->gpu[1])*k,
   (s->gpu[3]-s->gpu[2])*k,(s->gpu[3]-s->gpu[0])*k,
   (unsigned long long)s->gpu[0],(unsigned long long)s->gpu[1],(unsigned long long)s->gpu[2],(unsigned long long)s->gpu[3]);
 }else if(s->cause==NF84_DISJOINT)fprintf(stream," disjoint=1");
 unsigned cpu_valid=nf84_qpc_frequency && s->cpu_start && s->cpu_end>=s->cpu_start;
 fprintf(stream," cpu_valid=%u qpc_hz=%llu",cpu_valid,(unsigned long long)nf84_qpc_frequency);
 if(cpu_valid)fprintf(stream," cpu_interval_ms=%.6f",1000.0*(double)(s->cpu_end-s->cpu_start)/(double)nf84_qpc_frequency);
 for(unsigned i=0;i<NF_GPU_TIME84_STAGE_COUNT;i++){
  fprintf(stream," cpu_%s_ticks=%llu",stages[i],(unsigned long long)s->cpu[i]);
  if(cpu_valid)fprintf(stream," cpu_%s_ms=%.6f",stages[i],1000.0*(double)s->cpu[i]/(double)nf84_qpc_frequency);
 }
 fputc('\n',stream);s->printed=1;
}
static void nf_gpu_time84_report(ID3D11DeviceContext *ctx,FILE *stream){
 if(!stream)return;
 unsigned ready=0,invalid=0,unavailable=0,waiting=0,open=0;
 for(unsigned i=0;i<nf84_count;i++){
  NFGpuTime84Sample *s=&nf84_samples[i];nf84_poll(ctx,s);
  if(s->state==NF84_READY)ready++;
  else if(s->state==NF84_INVALID)invalid++;
  else if(s->state==NF84_UNAVAILABLE)unavailable++;
  else if(s->state==NF84_ISSUED)waiting++;
  else open++;
  if(s->cpu_done && !s->printed && (s->state==NF84_READY || s->state==NF84_INVALID || s->state==NF84_UNAVAILABLE))nf84_print(stream,i,s);
  else if(s->state==NF84_ISSUED && s->cpu_done && !s->pending_reported){
   fprintf(stream,"[GPU-TIMING84] id=%u frame=%u ordinal=%u status=not_ready polls=%u\n",i,s->frame,s->ordinal,s->polls);
   s->pending_reported=1;
  }
 }
 fprintf(stream,"[GPU-TIMING84-SUMMARY] attempts=%u valid=%u invalid=%u unavailable=%u pending=%u open=%u disabled=%u limit=%u polls_per_slot_limit=%u\n",
  nf84_count,ready,invalid,unavailable,waiting,open,nf84_disabled,NF84_LIMIT,NF84_POLL_LIMIT);
}
#else
static __inline void nf_gpu_time84_begin(void *dev,void *ctx,unsigned frame){(void)dev;(void)ctx;(void)frame;}
static __inline void nf_gpu_time84_stamp(void *ctx,unsigned point){(void)ctx;(void)point;}
static __inline void nf_gpu_time84_end(void *ctx){(void)ctx;}
static __inline void nf_gpu_time84_finish_cpu(void){}
static __inline void nf_gpu_time84_abandon(void *ctx){(void)ctx;}
static __inline uint64_t nf_gpu_time84_start(void){return 0;}
static __inline void nf_gpu_time84_cpu(unsigned stage,uint64_t start){(void)stage;(void)start;}
static __inline void nf_gpu_time84_report(void *ctx,FILE *stream){(void)ctx;(void)stream;}
#endif
#endif
