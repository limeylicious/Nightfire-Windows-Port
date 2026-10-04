#ifndef DRIVING_PLAN216_H
#define DRIVING_PLAN216_H
#include <stddef.h>
#include <string.h>
#include "gpu144/nightfire_vertex_program.h"
#include "driving_contract216.h"
/* State/program admission only. Caller must validate every resource binding,
 * span, command batch and vertex before submission. No guest memory accessed. */
static int driving_plan216(const uint32_t *s,const unsigned char *known,
                           const NFVertexProgram *p,unsigned primitive)
{
    if(!s || !known || !p || p->mode!=6 || p->start || primitive!=5 ||
       !known[0x208/4] || s[0x208/4]!=0x00001128u)return 0;
    for(unsigned i=0;i<2048;i++)if(!known216[i] && known[i])return 0;
    for(size_t i=0;i<sizeof fields216/sizeof fields216[0];i++){
        unsigned at=fields216[i].method/4;
        if(!known[at] || s[at]!=fields216[i].value)return 0;
    }
    for(unsigned i=0;i<42;i++)
        if(p->valid[i]!=15 || memcmp(p->code[i],code216[i],16))return 0;
    for(size_t i=0;i<sizeof constants216/sizeof constants216[0];i++){
        unsigned row=constants216[i].method/4,c=constants216[i].method%4;
        if(!(p->constant_valid[row]&(1u<<c)) ||
           p->constant_words[row][c]!=constants216[i].value)return 0;
    }
    for(unsigned i=3;i<16;i++)if((s[(0x1760+4*i)/4]>>4)&15)return 0;
    return 1;
}
#endif
