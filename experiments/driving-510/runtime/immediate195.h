/* Original immediate command collector for a future narrow movie adapter.
 * No guest-memory reads, rendering, object binding, DMA or fences here.
 * Caller must feed only the already validated singleton Kelvin instance.
 * Returned draw data is owned, not pointers into guest command/source memory. */
#ifndef IMMEDIATE195_H
#define IMMEDIATE195_H
#include <stdint.h>
#include <string.h>
#include "gpu144/nightfire_vertex_program.h"
typedef struct DrivingVertex195 {float attributes[16][4];} DrivingVertex195;
typedef struct DrivingDraw195 {
    uint32_t state[2048];unsigned char known[2048];NFVertexProgram program;
    DrivingVertex195 vertices[4];unsigned masks[4][16];
    unsigned primitive,count,invalid;
} DrivingDraw195;
typedef struct DrivingImmediate195 {
    uint32_t state[2048];unsigned char known[2048];NFVertexProgram program;
    DrivingVertex195 current;unsigned masks[16];
    DrivingDraw195 draw;unsigned active,selected;
} DrivingImmediate195;
static int driving_immediate195(DrivingImmediate195 *s,unsigned m,uint32_t v)
{
    if(!s||m>=8192||(m&3))return -1;
    s->state[m/4]=v;s->known[m/4]=1;nf_vp_method(&s->program,m,v);
    if(m==0x17fc){
        if(v){
            if(s->active)return -1;
            s->active=1;
            /* A candidate family only. Complete admission remains the caller's
             * job, including combiners, matrices, render state and source spans. */
            s->selected=(v==5||v==6)&&s->known[0x1b04/4]&&
                (s->state[0x1b04/4]==0x11129||s->state[0x1b04/4]==0x11229);
            if(s->selected){
                memset(&s->draw,0,sizeof s->draw);s->draw.primitive=v;
                memcpy(s->draw.state,s->state,sizeof s->state);
                memcpy(s->draw.known,s->known,sizeof s->known);
                s->draw.program=s->program;
            }
            return 0;
        }
        if(!s->active)return -1;
        s->active=0;
        if(!s->selected)return 0;
        s->selected=0;
        return s->draw.invalid||s->draw.count!=(s->draw.primitive==6?4u:3u)?-1:1;
    }
    if(m>=0x1880&&m<0x1900){
        unsigned a=(m-0x1880)/8,k=(m&7)/4;
        memcpy(&s->current.attributes[a][k],&v,4);
        s->current.attributes[a][2]=0;s->current.attributes[a][3]=1;
        s->masks[a]|=(1u<<k)|12;return 0;
    }
    if(m>=0x1940&&m<0x1980){
        unsigned a=(m-0x1940)/4;
        for(unsigned k=0;k<4;k++)s->current.attributes[a][k]=((v>>(k*8))&255)/255.0f;
        s->masks[a]=15;return 0;
    }
    if(m>=0x1518&&m<=0x1524){
        unsigned k=(m-0x1518)/4;
        memcpy(&s->current.attributes[0][k],&v,4);s->masks[0]|=1u<<k;
        if(m==0x1524&&s->active&&s->selected){
            if(s->draw.count>=4){s->draw.invalid=1;return 0;}
            unsigned n=s->draw.count++;
            s->draw.vertices[n]=s->current;
            memcpy(s->draw.masks[n],s->masks,sizeof s->masks);
        }
        return 0;
    }
    /* A state or unsupported attribute mutation inside an admitted candidate
     * cannot silently alter already captured vertices/program/constants. */
    if(s->active&&s->selected)s->draw.invalid=1;
    return 0;
}
#endif
