/*396 Default-OFF original captured HUD strips. No synthetic UI or mode change.
 * Pin the complete scalar/pixel states; read actual arrays/constants per draw.
 * Only two exact original programs, one256x256 palette layout, one untextured.
 * Submission must enforce four vertices, DMA/OS permissions and target aliases. */
#ifndef DRIVING_SPRITE_PLAN396_H
#define DRIVING_SPRITE_PLAN396_H
#include "driving_sprite_contract396.h"
static int sprite_enabled396(void){static int on=-1;return geometry_setting350("DRIVING_SPRITES396",&on);}
static int sprite_plan396(const uint32_t*s,const unsigned char*k,const NFVertexProgram*p,unsigned primitive,unsigned*profile){
 if(!sprite_enabled396()||!s||!k||!p||!profile||primitive!=6||p->mode!=6||p->start)return 0;
 for(unsigned family=0;family<2;family++){
  const DrivingProfile221*q=family?&sprite_profile15_396:&sprite_profile13_396;int okay=1;
  for(unsigned j=0;j<q->fields_count;j++){unsigned a=q->fields[j].at;if(!k[a]||s[a]!=q->fields[j].value){okay=0;break;}}
  if(!okay||!driving_known234(q->known,k))continue;
  for(unsigned j=0;j<q->length;j++)if(p->valid[j]!=15||memcmp(p->code[j],q->code[j],16)){okay=0;break;}
  if(!okay)continue;
  for(unsigned row=0;row<192;row++){
   if((p->constant_valid[row]&~15u)||(q->required[row]&&p->constant_valid[row]!=15)){okay=0;break;}
   for(unsigned c=0;c<4;c++)if(p->constant_valid[row]&(1u<<c)){float v;memcpy(&v,&p->constant_words[row][c],4);if(!isfinite(v))okay=0;}
  }
  if(!okay)continue;
  for(unsigned a=0;a<16;a++){
   unsigned n=s[0x1760/4+a]>>4&15;
   if(q->inputs>>a&1){if(!k[0x1760/4+a]||!k[0x1720/4+a]||!n)okay=0;}
   else if(n)okay=0;
  }
  if(!family&&(!k[0x1be0/4]||(s[0x1be0/4]&63)))okay=0;
  if(okay){*profile=25+family;return 1;}
 }
 return 0;
}
#endif
