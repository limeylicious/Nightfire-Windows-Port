#ifndef DRIVING_PLAN214_H
#define DRIVING_PLAN214_H
#include "gpu144/nightfire_vertex_program.h"
#include "driving_contract214.h"
static int driving_plan214(const uint32_t *s,const unsigned char *known,const NFVertexProgram *p,unsigned primitive)
{
    if(!p || p->mode!=6 || p->start || (primitive!=5 && primitive!=6))return -1;
    unsigned f=primitive==6,n=f?8:42;
    const unsigned char *observed=f?known214_1:known214_0;
    for(unsigned i=0;i<2048;i++)if(!observed[i] && known[i])return -1;
    const uint32_t (*code)[4]=f?code214_1:code214_0;
    const Field214 *fields=f?fields214_1:fields214_0,*constants=f?constants214_1:constants214_0;
    size_t count=f?sizeof fields214_1/sizeof *fields:sizeof fields214_0/sizeof *fields;
    for(size_t i=0;i<count;i++)if(!known[fields[i].method/4] || s[fields[i].method/4]!=fields[i].value)return -1;
    for(unsigned i=0;i<n;i++)if(p->valid[i]!=15 || memcmp(p->code[i],code[i],16))return -1;
    count=f?sizeof constants214_1/sizeof *constants:sizeof constants214_0/sizeof *constants;
    for(size_t i=0;i<count;i++){
        unsigned row=constants[i].method/4,c=constants[i].method%4;
        if(!(p->constant_valid[row]&(1u<<c)) || p->constant_words[row][c]!=constants[i].value)return -1;
    }
    /* Extra active input formats are not covered by these captured programs. */
    for(unsigned i=3;i<16;i++)if((s[(0x1760+4*i)/4]>>4)&15)return -1;
    return (int)f;
}
#endif
