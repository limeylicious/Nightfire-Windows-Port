/*392 candidate: single result store for the existing generic Kelvin translation.
 * Avoids the D3D compiler's unread-predicate failure with repeated full-output
 * stores inside early returns. Arithmetic and source/error order are unchanged.
 * Comparison-only until independent output/state/failure validation.
 *351 generic Kelvin compute translation.
 * Reconstructs the currently understood instruction set and every result slot.
 * Unlike the legacy one-UV HLSL route, errors are returned to the host with
 * original instruction/constant indices. No invalid primitive is silently culled.
 * Shader arithmetic still requires independent CPU/GPU equivalence checks. */
#ifndef NIGHTFIRE_VERTEX_COMPUTE351_H
#define NIGHTFIRE_VERTEX_COMPUTE351_H
#include "nightfire_vertex_program.h"
#include <stdio.h>
#include <stdarg.h>
typedef struct NFComputeSource351 {
 char text[131072];size_t length;unsigned failed,inputs,count;
 uint32_t constants[6];unsigned relative;
} NFComputeSource351;
static void compute_text351(NFComputeSource351 *b,const char *format,...)
{
 if(b->failed)return;va_list args;va_start(args,format);
 int n=vsnprintf(b->text+b->length,sizeof b->text-b->length,format,args);va_end(args);
 if(n<0||(size_t)n>=sizeof b->text-b->length){b->failed=1;return;}b->length+=(size_t)n;
}
static int compute_operand351(NFComputeSource351*b,char result[96],unsigned packed,
                             unsigned vi,unsigned ci,unsigned indexed,unsigned pc)
{
 unsigned mux=packed&3,reg=packed>>2&15,swz=packed>>6&255;
 char base[64],sw[5];for(unsigned i=0;i<4;i++)sw[i]="xyzw"[(swz>>(6-2*i))&3];sw[4]=0;
 if(mux==1){if(reg>12)return 0;snprintf(base,sizeof base,"r[%u]",reg);}
 else if(mux==2){b->inputs|=1u<<vi;snprintf(base,sizeof base,"input351[id*16+%u]",vi);}
 else if(mux==3){
  if(indexed){b->relative=1;snprintf(base,sizeof base,"kc[slot]");}
  else{if(ci>=192)return 0;b->constants[ci/32]|=1u<<(ci%32);snprintf(base,sizeof base,"kc[%u]",ci);}
  /* Preserve lazy source fetch order, including the first missing constant. */
  if(indexed)compute_text351(b,"if(slot>=192){status392=0;pc392=%u;constant392=slot;break;}if(valid351[slot>>2][slot&3]!=15){status392=0;pc392=%u;constant392=slot;break;}\n",pc,pc);
  else compute_text351(b,"if(valid351[%u][%u]!=15){status392=0;pc392=%u;constant392=%u;break;}\n",ci/4,ci%4,pc,ci);
 }else return 0;
 snprintf(result,96,"%s%s.%s",packed&0x4000?"-":"",base,sw);return 1;
}
static void compute_mask351(NFComputeSource351*b,const char*dst,const char*src,unsigned mask)
{
 if(!mask)return;char sw[5];unsigned n=0;
 for(unsigned i=0;i<4;i++)if(mask&(8u>>i))sw[n++]="xyzw"[i];sw[n]=0;
 compute_text351(b,"%s.%s=%s.%s;\n",dst,sw,src,sw);
}
static int nf_vertex_compute351(NFComputeSource351*b,const NFVertexProgram*p)
{
 if(!b||!p)return 0;memset(b,0,sizeof *b);
 if((p->mode&3)!=2||p->start>=136)return 0;
 compute_text351(b,
  "cbuffer C351:register(b0){float4 kc[192];uint4 valid351[48];uint4 params351;}\n"
  "StructuredBuffer<float4> input351:register(t0);\n"
  "struct R351{float4 o[16];uint valid;uint pc;uint constant;uint reserved;};\n"
  "RWStructuredBuffer<R351> result351:register(u0);\n"
  "void finish351(uint id,float4 o[16],uint valid,uint pc,uint constant){R351 r;"
  "[unroll]for(uint i=0;i<16;i++)r.o[i]=o[i];r.valid=valid;r.pc=pc;r.constant=constant;r.reserved=0;result351[id]=r;}\n"
  "[numthreads(64,1,1)]void transform(uint3 thread:SV_DispatchThreadID){"
  "uint id=thread.x;if(id>=params351.x)return;precise float4 r[13];precise float4 o[16];int address=0;\n"
  "[unroll]for(uint i=0;i<13;i++)r[i]=0;[unroll]for(uint j=0;j<16;j++)o[j]=0;"
  "o[0].w=1;[unroll]for(uint q=3;q<=12;q++)o[q].w=1;r[12].w=1;"
  "uint status392=0,pc392=0xffffffff,constant392=0xffffffff;do{\n");
 for(unsigned pc=p->start;pc<136;pc++){
  if(p->valid[pc]!=15)return 0;const uint32_t*w=p->code[pc];
  unsigned mac=nf_vp_field(w,85,4),ilu=nf_vp_field(w,89,3),ci=nf_vp_field(w,77,8),vi=nf_vp_field(w,73,4);
  unsigned dst=nf_vp_field(w,20,4),mm=nf_vp_field(w,24,4),im=nf_vp_field(w,16,4),om=nf_vp_field(w,12,4);
  unsigned indexed=nf_vp_field(w,1,1),scalar=nf_vp_field(w,2,1),output=nf_vp_field(w,3,8);
  if(mac==8||mac>13||ilu>5)return 0;
  if(ilu==5&&((im|(scalar?om:0))&~4u))return 0;
  if(om&&(!nf_vp_field(w,11,1)||output>=16))return 0;
  if(mac&&mac!=13&&mm&&!(ilu&&dst==1)&&dst>12)return 0;
  if(ilu&&im&&(mac?1:dst)>12)return 0;
  compute_text351(b,"{uint slot=(uint)(%u+address);\n",ci);
  char a[96]="float4(0,0,0,0)",c[96]="float4(0,0,0,0)",d[96]="float4(0,0,0,0)";
  if(mac&&!compute_operand351(b,a,nf_vp_field(w,58,15),vi,ci,indexed,pc))return 0;
  compute_text351(b,"precise float4 a=%s;\n",a);
  if((mac==2||mac==4||mac==5||mac==6||mac==7||(mac>=9&&mac<=12))&&
     !compute_operand351(b,d,nf_vp_field(w,43,15),vi,ci,indexed,pc))return 0;
  compute_text351(b,"precise float4 b=%s;\n",d);
  if((mac==3||mac==4||ilu)&&!compute_operand351(b,c,nf_vp_field(w,28,15),vi,ci,indexed,pc))return 0;
  compute_text351(b,"precise float4 c=%s,mv=0,iv=0;\n",c);
  switch(mac){
  case 0:case 13:break;
  case 1:compute_text351(b,"mv=a;\n");break;
  case 2:compute_text351(b,"mv=a*b;\n");break;
  case 3:compute_text351(b,"mv=a+c;\n");break;
  case 4:compute_text351(b,"mv=a*b+c;\n");break;
  case 5:case 6:case 7:
   compute_text351(b,"precise float dotv=0;dotv+=a.x*b.x;dotv+=a.y*b.y;dotv+=a.z*b.z;\n");
   if(mac==7)compute_text351(b,"dotv+=a.w*b.w;\n");
   if(mac==6)compute_text351(b,"dotv+=b.w;\n");
   compute_text351(b,"mv=dotv;\n");break;
  case 9:compute_text351(b,"mv=min(a,b);\n");break;
  case 10:compute_text351(b,"mv=max(a,b);\n");break;
  case 11:compute_text351(b,"mv=float4(a.x<b.x,a.y<b.y,a.z<b.z,a.w<b.w);\n");break;
  case 12:compute_text351(b,"mv=float4(a.x>=b.x,a.y>=b.y,a.z>=b.z,a.w>=b.w);\n");break;
  }
  switch(ilu){
  case 0:break;
  case 1:compute_text351(b,"iv=c;\n");break;
  case 2:compute_text351(b,"iv=1.0/c.x;\n");break;
  case 3:compute_text351(b,"float reciprocal=1.0/c.x;float mag=clamp(abs(reciprocal),5.4210109e-20,1.8446744e19);iv=asfloat(asuint(mag)|(asuint(reciprocal)&0x80000000));\n");break;
  case 4:compute_text351(b,"iv=1.0/sqrt(abs(c.x));\n");break;
  case 5:compute_text351(b,"if(!isfinite(c.x)||c.x< -126||c.x>127){status392=0;pc392=%u;constant392=0xffffffff;break;}iv.y=c.x-floor(c.x);\n",pc);break;
  }
  /* Both arithmetic units complete before any register or result write. */
  if(mac==13)compute_text351(b,"float index=floor(a.x);if(!isfinite(index)||index< -256||index>255){status392=0;pc392=%u;constant392=0xffffffff;break;}address=(int)index;\n",pc);
  char dest[32];
  if(mac&&mac!=13&&mm&&!(ilu&&dst==1)){
   snprintf(dest,sizeof dest,"r[%u]",dst);compute_mask351(b,dest,"mv",mm);
   if(dst==12)compute_mask351(b,"o[0]","mv",mm);
  }
  if(ilu&&im){unsigned reg=mac?1:dst;snprintf(dest,sizeof dest,"r[%u]",reg);
   compute_mask351(b,dest,"iv",im);if(reg==12)compute_mask351(b,"o[0]","iv",im);
  }
  if(om){snprintf(dest,sizeof dest,"o[%u]",output);compute_mask351(b,dest,scalar?"iv":"mv",om);
   if(!output)compute_mask351(b,"r[12]",scalar?"iv":"mv",om);
  }
  if(w[3]&1){compute_text351(b,"status392=all(isfinite(o[0]))?1:0;pc392=%u;constant392=0xffffffff;break;} }while(false);finish351(id,o,status392,pc392,constant392);}\n",pc);
   b->count=pc-p->start+1;return !b->failed;
  }
  compute_text351(b,"}\n");
 }
 return 0;
}
#endif
