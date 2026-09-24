/* Host displacement at the PAL Player_Update -> B7A50 boundary. The original
 * clamp, orientation addition, recoil, autoaim and camera update still run. */
#ifndef NIGHTFIRE_DIRECT_MOUSE122_H
#define NIGHTFIRE_DIRECT_MOUSE122_H
#include <math.h>
extern int nightfire_direct_mouse122_enabled(void);
extern void nightfire_window_mouse_take122(int *,int *);
static int nf_mouse122_span(uint32_t p,uint32_t n) {
    return (p>=0x1000 && (uint64_t)p+n<=0x04000000u) ||
           (p>=0x80000000u && (uint64_t)p+n<=0x84000000u);
}
static int nf_mouse122_scope(void) {
    unsigned depth=MEM16(0x17bfe8);
    /* PAL multiplayer sets 260018 to 1; the same Player_Update boundary and
     * owner/state checks apply there. Reject unknown modes above 1. */
    return depth>0 && depth<32 && MEM32(0x17bfec+depth*4)==2 &&
        !MEM16(0x1fec64) && !MEM8(0x1f65d0) && !MEM32(0x2451f8) &&
        !MEM32(0x1f6678) && !MEM8(0x2c5760) && MEM32(0x260018)<=1 &&
        !MEM8(0x1f6564) && !MEM8(0x1f65c0) && !MEM32(0x2ae288);
}
/* Checkpoint230 diagnostic: observe the existing mouse gate only when requested.
 * This never changes guest memory, native input, or the gate's return value. */
static void nightfire_mouse122_probe230(void) {
    static int enabled=-1;static unsigned calls;
    if(enabled<0)enabled=getenv("NIGHTFIRE_MOUSE_SCOPE_DIAG230")!=NULL;
    if(!enabled || (++calls>8 && calls%600))return;
    unsigned depth=MEM16(0x17bfe8);
    uint32_t flow=depth>0 && depth<32?MEM32(0x17bfec+depth*4):0;
    uint32_t ret=nf_mouse122_span(g_esp,8)?MEM32(g_esp):0;
    uint32_t owner=nf_mouse122_span(g_esp,8)?MEM32(g_esp+4):0;
    uint32_t player=MEM32(0x1f6654);
    uint32_t state=nf_mouse122_span(owner,0xdc)?MEM32(owner+0xbc):0;
    fprintf(stderr,"[MOUSE-SCOPE230] call=%u scope=%d depth=%u flow=%08X ret=%08X owner=%08X player=%08X state=%08X flags=%04X/%02X/%08X/%08X/%02X/%08X/%02X/%02X/%08X\n",
            calls,nf_mouse122_scope(),depth,flow,ret,owner,player,state,
            MEM16(0x1fec64),MEM8(0x1f65d0),MEM32(0x2451f8),
            MEM32(0x1f6678),MEM8(0x2c5760),MEM32(0x260018),
            MEM8(0x1f6564),MEM8(0x1f65c0),MEM32(0x2ae288));
}
static void nightfire_mouse122_boundary(void) {
    if(!nightfire_direct_mouse122_enabled())return;
    /* Guided mode reads its own steering later in Bullet_Update. Reserve this
     * update's sample for that verified boundary; Input_Update still expires it
     * if no eligible rocket consumes it, and focus release clears it. */
    if(nf_mouse122_span(g_esp,8) && MEM32(g_esp)==0xad930) {
        uint32_t owner=MEM32(g_esp+4);
        if(owner==MEM32(0x1f6654) && nf_mouse122_span(owner,0xdc) &&
           MEM16(owner+0xd2)==10 && MEM8(owner+0xdb)==3)return;
    }
    int dx,dy;nightfire_window_mouse_take122(&dx,&dy);
    /* Taking before the eligibility tests discards input during scripted look
     * and pause; a later frame must never replay an old movement. */
    if(!(dx||dy) || !nf_mouse122_scope() ||
       !nf_mouse122_span(g_esp,8) || MEM32(g_esp)!=0xad930)return;
    uint32_t p=MEM32(g_esp+4);
    if(p!=MEM32(0x1f6654) || !nf_mouse122_span(p,0xd4))return;
    uint32_t s=MEM32(p+0xbc);
    if(!nf_mouse122_span(s,0x8e0))return;
    int mode=SMEM16(p+0xd2),controller=SMEM8(s+0x8de);
    if(MEM8(s+0x8da)!=1 || MEM8(s+0x8db) || (MEM32(p+0xcc)&0x10) ||
       (mode>=8 && mode<=12) || mode==16 || controller<0 || controller>3 ||
       MEM8(0x262738+0x30*controller))return;
    float yaw=MEMF(s+0x28),pitch=MEMF(s+0x838),zoom=MEMF(s+0x860);
    if(!isfinite(yaw) || !isfinite(pitch) || !isfinite(zoom) || zoom<1.0f)return;
    /* Displacement, not angular velocity: deliberately no timestep factor.
     * Positive normalized pitch looks up, hence both raw input signs invert. */
    /* PAL initializes zoom to1; its look routines scale by1/S+860. */
    MEMF(s+0x28)=yaw+(float)(-dx*0.002/zoom);
    MEMF(s+0x838)=pitch+(float)(-dy*0.002*0.63661977236758134308/zoom);
    if(dy)MEM8(s+0xf0)=1; /* Original manual-look state; suppress autocenter. */
    static unsigned samples;
    if(++samples<=6 || samples%600==0)
        fprintf(stderr,"[DIRECT-MOUSE122] sample=%u dx=%d dy=%d player=%08X yaw_delta=%.6g pitch=%.6g\n",
                samples,dx,dy,p,MEMF(s+0x28),MEMF(s+0x838));
}
#endif
