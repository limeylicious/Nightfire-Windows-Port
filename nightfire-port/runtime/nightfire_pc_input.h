/* PC buttons/movement supply an Xbox pad. Optional direct mouse displacement
 * is sampled separately once per Input_Update, never per device poll. */
#ifndef NIGHTFIRE_PC_INPUT_H
#define NIGHTFIRE_PC_INPUT_H
#include <stdint.h>
#include <string.h>
typedef struct {
    unsigned char keys[256];
    int captured,left,right;
    int mouse_x,mouse_y;
} nf_pc_controls;
typedef struct {
    unsigned buttons,axis_mask;
    int16_t axes[4];
    unsigned char lt,rt;
} nf_pc_packet;
typedef struct { int x,y; } nf_pc_delta122;
static nf_pc_delta122 nf_pc_take122(nf_pc_delta122 *pending) {
    nf_pc_delta122 result=*pending;pending->x=pending->y=0;return result;
}
static void nf_pc_clear(nf_pc_controls *s) {memset(s,0,sizeof *s);}
static int nf_pc_bound(int64_t v,int limit) {return v>limit?limit:v< -limit?-limit:(int)v;}
static void nf_pc_mouse(nf_pc_controls *s,int x,int y) {
    if(s->captured) {s->mouse_x=nf_pc_bound((int64_t)s->mouse_x+x,4096);s->mouse_y=nf_pc_bound((int64_t)s->mouse_y+y,4096);}
}
static int16_t nf_pc_mouse_axis(int value) {
    if(!value)return 0;
    /* Get through the original pad deadzone while retaining small movements. */
    return (int16_t)nf_pc_bound((int64_t)value*1000+(value>0?9000:-9000),32767);
}
static unsigned nf_pc_buttons(const nf_pc_controls *s) {
    const unsigned char *k=s->keys;
    unsigned b=(k[0x0d]?16:0)|(k[0x26]?1:0)|(k[0x28]?2:0)|(k[0x25]?4:0)|(k[0x27]?8:0);
    if(k['Z']||k['E']||k['R'])b|=0x10000;
    if(k['X'])b|=0x20000;
    if(k['C']||k[0x11])b|=0x40000;
    if(k[0x20])b|=0x80000;
    return b;
}
static void nf_pc_sample(nf_pc_controls *s,unsigned layout,nf_pc_packet *p) {
    memset(p,0,sizeof *p);p->buttons=nf_pc_buttons(s);
    /* PAL jump table DF698, validated by compare_input_mapping.py.
     * Layouts 2/4/5/7 turn on LX. Layout 4 also exchanges vertical axes. */
    unsigned turn=(layout==2||layout==4||layout==5||layout==7)?0:2;
    unsigned strafe=2-turn,forward=layout==4?3:1,pitch=layout==4?1:3;
    const unsigned char *k=s->keys;
    if(k['A']||k['D']){p->axis_mask|=1u<<strafe;p->axes[strafe]=(int16_t)((!!k['D']-!!k['A'])*32767);}
    if(k['W']||k['S']){p->axis_mask|=1u<<forward;p->axes[forward]=(int16_t)((!!k['W']-!!k['S'])*32767);}
    if(s->captured) {
        if(s->mouse_x){p->axis_mask|=1u<<turn;p->axes[turn]=nf_pc_mouse_axis(s->mouse_x);}
        if(s->mouse_y){p->axis_mask|=1u<<pitch;p->axes[pitch]=nf_pc_mouse_axis(-s->mouse_y);}
        /* T is a touchpad-friendly alternative to holding right click. */
        p->lt=(s->right||k['T'])?255:0;p->rt=s->left?255:0;
    }
    s->mouse_x=s->mouse_y=0;
}
static void nf_pc_merge(const nf_pc_packet *p,unsigned char state[18],unsigned *buttons) {
    *buttons|=p->buttons;
    for(unsigned i=0;i<4;i++)if(p->axis_mask&(1u<<i))memcpy(state+10+i*2,&p->axes[i],2);
    if(p->lt)state[8]=p->lt;if(p->rt)state[9]=p->rt;
}
static void nf_pc_sample122(nf_pc_controls *s,unsigned layout,nf_pc_packet *p,
                            nf_pc_delta122 *pending,int direct) {
    pending->x=pending->y=0; /* Expire any sample not used by the last update. */
    if(direct) {
        if(s->captured){pending->x=s->mouse_x;pending->y=s->mouse_y;}
        s->mouse_x=s->mouse_y=0; /* Keep physical pad axes out of this path. */
    }
    nf_pc_sample(s,layout,p);
}
/* Modern keys still feed the original PAL pad/action path. The selected pad
 * layout is never changed. Values are verified against PAL DE720/DF698
 * by checkpoint140; use and reload are one original contextual action. */
static unsigned nf_pc_buttons140(const nf_pc_controls *s,unsigned layout) {
    static const unsigned use[8]={0x40000,0x10000,0x10000,0x10000,0x10000,0x10000,0x10000,0x10000};
    static const unsigned jump[8]={0x80000,0x80000,0x80000,0x80000,1,0x80000,0x80000,0x80000};
    static const unsigned crouch[8]={0x10000,0x20000,0x20000,0x40000,2,0x40000,0x40000,0x40000};
    static const unsigned alternate[8]={0x200000,0x200000,0x200000,0x20000,0x20000,0x20000,0x100000,0x100000};
    static const unsigned weapon[8]={0x20000,0x40000,0x40000,8,0x100000,8,0x20000,0x20000};
    static const unsigned gadget[8]={0x100000,0x100000,0x100000,1,0x80000,1,8,8};
    const unsigned char *k=s->keys;
    unsigned b=(k[0x0d]?16:0)|(k[0x26]?1:0)|(k[0x28]?2:0)|(k[0x25]?4:0)|(k[0x27]?8:0);
    /* Stable menu controls, irrespective of gameplay layout. */
    if(k['Z'])b|=0x10000;
    if(k['X'])b|=0x20000;
    if(k[0x09])b|=0x20; /* Original Back/objectives button. */
    if(layout>=8)return b; /* Never index beyond the verified PAL layouts. */
    if(k['E']||k['R'])b|=use[layout];
    if(k[0x20])b|=jump[layout];
    if(k[0x11]||k['C'])b|=crouch[layout];
    if(k['F'])b|=alternate[layout];
    if(k['Q'])b|=weapon[layout];
    if(k['G'])b|=gadget[layout];
    return b;
}
static void nf_pc_sample140(nf_pc_controls *s,unsigned layout,nf_pc_packet *p,
                            nf_pc_delta122 *pending,int direct,int modern) {
    nf_pc_sample122(s,layout,p,pending,direct);
    if(modern)p->buttons=nf_pc_buttons140(s,layout);
}
#endif
