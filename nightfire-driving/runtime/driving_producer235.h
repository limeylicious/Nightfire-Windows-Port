/* Bounded opt-in CPU-side producer history. Guest registers/memory are never
 * written. Wrappers call the unchanged generated function exactly once.
 * Thread-local history avoids mixing different descriptor transactions. */
#ifndef DRIVING_PRODUCER235_H
#define DRIVING_PRODUCER235_H
#include <string.h>
#include <stdlib.h>
typedef struct {
 uint32_t sequence,function,after,tid,registers[8],valid;
 uint32_t globals[12],descriptor_va,payload_va,selected_va,index_va;
 uint8_t stack[64],descriptor[128],payload[16],selected[80],indices[1024];
} DrivingProducerRecord235;
static __declspec(thread) DrivingProducerRecord235 history235[256];
static __declspec(thread) uint32_t history_count235;
static volatile LONG producer_done235,producer_enabled235=-1;
static int producer_active235(void){
 LONG v=InterlockedCompareExchange(&producer_enabled235,-1,-1);
 if(v<0){const char*p=getenv("DRIVING_PRODUCER235");v=p&&!strcmp(p,"1");InterlockedCompareExchange(&producer_enabled235,v,-1);}
 return v&&!InterlockedCompareExchange(&producer_done235,0,0);
}
static int producer_read235(uint32_t va,void*out,size_t n){
 uintptr_t base=(uintptr_t)g_xbox_mem_offset,a=base+va;SIZE_T got=0;
 if(!va||!n||n>(uint64_t)UINT32_MAX+1-va||a<base||n>UINTPTR_MAX-a)return 0;
 return ReadProcessMemory(GetCurrentProcess(),(const void*)a,out,n,&got)&&got==n;
}
static uint32_t producer_word235(const uint8_t *p){uint32_t n;memcpy(&n,p,4);return n;}
static void producer_record235(uint32_t function,unsigned after){
 DWORD error=GetLastError();
 if(producer_active235()){
  DrivingProducerRecord235*r=&history235[history_count235&255];memset(r,0,sizeof*r);
  r->sequence=history_count235++;r->function=function;r->after=after;r->tid=GetCurrentThreadId();
  uint32_t regs[]={g_eax,g_ebx,g_ecx,g_edx,g_esi,g_edi,g_esp,g_ebp};memcpy(r->registers,regs,sizeof regs);
  static const uint32_t addresses[]={0x2401bc,0x2401c0,0x2401c4,0x2401c8,0x2401cc,0x2401d0,0x23ff60,0x240470,0x240500,0x240504,0x2400f8,0x240100};
  for(unsigned i=0;i<12;i++)if(producer_read235(addresses[i],&r->globals[i],4))r->valid|=1u<<i;
  if(producer_read235(g_esp,r->stack,sizeof r->stack))r->valid|=1u<<12;
  r->descriptor_va=r->globals[2];r->payload_va=r->globals[1];
  if(function==0xf0fc0&&!after){producer_read235(g_ecx,&r->descriptor_va,4);r->payload_va=producer_word235(r->stack+4);}
  if(producer_read235(r->descriptor_va,r->descriptor,sizeof r->descriptor))r->valid|=1u<<13;
  if(producer_read235(r->payload_va,r->payload,sizeof r->payload))r->valid|=1u<<14;
  if((r->valid&0x6040)==0x6040){r->selected_va=producer_word235(r->payload+4)+producer_word235(r->descriptor+4)*r->globals[6];
   if(producer_read235(r->selected_va,r->selected,sizeof r->selected))r->valid|=1u<<15;}
  r->index_va=r->globals[8];
  if(r->globals[3]==512&&producer_read235(r->index_va,r->indices,sizeof r->indices))r->valid|=1u<<16;
 }
 SetLastError(error);
}
void driving_producer235_request(void){
 DWORD error=GetLastError();
 if(producer_active235()&&!InterlockedCompareExchange(&producer_done235,1,0)){
  const char*dir=getenv("DRIVING_CAPTURE_DIR");char path[2048];int okay=0;unsigned n=history_count235<256?history_count235:256;
  if(dir){int len=snprintf(path,sizeof path,"%s/producer235-history.bin",dir);
   if(len>0&&(size_t)len<sizeof path){FILE*f=fopen(path,"wb");if(f){uint32_t h[]={0x35333250,sizeof(DrivingProducerRecord235),n,history_count235};okay=fwrite(h,sizeof h,1,f)==1;
    for(unsigned i=history_count235-n;okay&&i<history_count235;i++)okay=fwrite(&history235[i&255],sizeof history235[0],1,f)==1;
    if(fclose(f))okay=0;}}}
  fprintf(stderr,"[PRODUCER235] first profile7/512-index draw: records=%u bytes_each=%zu tid=%lu saved=%d CPU history only\n",n,sizeof(DrivingProducerRecord235),GetCurrentThreadId(),okay);
 }
 SetLastError(error);
}
#define PRODUCER_WRAPPER235(name,va) extern void name(void); static void traced_##name(void){producer_record235(va,0);name();producer_record235(va,1);}
PRODUCER_WRAPPER235(sub_000F0FC0,0x000F0FC0)
PRODUCER_WRAPPER235(sub_000F6030,0x000F6030)
PRODUCER_WRAPPER235(sub_000F6270,0x000F6270)
PRODUCER_WRAPPER235(sub_000F6320,0x000F6320)
PRODUCER_WRAPPER235(sub_000F6600,0x000F6600)
#undef PRODUCER_WRAPPER235
#endif
