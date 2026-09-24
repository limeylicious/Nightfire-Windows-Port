/* Development-only use of the original PAL Player_HandlePain bypass byte.
 * Include in the validated main-thread observer; call for AC0A0 before/after.
 * Requires MEM8/16/32 and g_esp/g_esi. Does not replace the guest routine.
 * Protected calls suppress ordinary damage AND pain/armour feedback. Direct
 * Player_Kill/scripted failures remain possible. Never use for fidelity/FPS
 * comparisons: the absent damage effects change the workload.
 *
 * The original nonzero branch calls no helpers, so real protected calls cannot
 * nest. Bounded frames nevertheless preserve nested callback semantics. An
 * unbalanced callback permanently disables the aid after restoring its baseline.
 * Fatal guest exceptions terminate the process; no persistent setting is saved.
 */
#ifndef NIGHTFIRE_DEV_DAMAGE134_H
#define NIGHTFIRE_DEV_DAMAGE134_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NF_DAMAGE134_FLAG 0x001df197u
#define NF_DAMAGE134_FRAMES 32u
static struct {
    struct { uint32_t sp; unsigned char saved; } frames[NF_DAMAGE134_FRAMES];
    unsigned depth, disabled, protected_calls, rejected_calls;
    int enabled_checked, enabled;
} nf_damage134;
static int nf_damage134_enabled(void) {
    if(!nf_damage134.enabled_checked) {
        const char *v=getenv("NIGHTFIRE_DEV_INVULNERABLE134");
        const char *d=getenv("NIGHTFIRE_DEV_START_EXCHANGE");
        nf_damage134.enabled=v && !strcmp(v,"1") && d && !strcmp(d,"1");
        nf_damage134.enabled_checked=1;
        if(nf_damage134.enabled)
            fprintf(stderr,"[DEV-DAMAGE134] development-only player damage/pain bypass enabled; AI active, scripted kills possible\n");
    }
    return nf_damage134.enabled && !nf_damage134.disabled;
}
static int nf_damage134_span(uint32_t p,uint32_t n) {
    return (p>=0x1000u && (uint64_t)p+n<=0x04000000u) ||
           (p>=0x80000000u && (uint64_t)p+n<=0x84000000u);
}
static int nf_damage134_scope(void) {
    unsigned depth=MEM16(0x17bfe8);
    if(MEM32(0x260018) || depth==0 || depth>=32 ||
       MEM32(0x17bfec+depth*4)!=2 || !nf_damage134_span(g_esp,12))return 0;
    /* Verified PAL ABI: argument1 is player object; ESI is object->state.
     * Current-player identity is also used by the existing direct-input bridge.
     * No assumptions about caller address, controller layout or damage type. */
    uint32_t player=MEM32(g_esp+4);
    if(player!=MEM32(0x1f6654) || !nf_damage134_span(player,0xc0))return 0;
    uint32_t state=MEM32(player+0xbc);
    return state==g_esi && nf_damage134_span(state,0x8df);
}
static void nf_damage134_cancel(const char *why) {
    if(nf_damage134.depth)MEM8(NF_DAMAGE134_FLAG)=nf_damage134.frames[0].saved;
    nf_damage134.depth=0;nf_damage134.disabled=1;
    fprintf(stderr,"[DEV-DAMAGE134] disabled and original flag restored: %s\n",why);
}
static void nightfire_dev_damage134(uint32_t va,unsigned after) {
    if(va!=0xac0a0u || after>1 || !nf_damage134_enabled())return;
    if(!after) {
        if(nf_damage134.depth==NF_DAMAGE134_FRAMES) {
            nf_damage134_cancel("callback nesting limit");return;
        }
        unsigned n=nf_damage134.depth;
        unsigned char prior=MEM8(NF_DAMAGE134_FLAG);
        nf_damage134.frames[n].sp=g_esp;nf_damage134.frames[n].saved=prior;
        nf_damage134.depth=n+1;
        if(nf_damage134_scope()) {
            MEM8(NF_DAMAGE134_FLAG)=1;
            unsigned calls=++nf_damage134.protected_calls;
            if(calls<=4 || calls%256==0)
                fprintf(stderr,"[DEV-DAMAGE134] protected_calls=%u player=%08X state=%08X\n",
                        calls,MEM32(g_esp+4),g_esi);
        } else {
            /* Do not lend an outer player's injected flag to an ineligible
             * nested call. Its original baseline can itself be nonzero. */
            if(n)MEM8(NF_DAMAGE134_FLAG)=nf_damage134.frames[0].saved;
            nf_damage134.rejected_calls++;
        }
        return;
    }
    if(!nf_damage134.depth) {nf_damage134_cancel("unpaired return");return;}
    unsigned n=nf_damage134.depth-1;
    if(g_esp!=nf_damage134.frames[n].sp+4u) {
        nf_damage134_cancel("unexpected return stack");return;
    }
    MEM8(NF_DAMAGE134_FLAG)=nf_damage134.frames[n].saved;
    nf_damage134.depth=n;
}
#endif
