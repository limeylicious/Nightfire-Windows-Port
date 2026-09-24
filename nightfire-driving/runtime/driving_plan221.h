#ifndef DRIVING_PLAN221_H
#define DRIVING_PLAN221_H
#include <stddef.h>
#include <string.h>
#include "gpu144/nightfire_vertex_program.h"
#include "driving_contract221.h"
#include "driving_material292.h"
#include "driving_admission283.h"
#include "driving_inactive306.h"
/* Compare boolean knownness, not byte bit masks (recorded values include 2).
 * No state or profile cache: every call examines the current complete input. */
#if defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#endif
static int driving_known234(const unsigned char *allowed,const unsigned char *known)
{
#if defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
 const __m128i zero=_mm_setzero_si128();
 for(unsigned i=0;i<2048;i+=16){
  __m128i absent=_mm_cmpeq_epi8(_mm_loadu_si128((const __m128i*)(allowed+i)),zero);
  __m128i unset=_mm_cmpeq_epi8(_mm_loadu_si128((const __m128i*)(known+i)),zero);
  if(_mm_movemask_epi8(_mm_andnot_si128(unset,absent)))return 0;
 }
#else
 for(unsigned i=0;i<2048;i++)if(!allowed[i]&&known[i])return 0;
#endif
 return 1;
}
static int driving_texture221(uint32_t format,uint32_t address)
{
 unsigned kind=(format>>8)&255,mips=(format>>16)&15,
          u=(format>>20)&15,v=(format>>24)&15;
 if((format&255)!=0x29u || (format>>28) ||
    !(kind==5||kind==6||kind==12||kind==14)||u>11||v>11||
    !mips||mips>(u>v?u:v)+1u||(address&0xff000000u))return 0;
 for(unsigned k=0;k<3;k++){
  unsigned mode=(address>>(8*k))&255;
  if(mode!=1&&mode!=3)return 0;
 }
 return 1;
}
/* Analysis candidate: exact code/state, dynamic complete finite constants.
 * This is only admission, not submission permission by itself: the caller must
 * resolve/check every DMA span and attribute, execute nf_vp_run for every input,
 * reject every consumed nonfinite output/invalid projective divisor, and retain
 * exact original primitive/command ordering. No guest memory is read here.
 * Relative constant rows are validated by nf_vp_run with each vertex's address.
 * All referenced attributes are enabled arrays; no current/immediate defaults
 * are required by these programs. On refusal *profile is untouched. */
static int driving_plan221(const uint32_t *s,const unsigned char *known,
                          const NFVertexProgram *p,unsigned primitive,
                          unsigned *profile)
{
 if(!s||!known||!p||!profile||p->mode!=6||p->start||
    !known[0x208/4]||s[0x208/4]!=0x1128u||!known[0x358/4]||
    (s[0x358/4]!=0x00010101u&&s[0x358/4]!=0x01010101u))return 0;
 /* Reject nonfinite data in every declared-valid component. Unused, invalid
  * lanes are allowed to retain arbitrary old bits; they cannot be read safely. */
 for(unsigned row=0;row<192;row++){
  if(p->constant_valid[row]&~15u)return 0;
  for(unsigned c=0;c<4;c++)if(p->constant_valid[row]&(1u<<c)){
   float value;memcpy(&value,&p->constant_words[row][c],4);
   if(!isfinite(value))return 0;
  }
 }
 for(unsigned family=0;family<DRIVING_PROFILES221;family++){
  if(family==23&&!driving_material292_enabled())continue;
  const DrivingProfile221 *q=&profiles221[family];
  unsigned extension=driving_group283(s,known,family,primitive);
  int okay=primitive==q->primitive||driving_primitive283(extension,family,primitive);
  if(!okay)continue;
  for(unsigned i=0;i<q->fields_count;i++){
   unsigned at=q->fields[i].at;
   if(!known[at]){okay=0;break;}
   if(s[at]!=q->fields[i].value){
    if(driving_stencil306(s,known,at*4)||driving_field283(s,extension,family,at*4,q->fields[i].value))continue;
    /*241: current packed attribute3 is ignored by these exact array-only
     * shaders. Knownness, every enabled array and every consumed input remain
     * checked below; do not alter the actual current attribute or barriers. */
    if(at==0x194c/4)continue;
    /*239: only observed finite alpha/blend variants. Compare function,
     * blend factors/equation and shader remain exact profile fields. */
    if(family<22&&(at==0x300/4||at==0x304/4)&&s[at]<=1&&q->fields[i].value<=1)continue;
    if(family<22&&at==0x340/4&&(s[at]==1||s[at]==128||s[at]==160))continue;

    /*238: front/back selector has no raster effect with original culling
     * explicitly disabled. Keep the actual state, active culling, invalid
     * selectors and every other material field strict. */
    if(at!=0x39c/4||!known[0x308/4]||s[0x308/4]!=0||
       (s[at]!=0x404&&s[at]!=0x405)||
       (q->fields[i].value!=0x404&&q->fields[i].value!=0x405)){okay=0;break;}
   }
  }
  if(!okay)continue;
  if(!driving_known234(q->known,known)){
   for(unsigned at=0;at<2048;at++)if(!q->known[at]&&known[at]&&
      !driving_extra283(s,extension,family,at*4)){okay=0;break;}
   if(!okay)continue;
  }
  /* Shader-stage program itself is still an exact captured field. Only the
   * supported sampled modes access images; NONE/PASSTHRU ignore stale formats
   * and addressing/filter/control. Sampled filter and control stay pinned;
   * PASSTHRU coordinate-color domain checks remain the caller's duty. */
  for(unsigned stage=0;stage<4;stage++){
   unsigned mode=(s[0x1e70/4]>>(stage*5))&31,at=(0x1b00+stage*64)/4;
   if(mode==1){
    if(!known[at+1]||!known[at+2]||(!driving_texture221(s[at+1],s[at+2])&&!driving_indexed283(s,known,family,extension,stage))){okay=0;break;}
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
