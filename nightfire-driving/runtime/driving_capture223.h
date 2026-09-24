/* Diagnostic only: own six remaining original draw families, including inline
 * quads. Length/stage/topology select observations, never renderer admission. */
/*298: Snapshot selectors only. Reuse full exact code identity and the current
 * planner refusal, never admit a material by instruction count/fog alone. */
#include "driving_plan221.h"
static int lighting_code298(const NFVertexProgram*p,unsigned profile,unsigned length)
{
 const DrivingProfile221*q=&profiles221[profile];
 if(p->mode!=6||p->start||length!=q->length)return 0;
 for(unsigned i=0;i<length;i++)if(p->valid[i]!=15||memcmp(p->code[i],q->code[i],16))return 0;
 return 1;
}
static int lighting_capture298;
static FILE *commands223;
static unsigned records223,saved223[6],seen_world223;
static int capture_enabled223=-1,late_capture240,offscreen_capture243,palette_capture288,remaining_capture290;
static int write223(const char *dir,unsigned family,const char *name,const void *data,size_t size)
{
 char path[2048];int n=snprintf(path,sizeof path,"%s/world223-%u-%s",dir,family,name);
 if(n<0||(size_t)n>=sizeof path)return 0;FILE *f=fopen(path,"wb");if(!f)return 0;
 int okay=fwrite(data,1,size,f)==size;if(fclose(f))okay=0;return okay;
}
static void capture223(GPUObject143 *object,unsigned method,uint32_t value)
{
 if(capture_enabled223<0){const char *v=getenv("DRIVING_WORLD_CAPTURE223");capture_enabled223=v&&!strcmp(v,"1");const char *late=getenv("DRIVING_LATE_CAPTURE240");late_capture240=late&&!strcmp(late,"1");const char *off=getenv("DRIVING_OFFSCREEN_CAPTURE243");offscreen_capture243=off&&!strcmp(off,"1");const char *pal=getenv("DRIVING_PALETTE_CAPTURE288");palette_capture288=pal&&!strcmp(pal,"1");const char *remaining=getenv("DRIVING_REMAINING_CAPTURE290");remaining_capture290=remaining&&!strcmp(remaining,"1");const char *lighting=getenv("DRIVING_LIGHTING_CAPTURE298");lighting_capture298=lighting&&!strcmp(lighting,"1");if(late_capture240||offscreen_capture243||palette_capture288||remaining_capture290||lighting_capture298)capture_enabled223=1;}
 if(!capture_enabled223 || (!lighting_capture298 && late_capture240 && draws201[1]<60))return;
 if(commands223){
  uint32_t record[2]={method,value};int okay=records223<65536&&fwrite(record,sizeof record,1,commands223)==1;
  records223++;
  if(!okay||(method==0x17fc&&!value)){if(fclose(commands223))okay=0;commands223=NULL;fprintf(stderr,"[WORLD223] end records=%u okay=%d\n",records223,okay);}
  return;
 }
 if(method!=0x17fc||!value)return;
 unsigned length=0,stage=object->state[0x1e70/4],surface=object->state[0x208/4];
 for(unsigned i=0;i<136;i++){if(live201.program.valid[i]!=15)return;if(live201.program.code[i][3]&1){length=i+1;break;}}
 unsigned family=6;
 /*243: diagnostic selectors only; all original state/program/resources owned.
  * Six single-sample256 offscreen families, no renderer admission by length. */
 if(lighting_capture298){
  /* Own the first refused instance of each narrowly named tuple. Other flags
   * cannot consume these two slots or change the selector in this mode. */
  unsigned accepted_profile;
  const uint32_t*s=object->state;const unsigned char*k=object->written167;
  if(surface!=0x1128||value!=6||!k[0x2a4/4]||!k[0x2a8/4]||s[0x2a8/4]!=0xff140f16)return;
  if(stage==1&&s[0x2a4/4]==1&&lighting_code298(&live201.program,18,length))family=0;
  else if(draws201[1]>=30&&stage==0x8000&&s[0x2a4/4]<=1&&lighting_code298(&live201.program,12,length))family=1;
  else return;
  if(driving_plan221(s,k,&live201.program,value,&accepted_profile))return;
 }else if(remaining_capture290){
  if(surface!=0x1128||stage!=0x421)return;
  if(length==56&&value==6)family=0;
  else if(length==53&&value==5)family=1;
  else return;
 }else if(offscreen_capture243){
  if(surface!=0x08080228u||(value!=5&&value!=6))return;
  switch(length){case 42:family=0;break;case 48:family=1;break;
   case 56:family=2;break;case 53:family=3;break;case 66:family=4;break;
   case 8:family=5;break;default:return;}
 }else if(surface==0x1128){
  if(length==62&&value==6&&stage==0x20421)family=0;
  else if(length==66&&value==5&&stage==1)family=1;
  else if(length==82&&value==5&&stage==0x8421)family=2;
  else if(length==8&&stage==0x8000)family=3;
  else if(length==7&&stage==0)family=4;
  seen_world223=1;
 }
 if(!lighting_capture298&&!offscreen_capture243&&seen_world223&&length==3&&value==8)family=5;
 if(!lighting_capture298 && late_capture240 && family!=3 && family!=4)return;
 /*288 diagnostic only: own the exact missing paletted fog draw and its original commands. */
 if(!lighting_capture298&&palette_capture288&&(family!=3||surface!=0x1128||length!=8||value!=6||stage!=0x8000||object->state[0x2a4/4]!=1||object->state[0x1bc4/4]!=0x06610b29))return;
 if(family==6||saved223[family])return;
 saved223[family]=1;const char *dir=getenv("DRIVING_CAPTURE_DIR");if(!dir)return;
 unsigned bytes=xbox_ContiguousAllocatedBytes();SIZE_T copied=0;uint32_t ramht=0;
 if(!bytes||bytes>0x08000000||!nf_hw_sync())return;
 void *source=mapped201(0x80000000u,bytes,0),*ram=source?malloc(bytes):NULL,*pram=malloc(0x100000);
 int okay=ram&&pram&&read143(NULL,0xfd002210,&ramht)&&
  ReadProcessMemory(GetCurrentProcess(),source,ram,bytes,&copied)&&copied==bytes&&
  ReadProcessMemory(GetCurrentProcess(),(void*)((uintptr_t)xbox_GetMemoryOffset()+0xfd700000u),pram,0x100000,&copied)&&copied==0x100000;
 if(okay){
  okay=write223(dir,family,"ram.bin",ram,bytes)&&write223(dir,family,"pramin.bin",pram,0x100000)&&
   write223(dir,family,"state.bin",object->state,sizeof object->state)&&write223(dir,family,"known.bin",object->written167,sizeof object->written167)&&
   write223(dir,family,"program.bin",&live201.program,sizeof live201.program)&&write223(dir,family,"attributes.bin",&live201.current,sizeof live201.current)&&write223(dir,family,"attribute-masks.bin",live201.masks,sizeof live201.masks);
  char meta[400];int n=snprintf(meta,sizeof meta,"bytes=%u ramht=%08X family=%u length=%u primitive=%u stage=%08X surface=%08X program_bytes=%zu\nlimits=GPU-resource-copy-not-atomic-machine-snapshot;capture-selection-not-admission;before-original-draw\n",bytes,ramht,family,length,value,stage,surface,sizeof live201.program);
  if(n>0&&(size_t)n<sizeof meta)okay=okay&&write223(dir,family,"metadata.txt",meta,n);
  char path[2048];n=snprintf(path,sizeof path,"%s/world223-%u-commands.bin",dir,family);
  if(okay&&n>0&&(size_t)n<sizeof path){commands223=fopen(path,"wb");records223=0;
   if(commands223){uint32_t record[2]={method,value};if(fwrite(record,sizeof record,1,commands223)!=1){fclose(commands223);commands223=NULL;}else records223=1;}
  }
 }
 fprintf(stderr,"[WORLD223] family=%u length=%u bytes=%u okay=%d commands=%d\n",family,length,bytes,okay,commands223!=NULL);free(ram);free(pram);
}
