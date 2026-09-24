#ifndef DRIVING_PLAN201_H
#define DRIVING_PLAN201_H
#include "immediate195.h"
#include "driving_contract201.h"
#include "driving_inactive306.h"
/* Narrow observed movie/resolve contract. No host reads or guest writes. */
static int driving_plan201(const DrivingDraw195 *d,unsigned *family){
 const uint32_t *s=d->state;unsigned r=s[0x1b04/4]==0x11229;
 unsigned world222=0;
#ifdef NIGHTFIRE_WORLD_RESOLVE222
 world222=r&&s[0x1b14/4]==0x04073f01;
#endif
 if(s[0x1b04/4]!=(r?0x11229u:0x11129u)||d->primitive!=(r?5u:6u))return 0;
 const unsigned fields[]={0x200,0x204,0x208,0x20c,0x210,0x194,0x184,0x1b00,0x1b04,0x1b08,0x1b10,0x1b14,0x1b1c,0x2b4,0x2c0,0x2e0,0x17bc};
 for(unsigned i=0;i<sizeof fields/sizeof fields[0];i++)if(!d->known[fields[i]/4])return 0;
 if(s[0x200/4]!=0x2800000||s[0x204/4]!=0x1e00000||s[0x208/4]!=(r?0x128u:0x1128u)||
    s[0x20c/4]!=(r?0xa000a00u:0x14001400u)||
    s[0x1b1c/4]!=(r?0x50001e0u:0x28001e0u)||s[0x1b10/4]!=(r?0x14000000u:0x5000000u)||
    s[0x1b14/4]!=(r?(world222?0x04073f01u:0x04072000u):0x02062000u)||
    (s[0x1b08/4]!=0x10303&&s[0x1b08/4]!=0x30303)||
    s[0x2b4/4]||s[0x2c0/4]!=0x27f0000||s[0x2e0/4]!=0x1df0000||s[0x17bc/4])return 0;
 for(unsigned i=0;i<sizeof driving_fields201/sizeof driving_fields201[0];i++){
  unsigned m=driving_fields201[i].method;if(!d->known[m/4])return 0;
  if(driving_stencil306(s,d->known,m))continue;
  /* A disabled depth test does not consume its write-mask bit. */
  if(r&&m==0x35c){if(s[m/4]>1)return 0;continue;}
  /* Raw world resolve carries different inactive blend/depth selectors.
   * Keep them exact in this captured variant; enable bits stay pinned0. */
  if(world222 && (m==0x344||m==0x348||m==0x354)){
   if(s[m/4]!=(m==0x344?0x302u:m==0x348?0x303u:0x207u))return 0;continue;
  }
  /* Original world resolve disables culling; its retained face selector can
   * be FRONT or BACK without affecting this draw. Enable remains pinned0. */
  if(world222 && m==0x39c){if(s[m/4]!=0x404 && s[m/4]!=0x405)return 0;continue;}
  if(s[m/4]!=(r?driving_fields201[i].resolve:driving_fields201[i].movie))return 0;
 }
 const NFVertexProgram *p=&d->program;unsigned instructions=r?2:9;
 if(p->mode!=6||p->start)return 0;
 for(unsigned i=0;i<instructions;i++)if(p->valid[i]!=15||memcmp(p->code[i],driving_program201[r][i],16))return 0;
 if(!r){const unsigned constants[]={58,59,96,97,98,99};for(unsigned i=0;i<6;i++)if(p->constant_valid[constants[i]]!=15)return 0;}
 *family=r;return 1;
}
static int driving_vertices201(const DrivingDraw195 *d){
 if(d->invalid||d->count!=(d->primitive==6?4u:3u))return 0;
 for(unsigned i=0;i<d->count;i++){
  if(d->masks[i][0]!=15||d->masks[i][9]!=15||(d->primitive==6&&d->masks[i][3]!=15))return 0;
  for(unsigned a=0;a<16;a++)for(unsigned k=0;k<4;k++)if((d->masks[i][a]&(1u<<k))&&!isfinite(d->vertices[i].attributes[a][k]))return 0;
 }
 return 1;
}
#endif
