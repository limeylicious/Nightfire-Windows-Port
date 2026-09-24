#ifndef TEXTURE_SELECT161_H
#define TEXTURE_SELECT161_H
#include <stdint.h>
/* Exact captured-state preflight only. No guest memory reads, mutation, shader
 * generation, DMA resolution or evidence of actual output pixels. */
enum { NF161_PROGRAM=1u<<0,NF161_CONTROL=1u<<1,NF161_RGB=1u<<2,
 NF161_ALPHA=1u<<3,NF161_RGB_OUT=1u<<4,NF161_ALPHA_OUT=1u<<5,
 NF161_FINAL0=1u<<6,NF161_FINAL1=1u<<7,NF161_OTHER=1u<<8,
 NF161_REQUIRED=0x1ffu };
typedef struct NFTextureState161 {
 uint32_t known,program,control,rgb,alpha,rgb_out,alpha_out,final0,final1,other;
 uint32_t format[4],texture_control[4];
 uint32_t format_known,texture_control_known;
} NFTextureState161;
enum NFTextureReject161 { NF161_OK=0,NF161_ARGUMENT,NF161_UNOBSERVED,
 NF161_PROGRAM_UNKNOWN,NF161_COMBINER_UNKNOWN,NF161_TEXTURE_CONTROL,NF161_FORMAT };
enum { NF161_MODULATE=1,NF161_RESOLVE=2 };
typedef struct NFTextureSelection161 {
 uint32_t stage,vp_texcoord_stage,texture_result_register,palette_required;
 uint32_t family,pixel_unused_project2d_mask;
} NFTextureSelection161;
static int nf_texture_select161(const NFTextureState161 *s,NFTextureSelection161 *out)
{
 if(!s||!out)return NF161_ARGUMENT;
 if((s->known&NF161_REQUIRED)!=NF161_REQUIRED)return NF161_UNOBSERVED;
 if(s->program!=1 && s->program!=0x8000 && s->program!=0x8001)return NF161_PROGRAM_UNKNOWN;
 unsigned stage,family;
 if(s->control==0x11101 && s->rgb_out==0xc0 && s->alpha_out==0xc0 && s->final1==0x1c80 && !s->other){
  if(s->program==0x8000 && s->rgb==0xc4cb0000 && s->alpha==0xd4db1010 && s->final0==0xc00)stage=3;
  else if(s->program==1 && s->rgb==0xc4c80000 && s->alpha==0xd4d81010 && s->final0==0xc)stage=0;
  else return NF161_COMBINER_UNKNOWN;
  family=NF161_MODULATE;
 }else if(s->control==1 && s->rgb==0x08200000 && s->alpha==0x14200000
    && s->rgb_out==0xc00 && s->alpha_out==0xc00 && s->final0==0xc && s->final1==0x1c80
    && s->other==0x210000 && (s->program==1 || s->program==0x8001)){
  stage=0;family=NF161_RESOLVE;
 }else return NF161_COMBINER_UNKNOWN;
 /* All active texture shader stages must have the captured control. This
  * rejects alpha-kill/color-key even when the combiner ignores that texture.
  * Inactive mode0 stages have no texture fetch/discard dependency here. */
 for(unsigned i=0;i<4;i++)if((s->program>>(i*5))&31){
  if(!(s->texture_control_known&(1u<<i)))return NF161_UNOBSERVED;
  if(s->texture_control[i]!=0x4003ffc0)return NF161_TEXTURE_CONTROL;
  if(!(s->format_known&(1u<<i)))return NF161_UNOBSERVED;
  /* Unused-but-active resolve stage3 was ordinary RGBA6. Reject an unknown
   * depth/cubemap format rather than infer it has no independent behavior. */
  if(i!=stage && s->format[i]!=0x04410629)return NF161_FORMAT;
 }
 uint32_t fmt=s->format[stage];
 if(family==NF161_RESOLVE){if(fmt!=0x00011229)return NF161_FORMAT;}
 else if(stage==0){if(fmt!=0x00011f29)return NF161_FORMAT;}
 else if(fmt!=0x06610b29 && fmt!=0x05610b29 && fmt!=0x04410629)return NF161_FORMAT;
 NFTextureSelection161 result={stage,stage,8u+stage,((fmt>>8)&255)==0x0b,
  family,s->program==0x8001?8u:0u};
 *out=result;return NF161_OK;
}
#endif
