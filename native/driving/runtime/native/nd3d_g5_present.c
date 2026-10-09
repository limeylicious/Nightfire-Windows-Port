/* Native D3D, group 5: render targets, viewport, scissors, clear, copy,
 * Swap internals / presentation, visibility tests, device and frame-buffer
 * initialisation (Driving.xbe PAL, XDK 1.0.4831 D3D8).
 *
 * Every routine below keeps the original's CPU-side effects (device fields,
 * surface headers, refcounts, Lock stamps, dirty bits, render-state shadows)
 * and stores the state words it computed with ND3D_SET. Clears go to
 * nd3d_clear(), blits to nd3d_copy_rect(), flips to nd3d_present(), and the
 * visibility test to nd3d_visibility_*(). No push buffer is written.
 *
 * Source of truth: src/recomp/gen/recomp_0017.c (line numbers "L<n>") and
 * native-driving/phase2/specs/G5-targets-present.md. Constants quoted in
 * comments were read from Driving.xbe:
 *   0x189EB0 = 0.5f, 0x18A4A8 = 2^32, 0x173FE0 = 0.53125f, 0x189E00 = 2.0f,
 *   0x189ED4 = 4.0f, 0x1A1E74 = 65535.0f, 0x1A1E70 = 16777215.0f,
 *   0x1A1E68 = 511.9375 (double), 0x1A1E60 = 1e30 (double),
 *   0x1740D4 = {0, 0, 0x7FFFFFFF, 0x7FFFFFFF, 0.0f, 1.0f} (whole-surface viewport),
 *   0x1740E4 = {0, 1, 0, 0.585, 1, 1.322, 1.585}, 0x1741D0 = 20 (state,value) pairs,
 *   0x174178 = 11 (stage-0 TSS,value) pairs, 0x174270 = 2-instruction blit program.
 *
 * Words that are commands rather than state are not stored: 0x100 (NOP /
 * software method: the flip), 0x110 (WAIT_FOR_IDLE), 0x12C (FLIP_INCREMENT_WRITE),
 * 0x130 (FLIP_STALL), 0x1D94 (CLEAR_SURFACE -> nd3d_clear), 0x17C8/0x17CC/0x17D0
 * (visibility -> nd3d_visibility_*), 0x1E94/0x1EA0 (-> nd3d_vp_mode/start), and the
 * image-blit / surface2d object methods of CopyRects (other subchannels, so they
 * must never land in the Kelvin state block). */
#include <windows.h>
#include "nd3d_internal.h"

/* --------------------------------------------------- guest registers, ABI */
extern ND3D_TLS uint32_t g_esi;          /* 0x169C80 / 0x169E60 take the device in ESI */
extern ND3D_TLS uint32_t g_fs_base;      /* KPCR (fs:) base, for CDevice::Init's debug-monitor hook */

/* Recompiled routines this group calls (guest ABI, through nd3d_call). */
void sub_00166150(void);   /* pitch of a surface, stdcall(1) */
void sub_0016E0B0(void);   /* SET_SURFACE_FORMAT value for (rt, z), stdcall(2) */
void sub_0016E1D0(void);   /* Z scale of a depth format -> *out, stdcall(2) */
void sub_0016E020(void);   /* format is compressed, stdcall(1) */
void sub_0016DF60(void);   /* swizzled -> linear format, stdcall(1) */
void sub_0016E070(void);   /* tile pitch for (width, format), stdcall(2) */
void sub_0016E730(void);   /* surface Format/Size words and byte size, stdcall(10) */
void sub_001670D0(void);   /* initialise a surface header, stdcall(4) */
void sub_001661B0(void);   /* surface byte size, stdcall(1) */
void sub_001691B0(void);   /* internal AddRef, stdcall(1) */
void sub_001693E0(void);   /* internal Release, stdcall(1) */
void sub_001691F0(void);   /* public AddRef, stdcall(1) */
void sub_00169230(void);   /* public Release, stdcall(1) */
void sub_00167410(void);   /* SetRenderStateNotInline(state, value), stdcall(2) */
void sub_00167970(void);   /* SetRenderState_Dxt1NoiseEnable, stdcall(1) */
void sub_00167E40(void);   /* SetTextureStageStateNotInline(stage, type, value), stdcall(3) */
void sub_001675A0(void);   /* UpdateProjectionViewportTransform, no args */
void sub_00166830(void);   /* SetTexture(stage, texture), stdcall(2) */
void sub_0016AF60(void);   /* SetPixelShader(handle), stdcall(1) */
void sub_0016AC30(void);   /* raw vertex-program load at slot 0 (dwords, count), stdcall(2) */
void sub_00169530(void);   /* Swap state snapshot, EDX = buffer */
void sub_0016BA20(void);   /* Begin(prim), stdcall(1) */
void sub_0016B930(void);   /* SetVertexData2f(reg, a, b), stdcall(3) */
void sub_0016B970(void);   /* SetVertexData4f(reg, a, b, c, d), stdcall(5) */
void sub_0016BA60(void);   /* End() */
void sub_00166B70(void);   /* visibility slot for an index (lazy page), stdcall(1) */
void sub_0016C940(void);   /* CDevice_KickOff, ECX = device */
void sub_00166C40(void);   /* D3D_SetTileNoWait(index, tile), stdcall(2) */
void sub_0016C6B0(void);   /* push-buffer allocation, ECX = device */
void sub_0016FCAD(void);   /* CMiniport::InitHardware, ECX = miniport */
void sub_0016F99B(void);   /* CMiniport::CreateCtxDmaObject, ECX = miniport, stdcall(5) */
void sub_0016F723(void);   /* CMiniport channel creation, ECX = miniport, stdcall(5) */
void sub_0016F786(void);   /* CMiniport bind DMA object, ECX = miniport, stdcall(1) */
void sub_0016FA6A(void);   /* CMiniport graphics object, ECX = miniport, stdcall(3) */
void sub_0016CFC0(void);   /* subchannel/object binding (group 4 native) */
void sub_0016E9C8(void);   /* CMiniport::SetVideoMode, ECX = miniport, stdcall(7) */
void sub_0016AD90(void);   /* SetVertexShader(handle), stdcall(1) */
void sub_00165DC0(void);   /* SetRenderTarget(rt, z), stdcall(2) */
void sub_0016D0A0(void);   /* device default state, no args */
void sub_00166090(void);   /* SetFlickerFilter, stdcall(1) */
void sub_001660E0(void);   /* SetSoftDisplayFilter, stdcall(1) */

/* Indirect calls through the XBE import table (kernel thunks), resolved the same
 * way RECOMP_ICALL_SAFE resolves them. */
typedef void (*nd3d_fn_t)(void);
nd3d_fn_t recomp_lookup_manual(uint32_t xbox_va);
nd3d_fn_t recomp_lookup(uint32_t xbox_va);
nd3d_fn_t recomp_lookup_kernel(uint32_t xbox_va);
#define IMPORT_MmAllocateContiguousMemoryEx 0x00189C04u

static uint32_t mm_alloc_contiguous(uint32_t size, uint32_t lo, uint32_t hi, uint32_t align, uint32_t prot)
{
    uint32_t va = G32(IMPORT_MmAllocateContiguousMemoryEx);
    nd3d_fn_t fn = recomp_lookup_manual(va);
    if (!fn) fn = recomp_lookup(va);
    if (!fn) fn = recomp_lookup_kernel(va);
    if (!fn) { nd3d_log("MmAllocateContiguousMemoryEx import %08X unresolved\n", va); return 0; }
    return nd3d_call(fn, 0, 0, 5, size, lo, hi, align, prot);
}

/* ------------------------------------------------------------ small helpers */
#define DEV_RT        0x21B4u
#define DEV_Z         0x21B8u
#define DEV_NBUF      0x21BCu
#define DEV_BUF0      0x21C0u   /* back buffer */
#define DEV_BUF1      0x21C4u   /* display buffer shown after Swap */
#define DEV_BUF2      0x21C8u
#define DEV_AUTOZ     0x21CCu
#define RS_SWAPFILTER        0x00175820u
#define RS_PRESENTINTERVAL   0x00175824u
#define RS_ZENABLE           0x00175864u
#define RS_STENCILENABLE     0x00175868u
#define RS_MULTISAMPLEMODE   0x00175890u
#define RS_MSRTMODE          0x00175894u
#define RS_LINEWIDTH         0x0017589Cu
#define RS_DXT1NOISE         0x001758A4u
#define RS_YUVENABLE         0x001758A8u
#define FMT_FLAGS            0x00173FF8u   /* per-D3DFORMAT flag byte table */

static inline float u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static inline uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
/* cvttss2si: truncation, 0x80000000 for NaN or out of range. */
static inline int32_t cvtt(float f)
{
    double d = f;
    if (!(d > -2147483649.0 && d < 2147483648.0)) return INT32_MIN;
    return (int32_t)d;
}
/* Width/height rule used throughout the library (L27916, L23967, ...). */
static uint32_t surf_w(uint32_t s)
{
    uint32_t size = G32(s + 0x10);
    return size ? (size & 0xFFFu) + 1u : 1u << ((G32(s + 0xC) >> 20) & 15u);
}
static uint32_t surf_h(uint32_t s)
{
    uint32_t size = G32(s + 0x10);
    return size ? ((size >> 12) & 0xFFFu) + 1u : 1u << (G8(s + 0xF) & 15u);
}
static uint32_t surf_pitch(uint32_t s) { return nd3d_call(sub_00166150, 0, 0, 1, s); }
static uint32_t parent_or_self(uint32_t s) { uint32_t p = G32(s + 0x14); return p ? p : s; }

/* One-shot diagnostics. */
#define LOG_ONCE(...) do { static int _once; if (!_once) { _once = 1; nd3d_log(__VA_ARGS__); } } while (0)

static void set_viewport(uint32_t vp);
static uint32_t set_scissors(uint32_t count, uint32_t exclusive, uint32_t rects);

/* ======================================================================
 * 0x1682A0 D3D_CommonSetRenderTarget(pRenderTarget, pZStencil, pViewport)
 * stdcall, ret 12 (L27851).
 * ====================================================================== */
static void common_set_rt(uint32_t rt, uint32_t z, uint32_t vp)
{
    uint32_t dev = nd3d_device();
    if (rt == 0) rt = G32(dev + DEV_RT);                 /* NULL: keep the current target */
    uint32_t cpitch = surf_pitch(rt);
    uint32_t zpitch = z ? surf_pitch(z) : cpitch;        /* quirk: no Z -> zeta pitch = colour pitch */

    uint32_t msmode, w, h, sxb, syb;                     /* scales kept as float bit patterns */
    if (rt == G32(dev + DEV_BUF0)) {
        msmode = G32(RS_MULTISAMPLEMODE);
        sxb = G32(dev + 0x528); syb = G32(dev + 0x52C);
        uint32_t b1 = G32(dev + DEV_BUF1), b0;
        /* fild (unsigned fix-up) * scale + 0.5, rounded to float, truncated (0x16DED0). */
        w = (uint32_t)cvtt((float)((double)surf_w(b1) * (double)u2f(sxb) + 0.5));
        h = (uint32_t)cvtt((float)((double)surf_h(b1) * (double)u2f(syb) + 0.5));
        b0 = G32(dev + DEV_BUF0);
        if (!(w < surf_w(b0))) w = surf_w(b0);           /* unsigned min with the back buffer */
        if (!(h < surf_h(b0))) h = surf_h(b0);
    } else {
        msmode = G32(RS_MSRTMODE);
        sxb = syb = 0x3F800000u;                         /* 1.0f */
        w = surf_w(rt); h = surf_h(rt);
    }

    uint32_t fmt = nd3d_call(sub_0016E0B0, 0, 0, 2, rt, z);
    uint32_t flags = G32(dev + DEV_FLAGS) & ~0x8000u;
    G32(dev + DEV_FLAGS) = flags;
    if (msmode != 0 && !(fmt & 0x200u)) {                /* hardware multisample, not swizzled */
        flags |= 0x8000u;
        sxb = f2u((float)((double)u2f(sxb) * 0.5));
        w = (w + 1u) >> 1;
        G32(dev + DEV_FLAGS) = flags;
        if (msmode == 2) {
            syb = f2u((float)((double)u2f(syb) * 0.5));
            h = (h + 1u) >> 1;
            fmt |= 0x2000u;                              /* SQUARE_OFFSET_4 */
        } else fmt |= 0x1000u;                           /* CENTER_CORNER_2 */
    }

    /* Reference counts and Lock stamps (L28179-28243). */
    nd3d_call(sub_001691B0, 0, 0, 1, rt);
    uint32_t old = G32(dev + DEV_RT);
    if (old) {
        G32(parent_or_self(old) + 8) = G32(dev + DEV_TIME);
        nd3d_call(sub_001693E0, 0, 0, 1, G32(dev + DEV_RT));
    }
    old = G32(dev + DEV_Z);
    G32(dev + DEV_RT) = rt;
    if (old) {
        G32(parent_or_self(old) + 8) = G32(dev + DEV_TIME);
        nd3d_call(sub_001693E0, 0, 0, 1, G32(dev + DEV_Z));
    }
    G32(dev + DEV_Z) = z;
    if (z) {
        nd3d_call(sub_001691B0, 0, 0, 1, z);
        nd3d_call(sub_0016E1D0, 0, 0, 2, z, dev + 0x510); /* dev+0x510 = Z scale */
    }

    /* Surface words. The original sends pitch/colour/zeta twice (with NOP and
     * WAIT_FOR_IDLE around them, a hardware workaround); one store of each is the
     * same final state. Addresses are physical (header Data), as the library wrote them. */
    uint32_t cdata = G32(rt + 4), zdata = z ? G32(z + 4) : 0u;
    ND3D_SET(0x20C, (zpitch << 16) | (cpitch & 0xFFFFu));   /* SET_SURFACE_PITCH */
    ND3D_SET(0x210, cdata);                                /* SET_SURFACE_COLOR_OFFSET */
    ND3D_SET(0x214, zdata);                                /* SET_SURFACE_ZETA_OFFSET */
    ND3D_SET(0x200, w << 16);                              /* SET_SURFACE_CLIP_HORIZONTAL */
    ND3D_SET(0x204, h << 16);                              /* SET_SURFACE_CLIP_VERTICAL */

    uint32_t c0 = G32(RS_YUVENABLE) ? 0x10100001u : 0x100001u;
    if (G32(RS_ZENABLE) == 2) c0 |= 0x10000u;
    uint32_t zs = G32(dev + DEV_Z);
    if (zs) { uint32_t zf = G8(zs + 0xD); if (zf == 0x2D || zf == 0x2B || zf == 0x31 || zf == 0x2F) c0 |= 0x1000u; }
    ND3D_SET(0x290, c0);                                   /* SET_CONTROL0 */
    ND3D_SET(0x30C, (G32(RS_ZENABLE) && G32(dev + DEV_Z)) ? 1u : 0u);       /* DEPTH_TEST_ENABLE */
    ND3D_SET(0x32C, (G32(RS_STENCILENABLE) && G32(dev + DEV_Z)) ? 1u : 0u); /* STENCIL_TEST_ENABLE */

    flags = G32(dev + DEV_FLAGS);
    if ((flags & 0x100u) && rt == G32(dev + DEV_BUF0))
        G32(dev + DEV_FLAGS) = flags & ~0x100u;            /* deferred FLIP_STALL (0x130): command, nothing to store */
    ND3D_SET(0x208, fmt);                                  /* SET_SURFACE_FORMAT */

    G32(dev + 0x51C) = syb;
    G32(dev + 0x518) = sxb;
    /* m = min(sx, sy) (x87 fcomp: sx < sy picks sx, otherwise sy). */
    uint32_t mb = ((double)u2f(sxb) < (double)u2f(syb)) ? sxb : syb;
    if (!((double)u2f(mb) == (double)GF(dev + 0x520))) {   /* changed (or NaN) */
        G32(dev + 0x520) = mb;
        float m = u2f(mb);
        int32_t idx = cvtt((float)((double)m + (double)m + 0.5));
        G32(dev + 0x524) = G32(0x1740E4u + (uint32_t)idx * 4u);
        G32(ND3D_DIRTY) |= 0x100u;
        nd3d_call(sub_00167410, 0, 0, 2, 0x9Du, G32(RS_LINEWIDTH)); /* re-apply LINEWIDTH */
    }
    nd3d_call(nd3d_h_0016AC90, dev, 0, 0);                 /* pass-through program constants */
    set_viewport(vp);
    nd3d_call(sub_00167970, 0, 0, 1, G32(RS_DXT1NOISE));
}

void n_001682A0(void)
{
    uint32_t rt = ARG(1), z = ARG(2), vp = ARG(3);
    common_set_rt(rt, z, vp);
    RETV(12);
}

/* ======================================================================
 * 0x1666D0 D3DDevice_SetViewport(const D3DVIEWPORT8 *), stdcall, ret 4 (L23912).
 * ====================================================================== */
static void set_viewport(uint32_t vp)
{
    uint32_t dev = nd3d_device();
    uint32_t rt = G32(dev + DEV_RT);
    uint32_t s = (rt == G32(dev + DEV_BUF0)) ? G32(dev + DEV_BUF1) : rt;
    uint32_t x = G32(vp), y = G32(vp + 4);               /* the "max(0,v)" idiom is a no-op */
    uint32_t right = x + G32(vp + 8);
    if (!(right < surf_w(s))) right = surf_w(s);
    uint32_t bottom = G32(vp + 0xC) + y;
    if (!(bottom < surf_h(s))) bottom = surf_h(s);
    G32(dev + 0xA90) = x;
    G32(dev + 0xA94) = y;
    G32(dev + 0xA98) = right - x;
    G32(dev + 0xA9C) = bottom - y;
    G32(dev + 0xAA0) = G32(vp + 0x10);
    G32(dev + 0xAA4) = G32(vp + 0x14);
    nd3d_call(sub_001675A0, 0, 0, 0);
    set_scissors(0, 0, 0);
    nd3d_call(nd3d_h_00167F80, dev, G32(dev + DEV_PUT), 0); /* viewport offset/scale + clip range */
    G32(ND3D_DIRTY) |= 0x100u;
}

void n_001666D0(void)
{
    uint32_t vp = ARG(1);
    set_viewport(vp);
    RETV(4);
}

/* ======================================================================
 * 0x166D10 D3DDevice_SetScissors(Count, Exclusive, const D3DRECT *), stdcall,
 * ret 12 (L24835). Returns EAX = Exclusive as the original leaves it.
 * ====================================================================== */
static uint32_t set_scissors(uint32_t count, uint32_t exclusive, uint32_t rects)
{
    uint32_t dev = nd3d_device();
    float sx = GF(dev + 0x518), sy = GF(dev + 0x51C);
    uint32_t local[4];
    int use_local = 0;
    if (count == 0) {                                     /* whole viewport, inclusive */
        uint32_t x = G32(dev + 0xA90), y = G32(dev + 0xA94);
        local[0] = x; local[1] = y;
        local[2] = G32(dev + 0xA98) + x; local[3] = G32(dev + 0xA9C) + y;
        exclusive = count;                                /* 0 */
        count = 1; use_local = 1;
    }
    ND3D_SET(0x2B4, exclusive);                           /* SET_WINDOW_CLIP_TYPE */
    for (uint32_t i = 0; i < count; i++) {               /* no clamp to 8 rects, as the original */
        int32_t x1, y1, x2, y2;
        if (use_local) { x1 = (int32_t)local[0]; y1 = (int32_t)local[1]; x2 = (int32_t)local[2]; y2 = (int32_t)local[3]; }
        else { uint32_t r = rects + 16u * i; x1 = (int32_t)G32(r); y1 = (int32_t)G32(r + 4); x2 = (int32_t)G32(r + 8); y2 = (int32_t)G32(r + 0xC); }
        float t = (float)((double)x1 * (double)sx);       /* x1*sx is rounded to float first */
        int32_t X2 = cvtt((float)((double)x2 * (double)sx + 0.5));
        int32_t X1 = cvtt((float)((double)t + 0.5));
        ND3D_SET(0x2C0 + 4u * i, (uint32_t)X1 | ((uint32_t)(X2 - 1) << 16)); /* WINDOW_CLIP_HORIZONTAL(i) */
        t = (float)((double)y1 * (double)sy);
        int32_t Y2 = cvtt((float)((double)y2 * (double)sy + 0.5));
        int32_t Y1 = cvtt((float)((double)t + 0.5));
        ND3D_SET(0x2E0 + 4u * i, (uint32_t)Y1 | ((uint32_t)(Y2 - 1) << 16)); /* WINDOW_CLIP_VERTICAL(i) */
    }
    /* Save the rects (rep movsd of Count*16 bytes, no clamp). */
    uint32_t dwords = (count << 4) >> 2;
    for (uint32_t i = 0; i < dwords; i++)
        G32(dev + 0x2320 + 4u * i) = use_local ? local[i & 3] : G32(rects + 4u * i);
    G32(dev + 0x23A0) = count;
    G32(dev + 0x23A4) = exclusive;
    return exclusive;
}

void n_00166D10(void)
{
    uint32_t count = ARG(1), exclusive = ARG(2), rects = ARG(3);
    uint32_t r = set_scissors(count, exclusive, rects);
    RET(r, 12);
}

/* ======================================================================
 * 0x167030 D3DDevice_SetScreenSpaceOffset(float x, float y), stdcall, ret 8 (L25279).
 * ====================================================================== */
void n_00167030(void)
{
    float x = ARGF(1), y = ARGF(2);
    uint32_t dev = nd3d_device();
    G32(dev + 0xAA8) = f2u((float)(0.53125 + (double)x));
    G32(dev + 0xAAC) = f2u((float)(0.53125 + (double)y));
    nd3d_call(nd3d_h_0016AC90, dev, 0, 0);
    nd3d_call(nd3d_h_00167F80, dev, G32(dev + DEV_PUT), 0);
    RETV(8);
}

/* ======================================================================
 * 0x168C90 D3DDevice_Clear(Count, pRects, Flags, Color, Z, Stencil), stdcall,
 * ret 24 (L29271).
 * ====================================================================== */
static void clear_impl(uint32_t count, uint32_t rects, uint32_t flags, uint32_t color, uint32_t zbits, uint32_t stencil)
{
    uint32_t dev = nd3d_device();
    uint32_t z = G32(dev + DEV_Z), rt = G32(dev + DEV_RT);
    uint32_t restore = 0;
    if (G8(FMT_FLAGS + G8(rt + 0xD)) & 1u) {             /* swizzled RT: clear it as a pitch surface */
        uint32_t fmt = nd3d_call(sub_0016E0B0, 0, 0, 2, rt, z);
        restore = fmt;
        ND3D_SET(0x208, (fmt & ~0x200u) | 0x100u);
    }
    if (flags & 0xF0u) {                                  /* colour packing for 16-bit targets */
        uint32_t idx = (uint32_t)G8(rt + 0xD) - 3u;
        if (idx <= 0x19u) {
            uint32_t target = G32(0x16902Cu + 4u * G8(0x169038u + idx));
            uint32_t c = color, a;
            if (target == 0x168D1Eu) {                    /* R5G6B5 / LIN_R5G6B5 */
                a = (((c >> 3) & 0x1F0000u) | (c & 0xFC00u)) >> 2;
                color = (a | (c & 0xF8u)) >> 3;
            } else if (target == 0x168D3Bu) {             /* X1R5G5B5 / LIN_X1R5G5B5 */
                a = (((c >> 3) & 0x1F0000u) | (c & 0xF800u)) >> 3;
                color = (a | (c & 0xF8u)) >> 3;
            }                                             /* 0x168D65: raw ARGB */
        }
    }
    if (z == 0) {
        flags &= ~3u;
        if (flags == 0) return;   /* quirk: the swizzle SET_SURFACE_FORMAT above is not restored */
    }
    uint32_t zval = 0;
    if (flags & 1u) {
        uint32_t zi = (uint32_t)G8(z + 0xD) - 0x2Au;      /* no bounds check in the original */
        uint32_t target = G32(0x169054u + zi * 4u);
        float zf = u2f(zbits);
        if (target == 0x168D8Du) {                        /* D16 / LIN_D16 */
            int32_t r = cvtt((float)((double)zf * 65535.0));
            if (r > 0xFFFF) r = 0xFFFF;
            zval = r < 0 ? 0u : (uint32_t)r;
        } else if (target == 0x168DBFu) {                 /* D24S8 / LIN_D24S8 */
            int32_t r = cvtt((float)((double)zf * 16777215.0));
            if (r > 0xFFFFFF) r = 0xFFFFFF;
            zval = (r < 0 ? 0u : (uint32_t)r) << 8;
        } else if (target == 0x168DF1u) {                 /* F16 / LIN_F16 */
            if (!(zf == 0.0f)) { double d = (double)zf * 511.9375; uint64_t b; memcpy(&b, &d, 8);
                                 zval = (((uint32_t)(b >> 32) >> 8) - 0x8000u) & 0xFFFFu; }
        } else if (target == 0x168E27u) {                 /* F24S8 / LIN_F24S8 */
            if (!(zf == 0.0f)) { double d = (double)zf * 1e30; uint64_t b; memcpy(&b, &d, 8);
                                 zval = (((uint32_t)(b >> 32) + 0xF8000000u) & 0xFFFFFFF0u) << 4; }
        } else {
            /* ND3D-CHECK: a depth format outside 0x2A..0x31 makes the original jump through
             * garbage; the native version clears with Z = 0. */
            LOG_ONCE("Clear: depth format %02X has no Z conversion\n", G8(z + 0xD));
        }
    }
    uint32_t vx = G32(dev + 0xA90), vy = G32(dev + 0xA94);
    uint32_t vr = G32(dev + 0xA98) + vx, vb = G32(dev + 0xA9C) + vy;
    uint32_t local[4] = { vx, vy, vr, vb };
    int use_local = 0;
    if (count == 0) { count = 1; use_local = 1; }
    for (uint32_t i = 0; count != 0; count--, i++) {
        int32_t r0, r1, r2, r3;
        if (use_local) { r0 = (int32_t)local[0]; r1 = (int32_t)local[1]; r2 = (int32_t)local[2]; r3 = (int32_t)local[3]; }
        else { uint32_t p = rects + 16u * i; r0 = (int32_t)G32(p); r1 = (int32_t)G32(p + 4); r2 = (int32_t)G32(p + 8); r3 = (int32_t)G32(p + 0xC); }
        int32_t x1 = r0 > (int32_t)vx ? r0 : (int32_t)vx;   /* intersect with the viewport (signed) */
        int32_t y1 = r1 > (int32_t)vy ? r1 : (int32_t)vy;
        int32_t x2 = r2 < (int32_t)vr ? r2 : (int32_t)vr;
        int32_t y2 = r3 < (int32_t)vb ? r3 : (int32_t)vb;
        if (x1 >= x2 || y1 >= y2) continue;
        double sx = GF(dev + 0x518), sy = GF(dev + 0x51C);
        int32_t X1 = cvtt((float)((double)x1 * sx + 0.5));
        int32_t X2 = cvtt((float)((double)x2 * sx + 0.5));
        int32_t Y1 = cvtt((float)((double)y1 * sy + 0.5));
        int32_t Y2 = cvtt((float)((double)y2 * sy + 0.5));
        uint32_t hw = (((uint32_t)X2 << 16) - 0x10000u) | (uint32_t)X1;
        uint32_t vw = (((uint32_t)Y2 << 16) - 0x10000u) | (uint32_t)Y1;
        uint32_t zsv = zval | stencil;                     /* stencil OR'ed raw, as the original */
        ND3D_SET(0x1D98, hw);                              /* SET_CLEAR_RECT_HORIZONTAL */
        ND3D_SET(0x1D9C, vw);                              /* SET_CLEAR_RECT_VERTICAL */
        ND3D_SET(0x1D8C, zsv);                             /* SET_ZSTENCIL_CLEAR_VALUE */
        ND3D_SET(0x1D90, color);                           /* SET_COLOR_CLEAR_VALUE */
        nd3d_clear(flags, hw & 0xFFFFu, hw >> 16, vw & 0xFFFFu, vw >> 16, color, zsv); /* CLEAR_SURFACE */
    }
    if (restore) ND3D_SET(0x208, restore);
}

void n_00168C90(void)
{
    uint32_t count = ARG(1), rects = ARG(2), flags = ARG(3), color = ARG(4), z = ARG(5), stencil = ARG(6);
    clear_impl(count, rects, flags, color, z, stencil);
    RETV(24);
}

/* ======================================================================
 * 0x1663E0 D3DDevice_CopyRects(pSrc, pSrcRects, cRects, pDst, pDstPoints),
 * stdcall, ret 20 (L23504), and its blit emitter 0x166330(n, rects, points),
 * stdcall, ret 12 (L23412).
 *
 * The surface2d object (subchannel 3) keeps its format/pitch/offsets between
 * CopyRects and the per-rect image blits (subchannel 2), as the hardware object
 * state did. Old decoder equivalence: lean_gpu.c blit() (SRCCOPY, format 0xA).
 * ====================================================================== */
static struct { uint32_t format, pitch, src, dst; } s2d;   /* 0x300, 0x304, 0x308, 0x30C */

static void blit_one(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, uint32_t px, uint32_t py)
{
    uint32_t pin = (left & 0xFFFFu) | (top << 16);                          /* POINT_IN */
    uint32_t pout = (px & 0xFFFFu) | (py << 16);                            /* POINT_OUT */
    uint32_t size = ((right - left) & 0xFFFFu) ^ ((bottom - top) << 16);   /* SIZE */
    unsigned sx = pin & 0xFFFFu, sy = pin >> 16, dx = pout & 0xFFFFu, dy = pout >> 16;
    unsigned w = size & 0xFFFFu, h = size >> 16;
    unsigned spitch = s2d.pitch & 0xFFFFu, dpitch = s2d.pitch >> 16;
    uint32_t src = 0x80000000u + s2d.src, dst = 0x80000000u + s2d.dst;      /* DMA objects 3/0xB: base 0 */
    if (!w || !h) return;
    if (s2d.format == 0xAu) { nd3d_copy_rect(src, spitch, dst, dpitch, sx, sy, dx, dy, w, h); return; }
    /* ND3D-CHECK: Y8 (1) and R5G6B5 (4) blits (raw copies of swizzled/compressed
     * surfaces) were unsupported by the old decoder. A byte-aligned copy is the same
     * bytes as a 32-bit copy, so it is expressed that way; anything else is skipped. */
    unsigned bpp = s2d.format == 1u ? 1u : s2d.format == 4u ? 2u : 0u;
    if (bpp && !((w * bpp) & 3u) && !((sx * bpp) & 3u) && !((dx * bpp) & 3u)) {
        nd3d_copy_rect(src, spitch, dst, dpitch, sx * bpp / 4u, sy, dx * bpp / 4u, dy, w * bpp / 4u, h);
        return;
    }
    LOG_ONCE("CopyRects: blit format %X (%ux%u) not supported natively\n", s2d.format, w, h);
}

void n_00166330(void)
{
    uint32_t n = ARG(1), rects = ARG(2), points = ARG(3);
    for (; n != 0; n--, rects += 16u, points += 8u)
        blit_one(G32(rects), G32(rects + 4), G32(rects + 8), G32(rects + 0xC), G32(points), G32(points + 4));
    RETV(12);
}

static void copy_rects(uint32_t src, uint32_t srects, uint32_t n, uint32_t dst, uint32_t dpoints)
{
    uint32_t dev = nd3d_device();
    uint32_t sfmt = G8(src + 0xD);
    uint32_t fl = G8(FMT_FLAGS + sfmt);
    uint32_t bpp = (fl >> 3) & 7u;
    uint32_t spitch = surf_pitch(src), dpitch = surf_pitch(dst);
    int raw = (fl & 1u) ? 1 : ((nd3d_call(sub_0016E020, 0, 0, 1, sfmt) & 0xFFu) != 0);
    uint32_t cache = G32(dev + 0x20DC), fmt, rw = 0, rh = 0;
    if (n == 0) n = 1;
    if (raw) {                                            /* swizzled/compressed: raw byte rows */
        uint32_t size = nd3d_call(sub_001661B0, 0, 0, 1, src);
        if (size <= 0x1FC0u) { spitch = dpitch = (size + 0x3Fu) & ~0x3Fu; rw = size; rh = 1; }
        else { spitch = dpitch = 0x1000u; rw = 0x1000u; rh = size >> 12; }
        if (!cache) fmt = 1;                              /* Y8, width in bytes */
        else {
            if (bpp) rw /= bpp;
            else LOG_ONCE("CopyRects: zero pixel size with a blit format cache\n"); /* ND3D-CHECK: original divides by zero */
            fmt = cache;
        }
    } else fmt = cache ? cache : bpp == 1u ? 1u : bpp == 2u ? 4u : 0xAu;
    /* surface2d: SET_OFFSET_SOURCE/DESTIN, SET_COLOR_FORMAT, SET_PITCH */
    s2d.src = G32(src + 4); s2d.dst = G32(dst + 4);
    s2d.format = fmt; s2d.pitch = (dpitch << 16) | (spitch & 0xFFFFu);

    do {
        uint32_t b = n > 16u ? 16u : n;
        n -= b;
        uint32_t lr[16][4], lp[16][2], rp = 0, pp = 0;
        if (srects) { rp = srects; srects += 0x100u; }
        else for (uint32_t i = 0; i < b; i++) { lr[i][0] = 0; lr[i][1] = 0; lr[i][2] = surf_w(src); lr[i][3] = surf_h(src); }
        if (dpoints) { pp = dpoints; dpoints += 0x80u; }
        else for (uint32_t i = 0; i < b; i++) {           /* points = rect origins (before raw rewrite) */
            lp[i][0] = rp ? G32(rp + 16u * i) : lr[i][0];
            lp[i][1] = rp ? G32(rp + 16u * i + 4) : lr[i][1];
        }
        if (raw) for (uint32_t i = 0; i < b; i++) {       /* synthetic rects; written into the caller's array if given */
            if (rp) { G32(rp + 16u * i) = 0; G32(rp + 16u * i + 4) = 0; G32(rp + 16u * i + 8) = rw; G32(rp + 16u * i + 0xC) = rh; }
            else { lr[i][0] = 0; lr[i][1] = 0; lr[i][2] = rw; lr[i][3] = rh; }
        }
        for (uint32_t i = 0; i < b; i++) {               /* 0x166330 */
            uint32_t l, t, r, bo, x, y;
            if (rp) { l = G32(rp + 16u * i); t = G32(rp + 16u * i + 4); r = G32(rp + 16u * i + 8); bo = G32(rp + 16u * i + 0xC); }
            else { l = lr[i][0]; t = lr[i][1]; r = lr[i][2]; bo = lr[i][3]; }
            if (pp) { x = G32(pp + 8u * i); y = G32(pp + 8u * i + 4); } else { x = lp[i][0]; y = lp[i][1]; }
            blit_one(l, t, r, bo, x, y);
        }
    } while (n != 0);

    G32(parent_or_self(src) + 8) = G32(dev + DEV_TIME);
    G32(parent_or_self(dst) + 8) = G32(dev + DEV_TIME);
}

void n_001663E0(void)
{
    uint32_t src = ARG(1), srects = ARG(2), n = ARG(3), dst = ARG(4), dpoints = ARG(5);
    copy_rects(src, srects, n, dst, dpoints);
    RETV(20);
}

/* ======================================================================
 * Swap internals.
 *   0x169790 apply the filter-blit state (stdcall(pSnapshot), ret 4; the pointer
 *            is not used), L30857.
 *   0x1698B0 restore from the snapshot (stdcall(pSnapshot), ret 4), L31002.
 *   0x169C80 AA copy/filter (stdcall(Flags), ret 4, ESI = device), L31559.
 *   0x169E60 non-AA buffer rotation (ESI = device, ret), L31830.
 * ====================================================================== */
static void apply_filter_state(void)
{
    for (uint32_t i = 0; i < 0xA0u; i += 8u) {           /* 20 render states (table 0x1741D0) */
        uint32_t st = G32(0x1741D0u + i), v = G32(0x1741D4u + i);
        int force = (int32_t)st < 0x5C && (G8(nd3d_device() + 8) & 0x10u);   /* PUREDEVICE: resend simple states */
        if (force || v != G32(ND3D_RENDERSTATE + st * 4u)) nd3d_call(sub_00167410, 0, 0, 2, st, v);
    }
    for (uint32_t i = 0; i < 0x58u; i += 8u) {           /* 11 stage-0 TSS (table 0x174178) */
        uint32_t t = G32(0x174178u + i), v = G32(0x17417Cu + i);
        if (v != G32(ND3D_TEXSTATE + t * 4u)) nd3d_call(sub_00167E40, 0, 0, 3, 0u, t, v);
    }
    nd3d_call(sub_0016AF60, 0, 0, 1, 0u);                 /* SetPixelShader(0) */
    uint32_t f = G32(RS_SWAPFILTER);
    if (f != G32(0x175438u)) nd3d_call(sub_00167E40, 0, 0, 3, 0u, 4u, f);   /* stage 0 MIN/MAG filter */
    if (f != G32(0x175434u)) nd3d_call(sub_00167E40, 0, 0, 3, 0u, 3u, f);
    if (G32(0x1754D8u) != 1u) nd3d_call(sub_00167E40, 0, 0, 3, 1u, 0xCu, 1u); /* stage 1 COLOROP DISABLE */
    nd3d_call(sub_001716A0, 0, 0, 0);
    nd3d_call(sub_0016AC30, 0, 0, 2, 0x174270u, 8u);      /* MOV oPos, v0; MOV oT0, v9 */
    nd3d_vp_mode(6);                                      /* SET_TRANSFORM_EXECUTION_MODE */
    ND3D_SET(0x1E98, 0);                                  /* SET_TRANSFORM_PROGRAM_CXT_WRITE_EN */
    nd3d_vp_start(0);                                     /* SET_TRANSFORM_PROGRAM_START */
    ND3D_SET(0x394, 0);                                   /* SET_CLIP_MIN */
    ND3D_SET(0x398, 0x4B7FFFFFu);                         /* SET_CLIP_MAX = 16777215.0f */
}

void n_00169790(void)
{
    apply_filter_state();
    RETV(4);
}

static void restore_state(uint32_t snap)
{
    uint32_t dev = nd3d_device();
    nd3d_call(sub_0016AF60, 0, 0, 1, G32(snap));          /* saved pixel shader */
    uint32_t pure = G32(dev + DEV_FLAGS) & 0x10u;
    uint32_t c1 = G32(snap + 8);
    if (c1 != G32(0x1754D8u)) nd3d_call(sub_00167E40, 0, 0, 3, 1u, 0xCu, c1);
    for (uint32_t i = 0, p = snap + 0x10u; i < 0xA0u; i += 8u, p += 4u) {
        uint32_t st = G32(0x1741D0u + i), v = G32(p);
        if ((int32_t)st < 0x5C && (G8(nd3d_device() + 8) & 0x10u)) continue;  /* PUREDEVICE: not restored */
        if (v != G32(ND3D_RENDERSTATE + st * 4u)) nd3d_call(sub_00167410, 0, 0, 2, st, v);
    }
    for (uint32_t i = 0, p = snap + 0x60u; i < 0x58u; i += 8u, p += 4u) {
        uint32_t t = G32(0x174178u + i), v = G32(p);
        if (v != G32(ND3D_TEXSTATE + t * 4u)) nd3d_call(sub_00167E40, 0, 0, 3, 0u, t, v);
    }
    if (!pure) {
        if (G32(snap) != 0) {
            for (uint32_t k = 0; k < 0x39u; k++) {        /* 57 pixel-shader render states */
                uint32_t def = G32(G32(snap) + 8);
                if (G32(def + 0x20) || G32(def + 0x24) || k < 8u || k > 9u)
                    nd3d_call(sub_00167410, 0, 0, 2, k, G32(snap + 0x8Cu + 4u * k));
            }
            nd3d_call(sub_00167410, 0, 0, 2, 0x88u, G32(snap + 0xC)); /* PSTextureModes */
        }
        nd3d_call(sub_0016AC30, 0, 0, 2, dev + 0x1858u, 8u); /* reload program slots 0-1 */
    }
    uint32_t vsf = G32(G32(dev + 0x380) + 4);
    if (!(vsf & 0x12u)) nd3d_vp_mode(4);                  /* fixed function */
    else {
        nd3d_vp_mode(6);
        ND3D_SET(0x1E98, vsf & 1u);
        nd3d_vp_start(G32(dev + 0x388));
    }
    nd3d_call(nd3d_h_00167F80, dev, G32(dev + DEV_PUT), 0);
}

void n_001698B0(void)
{
    uint32_t snap = ARG(1);
    restore_state(snap);
    RETV(4);
}

/* Interval code and flip address as the flip software method carried them:
 * payload ((Data | code) << 5) | 1, address = (payload >> 5) & ~0xF. */
static uint32_t flip_front(uint32_t dev)
{
    uint32_t p = G32(RS_PRESENTINTERVAL), code = G32(G32(dev + DEV_BUF1) + 4);
    if (p == 0) p = 1;
    if (p & 1u) code |= 1u;
    if (p & 2u) code |= 2u;
    if (p & 4u) code |= 3u;
    if ((int32_t)p < 0) code |= 8u;                        /* IMMEDIATE */
    uint32_t payload = (code << 5) | 1u;
    return 0x80000000u | ((payload >> 5) & ~0xFu);
}

/* 0x169A30: save RT, Z, texture 0 (public AddRef each) and the viewport. */
static void save_targets(uint32_t dev)
{
    uint32_t v = G32(dev + DEV_RT);
    G32(dev + 0x223C) = v;
    nd3d_call(sub_001691F0, 0, 0, 1, v);
    v = G32(dev + DEV_Z);
    G32(dev + 0x2240) = v;
    if (v) nd3d_call(sub_001691F0, 0, 0, 1, v);
    v = G32(dev + 0xB38);
    G32(dev + 0x2244) = v;
    if (v) nd3d_call(sub_001691F0, 0, 0, 1, v);
    for (uint32_t i = 0; i < 6; i++) G32(dev + 0x2248 + 4u * i) = G32(dev + 0xA90 + 4u * i);
}

/* 0x169A90: restore RT/Z/viewport, release the saved references, restore texture 0. */
static void restore_targets(uint32_t dev)
{
    common_set_rt(G32(dev + 0x223C), G32(dev + 0x2240), dev + 0x2248);
    nd3d_call(sub_00169230, 0, 0, 1, G32(dev + 0x223C));
    if (G32(dev + 0x2240)) nd3d_call(sub_00169230, 0, 0, 1, G32(dev + 0x2240));
    nd3d_call(sub_00166830, 0, 0, 2, 0u, G32(dev + 0x2244));
    if (G32(dev + 0x2244)) nd3d_call(sub_00169230, 0, 0, 1, G32(dev + 0x2244));
}

/* 0x169AF0: the filter triangle (texcoords in texels of the 2x-wide back buffer),
 * drawn through the library's own Begin/SetVertexData/End, whose native versions
 * record it with nd3d_imm_begin/nd3d_attr/nd3d_imm_end. */
static void filter_draw(uint32_t dev)
{
    uint32_t sxb = G32(dev + 0x528), syb = G32(dev + 0x52C);
    uint32_t rt = G32(dev + DEV_RT);
    uint32_t w = surf_w(rt), h = surf_h(rt);
    uint32_t offu = 0, offv = 0, mf = G32(0x175438u);     /* stage 0 filter state */
    if (mf == 4u || mf == 5u) {                            /* QUINCUNX / GAUSSIANCUBIC */
        if ((double)u2f(sxb) == 2.0) offu = 0x3F000000u;  /* 0.5f */
        if ((double)u2f(syb) == 2.0) offv = 0x3F000000u;
    }
    nd3d_call(sub_0016BA20, 0, 0, 1, 5u);                  /* Begin(TRIANGLELIST) */
    nd3d_call(sub_0016B930, 0, 0, 3, 9u, offu, offv);
    nd3d_call(sub_0016B970, 0, 0, 5, 0xFFFFFFFFu, 0u, 0u, 0u, 0x3F800000u);
    float wf = (float)(double)w;
    float u = (float)((double)wf * (double)u2f(sxb) * 4.0 + (double)u2f(offu));
    nd3d_call(sub_0016B930, 0, 0, 3, 9u, f2u(u), offv);
    nd3d_call(sub_0016B970, 0, 0, 5, 0xFFFFFFFFu, f2u((float)((double)wf * 4.0)), 0u, 0u, 0x3F800000u);
    float hf = (float)(double)h;
    float v = (float)((double)h * (double)u2f(syb) * 4.0 + (double)u2f(offv));
    nd3d_call(sub_0016B930, 0, 0, 3, 9u, offu, f2u(v));
    nd3d_call(sub_0016B970, 0, 0, 5, 0xFFFFFFFFu, 0u, f2u((float)((double)hf * 4.0)), 0u, 0x3F800000u);
    nd3d_call(sub_0016BA60, 0, 0, 0);                      /* End (the original tail-calls it) */
}

void n_00169C80(void)
{
    uint32_t flags = ARG(1) & 0xFFu;                       /* only BL is tested */
    uint32_t dev = g_esi;
    uint32_t entry = g_esp;
    /* The snapshot block lives in the original's 0x170-byte frame; keep it on the
     * guest stack because 0x169530/0x1698B0 address it there. */
    uint32_t snap = entry - 0x170u;
    g_esp = entry - 0x17Cu;
    uint32_t show = 0;

    if (flags & 3u) {
        if (G32(dev + DEV_NBUF) == 3u) {                  /* swap Data/Lock of buf[1] and buf[2] */
            uint32_t b1 = G32(dev + DEV_BUF1), b2 = G32(dev + DEV_BUF2);
            uint32_t d1 = G32(b1 + 4), d2 = G32(b2 + 4), l1 = G32(b1 + 8), l2 = G32(b2 + 8);
            G32(b2 + 4) = d1; G32(b2 + 8) = l1;
            G32(b1 + 8) = l2; G32(b1 + 4) = d2;
        }
        save_targets(dev);
        common_set_rt(G32(dev + DEV_BUF1), 0, 0x1740D4u);
        nd3d_call(sub_00166830, 0, 0, 2, 0u, G32(dev + DEV_BUF0));
        if (flags & 1u) {
            nd3d_call(sub_00169530, 0, snap, 0);
            apply_filter_state();
        }
        if (G32(dev + DEV_NBUF) < 3u) {
            /* WAIT_FOR_IDLE (unless IMMEDIATE), flip NOP, FLIP_INCREMENT_WRITE, NOP,
             * FLIP_STALL. The flip is presented below, after the filter draw.
             * ND3D-CHECK: with fewer than 3 buffers the hardware queued this flip ahead
             * of the filter draw and stalled on it; presenting the resolved image is the
             * brief's rule. The game always has 3 buffers. */
            show = flip_front(dev);
        }
        /* 3 buffers: FLIP_INCREMENT_WRITE, NOP, FLIP_STALL only (commands). */
    }
    if (flags & 1u) {
        filter_draw(dev);
        restore_state(snap);
    }
    if (show) nd3d_present(show);
    if (flags & 4u) {
        restore_targets(dev);
        if (G32(dev + DEV_NBUF) == 3u)                     /* WAIT_FOR_IDLE + flip NOP */
            nd3d_present(flip_front(dev));
    }
    g_esp = entry;
    RETV(4);
}

void n_00169E60(void)
{
    uint32_t dev = g_esi;
    uint32_t count = G32(dev + DEV_NBUF);
    /* Rotate Data/Lock: buf[k] <- buf[k-1], buf[0] <- old last. InitializeFrameBuffers
     * always makes count >= 2 (ND3D-CHECK: count < 2 would make the original loop wrap). */
    if (count >= 2u) {
        uint32_t cur = G32(dev + count * 4u + 0x21BC);
        uint32_t lastlock = G32(cur + 8), lastdata = G32(cur + 4);
        uint32_t k = count - 1u, p = dev + k * 4u + 0x21BC;
        do {
            uint32_t prev = G32(p);
            G32(cur + 4) = G32(prev + 4);
            G32(cur + 8) = G32(prev + 8);
            k--; p -= 4u; cur = prev;
        } while (k != 0);
        G32(cur + 4) = lastdata;
        G32(cur + 8) = lastlock;
    } else LOG_ONCE("Swap: frame-buffer count %u, rotation skipped\n", count);
    /* WAIT_FOR_IDLE, flip NOP (buf[1]), FLIP_INCREMENT_WRITE, NOP. */
    nd3d_present(flip_front(dev));
    if (!(G32(dev + DEV_RT) == G32(dev + DEV_BUF0) || G32(RS_PRESENTINTERVAL) == 0x80000000u))
        G32(dev + DEV_FLAGS) |= 0x100u;                    /* FLIP_STALL deferred to SetRenderTarget(buf[0]) */
    ND3D_SET(0x210, G32(G32(dev + DEV_BUF0) + 4));         /* always, even if another RT is bound */
    RETV(0);
}

/* ======================================================================
 * 0x16D2A0 CDevice_InitializeFrameBuffers(pPresentationParameters),
 * thiscall (ECX = device), ret 4, HRESULT (L38932).
 * ====================================================================== */
static uint32_t init_frame_buffers(uint32_t dev, uint32_t pp)
{
    uint32_t save = g_esp;
    uint32_t F = save - 0x5Cu;                             /* the original's frame: out words and tile block */
    uint32_t fmt_out = F + 0x14u, size_out = F + 0x10u, tile = F + 0x44u;
    uint32_t hr = 0x8007000Eu;
    g_esp = F;

    uint32_t n = G32(pp + 0xC); if (n < 1u) n = 1u;
    uint32_t cfmt = nd3d_call(sub_0016DF60, 0, 0, 1, G32(pp + 8));
    uint32_t dfmt = nd3d_call(sub_0016DF60, 0, 0, 1, G32(pp + 0x24));
    uint32_t W = G32(pp + 0), H = G32(pp + 4);
    G32(RS_PRESENTINTERVAL) = G32(pp + 0x30);
    G32(dev + 0x2120) = G32(pp + 0x14);
    uint32_t count = n + 1u;
    uint32_t flags = G32(dev + DEV_FLAGS) & ~0x4000u;
    G32(dev + DEV_NBUF) = count;
    G32(dev + DEV_FLAGS) = flags;
    uint32_t sep = 0;
    if ((G32(pp + 0x10) & 0x3000u) || G32(pp + 0x14) == 3u) {
        flags |= 0x4000u; sep = 1;
        G32(dev + DEV_FLAGS) = flags;
    }
    uint32_t type = G32(pp + 0x10);
    if (type == 0) type = 0x11u;
    uint32_t xs = (type >> 4) & 0xFu, ys = type & 0xFu;
    G32(dev + 0x211C) = type;
    G32(dev + 0x528) = f2u((float)(double)xs);
    G32(dev + 0x52C) = f2u((float)(double)ys);
    uint32_t msm = 0;
    if (type & 0x1000u) msm = ((type & 0xFFu) == 0x21u) ? 1u : 2u;
    uint32_t sfil = (type & 0x100u) ? 4u : (type & 0x200u) ? 5u : 2u;
    G32(RS_MULTISAMPLEMODE) = msm;
    G32(RS_SWAPFILTER) = sfil;

    if (G32(pp + 0x34)) {                                  /* caller-supplied surfaces */
        for (uint32_t i = 0; i < count; i++) {
            uint32_t s = G32(pp + 0x34 + 4u * i), d = dev + 0x21D0 + 0x18u * i;
            for (uint32_t k = 0; k < 6; k++) G32(d + 4u * k) = G32(s + 4u * k);
            G32(dev + DEV_BUF0 + 4u * i) = d;
        }
        uint32_t zs = G32(pp + 0x40);
        if (zs) {
            for (uint32_t k = 0; k < 6; k++) G32(dev + 0x2218 + 4u * k) = G32(zs + 4u * k);
            G32(dev + DEV_AUTOZ) = dev + 0x2218;
        }
    } else {
        uint32_t bfmt = cfmt;                              /* prefilter format override */
        switch (G32(pp + 0x10) & 0xF0000u) {
        case 0x10000u: bfmt = 0x1C; break;
        case 0x20000u: bfmt = 0x11; break;
        case 0x30000u: bfmt = 0x1E; break;
        case 0x40000u: bfmt = 0x12; break;
        default: break;
        }
        uint32_t backW = xs * W, backH = ys * H;
        uint32_t pitch = nd3d_call(sub_0016E070, 0, 0, 2, backW, bfmt);
        uint32_t size = nd3d_call(sub_0016E730, 0, 0, 10, backW, backH, 1u, 1u, bfmt, pitch, 0u, 0u, fmt_out, size_out);
        uint32_t nb = sep ? 1u : count;
        if (!pitch) { LOG_ONCE("InitializeFrameBuffers: zero tile pitch\n"); goto out; } /* ND3D-CHECK: original divides by zero */
        uint32_t total = ((nb * size + pitch - 1u) / pitch) * pitch;
        while (total & 0x3FFFu) total += pitch;
        uint32_t base = mm_alloc_contiguous(total, 0, 0x7FFFFFFu, 0x4000u, 0x404u);
        if (!base) goto out;
        G32(dev + 0x2230) = base;
        for (uint32_t i = 0, data = base; i < nb; i++, data += size) {
            uint32_t hdr = dev + 0x21D0 + 0x18u * i;
            G32(dev + DEV_BUF0 + 4u * i) = hdr;
            nd3d_call(sub_001670D0, 0, 0, 4, hdr, G32(fmt_out), G32(size_out), data);
        }
        if (backW != 0x780u) {                             /* colour tile */
            for (uint32_t k = 0; k < 6; k++) G32(tile + 4u * k) = 0;
            G32(tile + 4) = G32(dev + 0x2230);
            G32(tile + 8) = total;
            G32(tile + 0xC) = surf_pitch(G32(dev + DEV_BUF0));
            nd3d_call(sub_00166C40, 0, 0, 2, 0u, tile);
        }
        if (sep) {                                         /* separate display buffers */
            uint32_t dsize = nd3d_call(sub_0016E730, 0, 0, 10, W, H, 1u, 1u, cfmt, 0u, 0u, 0u, fmt_out, size_out);
            uint32_t dbase = mm_alloc_contiguous(dsize * n, 0, 0x7FFFFFFu, 0x4000u, 0x404u);
            if (!dbase) goto out;
            G32(dev + 0x2234) = dbase;
            for (uint32_t i = 0, data = dbase; i < n; i++, data += dsize) {
                uint32_t hdr = dev + 0x21E8 + 0x18u * i;
                G32(dev + DEV_BUF1 + 4u * i) = hdr;
                nd3d_call(sub_001670D0, 0, 0, 4, hdr, G32(fmt_out), G32(size_out), data);
            }
        }
        if (G32(pp + 0x20)) {                              /* EnableAutoDepthStencil */
            uint32_t zpitch = nd3d_call(sub_0016E070, 0, 0, 2, backW, dfmt);
            uint32_t zsize = nd3d_call(sub_0016E730, 0, 0, 10, backW, backH, 1u, 1u, dfmt, zpitch, 0u, 0u, fmt_out, size_out);
            if (!zpitch) { LOG_ONCE("InitializeFrameBuffers: zero depth tile pitch\n"); goto out; }
            uint32_t ztotal = ((zsize + zpitch - 1u) / zpitch) * zpitch;
            while (ztotal & 0x3FFFu) ztotal += zpitch;
            uint32_t zbase = mm_alloc_contiguous(ztotal, 0, 0x7FFFFFFu, 0x4000u, 0x404u);
            if (!zbase) goto out;
            G32(dev + 0x2238) = zbase;
            G32(dev + DEV_AUTOZ) = dev + 0x2218;
            nd3d_call(sub_001670D0, 0, 0, 4, dev + 0x2218, G32(fmt_out), G32(size_out), zbase);
            if (backW != 0x780u) {                         /* Z-compression tile */
                for (uint32_t k = 0; k < 6; k++) G32(tile + 4u * k) = 0;
                G32(tile) = 0x80000001u;
                G32(tile + 4) = zbase;
                G32(tile + 8) = ztotal;
                G32(tile + 0xC) = surf_pitch(G32(dev + DEV_AUTOZ));
                if ((G8(FMT_FLAGS + dfmt) & 0x3Cu) == 0x20u) G32(tile) = 0x84000001u;
                nd3d_call(sub_00166C40, 0, 0, 2, 1u, tile);
            }
        }
    }
    for (uint32_t i = 0; i < G32(dev + DEV_NBUF); i++) nd3d_call(sub_001691F0, 0, 0, 1, G32(dev + DEV_BUF0 + 4u * i));
    if (G32(dev + DEV_AUTOZ)) nd3d_call(sub_001691F0, 0, 0, 1, G32(dev + DEV_AUTOZ));
    ND3D_SET(0x120, 0);                                    /* SET_FLIP_READ */
    ND3D_SET(0x124, 1);                                    /* SET_FLIP_WRITE */
    ND3D_SET(0x128, G32(dev + DEV_NBUF));                  /* SET_FLIP_MODULO */
    hr = 0;
out:
    g_esp = save;
    return hr;
}

void n_0016D2A0(void)
{
    uint32_t dev = g_ecx, pp = ARG(1);
    uint32_t hr = init_frame_buffers(dev, pp);
    RET(hr, 4);
}

/* ======================================================================
 * Visibility tests.
 *   0x166B40 BeginVisibilityTest(), ret (L24539)
 *   0x166BE0 EndVisibilityTest(Index), ret 4, HRESULT (L24646)
 *   0x165FD0 GetVisibilityTestResult(Index, UINT *pResult, ULONGLONG *pTimeStamp),
 *            ret 12, HRESULT (L22843)
 * The report slot is the guest memory the GPU wrote ({u64 time, u32 count,
 * u32 status}); the native GetResult fills it from nd3d_visibility_result() when the
 * renderer has the result, then answers exactly as the original reads it.
 * ====================================================================== */
void n_00166B40(void)
{
    /* CLEAR_REPORT_VALUE(ZPASS_PIXEL_CNT), SET_ZPASS_PIXEL_COUNT_ENABLE 1.
     * ND3D-CHECK: the Xbox API names the test only at End, so Begin passes index 0;
     * the renderer pairs a Begin with the next End. */
    nd3d_visibility_begin(0);
    RETV(0);
}

void n_00166BE0(void)
{
    uint32_t index = ARG(1);
    uint32_t dev = nd3d_device();
    uint32_t slot = nd3d_call(sub_00166B70, 0, 0, 1, index); /* marks slot+0xC pending */
    if (!slot) RET(0x8007000Eu, 4);
    /* SET_ZPASS_PIXEL_COUNT_ENABLE 0, GET_REPORT(slot). */
    nd3d_visibility_end(index);
    nd3d_call(sub_0016C940, dev, 0, 0);                    /* KickOff */
    RET(0, 4);
}

void n_00165FD0(void)
{
    uint32_t index = ARG(1), presult = ARG(2), pstamp = ARG(3);
    uint32_t dev = nd3d_device();
    uint32_t page = G32(dev + (index >> 8) * 4u + 0x39C);
    uint32_t slot = page + (index & 0xFFu) * 16u;          /* no NULL-page check, as the original */
    if (G32(slot + 0xC) == 0xFFFFFFFFu && page) {
        uint32_t count;
        if (nd3d_visibility_result(index, &count)) {        /* the "GPU" writes the report */
            LARGE_INTEGER c, f; uint64_t ns = 0;
            if (QueryPerformanceCounter(&c) && QueryPerformanceFrequency(&f) && f.QuadPart)
                ns = (uint64_t)(c.QuadPart / f.QuadPart) * 1000000000ull + (uint64_t)(c.QuadPart % f.QuadPart) * 1000000000ull / (uint64_t)f.QuadPart;
            G32(slot) = (uint32_t)ns;                       /* ND3D-CHECK: report time is host ns, not NV2A PTIMER */
            G32(slot + 4) = (uint32_t)(ns >> 32);
            G32(slot + 8) = count;
            G32(slot + 0xC) = 0;
        }
    }
    if (G32(slot + 0xC) == 0xFFFFFFFFu) RET(0x88760828u, 12); /* D3DERR_TESTINCOMPLETE */
    G32(presult) = G32(slot + 8);
    if (pstamp) { G32(pstamp) = G32(slot); G32(pstamp + 4) = G32(slot + 4); }
    RET(0, 12);
}

/* ======================================================================
 * 0x16D800 CDevice::Init(pPresentationParameters), thiscall (ECX = device),
 * ret 4, HRESULT (L39613).
 *
 * The routine writes no state words itself: all of its push-buffer output comes
 * from callees (0x16CFC0, InitializeFrameBuffers, SetVertexShader, SetRenderTarget,
 * 0x16D0A0, Clear), which are native. What it cannot keep is the GPU handshake
 * after KickOff: a spin until the channel GET register (MMIO dev+0x23BC +0x44)
 * reaches the put pointer, calling the delay loop 0x16C620. Nothing consumes a push
 * buffer natively, so that wait is dropped (ND3D-CHECK). Everything else (the
 * semaphore allocation, miniport hardware/DMA/object setup, SetVideoMode, the
 * marker words at physical 0, default state, the initial Z/stencil clear) runs as
 * the original did, through the same routines.
 * ====================================================================== */
/* Miniport DMA objects bound after channel creation, and graphics objects
 * {handle, class, output}; outputs are offsets into the frame (B below). */
static const uint16_t init_bind[11] = { 0x20, 0x50, 0x90, 0x60, 0xA0, 0x80, 0xB0, 0x40, 0x10, 0x30, 0x70 };
static const uint16_t init_obj[11][3] = {
    { 0x0D, 0x97, 0x110 }, { 0x0E, 0x39, 0x0C0 }, { 0x10, 0x9F, 0x160 }, { 0x11, 0x62, 0x0E0 },
    { 0x12, 0x44, 0x130 }, { 0x13, 0x57, 0x0D0 }, { 0x14, 0x43, 0x150 }, { 0x15, 0x12, 0x140 },
    { 0x16, 0x72, 0x120 }, { 0x18, 0x19, 0x100 }, { 0x19, 0x30, 0x0F0 } };

void n_0016D800(void)
{
    uint32_t dev = g_ecx, pp = ARG(1);
    uint32_t entry = g_esp;
    uint32_t A = entry - 0x168u;                           /* the original's frame */
    uint32_t B = A - 8u;                                   /* after push esi, edi */
    uint32_t mp = dev + 0x23C0u;                           /* CMiniport, embedded in the device */
    uint32_t hr;
    g_esp = A;
    G32(A) = dev;

    uint32_t sem = mm_alloc_contiguous(0x60, 0, 0x7FFFFFFu, 0, 4);
    G32(dev + 0x2C20) = sem;
    if (!sem) { g_esp = entry; RET(0x8007000Eu, 4); }
    G32(dev + DEV_DONE_PTR) = sem;                         /* GPU-written fence word */
    G32(dev + 0x2C28) = G32(dev + DEV_DONE_PTR) + 0x20u;
    G32(dev + 0x2C24) = G32(dev + 0x2C28) + 0x20u;
    g_esp = B;
    for (uint32_t i = 0; i < 16; i++) G32(G32(dev + 0x2C28) + 4u * i) = 0;
    hr = nd3d_call(sub_0016C6B0, dev, 0, 0);               /* push-buffer allocation */
    if ((int32_t)hr < 0) goto done;

    G32(ND3D_DIRTY) |= 0x7F7Fu;                            /* all state dirty */
    nd3d_call(sub_0016FCAD, mp, G32(ND3D_DIRTY), 0);       /* CMiniport::InitHardware */
    /* DMA context objects (class, flags, base, limit, out). */
    nd3d_call(sub_0016F99B, mp, 0, 5, 3u, 0x3Du, 0u, 0x7FFAFFFu, B + 0xB0u);
    nd3d_call(sub_0016F99B, mp, 0, 5, 5u, 2u, 0u, 0x7FFAFFFu, B + 0x80u);
    nd3d_call(sub_0016F99B, mp, 0, 5, 4u, 3u, 0u, 0x7FFAFFFu, B + 0xA0u);
    nd3d_call(sub_0016F99B, mp, 0, 5, 9u, 0x3Du, 0u, 0x7FFAFFFu, B + 0x40u);
    G32(dev + 0x2C14) = G32(B + 0x4Cu);
    nd3d_call(sub_0016F99B, mp, 0, 5, 0xAu, 0x3Du, 0u, 0x7FFAFFFu, B + 0x10u);
    G32(dev + 0x2C18) = G32(B + 0x1Cu);
    nd3d_call(sub_0016F99B, mp, 0, 5, 0xBu, 0x3Du, 0u, 0x7FFAFFFu, B + 0x30u);
    G32(dev + 0x2C1C) = G32(B + 0x3Cu);
    nd3d_call(sub_0016F99B, mp, 0, 5, 2u, 3u, G32(dev + 0x2C28), 0x1Fu, B + 0x90u);
    nd3d_call(sub_0016F99B, mp, 0, 5, 7u, 0x3Du, G32(dev + 0x2C24), 0x1Fu, B + 0x60u);
    nd3d_call(sub_0016F99B, mp, 0, 5, 0xCu, 0x3Du, 0x80000000u, 0x10000000u, B + 0x50u);
    nd3d_call(sub_0016F99B, mp, 0, 5, 8u, 0x3Du, G32(dev + DEV_DONE_PTR), 0x20u, B + 0x70u);
    nd3d_call(sub_0016F99B, mp, 0, 5, 6u, 2u, 0u, 0x7FFAFFFu, B + 0x20u);
    nd3d_call(sub_0016F723, mp, 0, 5, 0x206Eu, 0u, B + 0x20u, 0u, B + 0xCu);   /* channel */
    for (unsigned i = 0; i < 11; i++) nd3d_call(sub_0016F786, mp, 0, 1, B + init_bind[i]);
    for (unsigned i = 0; i < 11; i++)
        nd3d_call(sub_0016FA6A, mp, 0, 3, (uint32_t)init_obj[i][0], (uint32_t)init_obj[i][1], B + init_obj[i][2]);

    G32(dev + 0x4FC) = G32(mp);
    G32(dev + 0x23BC) = G32(B + 0xCu);                     /* channel control */
    G32(0x17540Cu) = G32(dev + 0x4FC);
    G32(0x80000000u) = (G32(dev + DEV_PB_START) & 0x0FFFFFFFu) + 1u;  /* jump to the push buffer */
    MemoryBarrier();
    nd3d_call(sub_0016C940, dev, 0, 0);                    /* KickOff */
    /* ND3D-CHECK: GET == PUT spin (0x16DBBD-0x16DBF2) removed; no GPU consumes the buffer. */
    G32(0x80000000u) = 0xDEADBEEFu;
    nd3d_call(sub_0016CFC0, 0, 0, 0);

    hr = init_frame_buffers(dev, pp);
    if ((int32_t)hr < 0) goto done;

    {
        uint32_t pitch = surf_pitch(G32(dev + DEV_BUF1));
        nd3d_call(sub_0016E9C8, mp, 0, 7, G32(pp + 0), G32(pp + 4), G32(pp + 0x2C), G32(pp + 0x28),
                  G32(pp + 8), G32(pp + 0x30), pitch); /* SetVideoMode: display-mode change pending */
    }
    G32(dev + 0x380) = 0x1786D8u;                          /* default vertex shader object */
    nd3d_call(sub_0016AD90, 0, 0, 1, 2u);                  /* SetVertexShader(2) */
    for (uint32_t i = 0; i < 0x46u; i++) G32(0x1785C0u + 4u * i) = 0;
    G32(0x1785C4u) = 0x10u;
    nd3d_call(sub_00165DC0, 0, 0, 2, G32(dev + DEV_BUF0), G32(dev + DEV_AUTOZ)); /* SetRenderTarget */
    nd3d_call(sub_0016D0A0, 0, 0, 0);                      /* default state */
    clear_impl(0, 0, 3u, 0u, 0x3F800000u, 0u);             /* Clear(0, NULL, Z|STENCIL, 0, 1.0f, 0) */
    {                                                      /* debug-monitor hook (NULL in retail) */
        uint32_t t = G32(G32(g_fs_base + 0x20) + 0x250);
        if (t && (t = G32(t + 0x20)) != 0) {
            G32(t) = dev + 0x257C;
            G32(t + 4) = dev + 0x2C10;
            G32(t + 8) = G32(dev + DEV_BUF1);
            G32(t + 0xC) = dev + 0x23C0;
            G32(t + 0x14) = dev + 0x49C;
            G32(t + 0x18) = dev + 0x4A4;
        }
    }
    nd3d_call(sub_00166090, 0, 0, 1, 5u);                  /* SetFlickerFilter(5) */
    nd3d_call(sub_001660E0, 0, 0, 1, 0u);                  /* SetSoftDisplayFilter(0) */
    hr = 0;
done:
    g_esp = entry;
    RET(hr, 4);
}
