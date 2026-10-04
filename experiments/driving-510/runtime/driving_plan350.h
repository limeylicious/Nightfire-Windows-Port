/*350 UNTESTED geometry admission. Material state and topology are separate:
 * never rewrite BEGIN/state to trick an old list/strip submitter into drawing.
 * This header authorizes no DMA access, target retention or early completion. */
#ifndef DRIVING_PLAN350_H
#define DRIVING_PLAN350_H
#include "driving_plan221.h"
#include "driving_contract350.h"
#include <errno.h>
enum {GEOMETRY_GLINT350=24};
static int geometry_setting350(const char *name,int *cached)
{
 if(*cached<0){DWORD error=GetLastError();int crt=errno;
  const char *value=getenv(name);*cached=value&&!strcmp(value,"1");
  errno=crt;SetLastError(error);}
 return *cached;
}
static int geometry_enabled350(void)
{static int on=-1;return geometry_setting350("DRIVING_GEOMETRY350",&on);}
static int geometry_topologies350(void)
{static int on=-1;return geometry_setting350("DRIVING_TOPOLOGY350",&on);}
static int geometry_glint350(const uint32_t *s,const unsigned char *known,
                            const NFVertexProgram *p,unsigned primitive)
{
 const DrivingProfile221 *q=&profile_glint350;
 if(!s||!known||!p||primitive!=8||p->mode!=6||p->start)return 0;
 for(unsigned i=0;i<q->fields_count;i++)
  if(!known[q->fields[i].at]||s[q->fields[i].at]!=q->fields[i].value)return 0;
 if(!driving_known234(q->known,known))return 0;
 /*221 treats RGB/RGBA masks separately from its pinned render fields. */
 if(!known[0x358/4]||(s[0x358/4]!=0x00010101u&&s[0x358/4]!=0x01010101u))return 0;
 for(unsigned pc=0;pc<q->length;pc++)
  if(p->valid[pc]!=15||memcmp(p->code[pc],q->code[pc],16))return 0;
 for(unsigned row=0;row<192;row++){
  if(p->constant_valid[row]&~15u)return 0;
  if(q->required[row]&&p->constant_valid[row]!=15)return 0;
  for(unsigned c=0;c<4;c++)if(p->constant_valid[row]&(1u<<c)){
   float value;memcpy(&value,&p->constant_words[row][c],4);if(!isfinite(value))return 0;
  }
 }
 for(unsigned i=0;i<q->pinned_count;i++){
  unsigned row=q->pinned[i].at/4,c=q->pinned[i].at%4;
  if(!(p->constant_valid[row]&(1u<<c))||p->constant_words[row][c]!=q->pinned[i].value)return 0;
 }
 for(unsigned stage=0;stage<4;stage++){
  unsigned mode=(s[0x1e70/4]>>(stage*5))&31,at=(0x1b00+stage*64)/4;
  if(mode==1){
   if(!known[at+1]||!known[at+2]||!driving_texture221(s[at+1],s[at+2]))return 0;
  }else if(mode!=0&&mode!=4)return 0;
 }
 for(unsigned slot=0;slot<16;slot++){
  unsigned n=s[(0x1760+slot*4)/4]>>4&15;
  if(q->inputs&(1u<<slot)){
   if(!n||!known[(0x1760+slot*4)/4]||!known[(0x1720+slot*4)/4])return 0;
  }else if(n)return 0;
 }
 return 1;
}
#include "driving_sprite_plan396.h"
static int geometry_plan350(const uint32_t *s,const unsigned char *k,
                            const NFVertexProgram *p,unsigned primitive,unsigned *profile)
{
 if(primitive==6)return sprite_plan396(s,k,p,primitive,profile);
 if(!profile||!s||!k||!p||(primitive!=7&&primitive!=8&&primitive!=9))return 0;
 if(geometry_glint350(s,k,p,primitive)){*profile=GEOMETRY_GLINT350;return 1;}
 if(!geometry_topologies350())return 0;
 /* Wider candidate is a separate OFF switch: only already admitted material
  * tuples, smooth interpolation, inactive stencil. Exact original topology is
  * assembled by350; original states, shader, texture/filter, fog and masks are
  * passed unchanged. Flat provoking-vertex conventions are not inferred. */
 if(!k[0x37c/4]||s[0x37c/4]!=0x1d01||!k[0x32c/4]||s[0x32c/4])return 0;
 unsigned matched;
 if(driving_plan221(s,k,p,5,&matched)||driving_plan221(s,k,p,6,&matched)){
  *profile=matched;return 1;
 }
 return 0;
}
#endif
