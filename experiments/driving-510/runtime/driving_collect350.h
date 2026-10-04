/*350 UNTESTED command transaction for original array/index geometry.
 * No synthetic indices are ever sent to the guest executor. On refusal it
 * receives exactly the retained original methods, then the rejected current
 * method once. Inline1818/attribute writes are NOT index data. */
#ifndef DRIVING_COLLECT350_H
#define DRIVING_COLLECT350_H
#include "driving_geometry350.h"
#include <stddef.h>
#include <string.h>
enum {GC350_IDLE,GC350_ACTIVE,GC350_COMPLETE,GC350_REPLAY};
enum {GC350_PASS,GC350_HOLD,GC350_READY,GC350_BEFORE_CURRENT};
enum {GC350_OK,GC350_STATE,GC350_MIXED,GC350_COMMAND_LIMIT,GC350_VERTEX_LIMIT,
      GC350_RANGE,GC350_BARRIER,GC350_BAD_TOPOLOGY};
enum {GC350_MAX_COMMANDS=4096};
typedef struct GeometryCommand350 {uint32_t method,value;} GeometryCommand350;
typedef struct GeometryCollector350 {
 unsigned phase,reason,primitive,kind,commands,count;
 GeometryCommand350 prefix[GC350_MAX_COMMANDS];
 uint32_t indices[GEOMETRY350_MAX_INPUT];
} GeometryCollector350;
static void geometry_reset350(GeometryCollector350 *c)
{
 c->phase=GC350_IDLE;c->reason=GC350_OK;c->primitive=c->kind=c->commands=c->count=0;
}
static int geometry_begin350(GeometryCollector350 *c,unsigned primitive)
{
 /* Existing list/strip paths retain their original collector and submitter. */
 if(!c||c->phase!=GC350_IDLE||(primitive!=6&&primitive!=7&&primitive!=8&&primitive!=9))return 0;
 c->phase=GC350_ACTIVE;c->reason=GC350_OK;c->primitive=primitive;
 c->kind=c->count=0;c->commands=1;
 c->prefix[0]=(GeometryCommand350){0x17fc,primitive};return 1;
}
static unsigned geometry_refuse350(GeometryCollector350 *c,unsigned reason)
{c->phase=GC350_REPLAY;c->reason=reason;return GC350_BEFORE_CURRENT;}
static int geometry_payload350(unsigned method,uint32_t value)
{return (method==0x17fc&&!value)||method==0x1800||method==0x1808||method==0x1810;}
static unsigned geometry_feed350(GeometryCollector350 *c,unsigned method,uint32_t value)
{
 unsigned kind=0,n=0;uint32_t first=0;
 if(!c||c->phase==GC350_IDLE)return GC350_PASS;
 if(c->phase!=GC350_ACTIVE)return GC350_BEFORE_CURRENT;
 if(method==0x17fc&&!value){
  if(c->commands==GC350_MAX_COMMANDS)return geometry_refuse350(c,GC350_COMMAND_LIMIT);
  c->prefix[c->commands++]=(GeometryCommand350){method,value};
  c->phase=GC350_COMPLETE;return GC350_READY;
 }
 if(method==0x1800){kind=1;n=2;}
 else if(method==0x1808){
  /*353: preserve the executor's explicit unsupported-index32 failure. */
  if(value>0xffffu)return geometry_refuse350(c,GC350_RANGE);
  kind=1;n=1;
 }
 else if(method==0x1810){
  kind=2;first=value&0xffffffu;n=(value>>24)+1;
  if((uint64_t)first+n>0x1000000ULL)return geometry_refuse350(c,GC350_RANGE);
 }else return geometry_refuse350(c,GC350_STATE);
 if(c->kind&&c->kind!=kind)return geometry_refuse350(c,GC350_MIXED);
 if(c->commands==GC350_MAX_COMMANDS)return geometry_refuse350(c,GC350_COMMAND_LIMIT);
 if(n>GEOMETRY350_MAX_INPUT-c->count)return geometry_refuse350(c,GC350_VERTEX_LIMIT);
 c->prefix[c->commands++]=(GeometryCommand350){method,value};c->kind=kind;
 if(method==0x1800){c->indices[c->count++]=value&65535u;c->indices[c->count++]=value>>16;}
 else if(method==0x1808)c->indices[c->count++]=value;
 else for(unsigned i=0;i<n;i++)c->indices[c->count++]=first+i;
 return GC350_HOLD;
}
static int geometry_result350(const GeometryCollector350 *c,DrivingTopology350 *t)
{
 return c&&c->phase==GC350_COMPLETE&&c->kind&&driving_topology350(c->primitive,c->count,t);
}
typedef void (*GeometryEmit350)(void *,unsigned,uint32_t);
static int geometry_replay350(GeometryCollector350 *c,GeometryEmit350 emit,void *ctx)
{
 if(!c||c->phase==GC350_IDLE)return 1;
 if(!emit)return 0;
 for(unsigned i=0;i<c->commands;i++)emit(ctx,c->prefix[i].method,c->prefix[i].value);
 geometry_reset350(c);return 1;
}
#endif
