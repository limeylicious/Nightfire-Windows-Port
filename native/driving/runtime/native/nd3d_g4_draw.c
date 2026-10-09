/* Native D3D, group 4: the draw path and vertex data.
 *
 * Spec: native-driving/phase2/specs/G4-draw-path.md. Reference: the recompiled
 * originals in src/recomp/gen/recomp_0017.c (sub_0016B4C0 ... sub_0016CFC0).
 *
 * Each routine keeps the original's CPU-side bookkeeping (device flags at
 * dev+8, fence calls, SetStateVB / lazy-state calls, resource Lock fields,
 * return values and stack pops). The push-buffer mechanics (space checks,
 * MakeSpace, headers, the write pointer at dev+0) are gone: draws go straight
 * to the native renderer through nd3d_draw_arrays / nd3d_draw_indexed16 /
 * nd3d_imm_*, which read vertices and indices from guest memory at the moment
 * of the call (nd3d_draw.c copies them into D3D11 buffers before returning).
 *
 * Calls to other library routines go through nd3d_call with the guest ABI:
 *   0x171720 CDevice_SetStateVB   thiscall ECX = device, 1 argument, ret 4 (group 2)
 *   0x1716A0 lazy state flush     no arguments, ret                  (recompiled, CPU only)
 *   0x16CA30 D3D_SetFence         1 argument, ret 4                  (nd3d_core.c, no wait)
 *   0x16CE90 vertex-buffer wait   1 argument, ret 4                  (nd3d_core.c, no wait)
 *   0x16AB30 SetShaderConstantMode 1 argument, ret 4
 *   0x16CED0 one-time Kelvin init (this file; tail-called by 0x16CFC0)  */
#include "nd3d_internal.h"

void sub_00171720(void);   /* CDevice_SetStateVB */
void sub_0016CA30(void);   /* D3D_SetFence */
void sub_0016CE90(void);   /* vertex-buffer wait (BlockOnTime on the buffer's Lock time) */
void sub_0016AB30(void);   /* D3DDevice_SetShaderConstantMode */
void sub_0016CED0(void);   /* one-time Kelvin binding/init (n_0016CED0 below) */

/* Device fields used only by this group (see the spec's section 0). */
#define DEV_INDEXBASE   0x1Cu    /* BaseVertexIndex from SetIndices (always 0 in this game) */
#define DEV_PB_BYTES    0x40u    /* RunPushBuffer bytes accumulator */
#define DEV_PB_TIME     0x48u    /* time of the last RunPushBuffer */
#define DEVF_RECORDING  0x0004u  /* dev+8: recording a push buffer */
#define DEVF_INPRIM     0x0800u  /* dev+8: inside Begin/End or DrawIndexedVertices */
#define DEVF_FENCEDUE   0x1000u  /* dev+8: a fence was deferred while DEVF_INPRIM was set */

#define ND3D_ONCE(...) do { static int once_; if (!once_) { once_ = 1; nd3d_log(__VA_ARGS__); } } while (0)

/* Deferred fence at the end of a primitive (End, DrawIndexedVertices):
 * if a fence was deferred inside the primitive, issue SetFence(1) now, then
 * leave the primitive (dev+8 &= ~0x1800). */
static void end_primitive(uint32_t dev)
{
    if (G32(dev + DEV_FLAGS) & DEVF_FENCEDUE) nd3d_call(sub_0016CA30, 0, 0, 1, 1u);
    G32(dev + DEV_FLAGS) &= 0xFFFFE7FFu;
}

/* ------------------------------------------------------------- vertex buffer */

/* 0x16B4C0 D3DVertexBuffer_Lock2(pVB, Flags), stdcall, ret 8.
 * Returns pVB->Data | 0x80000000 (the CPU mapping of the contiguous data). */
void n_0016B4C0(void)
{
    uint32_t vb = ARG(1), flags = ARG(2) & 0xFFu;   /* the original tests the low byte only */
    /* !(Flags & 0x10, lead READONLY): the original wrote W(0x1710,1), 0, a vertex-cache
     * invalidate (lead: BREAK_VERTEX_BUFFER_CACHE). Native draws read vertex memory when
     * they are recorded, so there is no cache to invalidate. The word is a trigger, not
     * state; it is stored only so nd3d_reg[] matches what the old decoder's K[] held.
     * Nothing reads it. */
    if (!(flags & 0x10u)) ND3D_SET(0x1710, 0);
    /* !(Flags & 0xA0, lead NOOVERWRITE|NOFLUSH): wait until the GPU no longer uses the
     * buffer. Every native draw has already copied its vertices when its routine
     * returned, so nothing pending can need the old bytes; the native wait returns at
     * once. Called anyway so the call sequence (and its completed-time bookkeeping)
     * matches the original. */
    if (!(flags & 0xA0u)) nd3d_call(sub_0016CE90, 0, 0, 1, vb);
    RET(G32(vb + 4) | 0x80000000u, 8);
}

/* --------------------------------------------------------------------- draws */

/* 0x16B620 D3DDevice_DrawVertices(Prim, StartVertex, VertexCount), stdcall, ret 12. */
void n_0016B620(void)
{
    uint32_t dev = nd3d_device();
    nd3d_call(sub_00171720, dev, 0, 1, 0u);          /* SetStateVB(IndexBase = 0) */
    uint32_t prim = ARG(1), start = ARG(2), count = ARG(3);
    /* ND3D-CHECK: the original's batching garbles these two cases (count 0 gives
     * 0x1000000 batches; a start of 2^24 or more spills into the count bits of the
     * hardware word). The game is not expected to do either; the native draw skips
     * count 0 and draws the requested range otherwise. Reported once. */
    if (!count) ND3D_ONCE("DrawVertices with VertexCount 0 (prim %u, caller %08X): skipped\n", prim, G32(g_esp));
    if (start >> 24) ND3D_ONCE("DrawVertices StartVertex %u exceeds the hardware's 24 bits (caller %08X)\n", start, G32(g_esp));
    /* The original wrote: begin(Prim), the DRAW_ARRAYS runs of at most 256 vertices,
     * end. One native draw covers the whole call. Does not touch dev+8. */
    nd3d_draw_arrays(prim, start, count);
    RETV(12);
}

/* 0x16B6C0 D3DDevice_DrawIndexedVertices(Prim, IndexCount, pIndexData), stdcall, ret 12.
 * Indices are 16-bit, at a CPU address; the original copied them into the push
 * buffer inline. nd3d_draw_indexed16 reads all IndexCount of them now. */
void n_0016B6C0(void)
{
    uint32_t dev = nd3d_device();
    nd3d_call(sub_00171720, dev, 0, 1, G32(dev + DEV_INDEXBASE));   /* SetStateVB(dev+0x1C) */
    G32(dev + DEV_FLAGS) |= DEVF_INPRIM;
    uint32_t prim = ARG(1), count = ARG(2), indices = ARG(3);
    /* Index values are not offset: SetStateVB folded IndexBase into the array offsets.
     * Count 0: the original sent begin, an empty index run and end (nothing drawn). */
    nd3d_draw_indexed16(prim, indices, count);
    end_primitive(dev);
    RETV(12);
}

/* 0x16BA20 D3DDevice_Begin(Prim), stdcall, ret 4. */
void n_0016BA20(void)
{
    uint32_t dev = nd3d_device();
    nd3d_call(sub_001716A0, 0, 0, 0);                /* lazy state flush; no SetStateVB */
    nd3d_imm_begin(ARG(1));
    G32(dev + DEV_FLAGS) |= DEVF_INPRIM;
    RETV(4);
}

/* 0x16BA60 D3DDevice_End(), ret 0. */
void n_0016BA60(void)
{
    uint32_t dev = nd3d_device();
    nd3d_imm_end();                                  /* the original's end word (0x17FC = 0) */
    end_primitive(dev);
    RETV(0);
}

/* ------------------------------------------------------- immediate attributes
 * The original wrote the immediate-attribute methods below; each call writes the
 * method's last component, which completes the attribute, and a completed write
 * to attribute 0 inside Begin/End produces one vertex (nd3d_attr checks slot 0
 * and Begin itself). Values outside Begin/End stay as the current attribute
 * values that array draws use for disabled slots. */

/* A register number whose method falls outside the attribute range: the original
 * wrote some other method (garbage). The game never does this. */
static void bad_register(const char *fn, uint32_t reg)
{
    /* ND3D-CHECK: dropped; the original would have written an unrelated method. */
    ND3D_ONCE("%s register %d outside the attribute range: dropped (caller %08X)\n", fn, (int32_t)reg, G32(g_esp));
}

/* 0x16B930 D3DDevice_SetVertexData2f(Reg, a, b), stdcall, ret 12.
 * Method 0x1880 + 8*Reg (VERTEX_DATA2F): attribute Reg = (a, b, 0, 1). */
void n_0016B930(void)
{
    uint32_t reg = ARG(1), w[2] = { ARG(2), ARG(3) };
    if (reg < 16u) {
        float v[4];
        memcpy(v, w, 8);                             /* exact float bit patterns */
        v[2] = 0.0f; v[3] = 1.0f;
        nd3d_attr(reg, v, 1);
    } else bad_register("SetVertexData2f", reg);
    RETV(12);
}

/* 0x16B970 D3DDevice_SetVertexData4f(Reg, a, b, c, d), stdcall, ret 20.
 * Reg == -1: method 0x1518 (lead: SET_VERTEX4F), i.e. attribute 0 (position) and a
 * vertex; otherwise method 0x1A00 + 16*Reg (VERTEX_DATA4F): attribute Reg. */
void n_0016B970(void)
{
    uint32_t reg = ARG(1), w[4] = { ARG(2), ARG(3), ARG(4), ARG(5) };
    float v[4];
    memcpy(v, w, 16);                                /* exact float bit patterns */
    if (reg == 0xFFFFFFFFu) nd3d_attr(0, v, 1);
    else if (reg < 16u) nd3d_attr(reg, v, 1);
    else bad_register("SetVertexData4f", reg);
    RETV(20);
}

/* 0x16B9D0 D3DDevice_SetVertexDataColor(Reg, D3DCOLOR c), stdcall, ret 8.
 * Method 0x1940 + 4*Reg (VERTEX_DATA4UB) with the word
 * (c & 0xFF00FF00) | ((c & 0xFF) << 16) | ((c >> 16) & 0xFF): its bytes from the
 * bottom are R, G, B, A, and the attribute is (R, G, B, A) / 255. */
void n_0016B9D0(void)
{
    uint32_t reg = ARG(1), c = ARG(2);
    uint32_t w = (c & 0xFF00FF00u) | ((c & 0xFFu) << 16) | ((c >> 16) & 0xFFu);
    if (reg < 16u) {
        float v[4] = { (float)(w & 0xFFu) / 255.0f, (float)((w >> 8) & 0xFFu) / 255.0f,
                       (float)((w >> 16) & 0xFFu) / 255.0f, (float)(w >> 24) / 255.0f };
        nd3d_attr(reg, v, 1);
    } else bad_register("SetVertexDataColor", reg);
    RETV(8);
}

/* ---------------------------------------------------------- recorded buffers */

/* 0x16BAA0 D3DDevice_RunPushBuffer(pPB, pFixup), stdcall, ret 8. Not used in play
 * (its only caller, 0xF4340, never ran). A recorded command buffer cannot run
 * natively, so it is skipped and reported once; the CPU bookkeeping is kept. */
void n_0016BAA0(void)
{
    uint32_t dev = nd3d_device();
    nd3d_call(sub_00171720, dev, 0, 1, 0u);          /* SetStateVB(0), as the original */
    uint32_t pb = ARG(1), fixup = ARG(2);
    ND3D_ONCE("RunPushBuffer: pre-recorded command buffer %08X (%u bytes) requested by %08X; skipped\n",
              pb, G32(pb + 0xC), G32(g_esp));
    if ((int32_t)G32(pb) < 0) RETV(8);               /* CPU-copy buffer: the original only copied it into the push buffer */
    uint32_t size = G32(pb + 0xC) - 4u;
    if (fixup) G32(fixup + 8) = G32(dev + DEV_TIME); /* pFixup->Lock = current time */
    /* ND3D-CHECK: not kept. The original picked a jump or call form from the GPU's
     * completed time (*(dev+0x34)), then called 0x16F070, which copies the fixups into
     * the recorded buffer, writes a return jump at its end, and in the jump form writes
     * a GPU register and adds the size to miniport+0x818. All of that only serves the
     * recorded commands, which are never executed here. Its locals also overwrote the
     * caller's pFixup argument slot. */
    G32(pb + 8) = G32(dev + DEV_TIME);               /* pPB->Lock = current time */
    if (!(G8(dev + DEV_FLAGS) & DEVF_RECORDING)) {
        G32(dev + DEV_PB_TIME) = G32(dev + DEV_TIME);
        G32(dev + DEV_PB_BYTES) += size;
        if (size > 0x2000u) nd3d_call(sub_0016CA30, 0, 0, 1, 0u);   /* SetFence(0) */
    }
    RETV(8);
}

/* ---------------------------------------------------------- one-time binding */

/* 0x16CED0 (no arguments, ret 0): Kelvin init words at device creation, then
 * SetShaderConstantMode(0). */
void n_0016CED0(void)
{
    /* Context DMA handles: 0x180..0x188 = 2,3,3 (notifier, texture DMA A/B) and
     * 0x190..0x1A4 = 4,9,0xA,3,3,8 (state, colour, zeta, vertex A/B, semaphore), and
     * the semaphore offset 0x1D6C = 0: nothing (nd3d_api.h special slots). Vertex DMA
     * A and B are object 3, created with base 0 and limit 0x7FFAFFF, so vertex offsets
     * are physical addresses from 0; the renderer's phys() mapping assumes exactly that. */
    /* ND3D-CHECK: 0x1A8 = 0xC is another DMA handle (lead: report DMA, used by
     * visibility tests) but is not in the special list, so it is stored as an ordinary
     * word per contract rule 2. Nothing reads it. */
    ND3D_SET(0x1A8, 0xCu);
    /* Plain state words (meanings not established; stored as written). */
    ND3D_SET(0x9FC, 1u);
    {
        static const uint32_t a50[4] = { 0u, 0u, 0u, 0x3F800000u };   /* 0, 0, 0, 1.0f */
        ND3D_SETN(0xA50, a50, 4);
    }
    ND3D_SET(0x16BC, 1u);
    ND3D_SET(0x1E78, 0x210000u);
    ND3D_SET(0x1D80, 1u);
    ND3D_SET(0x1E68, 0x7F800000u);                   /* +infinity */
    nd3d_call(sub_0016AB30, 1, 0, 1, 0u);            /* SetShaderConstantMode(0); ECX was 1 there */
    RETV(0);
}

/* 0x16CFC0 (no arguments; tail-jumps to 0x16CED0): object bindings and blit
 * defaults at device creation. */
void n_0016CFC0(void)
{
    uint32_t dev = nd3d_device();
    /* Subchannel bindings (subch1 obj 0xE, subch2 obj 0x10, subch3 obj 0x11, subch0
     * obj 0xD) and the blit objects' defaults (subch1 0x180 = 7; subch2 0x2FC = 3, the
     * SRCCOPY operation; subch3 0x184/0x188 = 3/0xB; subch2 0x184..0x19C = 0x19 x6,
     * 0x11): nothing. They are not Kelvin state, and native copies pass everything to
     * nd3d_copy_rect explicitly. */
    for (unsigned i = 0; i < 8; i++) G32(dev + 0x20FCu + 4u * i) = 0;
    for (unsigned i = 0; i < 8; i++) G32(dev + 0x20DCu + 4u * i) = 0;
    G32(dev + 0x20DCu) = 0;
    G32(dev + 0x20E0u) = 3;
    /* tail jmp 0x16CED0: same stack, so call it and then pop our own return address */
    nd3d_call(sub_0016CED0, 0, 0, 0);
    RETV(0);
}
