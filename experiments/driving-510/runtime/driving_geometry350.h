/*350 UNTESTED, pure geometry assembly. No GPU/guest pointers or allocations.
 * Winding/first-use order follows gpu144/nv2a_pb_exec.c gather_hardware_indices.
 * Incomplete primitives are refused rather than partially acknowledged. */
#ifndef DRIVING_GEOMETRY350_H
#define DRIVING_GEOMETRY350_H
#include <stdint.h>

enum {GEOMETRY350_MAX_INPUT=8192,GEOMETRY350_MAX_DENSE=16384};
typedef struct DrivingTopology350 {unsigned primitive,count,dense;} DrivingTopology350;
static int driving_topology350(unsigned primitive,unsigned count,DrivingTopology350 *out)
{
 unsigned dense=0;
 if(!out||count>GEOMETRY350_MAX_INPUT)return 0;
 switch(primitive){
 case 5:if(count<3||count%3)return 0;dense=count;break;
 case 6:case 7:if(count<3)return 0;dense=(count-2)*3;break;
 case 8:if(count<4||count%4)return 0;dense=count/4*6;break;
 case 9:if(count<4||count%2)return 0;dense=(count-2)*3;break;
 default:return 0;
 }
 if(dense>GEOMETRY350_MAX_DENSE)return 0;
 out->primitive=primitive;out->count=count;out->dense=dense;return 1;
}
/* Called only after successful topology validation, for corner<dense. */
static unsigned driving_topology_corner350(const DrivingTopology350 *t,unsigned corner)
{
 unsigned tri=corner/3,k=corner%3;
 static const unsigned quads[6]={0,1,2,0,2,3};
 static const unsigned strips[6]={0,1,3,0,3,2};
 switch(t->primitive){
 case 5:return corner;
 case 6:return tri+(k==0?0:k==1?1+(tri&1):2-(tri&1));
 case 7:return k==0?0:tri+k;
 case 8:return corner/6*4+quads[corner%6];
 case 9:return corner/6*2+strips[corner%6];
 default:return t->count; /* Invalid plans are never submitted. */
 }
}
static unsigned driving_topology_first350(const DrivingTopology350 *t,unsigned visit)
{
 return t->primitive==9&&visit>=2?visit^1u:visit;
}
#endif
