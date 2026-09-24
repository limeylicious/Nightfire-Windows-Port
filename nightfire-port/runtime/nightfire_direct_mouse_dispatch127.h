/* Include after normal mouse122 and the guest register definitions. */
#ifndef NIGHTFIRE_DIRECT_MOUSE_DISPATCH127_H
#define NIGHTFIRE_DIRECT_MOUSE_DISPATCH127_H
#include "nightfire_rocket_mouse127.h"
static void nightfire_mouse127_boundary(void) {
    /* Most matrix calls are unrelated; avoid global scope reads for them. */
    if(!nf_rocket_mouse127_span(g_esp,4) || MEM32(g_esp)!=0x2383d ||
       !nightfire_direct_mouse122_enabled())return;
    int scope=nf_mouse122_scope();
    if(!nf_rocket_mouse127_ready(scope))return;
    int dx,dy;nightfire_window_mouse_take122(&dx,&dy);
    if(nf_rocket_mouse127_apply(dx,dy,scope)) {
        static unsigned samples;
        if(++samples<=6 || samples%600==0)
            fprintf(stderr,"[ROCKET-MOUSE127] sample=%u dx=%d dy=%d rocket=%08X pitch=%.6g yaw=%.6g\n",
                    samples,dx,dy,g_edi,MEMF(g_esp+0x3c),MEMF(g_esp+0x40));
    }
}
#endif
