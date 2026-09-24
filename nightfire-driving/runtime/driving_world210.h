/* Optional bounded owned scene capture. No draw is replaced. Captures physical
 * allocated RAM at the first indexed/array submission, before baseline execution,
 * plus CPU-side VP state and the remaining commands through END. Not a savegame. */
#ifndef DRIVING_WORLD210_H
#define DRIVING_WORLD210_H
#include "driving_contract216.h"
#include "driving_vertex218.h"
static FILE *commands210;
static unsigned records210,done210[3];
static int enabled210=-1,batch217,batch218,batch219,batch221,visible218,scenery218,attempted218;
/* Diagnostic selector only. Evaluate two original indices on private shader
 * state, with bounded original DMA reads. This changes no draw or guest data. */
static int world218_in_view(GPUObject143 *object,unsigned method,uint32_t value)
{
 if(method!=0x1800&&method!=0x1808)return 0;
 uint32_t ramht,allocated=xbox_ContiguousAllocatedBytes();DrivingSpan183 span;
 if(!read143(NULL,0xfd002210,&ramht))return 0;
 unsigned indices[2]={method==0x1800?value&65535:value,value>>16};
 unsigned count=method==0x1800?2:1;
 NFVertexProgram program=live201.program;
 for(unsigned i=0;i<count;i++){
  float in[16][4],out[16][4];memcpy(in,&live201.current,sizeof in);
  for(unsigned slot=0;slot<16;slot++){
   uint32_t fmt=object->state[(0x1760+slot*4)/4];unsigned n=(fmt>>4)&15,kind=fmt&15;
   if(!n)continue;
   if(n>4||(kind!=0&&kind!=2&&kind!=5&&kind!=6))return 0;
   unsigned size=kind==6?4:n*(kind==2?4:kind==5?2:1);
   uint64_t need=(uint64_t)indices[i]*(fmt>>8)+size;
   if(!need||need>0x08000000||!driving_span183(NULL,read143,ramht,object->state,object->written167,DRIVING_VERTEX183,slot,(uint32_t)need,allocated,&span))return 0;
   const uint8_t *src=mapped201(span.address,(size_t)need,0);
   if(!src||!driving_vertex218(src,(size_t)need,fmt,0,indices[i],in[slot]))return 0;
  }
  if(nf_vp_run(&program,in,out)&&out[0][3]>0&&out[0][0]>=0&&out[0][0]<640&&out[0][1]>=0&&out[0][1]<480&&out[0][2]>=0&&out[0][2]<=16777215){
   fprintf(stderr,"[WORLD218] visible-selector index=%u xyzw=%.9g,%.9g,%.9g,%.9g\n",indices[i],out[0][0],out[0][1],out[0][2],out[0][3]);return 1;
  }
 }
 return 0;
}
static void world210_write(const char *dir,unsigned family,const char *name,const void *data,size_t bytes)
{
 char path[2048];int n=snprintf(path,sizeof path,"%s/%s-%u-%s",dir,batch221?"world221":batch219?"world219":batch218?"world218":batch217?"world217":"world210",family,name);
 if(n<0||(size_t)n>=sizeof path)return;
 FILE *f=fopen(path,"wb");int ok=0;
 if(f){ok=fwrite(data,1,bytes,f)==bytes;if(fclose(f))ok=0;}
 fprintf(stderr,"[WORLD210] family=%u file=%s bytes=%zu ok=%d owned-data=1\n",family,name,bytes,ok);
}
static void world210(GPUObject143 *object,unsigned method,uint32_t value)
{
 if(enabled210<0){const char *v=getenv("DRIVING_WORLD_CAPTURE210"),*b=getenv("DRIVING_WORLD_CAPTURE_BATCH217"),*c=getenv("DRIVING_WORLD_CAPTURE_BATCH218"),*d=getenv("DRIVING_WORLD_CAPTURE_VISIBLE218"),*e=getenv("DRIVING_WORLD_CAPTURE_SCENERY218");batch218=c&&!strcmp(c,"1");visible218=d&&!strcmp(d,"1");scenery218=e&&!strcmp(e,"1");batch217=!batch218&&b&&!strcmp(b,"1");enabled210=batch218||batch217||(v&&!strcmp(v,"1"));}
 {static int once219;if(!once219){const char *v=getenv("DRIVING_WORLD_CAPTURE_BATCH219");once219=1;batch219=v&&!strcmp(v,"1");if(batch219){enabled210=1;batch217=batch218=0;}}}
 {static int once221;if(!once221){const char *v=getenv("DRIVING_WORLD_CAPTURE_BATCH221");once221=1;batch221=v&&!strcmp(v,"1");if(batch221){enabled210=1;batch217=batch218=batch219=0;}}}
 if(!enabled210)return;
 if(method==0x17fc&&value)attempted218=0;
 if(commands210){
  uint32_t record[2]={method,value};
  if(records210>=65536||fwrite(record,sizeof record,1,commands210)!=1){
   fclose(commands210);commands210=NULL;fprintf(stderr,"[WORLD210] commands truncated limit-or-IO\n");return;
  }
  records210++;
  if(method==0x17fc&&!value){int ok=fclose(commands210)==0;commands210=NULL;
   fprintf(stderr,"[WORLD210] END records=%u ok=%d\n",records210,ok);}
  return;
 }
 unsigned family=method==0x1810?1:0;
 if(batch221){
  /* Observe full52 strip/81/79 list submissions; no visibility shortcut or
   * renderer admission based on length. Preserve original topology. */
  unsigned primitive=object->state[0x17fc/4];
  if(object->state[0x208/4]!=0x1128 || (primitive!=5&&primitive!=6))return;
  unsigned length=0;
  for(unsigned i=0;i<136;i++){
   if(live201.program.valid[i]!=15)return;
   if(live201.program.code[i][3]&1){length=i+1;break;}
  }
  if(length!=52&&length!=81&&length!=79)return;
  family=length==52?0:length==81?1:2;
 }
 if(batch219){
  /* Three original multi-texture families in one startup. Observation only. */
  if(object->state[0x208/4]!=0x1128 || (object->state[0x17fc/4]!=5&&object->state[0x17fc/4]!=6))return; /*242: capture original lists and corrected strips; no draw admission. */
  unsigned length=0;
  for(unsigned i=0;i<136;i++){
   if(live201.program.valid[i]!=15)return;
   if(live201.program.code[i][3]&1){length=i+1;break;}
  }
  if(length!=48&&length!=53&&length!=56)return;
  family=length==48?0:length==53?1:2;
 }
 if(batch218){
  /* Diagnostic observation only: recurring packed-input single-texture shaders.
   * Length is a capture selector, never sufficient rendering admission. */
  if(object->state[0x208/4]!=0x1128 || object->state[0x17fc/4]!=5 || (!scenery218&&object->state[0x1e70/4]!=1))return;
  unsigned length=0;
  for(unsigned i=0;i<136;i++){
   if(live201.program.valid[i]!=15)return;
   if(live201.program.code[i][3]&1){length=i+1;break;}
  }
  if(scenery218){if(length!=81&&length!=79)return;family=length==79;}
  else {if(length!=55&&length!=62)return;family=length==62;}
 }
 if(batch217){
  /* Observe both next material families in one startup. No new rendering
   * admission: exact known shader, single-stage DXT1, two recorded mip chains. */
  if(object->state[0x208/4]!=0x1128 || object->state[0x17fc/4]!=5 || object->state[0x1e70/4]!=1)return;
  uint32_t fmt=object->state[0x1b04/4];
  if(fmt!=0x07850c29u&&fmt!=0x05530c29u)return;
  for(unsigned i=0;i<42;i++)if(live201.program.valid[i]!=15||memcmp(live201.program.code[i],code216[i],16))return;
  family=fmt==0x05530c29u;
 }
 if((method!=0x1800&&method!=0x1808&&method!=0x1810)||done210[family]||!scene_capture240(draws201[0],enabled201)
    ||!object->state[0x17fc/4]||object->state[0x1e94/4]!=6)return;
 if(batch218&&visible218){
  if(attempted218)return;attempted218=1;
  if(!world218_in_view(object,method,value))return;
 }
 /* Optional next-stage observation; same owned resource format, different
  * selected surface. Existing captures retain their original default. */
 const char *surface214=getenv("DRIVING_WORLD_CAPTURE_SURFACE214");
 if(surface214&&*surface214){char *end;unsigned long format=strtoul(surface214,&end,16);
  if(*end||format>0xffffffffUL||object->state[0x208/4]!=(uint32_t)format)return;}
 done210[family]=1;const char *dir=getenv("DRIVING_CAPTURE_DIR");if(!dir)return;
 uint32_t bytes=xbox_ContiguousAllocatedBytes();SIZE_T copied=0;
 if(!bytes||bytes>0x08000000u||!nf_hw_sync()){
  fprintf(stderr,"[WORLD210] capture refused size/sync bytes=%u\n",bytes);return;}
 void *source=mapped201(0x80000000u,bytes,0),*owned=source?malloc(bytes):NULL;
 if(!owned||!ReadProcessMemory(GetCurrentProcess(),source,owned,bytes,&copied)||copied!=bytes){
  free(owned);fprintf(stderr,"[WORLD210] capture refused unreadable allocated physical RAM bytes=%u copied=%zu\n",bytes,(size_t)copied);return;}
 uint32_t ramht=0;void *pram=malloc(0x100000);
 if(!pram||!read143(NULL,0xfd002210u,&ramht)||
    !ReadProcessMemory(GetCurrentProcess(),(void*)((uintptr_t)xbox_GetMemoryOffset()+0xfd700000u),pram,0x100000,&copied)||copied!=0x100000){
  free(owned);free(pram);fprintf(stderr,"[WORLD210] capture refused PRAM/RAMHT\n");return;}
 /* Texture/vertex/target bytes are now owned, before file I/O. Timer memory may
  * change during copying: this is a GPU resource diagnostic, not a global
  * atomic machine snapshot or proof of correct emulated synchronization. */
 world210_write(dir,family,"ram.bin",owned,bytes);free(owned);
 world210_write(dir,family,"pramin.bin",pram,0x100000);free(pram);
 world210_write(dir,family,"state.bin",object->state,sizeof object->state);
 world210_write(dir,family,"known.bin",object->written167,sizeof object->written167);
 world210_write(dir,family,"program.bin",&live201.program,sizeof live201.program);
 world210_write(dir,family,"attributes.bin",&live201.current,sizeof live201.current);
 world210_write(dir,family,"attribute-masks.bin",live201.masks,sizeof live201.masks);
 char metadata[768];int count=snprintf(metadata,sizeof metadata,
  "physical_base=80000000 bytes=%u family=%u primitive=%u first_method=%04X first_value=%08X cursor=%08X handle=%08X instance=%08X program_bytes=%zu ramht=%08X\nlimits=GPU-resource-copy-not-atomic-machine-snapshot; immediate-default-attribute-tracking-incomplete; not-rendered-image\n",
  bytes,family,object->state[0x17fc/4],method,value,pb.cursor,object->handle,object->instance,sizeof live201.program,ramht);
 if(count>0&&(size_t)count<sizeof metadata)world210_write(dir,family,"metadata.txt",metadata,(size_t)count);
 char path[2048];int n=snprintf(path,sizeof path,"%s/%s-%u-commands.bin",dir,batch221?"world221":batch219?"world219":batch218?"world218":batch217?"world217":"world210",family);
 if(n<0||(size_t)n>=sizeof path)return;
 commands210=fopen(path,"wb");records210=0;
 if(commands210){uint32_t record[2]={method,value};
  if(fwrite(record,sizeof record,1,commands210)!=1){fclose(commands210);commands210=NULL;}
  else records210=1;}
}
#endif
