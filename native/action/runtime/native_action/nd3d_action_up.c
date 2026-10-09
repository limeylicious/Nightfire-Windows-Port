/* Native D3D, Action-only routines (default.xbe PAL; Driving does not link them).
 * Same rules as the shared library (runtime/native, generated from the Driving
 * sources): keep the library's CPU-side bookkeeping exactly, draw through the
 * Direct3D 11 renderer, write no chip commands. Source of truth: the generated
 * C of the originals in src/recomp/gen (recomp_0007.c). */
#include <windows.h>
#include <stdlib.h>
#include "nd3d_internal.h"

void sub_00109CD0(void);   /* lazy state flush (CPU only, Driving 0x1716A0), ecx = device */
void sub_00109EF0(void);   /* CDevice::SetStateUP (native below)                          */
void sub_001065A0(void);   /* D3D_SetFence (native), stdcall(1)                            */

#define DIRTY        0x001117CCu   /* dirty-flag word (Driving 0x175424) */
#define SLOT_REMAP   0x001D5BCCu   /* vertex-shader slot remap bytes (Driving 0x1D3558) */
#define FMT_SIZES    0x0010DA00u   /* components per format size code */
#define FMT_TYPES    0x0010DA08u   /* bytes per component per format type code */

/* 0x109EF0 CDevice::SetStateUP, ecx = device, ret 0. Flushes lazy state, then, if
 * the vertex format is dirty (0x20), stores the 16 attribute formats from the
 * current vertex shader's stream description and builds the user-pointer copy
 * table the draw uses: dev+0x784 + 8*i = {dwords, bytes to skip after}, dev+0x804
 * segments, dev+0x778 dwords per vertex, dev+0x77C offset of the first attribute,
 * dev+0x780 first offset minus the end of the last attribute (the draw adds the
 * stride to get the last segment's skip). The original also wrote the 16 formats
 * as one chip command (method 0x1760); here they go to the state block. */
void n_00109EF0(void)
{
    uint32_t dev = g_ecx;
    if (G32(DIRTY) & 0x3FFFFF8Fu) nd3d_call(sub_00109CD0, dev, 0, 0);
    uint32_t dirty = G32(DIRTY);
    if (dirty & 0x20u) {
        G32(DIRTY) = (dirty & ~0x20u) | 0x50u;
        uint32_t vs = G32(dev + 0x380), remap = SLOT_REMAP + (G32(vs + 4) & 0x10u);
        uint32_t nseg = 0, total = 0, next = 0, first = 0;
        for (unsigned i = 0; i < 16; i++) {
            uint32_t ent = vs + (uint32_t)G8(remap + i) * 16u, fmt = G32(ent + 0x1C);
            ND3D_SET(0x1760 + 4 * i, fmt);
            if (fmt == 2u) continue;                         /* disabled attribute */
            uint32_t off = G32(ent + 0x18);
            uint32_t dw = ((uint32_t)G8(FMT_SIZES + ((fmt >> 4) & 15u)) * G8(FMT_TYPES + (fmt & 15u)) + 3u) >> 2;
            if (!nseg) { first = off; G32(dev + 0x784) = dw; nseg = 1; }
            else if (off == next) G32(dev + 0x77C + nseg * 8u) += dw;
            else { G32(dev + 0x780 + nseg * 8u) = off - next; G32(dev + 0x784 + nseg * 8u) = dw; nseg++; }
            total += dw; next = off + dw * 4u;
        }
        G32(dev + 0x778) = total; G32(dev + 0x804) = nseg;
        G32(dev + 0x77C) = first; G32(dev + 0x780) = first - next;
    }
    RETV(0);
}

/* 0x104860 D3DDevice_DrawVerticesUP(Prim, VertexCount, pVertexStreamZeroData,
 * VertexStreamZeroStride), stdcall, ret 16. The original copied each vertex's
 * attributes (the SetStateUP copy table) inline into the push buffer between
 * begin(Prim) and end, in batches. Here the same packed vertices are gathered
 * from guest memory and drawn in one call. dev+8 0x800 is set for the draw and
 * 0x1800 cleared after, with the deferred fence (0x1000) issued as the original. */
void n_00104860(void)
{
    static uint32_t *packed; static size_t cap;
    uint32_t dev = nd3d_device();
    nd3d_call(sub_00109EF0, dev, 0, 0);
    uint32_t prim = ARG(1), count = ARG(2), data = ARG(3), stride = ARG(4);
    G32(dev + 8) |= 0x800u;
    uint32_t dpv = G32(dev + 0x778), nseg = G32(dev + 0x804);
    G32(dev + 0x780 + nseg * 8u) = G32(dev + 0x780) + stride;   /* last segment: on to the next vertex */
    if (count && dpv && nseg && nseg <= 16u) {
        size_t need = (size_t)count * dpv;
        if (need > cap) { uint32_t *p = realloc(packed, need * 4); if (!p) { RETV(16); } packed = p; cap = need; }
        uint32_t src = data + G32(dev + 0x77C);
        size_t o = 0;
        for (uint32_t v = 0; v < count; v++)
            for (uint32_t s = 0; s < nseg; s++) {
                uint32_t n = G32(dev + 0x784 + s * 8u);
                for (uint32_t i = 0; i < n && o < need; i++) packed[o++] = G32(src + 4u * i);
                src += n * 4u + G32(dev + 0x788 + s * 8u);
            }
        nd3d_draw_inline(prim, packed, count, dpv);
    }
    if (G32(dev + 8) & 0x1000u) nd3d_call(sub_001065A0, 0, 0, 1, 1u);
    G32(dev + 8) &= 0xFFFFE7FFu;
    RETV(16);
}

/* 0x1019F0 D3DDevice_SetDepthClipPlanes(Near, Far, Flags), stdcall, ret 12.
 * Flags 1: user clip range (dev+0x23B0/0x23B4, dev+0x23B8 |= 2); 2: the second
 * pair (dev+0x23A8/0x23AC, |= 1); 3 / 4 clear those bits again. Then the original
 * re-emitted the viewport transform and clip range through 0x101180 (Driving
 * 0x167F80, native nd3d_h_00167F80: VIEWPORT_OFFSET/SCALE constants, CLIP_MIN/MAX
 * into the state block) and stored the push-buffer pointer it returned. */
void nd3d_h_00167F80(void);
void n_001019F0(void)
{
    uint32_t nearv = ARG(1), farv = ARG(2), flags = ARG(3), dev = nd3d_device();
    switch (flags) {
    case 1: G32(dev + 0x23B0) = nearv; G32(dev + 0x23B4) = farv; G32(dev + 0x23B8) |= 2u; break;
    case 2: G32(dev + 0x23A8) = nearv; G32(dev + 0x23AC) = farv; G32(dev + 0x23B8) |= 1u; break;
    case 3: G32(dev + 0x23B8) &= ~2u; break;
    case 4: G32(dev + 0x23B8) &= ~1u; break;
    }
    G32(dev) = nd3d_call(nd3d_h_00167F80, dev, G32(dev), 0);
    RETV(12);
}

/* 0x103CA0 D3DDevice_GetRasterStatus(pRasterStatus), stdcall, ret 4. The original
 * read the chip's current scan line (PCRTC 0x600808 through dev+0x4FC) and
 * reported InVBlank when it was 0 or past the display height. No chip scans out
 * here and that register has always read 0 in this runtime, so the movie player
 * (its only caller, 0x1309D1) has always been told "in vertical blank, line 0";
 * the native routine keeps exactly that answer without the register read. */
void n_00103CA0(void)
{
    uint32_t rs = ARG(1);
    G32(rs) = 1u;       /* InVBlank */
    G32(rs + 4) = 0u;   /* ScanLine */
    RETV(4);
}
