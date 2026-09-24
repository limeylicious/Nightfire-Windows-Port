/* Translate supported Kelvin programs to D3D11 vertex shaders.
 * Shares instruction decoding with the existing independent CPU executor.
 * Indexed constant reads validate uploaded slots on the GPU. Unsupported
 * operations return 0 and keep the CPU path available.
 */
#ifndef NIGHTFIRE_VERTEX_HLSL_H
#define NIGHTFIRE_VERTEX_HLSL_H
#include "nightfire_vertex_program.h"
#include "nightfire_fog.h"
#include <stdio.h>
#include <stdarg.h>
typedef struct {char text[65536];size_t length;int failed;unsigned inputs,dynamic;uint32_t constants[6];} NFVertexHlsl;
static void nf_hlsl_add(NFVertexHlsl *b,const char *fmt,...){
 if(b->failed)return;va_list a;va_start(a,fmt);int n=vsnprintf(b->text+b->length,sizeof b->text-b->length,fmt,a);va_end(a);
 if(n<0 || (size_t)n>=sizeof b->text-b->length){b->failed=1;return;}b->length+=(size_t)n;
}
static int nf_hlsl_source(NFVertexHlsl *b,char *dst,unsigned packed,unsigned vi,unsigned ci,unsigned indexed){
 unsigned mux=packed&3,reg=(packed>>2)&15,swz=(packed>>6)&255;char base[32],sw[5];
 for(unsigned k=0;k<4;k++)sw[k]="xyzw"[(swz>>(6-2*k))&3];sw[4]=0;
 if(mux==1){if(reg>12)return 0;snprintf(base,sizeof base,"r[%u]",reg);}
 else if(mux==2){snprintf(base,sizeof base,"v.a%u",vi);b->inputs|=1u<<vi;}
 else if(mux==3){
  if(indexed){snprintf(base,sizeof base,"kc[slot]");b->dynamic=1;}
  else {if(ci>=192)return 0;snprintf(base,sizeof base,"kc[%u]",ci);b->constants[ci/32]|=1u<<(ci%32);}
 }
 else return 0;
 snprintf(dst,64,"%s%s.%s",packed&0x4000?"-":"",base,sw);return 1;
}
static void nf_hlsl_mask(NFVertexHlsl *b,const char *dst,const char *src,unsigned mask){
 if(!mask)return;char sw[5];unsigned n=0;for(unsigned k=0;k<4;k++)if(mask&(8u>>k))sw[n++]="xyzw"[k];sw[n]=0;
 nf_hlsl_add(b,"%s.%s=%s.%s;\n",dst,sw,src,sw);
}
static int nf_vertex_hlsl(NFVertexHlsl *b,const NFVertexProgram *s){
 memset(b,0,sizeof *b);if((s->mode&3)!=2 || s->start>=136)return 0;
 nf_hlsl_add(b,NF_FOG_PARAMS_HLSL NF_FOG_HLSL "\ncbuffer Kelvin:register(b1){float4 kc[192];uint4 known[2];}\nstruct V{");
 for(unsigned i=0;i<16;i++)nf_hlsl_add(b,"float4 a%u:TEXCOORD%u;",i,i);
 /* Keep pixel inputs in the same signature slots as the fixed vertex shader:
  * fog precedes the geometry-only validity output. D3D11 links by register. */
 nf_hlsl_add(b,"};struct P{float4 p:SV_POSITION;float2 uv:TEXCOORD0;float4 c:COLOR0;float fog:TEXCOORD2;float ok:TEXCOORD1;};\nP invalid(){P p;p.p=0;p.uv=0;p.c=0;p.ok=0;p.fog=1;return p;}\nP vertex(V v){precise float4 r[13];precise float4 o[16];int address=0;");
 for(unsigned i=0;i<13;i++)nf_hlsl_add(b,"r[%u]=0;",i);
 for(unsigned i=0;i<16;i++)nf_hlsl_add(b,"o[%u]=0;",i);
 for(unsigned pc=s->start;pc<136;pc++){
  if(s->valid[pc]!=15)return 0;const uint32_t *w=s->code[pc];
  unsigned mac=nf_vp_field(w,85,4),ilu=nf_vp_field(w,89,3),ci=nf_vp_field(w,77,8),vi=nf_vp_field(w,73,4);
  unsigned dst=nf_vp_field(w,20,4),mm=nf_vp_field(w,24,4),im=nf_vp_field(w,16,4),om=nf_vp_field(w,12,4);
  unsigned indexed=nf_vp_field(w,1,1);
  if(mac==8 || mac>13 || ilu>4)return 0;
  if(mac==13)b->dynamic=1;
  char a[64]="float4(0,0,0,0)",c[64]="float4(0,0,0,0)",d[64]="float4(0,0,0,0)";
  if(mac && !nf_hlsl_source(b,a,nf_vp_field(w,58,15),vi,ci,indexed))return 0;
  if((mac==2 || mac==4 || mac==5 || mac==6 || mac==7 || (mac>=9 && mac<=12)) && !nf_hlsl_source(b,d,nf_vp_field(w,43,15),vi,ci,indexed))return 0;
  if((mac==3 || mac==4 || ilu) && !nf_hlsl_source(b,c,nf_vp_field(w,28,15),vi,ci,indexed))return 0;
  nf_hlsl_add(b,"{");
  if(indexed && (strstr(a,"kc[slot]") || strstr(d,"kc[slot]") || strstr(c,"kc[slot]"))){
   nf_hlsl_add(b,"int slot=%u+address;if(slot<0 || slot>=192)return invalid();if(((known[slot>>7][(slot>>5)&3]>>(slot&31))&1)==0)return invalid();",ci);
  }
  nf_hlsl_add(b,"precise float4 a=%s,b=%s,c=%s,mv=0,iv=0;\n",a,d,c);
  switch(mac){
   case 0:break;case 1:nf_hlsl_add(b,"mv=a;");break;case 2:nf_hlsl_add(b,"mv=a*b;");break;
   case 3:nf_hlsl_add(b,"mv=a+c;");break;case 4:nf_hlsl_add(b,"mv=a*b+c;");break;
   case 5:case 6:case 7:nf_hlsl_add(b,"precise float dotv=0;dotv+=a.x*b.x;dotv+=a.y*b.y;dotv+=a.z*b.z;");
    if(mac==7)nf_hlsl_add(b,"dotv+=a.w*b.w;");if(mac==6)nf_hlsl_add(b,"dotv+=b.w;");nf_hlsl_add(b,"mv=dotv;");break;
   case 9:nf_hlsl_add(b,"mv=min(a,b);");break;case 10:nf_hlsl_add(b,"mv=max(a,b);");break;
   case 11:nf_hlsl_add(b,"mv=float4(a.x<b.x,a.y<b.y,a.z<b.z,a.w<b.w);");break;
   case 12:nf_hlsl_add(b,"mv=float4(a.x>=b.x,a.y>=b.y,a.z>=b.z,a.w>=b.w);");break;
   case 13:nf_hlsl_add(b,"float addr=floor(a.x);if(!isfinite(addr) || addr< -256 || addr>255)return invalid();address=(int)addr;");break;
  }
  switch(ilu){case 0:break;case 1:nf_hlsl_add(b,"iv=c;");break;case 2:nf_hlsl_add(b,"iv=1.0/c.x;");break;
   case 3:nf_hlsl_add(b,"float q=1.0/c.x;iv=(asuint(q)&0x80000000)?-clamp(abs(q),5.4210109e-20,1.8446744e19):clamp(abs(q),5.4210109e-20,1.8446744e19);");break;
   case 4:nf_hlsl_add(b,"iv=1.0/sqrt(abs(c.x));");break;}
  char dest[32];
  if(mac && mac!=13 && mm && !(ilu && dst==1)){if(dst>12)return 0;snprintf(dest,sizeof dest,"r[%u]",dst);nf_hlsl_mask(b,dest,"mv",mm);if(dst==12)nf_hlsl_mask(b,"o[0]","mv",mm);}
  if(ilu && im){unsigned reg=mac?1:dst;if(reg>12)return 0;snprintf(dest,sizeof dest,"r[%u]",reg);nf_hlsl_mask(b,dest,"iv",im);if(reg==12)nf_hlsl_mask(b,"o[0]","iv",im);}
  if(om){unsigned output=nf_vp_field(w,3,8);if(!nf_vp_field(w,11,1)||output>=16)return 0;snprintf(dest,sizeof dest,"o[%u]",output);const char *src=nf_vp_field(w,2,1)?"iv":"mv";nf_hlsl_mask(b,dest,src,om);if(!output)nf_hlsl_mask(b,"r[12]",src,om);}
  nf_hlsl_add(b,"}\n");
  if(w[3]&1){nf_hlsl_add(b,"if(!all(isfinite(o[0])))return invalid();P p;p.p=float4((o[0].x*2/surface.x-1)*o[0].w,(1-o[0].y*2/surface.y)*o[0].w,o[0].z/16777215*o[0].w,o[0].w);p.uv=o[9].xy;p.c=floor(saturate(o[3])*255)/255;p.ok=1;p.fog=fogFactor(o[5].x);return p;}\n");return !b->failed;}
 }
 return 0;
}
#endif
