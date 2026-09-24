/* Diagnostic only: called at the existing unadmitted-BEGIN publication branch.
 * Already-owned state/program/span metadata only. No guest reads, new clocks,
 * admission decisions, queue lifetime changes or descriptor resolution. */
#ifndef DRIVING_BEGIN280_H
#define DRIVING_BEGIN280_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
enum {B280_OK,B280_MODE,B280_START,B280_SURFACE,B280_COLOR_MASK,
 B280_CONSTANT_MASK,B280_NONFINITE,B280_PRIMITIVE,B280_MISSING_FIELD,
 B280_FIELD,B280_EXTRA_KNOWN,B280_TEXTURE,B280_STAGE,B280_REQUIRED_CONSTANT,
 B280_PIN_VALID,B280_PIN_VALUE,B280_ARRAY_MISSING,B280_EXTRA_ARRAY,B280_DISABLED292};
static const char *const begin280_names[]={"admitted","mode","start","surface","color-mask",
 "constant-mask","nonfinite-constant","primitive","missing-field","field","extra-known",
 "texture-format","stage-mode","required-constant","pin-valid","pin-value","array-missing","extra-array","disabled292"};
typedef struct {uint32_t why,at,actual,expected;} BeginReason280;
typedef struct {
 uint32_t primitive,surface,pixel,mode,start,length,complete,code_hash;
 uint32_t code_mask,primitive_mask,same_fields,same_handles,known_targets,held,enabled;
 BeginReason280 rejection[DRIVING_PROFILES221];
} BeginKey280;
typedef struct {BeginKey280 key;uint64_t count,draws;} BeginRow280;
static struct {uint64_t total,draws,same_main,offscreen,other,overflow;unsigned used;BeginRow280 row[32];} begin280;
static int begin280_setting=-1;
_Static_assert(DRIVING_PROFILES221<=32,"280 exact code mask width");
static BeginReason280 begin280_reason(unsigned why,unsigned at,uint32_t actual,uint32_t expected){
 BeginReason280 r={why,at,actual,expected};return r;
}
/* Mirrors only the existing planner's diagnostics. The result never authorizes
 * rendering. Exact code equality was independently established by the caller. */
static BeginReason280 begin280_reject(const uint32_t*s,const unsigned char*k,
 const NFVertexProgram*p,unsigned primitive,unsigned family){
 const DrivingProfile221*q=&profiles221[family];
 unsigned extension=driving_group283(s,k,family,primitive);
#define B280_FAIL(w,a,x,e) return begin280_reason(w,a,x,e)
 if(family==23&&!driving_material292_enabled())B280_FAIL(B280_DISABLED292,23,0,1);
 if(p->mode!=6)B280_FAIL(B280_MODE,0,p->mode,6);
 if(p->start)B280_FAIL(B280_START,0,p->start,0);
 if(!k[0x208/4]||s[0x208/4]!=0x1128)B280_FAIL(B280_SURFACE,0x208,s[0x208/4],0x1128);
 if(!k[0x358/4]||(s[0x358/4]!=0x10101&&s[0x358/4]!=0x1010101))B280_FAIL(B280_COLOR_MASK,0x358,s[0x358/4],0x10101);
 for(unsigned row=0;row<192;row++){
  if(p->constant_valid[row]&~15u)B280_FAIL(B280_CONSTANT_MASK,row,p->constant_valid[row],15);
  for(unsigned c=0;c<4;c++)if((p->constant_valid[row]&(1u<<c))&&
    (p->constant_words[row][c]&0x7f800000u)==0x7f800000u)
   B280_FAIL(B280_NONFINITE,row*4+c,p->constant_words[row][c],0);
 }
 if(primitive!=q->primitive&&!driving_primitive283(extension,family,primitive))B280_FAIL(B280_PRIMITIVE,0x17fc,primitive,q->primitive);
 for(unsigned i=0;i<q->fields_count;i++){
  unsigned at=q->fields[i].at;uint32_t expected=q->fields[i].value;
  if(!k[at])B280_FAIL(B280_MISSING_FIELD,at*4,s[at],expected);
  if(s[at]==expected||at==0x194c/4||driving_stencil306(s,k,at*4)||driving_field283(s,extension,family,at*4,expected))continue;
  if(family<22&&(at==0x300/4||at==0x304/4)&&s[at]<=1&&expected<=1)continue;
  if(family<22&&at==0x340/4&&(s[at]==1||s[at]==128||s[at]==160))continue;
  if(at==0x39c/4&&k[0x308/4]&&!s[0x308/4]&&
   (s[at]==0x404||s[at]==0x405)&&(expected==0x404||expected==0x405))continue;
  B280_FAIL(B280_FIELD,at*4,s[at],expected);
 }
 for(unsigned at=0;at<2048;at++)if(!q->known[at]&&k[at]&&!driving_extra283(s,extension,family,at*4))B280_FAIL(B280_EXTRA_KNOWN,at*4,s[at],0);
 for(unsigned stage=0;stage<4;stage++){
  unsigned mode=(s[0x1e70/4]>>(stage*5))&31,at=(0x1b00+stage*64)/4;
  if(mode==1){if(!k[at+1]||!k[at+2]||(!driving_texture221(s[at+1],s[at+2])&&!driving_indexed283(s,k,family,extension,stage)))B280_FAIL(B280_TEXTURE,(at+1)*4,s[at+1],0);}
  else if(mode!=0&&mode!=4)B280_FAIL(B280_STAGE,0x1e70,mode,stage);
 }
 for(unsigned row=0;row<192;row++)if(q->required[row]&&p->constant_valid[row]!=15)
  B280_FAIL(B280_REQUIRED_CONSTANT,row,p->constant_valid[row],15);
 for(unsigned i=0;i<q->pinned_count;i++){
  unsigned at=q->pinned[i].at,row=at/4,c=at%4;
  if(!(p->constant_valid[row]&(1u<<c)))B280_FAIL(B280_PIN_VALID,at,p->constant_valid[row],1u<<c);
  if(p->constant_words[row][c]!=q->pinned[i].value)B280_FAIL(B280_PIN_VALUE,at,p->constant_words[row][c],q->pinned[i].value);
 }
 for(unsigned slot=0;slot<16;slot++){
  unsigned n=(s[(0x1760+slot*4)/4]>>4)&15;
  if(q->inputs&(1u<<slot)){
   if(!k[(0x1760+slot*4)/4]||!k[(0x1720+slot*4)/4]||!n)B280_FAIL(B280_ARRAY_MISSING,slot,n,1);
  }else if(n)B280_FAIL(B280_EXTRA_ARRAY,slot,n,0);
 }
#undef B280_FAIL
 return begin280_reason(B280_OK,0,0,0);
}
static void driving_begin280(const uint32_t*s,const unsigned char*k,const NFVertexProgram*p,
 unsigned primitive,const uint32_t old_fields[7],const DrivingSpan183 old_spans[2],
 unsigned pending,int held,int enabled){
 DWORD saved=GetLastError();
 if(begin280_setting<0){const char*v=getenv("DRIVING_BEGIN280");begin280_setting=v&&!strcmp(v,"1");}
 if(!begin280_setting||!s||!k||!p||!old_fields||!old_spans||!pending){SetLastError(saved);return;}
 BeginKey280 key={0};key.primitive=primitive;key.surface=s[0x208/4];key.pixel=s[0x1e70/4];
 key.mode=p->mode;key.start=p->start;key.held=!!held;key.enabled=enabled>0;
 key.same_fields=!memcmp(old_fields,s+0x200/4,7*sizeof(uint32_t));
 key.known_targets=k[0x194/4]&&k[0x198/4];
 for(unsigned at=0x200/4;at<=0x214/4;at++)key.known_targets&=!!k[at];
 key.same_handles=s[0x194/4]==old_spans[0].handle&&s[0x198/4]==old_spans[1].handle;
 key.code_hash=2166136261u;
 for(unsigned pc=p->start;pc<136;pc++){
  if(p->valid[pc]!=15)break;
  for(unsigned c=0;c<4;c++)for(unsigned b=0;b<4;b++)key.code_hash=(key.code_hash^((p->code[pc][c]>>(8*b))&255))*16777619u;
  key.length++;if(p->code[pc][3]&1){key.complete=1;break;}
 }
 for(unsigned q=0;q<DRIVING_PROFILES221;q++){
  const DrivingProfile221*c=&profiles221[q];unsigned pc;
  for(pc=0;pc<c->length;pc++)if(p->valid[pc]!=15||memcmp(p->code[pc],c->code[pc],16))break;
  if(pc==c->length){key.code_mask|=1u<<q;if(primitive==c->primitive||driving_primitive283(driving_group283(s,k,q,primitive),q,primitive))key.primitive_mask|=1u<<q;
   key.rejection[q]=begin280_reject(s,k,p,primitive,q);}
 }
 begin280.total++;begin280.draws+=pending;
 if(key.surface==0x1128&&key.same_fields&&key.same_handles&&key.known_targets)begin280.same_main++;
 else if(key.surface==0x08080228)begin280.offscreen++;else begin280.other++;
 unsigned at;for(at=0;at<begin280.used;at++)if(!memcmp(&begin280.row[at].key,&key,sizeof key))break;
 if(at==begin280.used&&at<32){
  begin280.row[at].key=key;begin280.used++;
  fprintf(stderr,"[BEGIN280-KEY] id=%u primitive=%u surface=%08X mode=%u start=%u instructions=%u complete=%u code_fnv=%08X code_profiles=%08X primitive_profiles=%08X pixel=%08X same_target_fields=%u same_handles=%u known_targets=%u held=%u enabled221=%u old_color=%08X old_depth=%08X incoming_color_offset=%08X incoming_depth_offset=%08X metadata_only=1 unresolved_incoming_DMA=1\n",at,key.primitive,key.surface,key.mode,key.start,key.length,key.complete,key.code_hash,key.code_mask,key.primitive_mask,key.pixel,key.same_fields,key.same_handles,key.known_targets,key.held,key.enabled,old_spans[0].address,old_spans[1].address,s[0x210/4],s[0x214/4]);
  for(unsigned q=0;q<DRIVING_PROFILES221;q++)if(key.code_mask&(1u<<q)){
   const BeginReason280*r=&key.rejection[q];
   fprintf(stderr,"[BEGIN280-REJECT] id=%u profile=%u reason=%s at=%04X actual=%08X expected=%08X diagnostic_not_admission=1\n",at,q,begin280_names[r->why],r->at,r->actual,r->expected);
  }
 }
 if(at<32){begin280.row[at].count++;begin280.row[at].draws+=pending;}else begin280.overflow++;
 if(begin280.total==1||!(begin280.total%256)){
  fprintf(stderr,"[BEGIN280] total=%llu queued_draws=%llu same_main_registers=%llu offscreen256=%llu other=%llu keys=%u overflow=%llu rows=id:events:queued_draws",(unsigned long long)begin280.total,(unsigned long long)begin280.draws,(unsigned long long)begin280.same_main,(unsigned long long)begin280.offscreen,(unsigned long long)begin280.other,begin280.used,(unsigned long long)begin280.overflow);
  for(unsigned i=0;i<begin280.used;i++)fprintf(stderr," %u:%llu:%llu",i,(unsigned long long)begin280.row[i].count,(unsigned long long)begin280.row[i].draws);
  fputc('\n',stderr);
 }
 SetLastError(saved);
}
#endif
