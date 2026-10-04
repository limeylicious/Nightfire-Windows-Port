/* Independently written bounded Kelvin combiner emitter. Raw command words,
 * not the toolkit's differently encoded D3D pixel state. No emulator code.
 * Admits observed Driving output routes; unknown forms remain explicit refusal. */
#ifndef NF_PIXEL221_H
#define NF_PIXEL221_H
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
typedef struct {uint32_t control,program,rgb[8],alpha[8],rgb_out[8],alpha_out[8],final0,final1,constant0[8],constant1[8],final_constant0,final_constant1;
 /* Derived opt-in compatibility flag, included in shader key. Raw original
  * words and vertex outputs remain unchanged. Never accept unchecked data. */
 uint32_t white_stage2_242;
} NFPixel221;

static int nf_pixel_white_contract242(const NFPixel221 *p){
 static const uint32_t rgb[3]={0x18c8c938,0xc4cc0000,0xccca0000},alpha[3]={0x18d8d938,0xd4dc1010,0xdcda1010},ow[3]={0xc00,0x100c0,0xc0};
 return p&&p->control==0x11103&&p->program==0x421&&p->final0==0x130c0300&&p->final1==0x2080&&
  !memcmp(p->rgb,rgb,sizeof rgb)&&!memcmp(p->alpha,alpha,sizeof alpha)&&
  !memcmp(p->rgb_out,ow,sizeof ow)&&!memcmp(p->alpha_out,ow,sizeof ow);
}
/* Exact captured290 two-stage lit texture, distinct proof tag2. */
static int nf_pixel_white_contract292(const NFPixel221 *p){
 static const uint32_t rgb[2]={0xc4c80000,0xccca0000},alpha[2]={0xd4d81010,0xdcda1010},ow[2]={0x100c0,0xc0};
 return p&&p->control==0x11102&&p->program==0x421&&p->final0==0x130c0300&&p->final1==0x1880&&
  !memcmp(p->rgb,rgb,sizeof rgb)&&!memcmp(p->alpha,alpha,sizeof alpha)&&
  !memcmp(p->rgb_out,ow,sizeof ow)&&!memcmp(p->alpha_out,ow,sizeof ow);
}
/* Only the explicitly specialized stage can retain its exact original zero
 * vector. Other stages retain the ordinary divisor/domain checks. */
static int nf_pixel_zero_uv242(const NFPixel221 *p,unsigned stage,const float *uv){
 return (p->white_stage2_242==1||p->white_stage2_242==2)&&stage==2&&uv[0]==0&&uv[1]==0&&uv[2]==0&&uv[3]==0;
}
typedef struct {char *out;size_t cap,n;int ok;} NFText221;
static void nf_text221(NFText221 *b,const char *fmt,...){
 if(!b->ok)return;va_list ap;va_start(ap,fmt);int n=vsnprintf(b->out+b->n,b->cap-b->n,fmt,ap);va_end(ap);
 if(n<0||(size_t)n>=b->cap-b->n){b->ok=0;return;}b->n+=(size_t)n;
}
static int nf_reg221(unsigned r){return r<=2||r==3||r==4||r==5||(r>=8&&r<=13);}
static int nf_pixel221_valid(const NFPixel221 *p){
 if(!p || (p->control&~15u)!=0x11100 || !(p->control&15) || (p->control&15)>8 || (p->program&~0xfffffu))return 0;
 if(p->white_stage2_242&&!((p->white_stage2_242==1&&nf_pixel_white_contract242(p))||
    (p->white_stage2_242==2&&nf_pixel_white_contract292(p))))return 0;
 for(unsigned i=0;i<4;i++){unsigned mode=(p->program>>(5*i))&31;if(mode!=0&&mode!=1&&mode!=4)return 0;}
 for(unsigned i=0;i<(p->control&15);i++)for(unsigned a=0;a<2;a++){
  uint32_t iw=a?p->alpha[i]:p->rgb[i],ow=a?p->alpha_out[i]:p->rgb_out[i];
  for(unsigned k=0;k<4;k++)if(!nf_reg221((iw>>(8*k))&15))return 0;
  if(ow!=0 && ow!=0xb0 && ow!=0xc0 && ow!=0xc00 && ow!=0x4c00 && ow!=0x100c0 && !(ow==0xc30cb&&!a))return 0;
  if(a && ow==0xc30cb)return 0;
 }
 if((p->final1&0xffff00ffu)!=0x80)return 0; /* E/F unused, sum clamp flag only. */
 for(unsigned k=0;k<4;k++){unsigned v=(p->final0>>(8*k))&255;if(!nf_reg221(v&15)||(v>>5)>1)return 0;}
 {unsigned v=(p->final1>>8)&255;if(!nf_reg221(v&15)||(v>>5)>1)return 0;}
 return 1;
}
static void nf_input221(char out[80],unsigned byte,int alpha){
 snprintf(out,80,"map221(rr[%u],%u).%s",byte&15,byte>>5,alpha?(byte&16?"a":"b"):(byte&16?"aaa":"rgb"));
}
static void nf_color221(NFText221 *b,unsigned reg,uint32_t value){
 nf_text221(b,"rr[%u]=float4(%u,%u,%u,%u)/255.0;\n",reg,value>>16&255,value>>8&255,value&255,value>>24);
}
static int nf_pixel221_emit(const NFPixel221 *p,char *out,size_t cap){
 if(!out||!cap||!nf_pixel221_valid(p))return 0;NFText221 b={out,cap,0,1};
 nf_text221(&b,"float4 map221(float4 x,uint m){if(m==0)return max(x,0);if(m==1)return 1-saturate(x);if(m==2)return 2*max(x,0)-1;if(m==3)return 1-2*max(x,0);if(m==4)return max(x,0)-.5;if(m==5)return .5-max(x,0);if(m==6)return x;return -x;}\n");
 nf_text221(&b,"Texture2D tx0:register(t0),tx1:register(t1),tx2:register(t2),tx3:register(t3);SamplerState ss0:register(s0),ss1:register(s1),ss2:register(s2),ss3:register(s3);\n");
 nf_text221(&b,"float4 pixel(P p):SV_TARGET{float4 rr[16];[unroll]for(uint i=0;i<16;i++)rr[i]=0;rr[3]=float4(fogColor.rgb,saturate(p.fog));rr[4]=p.c;rr[5]=p.spec;\n");
 for(unsigned i=0;i<4;i++){
  unsigned mode=(p->program>>(5*i))&31;
  if(p->white_stage2_242&&i==2)nf_text221(&b,"rr[10]=float4(1,1,1,1); // verified owned neutral texture242\n");
  else if(mode==1)nf_text221(&b,"rr[%u]=tx%u.Sample(ss%u,p.uv%u.xy/p.uv%u.w);\n",8+i,i,i,i,i);
  if(mode==4)nf_text221(&b,"rr[%u]=p.uv%u;\n",8+i,i); /* Caller bounds passthrough to[0,1]. */
 }
 nf_text221(&b,"rr[12].a=rr[8].a;\n");
 for(unsigned i=0;i<(p->control&15);i++){
  nf_text221(&b,"{\n");nf_color221(&b,1,p->constant0[i]);nf_color221(&b,2,p->constant1[i]);
  for(unsigned a=0;a<2;a++){
   uint32_t iw=a?p->alpha[i]:p->rgb[i],ow=a?p->alpha_out[i]:p->rgb_out[i];char v[4][80];for(unsigned k=0;k<4;k++)nf_input221(v[k],(iw>>(24-8*k))&255,a);
   if(ow==0xc30cb)nf_text221(&b,"float3 ab%u=dot(%s,%s),cd%u=dot(%s,%s);\n",a,v[0],v[1],a,v[2],v[3]);
   else nf_text221(&b,"%s ab%u=%s*%s,cd%u=%s*%s;\n",a?"float":"float3",a,v[0],v[1],a,v[2],v[3]);
   nf_text221(&b,"%s sum%u=%s;\n",a?"float":"float3",a,ow&0x4000?(a?"rr[12].a>=.5?cd1:ab1":"rr[12].a>=.5?cd0:ab0"):(a?"ab1+cd1":"ab0+cd0"));
   nf_text221(&b,"ab%u=clamp(ab%u*%u,-1,1);cd%u=clamp(cd%u*%u,-1,1);sum%u=clamp(sum%u*%u,-1,1);\n",a,a,ow&0x10000?2:1,a,a,ow&0x10000?2:1,a,a,ow&0x10000?2:1);
  }
  /* Both units read the pre-stage register set before either commits. */
  for(unsigned a=0;a<2;a++){
   uint32_t ow=a?p->alpha_out[i]:p->rgb_out[i];unsigned dest[3]={ow>>4&15,ow&15,ow>>8&15};const char *val[3]={"ab","cd","sum"};
   for(unsigned k=0;k<3;k++)if(dest[k]){nf_text221(&b,"rr[%u].%s=%s%u;\n",dest[k],a?"a":"rgb",val[k],a);if(!a&&((k==0&&(ow&0x80000))||(k==1&&(ow&0x40000))))nf_text221(&b,"rr[%u].a=%s0.b;\n",dest[k],val[k]);}
  }
  nf_text221(&b,"}\n");
 }
 nf_color221(&b,1,p->final_constant0);nf_color221(&b,2,p->final_constant1);
 char v[5][80];for(unsigned i=0;i<4;i++)nf_input221(v[i],p->final0>>(24-8*i)&255,0);nf_input221(v[4],p->final1>>8&255,1);
 nf_text221(&b,"float4 result=saturate(float4(%s*%s+(1-%s)*%s+%s,%s));\n",v[0],v[1],v[0],v[2],v[3],v[4]);
 nf_text221(&b,"if(alphaOn!=0){float a=floor(result.a*255+.5);bool ok=alphaFunc==0?false:alphaFunc==1?a<alphaRef:alphaFunc==2?a==alphaRef:alphaFunc==3?a<=alphaRef:alphaFunc==4?a>alphaRef:alphaFunc==5?a!=alphaRef:alphaFunc==6?a>=alphaRef:true;if(!ok)discard;}return result;}\n");
 return b.ok;
}
#endif
