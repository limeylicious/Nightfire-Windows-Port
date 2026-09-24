#ifndef DRIVING_DEPTH202_H
#define DRIVING_DEPTH202_H
#include "driving_surface145.h"
/* Observed pitched integer Z24S8 and scoped Z16; original clear bits/rectangle.
 * Unlike rasterized AA coverage, clears assign the same value to both lanes. */
static int driving_depth202(void *memory,size_t size,const DrivingSurface145 *s,
 uint32_t horizontal,uint32_t vertical,uint32_t flags,uint32_t value){
 unsigned x0=horizontal&65535,x1=horizontal>>16,y0=vertical&65535,y1=vertical>>16;
 if(!memory||!s||size<s->span||((s->format&0xffffu)!=0x128&&(s->format&0xffffu)!=0x1128&&(s->format&0xffffu)!=0x113)||
    x0>x1||y0>y1||x1>=s->width||y1>=s->height||(flags&~0xf3u))return 0;
 unsigned bytes=(s->format&0xffffu)==0x113?2u:4u;
 /* Z16 has no stencil component; its original clear is unshifted low16. */
 uint32_t mask=bytes==2?(flags&1?0xffffu:0):((flags&1?0xffffff00u:0)|(flags&2?255u:0));
 if(!mask)return 1;
#ifdef DRIVING_ACCESS321
 for(unsigned y=y0;y<=y1;y++){
  const void *row=(unsigned char*)memory+(size_t)y*s->pitch+(size_t)x0*s->lanes*bytes;
  size_t n=(size_t)(x1-x0+1)*s->lanes*bytes;
  driving_access321_note(row,n,DA321_DEPTH_CLEAR_READ);
  driving_access321_note(row,n,DA321_DEPTH_CLEAR_WRITE);
 }
#endif
 for(unsigned y=y0;y<=y1;y++)for(unsigned x=x0*s->lanes;x<(x1+1)*s->lanes;x++){
  unsigned char *p=(unsigned char*)memory+(size_t)y*s->pitch+x*bytes;uint32_t old=0;
  memcpy(&old,p,bytes);old=(old&~mask)|(value&mask);memcpy(p,&old,bytes);
 }return 1;
}
#endif
