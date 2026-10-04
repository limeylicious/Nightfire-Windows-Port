#ifndef DRIVING_PLAN243_H
#define DRIVING_PLAN243_H
#include "driving_plan221.h"
#include "driving_contract243.h"
/* Named state proven unused by these exact programmable triangle/array profiles.
 * This is not a command barrier exemption; methods still execute normally. */
static int driving_inactive243(const uint32_t *s,unsigned m){
 if(m==0x194c||m==0x43c||m==0x181c||m==0x1820||m==0x1824)return 1;
 if(!s[0x2a4/4]&&(m==0x29c||m==0x2a0||m==0x9c0||m==0x9c4||m==0x9c8))return 1;
 if(m>=0x1b00&&m<0x1c00){unsigned stage=(m-0x1b00)/64,k=(m-0x1b00)%64,mode=(s[0x1e70/4]>>(stage*5))&31;
  /* Only named format/address/control/pitch/filter/rectangle/bump registers. */
  if(mode==0&&(k==4||k==8||k==12||k==16||k==20||k==28||k==40||k==44||k==48||k==52||k==56||k==60))return 1;
  /* All admitted sampled formats below are swizzled 2D, no bump stage mode. */
  if(mode==1&&(k==16||k==28||k==40||k==44||k==48||k==52||k==56||k==60))return 1;
 }
 return 0;
}
static int driving_fog_fields243(const uint32_t *s,const unsigned char *known){
 if(!s[0x2a4/4])return 1;
 if(vehicle439_enabled()&&vehicle439_fog(s,known))return 1;
 const unsigned m[]={0x29c,0x2a0,0x9c0,0x9c4,0x9c8};
 const uint32_t v[]={0x2601,2,0x40000b4e,0xba34e335,0};
 for(unsigned i=0;i<5;i++)if(!known[m[i]/4]||s[m[i]/4]!=v[i])return 0;
 return 1;
}
static int driving_fog_method243(unsigned m){return m==0x29c||m==0x2a0||m==0x9c0||m==0x9c4||m==0x9c8;}
static int driving_variant243(const uint32_t *s,unsigned m,uint32_t expected){
 uint32_t v=s[m/4];
 if(driving_inactive243(s,m))return 1;
 if(m==0x39c&&!s[0x308/4]&&(v==0x404||v==0x405)&&(expected==0x404||expected==0x405))return 1;
 /* Observed boolean states use the existing full material backend mapping. */
 if((m==0x2a4||m==0x35c||m==0x300||m==0x304)&&v<=1&&expected<=1)return 1;
 if(m==0x340&&(v==1||v==16||v==128||v==160))return 1;
 if(m>=0x1b00&&m<0x1c00){unsigned stage=(m-0x1b00)/64,k=(m-0x1b00)%64;
  if(((s[0x1e70/4]>>(stage*5))&31)==1){
   if(k==4)return driving_texture221(v,s[(0x1b08+stage*64)/4]);
   if(k==12)return v==0x4003fff0||v==0x4003ffc0;
  }
 }
 return 0;
}
/* Analysis candidate: exact code/state, dynamic complete finite constants.
 * This is only admission, not submission permission by itself: the caller must
 * resolve/check every DMA span and attribute, execute nf_vp_run for every input,
 * reject every consumed nonfinite output/invalid projective divisor, and retain
 * exact original primitive/command ordering. No guest memory is read here.
 * Relative constant rows are validated by nf_vp_run with each vertex's address.
 * All referenced attributes are enabled arrays; no current/immediate defaults
 * are required by these programs. On refusal *profile is untouched. */
static int driving_plan243(const uint32_t *s,const unsigned char *known,
                          const NFVertexProgram *p,unsigned primitive,
                          unsigned *profile)
{
 if(!s||!known||!p||!profile||p->mode!=6||p->start||
    !known[0x208/4]||s[0x208/4]!=0x08080228u||!known[0x358/4]||
    s[0x358/4]!=0x00010101u||!driving_fog_fields243(s,known))return 0;
 /* Reject nonfinite data in every declared-valid component. Unused, invalid
  * lanes are allowed to retain arbitrary old bits; they cannot be read safely. */
 for(unsigned row=0;row<192;row++){
  if(p->constant_valid[row]&~15u)return 0;
  for(unsigned c=0;c<4;c++)if(p->constant_valid[row]&(1u<<c)){
   float value;memcpy(&value,&p->constant_words[row][c],4);
   if(!isfinite(value))return 0;
  }
 }
 for(unsigned family=0;family<DRIVING_PROFILES243;family++){
  const DrivingProfile243 *q=&profiles243[family];int okay=primitive==q->primitive;
  if(!okay)continue;
  for(unsigned i=0;i<q->fields_count;i++){
   unsigned at=q->fields[i].at;
   if(!known[at]){okay=0;break;}
   if(s[at]!=q->fields[i].value&&!driving_stencil306(s,known,at*4)&&!driving_variant243(s,at*4,q->fields[i].value)&&!vehicle439_field(s,known,at*4,q->fields[i].value)){okay=0;break;}
  }
  if(!okay)continue;
  for(unsigned at=0;at<2048;at++)if(!q->known[at]&&known[at]&&
    !driving_inactive243(s,at*4)&&!(s[0x2a4/4]&&driving_fog_method243(at*4))){okay=0;break;}
  if(!okay)continue;
  /* Shader-stage program itself is still an exact captured field. Only the
   * supported sampled modes access images; NONE/PASSTHRU ignore stale formats
   * and addressing/filter/control. Sampled filter and control stay pinned;
   * PASSTHRU coordinate-color domain checks remain the caller's duty. */
  for(unsigned stage=0;stage<4;stage++){
   unsigned mode=(s[0x1e70/4]>>(stage*5))&31,at=(0x1b00+stage*64)/4;
   if(mode==1){
    if(!known[at+1]||!known[at+2]||!driving_texture221(s[at+1],s[at+2])){okay=0;break;}
   }else if(mode!=0&&mode!=4){okay=0;break;}
  }
  if(!okay)continue;
  for(unsigned pc=0;pc<q->length;pc++)
   if(p->valid[pc]!=15||memcmp(p->code[pc],q->code[pc],16)){okay=0;break;}
  if(!okay)continue;
  for(unsigned row=0;row<192;row++)if(q->required[row]&&p->constant_valid[row]!=15){okay=0;break;}
  if(!okay)continue;
  for(unsigned i=0;i<q->pinned_count;i++){
   unsigned row=q->pinned[i].at/4,c=q->pinned[i].at%4;
   if(!(p->constant_valid[row]&(1u<<c))||p->constant_words[row][c]!=q->pinned[i].value){okay=0;break;}
  }
  if(!okay)continue;
  for(unsigned slot=0;slot<16;slot++){
   unsigned count=(s[(0x1760+slot*4)/4]>>4)&15;
   if(q->inputs&(1u<<slot)){
    if(!known[(0x1760+slot*4)/4]||!known[(0x1720+slot*4)/4]||!count){okay=0;break;}
   }else if(count){okay=0;break;}
  }
  if(okay){*profile=family;return 1;}
 }
 return 0;
}
#endif
