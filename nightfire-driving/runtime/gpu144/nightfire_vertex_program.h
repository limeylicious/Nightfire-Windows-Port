/* Small NV2A vertex-program executor for captured PAL shaders.
 * Encoding: envytools XF ISA (Kelvin). Independent implementation; no emulator
 * source imported. Unsupported operations/constant writes fail explicitly.
 */
#ifndef NIGHTFIRE_VERTEX_PROGRAM_H
#define NIGHTFIRE_VERTEX_PROGRAM_H
#include <stdint.h>
#include <string.h>
#include <math.h>
typedef struct {
 unsigned ready;
 unsigned mac,ilu,ci,vi,dst,mm,im,om,source[3],indexed,output,output_register,scalar,final;
} NFVertexInstruction;
typedef struct {
 NFVertexInstruction decoded[136];
 uint32_t code[136][4],valid[136],constant_words[192][4],constant_valid[192];
 unsigned load,constant_load,start,mode;
 unsigned error_pc,error_constant;
} NFVertexProgram;
static void nf_vp_method(NFVertexProgram *s,unsigned m,uint32_t v) {
 /* NV097 viewport methods share XF context slots VPSCL=0x3a/VPOFF=0x3b
  * (the pinned nv2a_regs.h). Keep component upload validity explicit. */
 if((m>=0xa20 && m<=0xa2c) || (m>=0xaf0 && m<=0xafc)){
  unsigned row=m>=0xaf0?58:59,component=(m&15)/4;
  s->constant_words[row][component]=v;s->constant_valid[row]|=1u<<component;
 }
 else if(m==0x1e9c)s->load=v;
 else if(m==0x1ea4)s->constant_load=v;
 else if(m==0x1ea0)s->start=v;
 else if(m==0x1e94)s->mode=v;
 else if(m>=0xb00 && m<0xb80){unsigned c=((m-0xb00)/4)%4;
  if(s->load<136){s->code[s->load][c]=v;s->valid[s->load]|=1u<<c;s->decoded[s->load].ready=0;}if(c==3)s->load++;
 }else if(m>=0xb80 && m<0xc00){unsigned c=((m-0xb80)/4)%4;
  if(s->constant_load<192){s->constant_words[s->constant_load][c]=v;s->constant_valid[s->constant_load]|=1u<<c;}if(c==3)s->constant_load++;
 }
}
static unsigned nf_vp_field(const uint32_t *w,unsigned bit,unsigned size) {
 /* Kelvin's low 32 bits are submitted DWORD 3; next DWORD 2, then DWORD 1. */
 unsigned word=3-bit/32,shift=bit%32;uint64_t v=w[word];
 if(shift+size>32 && word)v|=(uint64_t)w[word-1]<<32;
 return (unsigned)(v>>shift)&((1u<<size)-1);
}
static int nf_vp_source(NFVertexProgram *s,unsigned packed,unsigned input,unsigned ci,
 const float in[16][4],float r[13][4],float value[4]) {
 unsigned mux=packed&3,reg=(packed>>2)&15,swz=(packed>>6)&255;float c[4];const float *v;
 if(mux==1){if(reg>12)return 0;v=r[reg];}
 else if(mux==2)v=in[input];
 else if(mux==3){if(ci>=192 || s->constant_valid[ci]!=15){s->error_constant=ci;return 0;}memcpy(c,s->constant_words[ci],16);v=c;}
 else return 0;
 for(unsigned k=0;k<4;k++)value[k]=v[(swz>>(6-2*k))&3]*((packed&0x4000)?-1.0f:1.0f);
 return 1;
}
static void nf_vp_mask(float *dst,const float *src,unsigned mask) {for(unsigned k=0;k<4;k++)if(mask&(8u>>k))dst[k]=src[k];}
static int nf_vp_run(NFVertexProgram *s,const float in[16][4],float out[16][4]) {
 s->error_pc=s->error_constant=~0u;
 float r[13][4]={{0}};memset(out,0,16*4*sizeof(float));
 /*219: Kelvin result defaults are (0,0,0,1), unlike zeroed temporaries.
  * Slots1/2/13/14/15 are not ordinary Kelvin results; retain their old zeros.
  * r12 is the readable alias of position, not an additional temporary. */
 out[0][3]=1.0f;for(unsigned result=3;result<=12;result++)out[result][3]=1.0f;
 r[12][3]=1.0f;
 int address=0;
 if((s->mode&3)!=2 || s->start>=136)return 0;
 for(unsigned pc=s->start;pc<136;pc++){
  s->error_pc=pc;
  if(s->valid[pc]!=15)return 0;const uint32_t *w=s->code[pc];
  NFVertexInstruction *d=&s->decoded[pc];
  if(!d->ready){
   d->mac=nf_vp_field(w,85,4);d->ilu=nf_vp_field(w,89,3);d->ci=nf_vp_field(w,77,8);d->vi=nf_vp_field(w,73,4);
   d->dst=nf_vp_field(w,20,4);d->mm=nf_vp_field(w,24,4);d->im=nf_vp_field(w,16,4);d->om=nf_vp_field(w,12,4);
   d->source[0]=nf_vp_field(w,58,15);d->source[1]=nf_vp_field(w,43,15);d->source[2]=nf_vp_field(w,28,15);
   d->indexed=nf_vp_field(w,1,1);d->output=nf_vp_field(w,3,8);d->output_register=nf_vp_field(w,11,1);d->scalar=nf_vp_field(w,2,1);d->final=w[3]&1;d->ready=1;
  }
  unsigned mac=d->mac,ilu=d->ilu,ci=d->ci,vi=d->vi,dst=d->dst,mm=d->mm,im=d->im,om=d->om;
  float a[4]={0},b[4]={0},c[4]={0},mv[4]={0},iv[4]={0};
  if(d->indexed)ci=(unsigned)((int)ci+address);
  if(mac && !nf_vp_source(s,d->source[0],vi,ci,in,r,a))return 0;
  if((mac==2 || mac==4 || mac==5 || mac==6 || mac==7 || mac==9 || mac==10 || mac==11 || mac==12) &&
     !nf_vp_source(s,d->source[1],vi,ci,in,r,b))return 0;
  if((mac==3 || mac==4 || ilu) && !nf_vp_source(s,d->source[2],vi,ci,in,r,c))return 0;
  float dot=0;if(mac==5 || mac==6 || mac==7){unsigned n=mac==7?4:3;for(unsigned k=0;k<n;k++)dot+=a[k]*b[k];if(mac==6)dot+=b[3];}
  for(unsigned k=0;k<4;k++)switch(mac){
   case 0:break;case 1:mv[k]=a[k];break;case 2:mv[k]=a[k]*b[k];break;case 3:mv[k]=a[k]+c[k];break;
   case 4:mv[k]=a[k]*b[k]+c[k];break;case 5:case 6:case 7:mv[k]=dot;break;
   case 9:mv[k]=fminf(a[k],b[k]);break;case 10:mv[k]=fmaxf(a[k],b[k]);break;
   case 11:mv[k]=a[k]<b[k]?1.0f:0.0f;break;case 12:mv[k]=a[k]>=b[k]?1.0f:0.0f;break;case 13:break;default:return 0;
  }
  /*219: only EXP's fractional Y is supported; do not approximate its
   * unverified exponential X/Z outputs. Refuse any consumed unsupported lane. */
  if(ilu==5){
   unsigned used=im|(d->scalar?om:0);
   if((used&~4u) || !isfinite(c[0]) || c[0]<-126.0f || c[0]>127.0f)return 0;
   iv[1]=c[0]-floorf(c[0]);
  }else for(unsigned k=0;k<4;k++)switch(ilu){
   case 0:break;case 1:iv[k]=c[k];break;case 2:iv[k]=1.0f/c[0];break;
   case 3:{float q=1.0f/c[0];iv[k]=copysignf(fminf(1.8446744e19f,fmaxf(5.4210109e-20f,fabsf(q))),q);break;}
   case 4:iv[k]=1.0f/sqrtf(fabsf(c[0]));break;default:return 0;
  }
  /* Compute both units before writes. Paired scalar writes target r1. */
  /* ARL floors its swizzled scalar source into the private address register.
   * Bound to Kelvin's documented signed nine-bit range; reject undefined
   * overflow instead of invoking an out-of-range host float-to-int cast. */
  if(mac==13){float index=floorf(a[0]);if(!isfinite(index) || index < -256 || index > 255)return 0;address=(int)index;}
  if(mac && mac!=13 && mm && !(ilu && dst==1)){if(dst>12)return 0;nf_vp_mask(r[dst],mv,mm);if(dst==12)nf_vp_mask(out[0],mv,mm);}
  if(ilu && im){unsigned d=mac?1:dst;if(d>12)return 0;nf_vp_mask(r[d],iv,im);if(d==12)nf_vp_mask(out[0],iv,im);}
  if(om){unsigned output=d->output;if(!d->output_register || output>=16)return 0;
   const float *v=d->scalar?iv:mv;nf_vp_mask(out[output],v,om);if(!output)nf_vp_mask(r[12],v,om);}
  if(d->final){for(unsigned k=0;k<4;k++)if(!isfinite(out[0][k]))return 0;return 1;}
 }
 return 0;
}
#endif
