/* Native D3D, group 6 (phase 4): routines whose only remaining work was
 * programming graphics-chip registers through the miniport (CMiniport at
 * dev+0x23C0). The library's own bookkeeping is kept exactly; the register
 * programming is dropped because no graphics chip exists.
 *
 * Why these matter: with the register-poll thread off (LEAN_NO_REGPOLL=1) the
 * miniport's interrupt-status registers are plain memory again, and its
 * write-1-to-clear acknowledgement sets the bit instead of clearing it.
 * CMiniport::ServiceGrInterrupt (0x16F380) then re-enters itself through
 * 0x17353C until the stack runs out (crash report, start-up, SetTile path:
 * n_0016D800 -> SetTileNoWait -> 0x16FE0F -> 0x16FAFA -> 0x16F380). */
#include <windows.h>
#include "nd3d_internal.h"

/* 0x166C40 D3D_SetTileNoWait(index, tile), stdcall(2). Copies the D3DTILE
 * (Flags, pMemory, Size, Pitch, ZStartTag, ZOffset) into the device's tile
 * table at dev+0x2260 + index*24; without D3DTILE_FLAGS_ZCOMPRESS (bit 31)
 * the Z fields are stored as zero. A null tile or null pMemory clears the
 * entry. The original then calls CMiniport::SetTile (0x16FE0F) or ClearTile
 * (0x16FFFD), which only program the frame-buffer tile registers. */
void n_00166C40(void)
{
    uint32_t index = ARG(1), tile = ARG(2), dev = nd3d_device();
    uint32_t slot = dev + 0x2260u + index * 24u, w[6] = {0, 0, 0, 0, 0, 0};
    unsigned i;
    if (tile && G32(tile + 4)) {
        for (i = 0; i < 6; i++) w[i] = G32(tile + 4u * i);
        if ((int32_t)w[0] >= 0) { w[4] = 0; w[5] = 0; }
    }
    for (i = 0; i < 6; i++) G32(slot + 4u * i) = w[i];
    RETV(8);
}
