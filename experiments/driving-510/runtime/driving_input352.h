/*352 untested common input transaction. Original command words are retained,
 * not rewritten.1818 is a packed vertex record, NEVER an index. No DMA reads
 * here. Array descriptors/current attributes remain caller-owned snapshots. */
#ifndef DRIVING_INPUT352_H
#define DRIVING_INPUT352_H
#include "driving_geometry350.h"
#include "driving_vertex218.h"
#include <string.h>
enum {INPUT352_COMMANDS=8192,INPUT352_WORDS=4096};
enum {INPUT352_IDLE,INPUT352_ACTIVE,INPUT352_COMPLETE,INPUT352_REPLAY};
enum {INPUT352_PASS,INPUT352_HOLD,INPUT352_READY,INPUT352_BEFORE};
enum {INPUT352_NONE,INPUT352_INDEX,INPUT352_ARRAY,INPUT352_INLINE};
enum {INPUT352_OK,INPUT352_STATE,INPUT352_MIXED,INPUT352_CAPACITY,INPUT352_RANGE,INPUT352_LAYOUT,INPUT352_TOPOLOGY};
typedef struct InputLayout352 {unsigned stride,mask,offset[16],format[16];} InputLayout352;
typedef struct InputPacket352 {
 unsigned phase,reason,primitive,kind,commands,count,words;
 uint32_t prefix[INPUT352_COMMANDS][2],indices[GEOMETRY350_MAX_INPUT],inline_words[INPUT352_WORDS];
 InputLayout352 inline_layout;
} InputPacket352;
/* Only DWORD-exact formats whose inline layout is independently visible in
 * nv2a_pb_exec::draw_inline_array. Sub-DWORD packing is unresolved; don't
 * extrapolate its old default branch to SHORT/CMP/UBYTE inline streams. */
static int input_layout352(InputLayout352 *layout,const uint32_t *state,const unsigned char *known)
{
 if(!layout||!state||!known)return 0;memset(layout,0,sizeof *layout);
 for(unsigned slot=0;slot<16;slot++){
  unsigned at=(0x1760+4*slot)/4;
  if(!known[at])return 0;
  unsigned format=state[at],n=format>>4&15,kind=format&15;
  if(!n)continue;
  if(n>4||!(kind==2||(kind==0&&n==4)))return 0;
  unsigned bytes=kind==0?4:n*4;
  layout->mask|=1u<<slot;layout->offset[slot]=layout->stride;
  layout->format[slot]=format&255;layout->stride+=bytes;
 }
 if(!layout->stride)return 0;
 for(unsigned slot=0;slot<16;slot++)if(layout->mask&(1u<<slot))
  layout->format[slot]|=layout->stride<<8;
 return 1;
}
static void input_reset352(InputPacket352 *p)
{p->phase=INPUT352_IDLE;p->reason=p->primitive=p->kind=p->commands=p->count=p->words=0;}
static int input_begin352(InputPacket352 *p,unsigned primitive,const uint32_t *state,const unsigned char *known)
{
 if(!p||p->phase!=INPUT352_IDLE||primitive<5||primitive>9)return 0;
 p->phase=INPUT352_ACTIVE;p->reason=INPUT352_OK;p->primitive=primitive;
 p->kind=p->count=p->words=0;p->commands=1;p->prefix[0][0]=0x17fc;p->prefix[0][1]=primitive;
 /* A bad inline layout must not refuse a valid indexed/array draw. */
 if(!input_layout352(&p->inline_layout,state,known))memset(&p->inline_layout,0,sizeof p->inline_layout);
 return 1;
}
static int input_payload352(unsigned method,uint32_t value)
{return (method==0x17fc&&!value)||method==0x1800||method==0x1808||method==0x1810||method==0x1818;}
static unsigned input_refuse352(InputPacket352 *p,unsigned why)
{p->phase=INPUT352_REPLAY;p->reason=why;return INPUT352_BEFORE;}
static unsigned input_feed352(InputPacket352 *p,unsigned method,uint32_t value)
{
 if(!p||p->phase==INPUT352_IDLE)return INPUT352_PASS;
 if(p->phase!=INPUT352_ACTIVE)return INPUT352_BEFORE;
 if(p->commands==INPUT352_COMMANDS)return input_refuse352(p,INPUT352_CAPACITY);
 if(method==0x17fc&&!value){
  /* END retained even on an invalid final layout, so caller replays it once. */
  p->prefix[p->commands][0]=method;p->prefix[p->commands++][1]=value;
  p->phase=INPUT352_COMPLETE;
  if(p->kind==INPUT352_INLINE){
   unsigned bytes=p->words*4,stride=p->inline_layout.stride;
   if(!stride||bytes%stride||bytes/stride>GEOMETRY350_MAX_INPUT){p->reason=INPUT352_LAYOUT;return INPUT352_READY;}
   p->count=bytes/stride;for(unsigned i=0;i<p->count;i++)p->indices[i]=i;
  }
  return INPUT352_READY;
 }
 unsigned kind,n=0;uint32_t first=0;
 if(method==0x1800){kind=INPUT352_INDEX;n=2;}
 else if(method==0x1808){
  /*353: the original executor stops on ARRAY_ELEMENT32 above0xffff.
   * Replay the prefix before this word, then let that original error happen.
   * Never mask the index or turn a large readable array into new support. */
  if(value>0xffffu)return input_refuse352(p,INPUT352_RANGE);
  kind=INPUT352_INDEX;n=1;
 }
 else if(method==0x1810){kind=INPUT352_ARRAY;n=(value>>24)+1;first=value&0xffffff;
  if((uint64_t)first+n>0x1000000ULL)return input_refuse352(p,INPUT352_RANGE);
 }else if(method==0x1818){kind=INPUT352_INLINE;
  if(!p->inline_layout.stride)return input_refuse352(p,INPUT352_LAYOUT);
  if(p->words==INPUT352_WORDS)return input_refuse352(p,INPUT352_CAPACITY);
 }else return input_refuse352(p,INPUT352_STATE);
 if(p->kind&&p->kind!=kind)return input_refuse352(p,INPUT352_MIXED);
 if(n>GEOMETRY350_MAX_INPUT-p->count)return input_refuse352(p,INPUT352_CAPACITY);
 p->kind=kind;p->prefix[p->commands][0]=method;p->prefix[p->commands++][1]=value;
 if(method==0x1800){p->indices[p->count++]=value&65535;p->indices[p->count++]=value>>16;}
 else if(method==0x1808)p->indices[p->count++]=value;
 else if(method==0x1810)for(unsigned i=0;i<n;i++)p->indices[p->count++]=first+i;
 else p->inline_words[p->words++]=value;
 return INPUT352_HOLD;
}
static int input_result352(const InputPacket352 *p,DrivingTopology350 *topology)
{
 if(!p||!topology||p->phase!=INPUT352_COMPLETE||p->reason!=INPUT352_OK||!p->kind)return 0;
 /* Existing221 lists process every source vertex but submit complete triples.
  * In particular the original count512 producer case must not be diverted to
  * generic replay merely because its final two vertices form no triangle. */
 if(p->primitive==5){
  if(p->count<3||p->count>GEOMETRY350_MAX_INPUT)return 0;
  *topology=(DrivingTopology350){5,p->count,p->count/3*3};return 1;
 }
 return driving_topology350(p->primitive,p->count,topology);
}
static unsigned input_corner352(const DrivingTopology350 *topology,unsigned i)
{
 /* Preserve221's exact cyclic vertex order, including provoking vertices.
  * The generic350 strip uses an equivalent winding with different rotation. */
 if(topology->primitive==6){unsigned tri=i/3,k=i%3;
  return tri+(k==2?2:((tri&1)?1-k:k));}
 return driving_topology_corner350(topology,i);
}
static int input_inline352(const InputPacket352 *p,unsigned vertex,float attributes[16][4])
{
 if(!p||p->kind!=INPUT352_INLINE||vertex>=p->count||p->reason!=INPUT352_OK)return 0;
 memset(attributes,0,16*4*sizeof(float));
 for(unsigned slot=0;slot<16;slot++)if(p->inline_layout.mask&(1u<<slot)){
  if(!driving_vertex218((const uint8_t*)p->inline_words,p->words*4,
     p->inline_layout.format[slot],p->inline_layout.offset[slot],vertex,attributes[slot]))return 0;
 }
 return 1;
}
typedef void (*InputEmit352)(void*,unsigned,uint32_t);
static void input_replay352(InputPacket352 *p,InputEmit352 emit,void *context)
{
 if(!p||!emit||p->phase==INPUT352_IDLE)return;
 for(unsigned i=0;i<p->commands;i++)emit(context,p->prefix[i][0],p->prefix[i][1]);
 input_reset352(p);
}
#endif
