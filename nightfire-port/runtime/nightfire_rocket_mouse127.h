/* Guided Bullet_Update only. Caller owns focus/sample consumption and passes
 * its armed, validated main-thread/global gameplay scope as scope.
 * Include after the normal guest memory/register definitions. */
#ifndef NIGHTFIRE_ROCKET_MOUSE127_H
#define NIGHTFIRE_ROCKET_MOUSE127_H
#include <stdint.h>
#include <math.h>
static int nf_rocket_mouse127_span(uint32_t p,uint32_t n) {
    return (p>=0x1000u && (uint64_t)p+n<=0x04000000u) ||
           (p>=0x80000000u && (uint64_t)p+n<=0x84000000u);
}
static int nf_rocket_mouse127_ready(int scope) {
    if(!scope || !nf_rocket_mouse127_span(g_esp,0xfcu) ||
       MEM32(g_esp)!=0x2383du)return 0;
    uint32_t rocket=g_edi,data=g_esi;
    if(!nf_rocket_mouse127_span(rocket,0xdcu) ||
       !nf_rocket_mouse127_span(data,0xdcu) || MEM32(rocket+0xbc)!=data ||
       MEM32(g_esp+0xf8)!=rocket || MEM32(g_esp+8)!=rocket+0x70 ||
       MEM32(g_esp+4)!=g_esp+0x58 || (MEM8(rocket+0xda)&1) ||
       MEM16(rocket+0xd0)!=1)return 0;
    uint32_t owner=MEM32(data+0x24),def=MEM32(data+0x38);
    if(!nf_rocket_mouse127_span(def,0x6d) || !(MEM8(def+0x6c)&4) ||
       owner!=MEM32(0x1f6654) || !nf_rocket_mouse127_span(owner,0xdc) ||
       MEM8(owner+0xdb)!=3 || MEM16(owner+0xd2)!=10)return 0;
    uint32_t state=MEM32(owner+0xbc);
    if(!nf_rocket_mouse127_span(state,0x8df) || MEM32(state+0x808)!=rocket)return 0;
    int controller=(int8_t)MEM8(state+0x8de);
    if(controller<0 || controller>3 || MEM8(0x262738+controller*0x30))return 0;
    float limit=(float)((double)MEMF(0x17c100)*(double)MEMF(0x15d46c));
    return isfinite(limit) && limit>0.0f &&
           isfinite(MEMF(g_esp+0x3c)) && isfinite(MEMF(g_esp+0x40));
}
static float nf_rocket_mouse127_axis(float old,int delta,float limit) {
    float sum=old+(float)(delta*0.002);
    return sum>limit?limit:sum< -limit?-limit:sum;
}
static int nf_rocket_mouse127_apply(int dx,int dy,int scope) {
    if(!(dx||dy) || !nf_rocket_mouse127_ready(scope))return 0;
    /* These are already scaled PAL per-update radians. The original steering
     * subtracts yaw; positive pitch rotates downward. Mouse displacement is
     * not multiplied by dt, but guided projectiles retain PAL's full-stick
     * turn envelope. Consume the complete sample; never queue capped excess.
     * An axis with no mouse delta preserves every pad-only bit unchanged. */
    float limit=(float)((double)MEMF(0x17c100)*(double)MEMF(0x15d46c));
    if(dy)MEMF(g_esp+0x3c)=nf_rocket_mouse127_axis(MEMF(g_esp+0x3c),dy,limit);
    if(dx)MEMF(g_esp+0x40)=nf_rocket_mouse127_axis(MEMF(g_esp+0x40),dx,limit);
    return 1;
}
#endif
