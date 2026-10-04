/* Private-pair-only, one lifetime GPU timeline sample. No waits or flushes. */
#ifndef NIGHTFIRE_GPU_SAMPLE254_H
#define NIGHTFIRE_GPU_SAMPLE254_H
#ifdef NIGHTFIRE_GPU_SAMPLE254
#include <errno.h>
enum { GT254_STAMPS=22,GT254_POLLS=8,GT254_OPEN=1,GT254_ISSUED,
 GT254_READY,GT254_INVALID,GT254_UNAVAILABLE };
static struct {
 ID3D11Query *disjoint,*stamp[GT254_STAMPS];
 UINT64 gpu[GT254_STAMPS];D3D11_QUERY_DATA_TIMESTAMP_DISJOINT clock;
 unsigned attempt,state,open,scope,next,lane,draw_arm,boundary_arm,polls,printed;
 unsigned draws,vertices[2];uint32_t program[2],flags[2];
 uint64_t ordinal,requested;const char *cause;HRESULT error;
} gt254;
static int gt254_setting=-1;
static int gt254_enabled(void){
 if(gt254_setting<0){
  const char*v=getenv("DRIVING_GPU_SAMPLE254");gt254_setting=v&&!strcmp(v,"1");
  gt254.requested=1024;
  if(gt254_setting){const char*n=getenv("DRIVING_GPU_SAMPLE254_BATCH");
   if(n&&*n){char*end;errno=0;unsigned long x=strtoul(n,&end,10);
    if(!errno&&!*end&&n[0]>='0'&&n[0]<='9'&&x>=1&&x<=1000000)gt254.requested=x;
   }
   fprintf(stderr,"[GPU-SAMPLE254] enabled=1 requested_batch=%llu lifetime_attempts=1 selected_draw=1 boundary_before_draw=1 max_polls=8\n",(unsigned long long)gt254.requested);
  }
 }
 return gt254_setting;
}
static void gt254_close(ID3D11DeviceContext*c){
 if(gt254.open){ID3D11DeviceContext_End(c,(ID3D11Asynchronous*)gt254.disjoint);gt254.open=0;}
}
static void gt254_invalid(ID3D11DeviceContext*c,const char*cause,HRESULT hr){
 gt254_close(c);gt254.state=GT254_INVALID;gt254.cause=cause;gt254.error=hr;
 gt254.draw_arm=gt254.boundary_arm=0;
}
static void gt254_stamp(ID3D11DeviceContext*c,unsigned index){
 if(!gt254.scope||gt254.state!=GT254_OPEN)return;
 if(index!=gt254.next||index>=GT254_STAMPS){gt254_invalid(c,"stamp_order",E_FAIL);return;}
 ID3D11DeviceContext_End(c,(ID3D11Asynchronous*)gt254.stamp[index]);gt254.next++;
}
static void gt254_begin(ID3D11Device*d,ID3D11DeviceContext*c,uint64_t completed,unsigned count){
 if(!gt254_enabled()||gt254.attempt||completed+1<gt254.requested||count<2)return;
 gt254.attempt=1;gt254.ordinal=completed+1;gt254.draws=count;gt254.cause="none";
 D3D11_QUERY_DESC desc={D3D11_QUERY_TIMESTAMP_DISJOINT,0};
 HRESULT hr=ID3D11Device_CreateQuery(d,&desc,&gt254.disjoint);
 desc.Query=D3D11_QUERY_TIMESTAMP;
 for(unsigned i=0;SUCCEEDED(hr)&&i<GT254_STAMPS;i++)hr=ID3D11Device_CreateQuery(d,&desc,&gt254.stamp[i]);
 if(FAILED(hr)){
  for(unsigned i=0;i<GT254_STAMPS;i++)if(gt254.stamp[i]){ID3D11Query_Release(gt254.stamp[i]);gt254.stamp[i]=NULL;}
  if(gt254.disjoint){ID3D11Query_Release(gt254.disjoint);gt254.disjoint=NULL;}
  gt254.state=GT254_UNAVAILABLE;gt254.cause="create_failed";gt254.error=hr;return;
 }
 gt254.state=GT254_OPEN;gt254.scope=1;
 ID3D11DeviceContext_Begin(c,(ID3D11Asynchronous*)gt254.disjoint);gt254.open=1;
 gt254_stamp(c,0);
}
static void gt254_select(unsigned lane,unsigned draw,unsigned vertices,uint32_t program,uint32_t flags){
 if(!gt254.scope||gt254.state!=GT254_OPEN)return;
 gt254.lane=lane;gt254.draw_arm=gt254.boundary_arm=draw==1;
 if(draw==1){gt254.vertices[lane]=vertices;gt254.program[lane]=program;gt254.flags[lane]=flags;}
}
static void gt254_init(ID3D11DeviceContext*c,unsigned lane,unsigned end){gt254_stamp(c,1+lane*10+end);}
static void gt254_boundary(ID3D11DeviceContext*c,unsigned point){
 if(gt254.boundary_arm){gt254_stamp(c,3+gt254.lane*10+point);if(point==3)gt254.boundary_arm=0;}
}
static void gt254_dispatch(ID3D11DeviceContext*c,UINT x,UINT y,UINT z){
 gt254_boundary(c,1);ID3D11DeviceContext_Dispatch(c,x,y,z);gt254_boundary(c,2);
}
static void gt254_draw(ID3D11DeviceContext*c,unsigned end){
 if(gt254.draw_arm){gt254_stamp(c,7+gt254.lane*10+end);if(end)gt254.draw_arm=0;}
}
static void gt254_copies(ID3D11DeviceContext*c,unsigned lane,unsigned end){gt254_stamp(c,9+lane*10+end);}
static void gt254_end(ID3D11DeviceContext*c){
 if(!gt254.scope||gt254.state!=GT254_OPEN)return;
 gt254.draw_arm=gt254.boundary_arm=0;gt254_stamp(c,21);gt254_close(c);
 if(gt254.state==GT254_OPEN)gt254.state=GT254_ISSUED;
}
static void gt254_finish(ID3D11DeviceContext*c,int ok){
 if(!gt254.scope)return;
 if(!ok||gt254.state==GT254_OPEN)gt254_invalid(c,ok?"missing_end":"render_aborted",E_FAIL);
 gt254.scope=gt254.draw_arm=gt254.boundary_arm=0;
}
static double gt254_ms(unsigned a,unsigned b){return (gt254.gpu[b]-gt254.gpu[a])*1000.0/(double)gt254.clock.Frequency;}
static void gt254_report(ID3D11DeviceContext*c){
 if(!gt254.attempt||gt254.printed||gt254.scope)return;
 if(gt254.state==GT254_ISSUED){
  gt254.polls++;HRESULT hr=ID3D11DeviceContext_GetData(c,(ID3D11Asynchronous*)gt254.disjoint,&gt254.clock,sizeof gt254.clock,D3D11_ASYNC_GETDATA_DONOTFLUSH);
  if(hr==S_OK){
   if(gt254.clock.Disjoint){gt254.state=GT254_INVALID;gt254.cause="disjoint";}
   else if(!gt254.clock.Frequency){gt254.state=GT254_INVALID;gt254.cause="zero_frequency";}
   else{
    for(unsigned i=0;i<GT254_STAMPS;i++){
     hr=ID3D11DeviceContext_GetData(c,(ID3D11Asynchronous*)gt254.stamp[i],&gt254.gpu[i],sizeof gt254.gpu[i],D3D11_ASYNC_GETDATA_DONOTFLUSH);
     if(hr!=S_OK)break;
    }
    if(hr==S_OK){gt254.state=GT254_READY;
     for(unsigned i=1;i<GT254_STAMPS;i++)if(gt254.gpu[i]<gt254.gpu[i-1]){gt254.state=GT254_INVALID;gt254.cause="timestamp_order";break;}
    }
   }
  }
  if(gt254.state==GT254_ISSUED){
   if(hr!=S_FALSE){gt254.state=GT254_INVALID;gt254.cause="getdata_failed";gt254.error=hr;}
   else if(gt254.polls>=GT254_POLLS){gt254.state=GT254_UNAVAILABLE;gt254.cause="poll_limit";}
  }
 }
 if(gt254.state==GT254_ISSUED)return;
 if(gt254.state!=GT254_READY&&gt254.state!=GT254_INVALID&&gt254.state!=GT254_UNAVAILABLE)return;
 fprintf(stderr,"[GPU-SAMPLE254] status=%s cause=%s hr=%08lX requested_batch=%llu actual_batch=%llu original_draws=%u selected_draw=1 polls=%u stamps=%u GPU-timeline-includes-submission-gaps=1 phase-samples-not-aggregate=1",
  gt254.state==GT254_READY?"valid":gt254.state==GT254_UNAVAILABLE?"unavailable":"invalid",gt254.cause,(unsigned long)gt254.error,
  (unsigned long long)gt254.requested,(unsigned long long)gt254.ordinal,gt254.draws,gt254.polls,gt254.next);
 if(gt254.state==GT254_READY){
  fprintf(stderr," frequency=%llu batch_ms=%.6f",(unsigned long long)gt254.clock.Frequency,gt254_ms(0,21));
  for(unsigned lane=0;lane<2;lane++){unsigned b=1+lane*10;
   fprintf(stderr," lane%u_vertices=%u program=%08X flags=%08X initial_begin_ms=%.6f selected_draw_ms=%.6f selected_boundary_ms=%.6f boundary_dispatch_ms=%.6f boundary_before_dispatch_ms=%.6f boundary_after_dispatch_ms=%.6f readback_copies_ms=%.6f",
    lane,gt254.vertices[lane],gt254.program[lane],gt254.flags[lane],gt254_ms(b,b+1),gt254_ms(b+6,b+7),gt254_ms(b+2,b+5),gt254_ms(b+3,b+4),gt254_ms(b+2,b+3),gt254_ms(b+4,b+5),gt254_ms(b+8,b+9));
  }
  fprintf(stderr," raw_ticks=");for(unsigned i=0;i<GT254_STAMPS;i++)fprintf(stderr,"%s%llu",i?"/":"",(unsigned long long)gt254.gpu[i]);
 }
 fputc('\n',stderr);gt254.printed=1;
}
#else
#define gt254_begin(d,c,n,k) ((void)0)
#define gt254_select(l,i,n,p,f) ((void)0)
#define gt254_init(c,l,e) ((void)0)
#define gt254_boundary(c,p) ((void)0)
#define gt254_draw(c,e) ((void)0)
#define gt254_copies(c,l,e) ((void)0)
#define gt254_end(c) ((void)0)
#define gt254_finish(c,ok) ((void)0)
#define gt254_report(c) ((void)0)
#endif
#endif
