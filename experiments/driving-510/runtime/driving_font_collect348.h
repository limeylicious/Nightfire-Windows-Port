/* Analysis-only bounded inline QUADS collector. Caller owns exact code/state
 * admission, resources, synchronous submission and command replay. No draw or
 * guest memory access here. On -1, replay retained prefix BEFORE current method;
 * the rejected current method was not retained. Never silently drop a command.
 * Begin receives previously tracked immediate attributes and component masks.
 * Array descriptors are stale state and must NOT fetch data for inline draws. */
#ifndef INLINE223_H
#define INLINE223_H
#include <stdint.h>
#include <string.h>
#include <math.h>
enum {INLINE223_VERTICES=1024,INLINE223_COMMANDS=8192};
typedef struct {
 float current[16][4],vertices[INLINE223_VERTICES][16][4];
 unsigned masks[16],count,commands,active;
 uint32_t prefix[INLINE223_COMMANDS][2];
} Inline223;
static int inline223_begin(Inline223 *s,const float current[16][4],const unsigned masks[16])
{
 if(!s||!current||!masks||s->active)return 0;
 memcpy(s->current,current,sizeof s->current);memcpy(s->masks,masks,sizeof s->masks);
 s->count=0;s->commands=1;s->prefix[0][0]=0x17fc;s->prefix[0][1]=8;s->active=1;return 1;
}
/* 0 held, 1 complete valid quads, -1 refusal requiring exact prefix replay. */
static int inline223_feed(Inline223 *s,unsigned m,uint32_t v)
{
 if(!s||!s->active||s->commands==INLINE223_COMMANDS)return -1;
 if(m==0x17fc&&!v){
  if(!s->count||s->count%4)return -1;
  s->prefix[s->commands][0]=m;s->prefix[s->commands++][1]=v;s->active=0;return 1;
 }
 unsigned slot,k;
 if(m>=0x1880&&m<0x1900&&!(m&3)){
  slot=(m-0x1880)/8;k=(m&7)/4;
  if(slot!=9)return -1;
  float f;memcpy(&f,&v,4);if(!isfinite(f))return -1;
  s->current[slot][k]=f;s->current[slot][2]=0;s->current[slot][3]=1;s->masks[slot]|=(1u<<k)|12;
 }else if(m==0x194c){
  for(k=0;k<4;k++)s->current[3][k]=(float)(v>>(k*8)&255)/255.f;
  s->masks[3]=15;
 }else if(m>=0x1518&&m<=0x1524&&!(m&3)){
  float f;memcpy(&f,&v,4);if(!isfinite(f))return -1;k=(m-0x1518)/4;
  /* Validate a would-be emit before changing held state. Original methods1518
   * through1524 supply all four components;1524 is the vertex emit boundary. */
  if(k==3&&(s->count==INLINE223_VERTICES||((s->masks[0]|8)!=15)||s->masks[3]!=15||s->masks[9]!=15))return -1;
  s->current[0][k]=f;s->masks[0]|=1u<<k;
  if(k==3){
   for(unsigned a=0;a<16;a++)if(a==0||a==3||a==9)for(unsigned c=0;c<4;c++)if(!isfinite(s->current[a][c]))return -1;
   memcpy(s->vertices[s->count++],s->current,sizeof s->current);
  }
 }else return -1;
 s->prefix[s->commands][0]=m;s->prefix[s->commands++][1]=v;return 0;
}
/* Convex, captured axis-aligned rectangles only. W and texture Q must be1;
 * reject general projective/nonplanar quads until their diagonal is verified. */
static int inline223_rectangles(const Inline223 *s)
{
 if(!s||s->active||!s->count||s->count%4)return 0;
 for(unsigned i=0;i<s->count;i+=4){
  const float (*a)[4]=s->vertices[i],(*b)[4]=s->vertices[i+1],(*c)[4]=s->vertices[i+2],(*d)[4]=s->vertices[i+3];
  if(a[0][0]!=d[0][0]||b[0][0]!=c[0][0]||a[0][1]!=b[0][1]||c[0][1]!=d[0][1]||a[0][0]>=b[0][0]||a[0][1]>=d[0][1])return 0;
  if(a[9][0]!=d[9][0]||b[9][0]!=c[9][0]||a[9][1]!=b[9][1]||c[9][1]!=d[9][1])return 0;
  for(unsigned j=0;j<4;j++){
   const float (*v)[4]=s->vertices[i+j];
   if(v[0][2]!=a[0][2]||v[0][3]!=1||v[9][2]!=0||v[9][3]!=1||memcmp(v[3],a[3],16))return 0;
  }
 }
 return 1;
}
#endif
