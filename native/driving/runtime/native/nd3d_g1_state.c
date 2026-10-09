/* Native D3D, group 1: render-state setters, the immediate texture-stage
 * setters, and the shared helpers declared in nd3d_internal.h.
 *
 * Source of truth: the recompiled originals in src/recomp/gen/recomp_0017.c
 * (spec native-driving/phase2/specs/G1-render-state.md, G2 3.4-3.7 / 7.2,
 * G3 4.16). Each routine keeps the original's CPU effects (g_RenderState /
 * texture-stage shadows, dirty flags 0x175424, device fields, remap bytes) and
 * stores the state words the original wrote into the push buffer with
 * ND3D_SET. Where the G1 spec says "write the shadow only", the brief's rule
 * (store the computed state words too) is followed, so the renderer sees the
 * same register image the hardware would have latched.
 *
 * Push-buffer mechanics (space checks, MakeSpace, headers, advancing dev+0)
 * are gone. Routines whose original returned the advanced write pointer in EAX
 * return the (unchanged) write pointer; no caller uses that value.
 * ND3D-CHECK: EAX for those paths is the current put pointer, not put+N. */
#include "nd3d_internal.h"

/* Recompiled CPU-only library routines called from here (guest ABI). */
void sub_00167470(void);   /* D3DDevice_GetViewportOffsetAndScale(float4 *off, float4 *scale), ret 8 */
void sub_001675A0(void);   /* D3D_UpdateProjectionViewportTransform(), ret */
void sub_0016DED0(void);   /* cvttss2si(float), ret 4 */

/* g_RenderState slots (0x175628 + 4 * D3DRS index). */
#define RS_DEPTHCLIPCONTROL      0x00175770u  /*  82 */
#define RS_FOGTABLEMODE          0x0017579Cu  /*  93 */
#define RS_VERTEXBLEND           0x0017584Cu  /* 137 */
#define RS_FOGCOLOR              0x00175850u  /* 138 */
#define RS_FILLMODE              0x00175854u  /* 139 */
#define RS_BACKFILLMODE          0x00175858u  /* 140 */
#define RS_TWOSIDEDLIGHTING      0x0017585Cu  /* 141 */
#define RS_NORMALIZENORMALS      0x00175860u  /* 142 */
#define RS_ZENABLE               0x00175864u  /* 143 */
#define RS_STENCILENABLE         0x00175868u  /* 144 */
#define RS_STENCILFAIL           0x0017586Cu  /* 145 */
#define RS_FRONTFACE             0x00175870u  /* 146 */
#define RS_CULLMODE              0x00175874u  /* 147 */
#define RS_TEXTUREFACTOR         0x00175878u  /* 148 */
#define RS_LOGICOP               0x00175880u  /* 150 */
#define RS_EDGEANTIALIAS         0x00175884u  /* 151 */
#define RS_MULTISAMPLEANTIALIAS  0x00175888u  /* 152 */
#define RS_MULTISAMPLEMASK       0x0017588Cu  /* 153 */
#define RS_SHADOWFUNC            0x00175898u  /* 156 */
#define RS_LINEWIDTH             0x0017589Cu  /* 157 */
#define RS_SAMPLEALPHA           0x001758A0u  /* 158 */
#define RS_DXT1NOISEENABLE       0x001758A4u  /* 159 */
#define RS_YUVENABLE             0x001758A8u  /* 160 */
#define RS_OCCLUSIONCULLENABLE   0x001758ACu  /* 161 */
#define RS_STENCILCULLENABLE     0x001758B0u  /* 162 */
#define RS_ROPZCMPALWAYSREAD     0x001758B4u  /* 163 */
#define RS_ROPZREAD              0x001758B8u  /* 164 */
#define RS_DONOTCULLUNCOMPRESSED 0x001758BCu  /* 165 */

/* Read-only float constants in the XBE, read as the originals read them. */
#define K_ONE    0x00189DE8u   /* 1.0f */
#define K_ZERO   0x00189DECu   /* 0.0f */
#define K_HALF   0x00189EB0u   /* 0.5f */
#define K_EIGHT  0x0018B5DCu   /* 8.0f */

static uint32_t fbits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

/* The push-buffer write pointer (never advanced by native routines). */
static uint32_t put_now(uint32_t dev) { return G32(dev + DEV_PUT); }

/* 0x1D84 OCCLUDE_ZSTENCIL_EN as StencilEnable/StencilFail/OcclusionCullEnable/
 * StencilCullEnable compute it from the current shadows. */
static uint32_t occlude_word(void)
{
    uint32_t w = G32(RS_STENCILCULLENABLE) ? 2u : 0u;
    if (G32(RS_OCCLUSIONCULLENABLE) && (!G32(RS_STENCILENABLE) || G32(RS_STENCILFAIL) == 0x1E00u)) w |= 1u;
    return w;
}

/* 0x1D7C ANTI_ALIASING_CONTROL: (mask << 16) | sample-alpha | (RT multisampled && MSAA on). */
static uint32_t aa_word(uint32_t dev, uint32_t mask, uint32_t alpha)
{
    uint32_t w = (mask << 16) | alpha;
    if ((G32(dev + 8) & 0x8000u) && G32(RS_MULTISAMPLEANTIALIAS)) w |= 1u;
    return w;
}

/* Methods that nd3d_api.h routes to explicit calls instead of the state block. */
static int special_method(uint32_t m)
{
    return m == 0u || m == 0x100u || m == 0x130u || (m >= 0x180u && m <= 0x1A4u) ||
           (m >= 0xA20u && m <= 0xA2Cu) || (m >= 0xAF0u && m <= 0xAFCu) || (m >= 0xB00u && m <= 0xBFCu) ||
           (m >= 0x1500u && m <= 0x15FCu) || (m >= 0x17C8u && m <= 0x17D0u) || (m >= 0x17FCu && m <= 0x1818u) ||
           (m >= 0x1880u && m <= 0x1AFCu) || m == 0x1D6Cu || m == 0x1D70u || m == 0x1D94u ||
           m == 0x1E94u || m == 0x1E9Cu || m == 0x1EA0u || m == 0x1EA4u;
}

/* =================================================================== helpers
 * Guest ABI exactly as the originals (ECX = device; EDX = push pointer where
 * the original took one; plain `ret`). The ones that took a push pointer
 * return EDX unchanged in EAX (nd3d_internal.h). */

/* 0x167F80: viewport offset/scale constants and the clip range.
 * Program/pass-through vertex shader (flags & 0x12): GetViewportOffsetAndScale,
 * constants 59 (offset) / 58 (scale) unless dev+8 & 0x200, clip range from
 * dev+0x23B0/0x23B4. Fixed function: constant 59 = (x0, y0, 0, 0), clip range
 * from dev+0x23A8/0x23AC. Both: CLIP_MIN 0x394 / CLIP_MAX 0x398. */
void nd3d_h_00167F80(void)
{
    uint32_t dev = g_ecx, put = g_edx;
    uint32_t vs = G32(dev + 0x380), cmin, cmax;
    if (G8(vs + 4) & 0x12u) {
        uint32_t o[4], s[4];
        g_esp -= 0x20u;                          /* the original's locals */
        uint32_t off = g_esp, scl = g_esp + 0x10u;
        nd3d_call(sub_00167470, scl, off, 2, off, scl);
        for (unsigned i = 0; i < 4; i++) { o[i] = G32(off + 4u * i); s[i] = G32(scl + 4u * i); }
        g_esp += 0x20u;
        if (!(G32(dev + 8) & 0x200u)) {
            nd3d_vp_constants(59, o, 1);          /* 0x0A20 VIEWPORT_OFFSET */
            nd3d_vp_constants(58, s, 1);          /* 0x0AF0 VIEWPORT_SCALE */
        }
        if (!(G8(dev + 0x23B8) & 2u)) {
            if (G32(RS_ZENABLE) == 2u) {
                G32(dev + 0x23B4) = G32(dev + 0x510);
                GF(dev + 0x23B0) = (float)((double)GF(dev + 0x50C) * GF(dev + 0x504) * GF(dev + 0x510));
            } else {
                G32(dev + 0x23B0) = 0;
                G32(dev + 0x23B4) = G32(dev + 0x510);
            }
        }
        cmin = G32(dev + 0x23B0); cmax = G32(dev + 0x23B4);
    } else {
        /* X/Y are converted as unsigned (fild + 2^32 when negative). */
        double x = (double)G32(dev + 0xA90) * GF(dev + 0x518) + GF(dev + 0xAA8);
        double y = (double)G32(dev + 0xA94) * GF(dev + 0x51C) + GF(dev + 0xAAC);
        if ((G32(dev + 8) & 0x8000u) && G32(RS_MULTISAMPLEANTIALIAS) != 0) { x -= GF(K_HALF); y -= GF(K_HALF); }
        uint32_t o[4];
        o[0] = fbits((float)x); o[1] = fbits((float)y); o[2] = 0; o[3] = 0;
        nd3d_vp_constants(59, o, 1);              /* 0x0A20 VIEWPORT_OFFSET */
        if (!(G8(dev + 0x23B8) & 1u)) {
            if (G32(RS_ZENABLE) == 2u) {
                G32(dev + 0x23AC) = G32(dev + 0x510);
                GF(dev + 0x23A8) = (float)((double)GF(dev + 0x50C) * GF(dev + 0x504) * GF(dev + 0x510));
            } else {
                GF(dev + 0x23A8) = (float)((double)GF(dev + 0xAA0) * GF(dev + 0x510));
                GF(dev + 0x23AC) = (float)((double)GF(dev + 0xAA4) * GF(dev + 0x510));
            }
        }
        cmin = G32(dev + 0x23A8); cmax = G32(dev + 0x23AC);
    }
    ND3D_SET(0x394, cmin);                        /* CLIP_MIN */
    ND3D_SET(0x398, cmax);                        /* CLIP_MAX */
    RET(put, 0);
}

/* 0x168170: CONTROL0 (0x290) from YuvEnable, ZEnable == W-buffer and a float
 * depth format (dev+0x21B8 surface format byte +0xD). No CPU writes. */
void nd3d_h_00168170(void)
{
    uint32_t dev = g_ecx, put = g_edx;
    uint32_t w = G32(RS_YUVENABLE) ? 0x10100001u : 0x100001u;
    if (G32(RS_ZENABLE) == 2u) w |= 0x10000u;
    uint32_t ds = G32(dev + 0x21B8);
    if (ds) {
        uint32_t f = G8(ds + 0xDu);
        if (f == 0x2Du || f == 0x2Bu || f == 0x31u || f == 0x2Fu) w |= 0x1000u;
    }
    ND3D_SET(0x290, w);
    RET(put, 0);
}

/* 0x16AC90: pass-through (XYZRHW) vertex program. Only when the current vertex
 * shader object has flag 2: constants 0..1 = {sx, sy, zs, w'}, {ax-c, ay-c, 0, 0}
 * and the program (12 or 11 instructions) at program slot 0. */
void nd3d_h_0016AC90(void)
{
    uint32_t dev = g_ecx, vs = G32(dev + 0x380);
    if (!(G8(vs + 4) & 2u)) RET(vs, 0);
    uint32_t prog, ndw;
    if (G32(RS_FOGTABLEMODE) == 0) { prog = 0x174480u; ndw = 0x30u; }
    else if (G8(dev + 8) & 2u) { prog = 0x1743D0u; ndw = 0x2Cu; }
    else { prog = 0x174310u; ndw = 0x30u; }
    /* 0x1EA4 = 0, then 8 constant words (slots 0 and 1). */
    float w = (G32(RS_ZENABLE) == 2u) ? (float)((double)GF(dev + 0x510) * GF(dev + 0x50C))
                                      : (float)(double)GF(K_ONE);
    double c = GF(K_ZERO);
    if ((G32(dev + 8) & 0x8000u) && G32(RS_MULTISAMPLEANTIALIAS) != 0) c = GF(K_HALF);
    uint32_t k[8];
    k[0] = G32(dev + 0x518); k[1] = G32(dev + 0x51C); k[2] = G32(dev + 0x510); k[3] = fbits(w);
    k[4] = fbits((float)((double)GF(dev + 0xAA8) - c));
    k[5] = fbits((float)((double)GF(dev + 0xAAC) - c));
    k[6] = 0; k[7] = 0;
    nd3d_vp_constants(0, k, 2);
    /* 0x1E9C = 0, then the program words (0x16A340 copies them as 0xB00 chunks). */
    uint32_t words[0x30];
    for (unsigned i = 0; i < ndw; i++) words[i] = G32(prog + 4u * i);
    nd3d_vp_program_words(0, words, ndw / 4u);
    RET(put_now(dev), 0);
}

/* 0x167F30: bump-environment words (BUMP_ENV_MAT00..LOFFSET, 6 words) of
 * hardware stages 1..3 (0x1B68 + 0x40*i) from texture-stage slots 22..27 of
 * D3D stage hw-1 (no pixel shader) or hw (pixel shader bound). */
void nd3d_h_00167F30(void)
{
    uint32_t dev = g_ecx, put = g_edx;
    uint32_t src = 0x175480u + (G32(dev + 0x370) ? 0x80u : 0u);
    for (uint32_t m = 0x1B68u; m <= 0x1BE8u; m += 0x40u, src += 0x80u) {
        uint32_t w[6];
        for (unsigned i = 0; i < 6; i++) w[i] = G32(src + 4u * i);
        ND3D_SETN(m, w, 6);
    }
    RET(put, 0);
}

/* ==================================================== render-state setters */

/* 0x1673E0 D3DDevice_SetRenderState_Simple: ECX = complete header (count 1,
 * subchannel 0, method), EDX = value; plain ret. No shadow write (callers do
 * it). ECX/EDX are preserved. */
void n_001673E0(void)
{
    uint32_t hdr = g_ecx, v = g_edx, m = hdr & 0x1FFCu;
    int special = special_method(m);
    if (special || (hdr >> 18) != 1u || ((hdr >> 13) & 7u) != 0u) {
        /* ND3D-CHECK: never seen (live callers pass constant count-1 headers for
         * D3DRS 57..83, D3D internals the 0x18E888 table); special methods are
         * not stored, a malformed plain header stores its one value. */
        static int warned;
        if (!warned) { warned = 1; nd3d_log("SetRenderState_Simple: unexpected header %08X value %08X (caller %08X)\n", hdr, v, G32(g_esp)); }
    }
    if (!special) ND3D_SET(m, v);
    RET(G32(ND3D_DEV + DEV_PUT), 0);
}

/* 0x1676E0 EdgeAntiAlias (D3DRS 151): LINE_SMOOTH_ENABLE, POLY_SMOOTH_ENABLE. */
void n_001676E0(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    ND3D_SET(0x320, v);
    ND3D_SET(0x324, v);
    G32(RS_EDGEANTIALIAS) = v;
    RET(put_now(dev), 4);
}

/* 0x167720 ShadowFunc (D3DRS 156): SHADOW_COMPARE_FUNC = value - 0x200. */
void n_00167720(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    ND3D_SET(0x1E6C, v - 0x200u);
    G32(RS_SHADOWFUNC) = v;
    RET(put_now(dev), 4);
}

/* 0x167760 FogColor (D3DRS 138): FOG_COLOR = ARGB with R and B swapped. */
void n_00167760(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    ND3D_SET(0x2A8, (v & 0xFF00FF00u) | ((v & 0xFFu) << 16) | ((v >> 16) & 0xFFu));
    G32(RS_FOGCOLOR) = v;
    RET(put_now(dev), 4);
}

/* 0x1677B0 CullMode (D3DRS 147): CULL_FACE_ENABLE; CULL_FACE = 0x404 FRONT when
 * the cull mode equals FrontFace, else 0x405 BACK. */
void n_001677B0(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    if (v == 0) {
        ND3D_SET(0x308, 0);
    } else {
        ND3D_SET(0x308, 1);
        ND3D_SET(0x39C, 0x404u + (v != G32(RS_FRONTFACE) ? 1u : 0u));
    }
    G32(RS_CULLMODE) = v;
    RET(put_now(dev), 4);
}

/* 0x167820 FrontFace (D3DRS 146): FRONT_FACE, then the original overwrites its
 * argument with the current CullMode and tail-jumps to CullMode (whose ret 4
 * returns to our caller). */
void n_00167820(void)
{
    uint32_t v = ARG(1);
    ND3D_SET(0x3A0, v);
    uint32_t cull = G32(RS_CULLMODE);
    G32(RS_FRONTFACE) = v;
    ARG(1) = cull;
    n_001677B0();
}

/* 0x167860 NormalizeNormals (D3DRS 142): NORMALIZATION_ENABLE; dirty 0x200. */
void n_00167860(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    ND3D_SET(0x3A4, v);
    G32(ND3D_DIRTY) |= 0x200u;
    G32(RS_NORMALIZENORMALS) = v;
    RET(put_now(dev), 4);
}

/* 0x1678A0 TextureFactor (D3DRS 148): with no pixel shader, both combiner
 * factor banks (0xA60..0xA9C, 16 words) = value; with one, shadow only. The
 * PS-constant shadows g_RenderState[10..25] are not updated (as the original). */
void n_001678A0(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    if (G32(dev + 0x370)) { G32(RS_TEXTUREFACTOR) = v; RET(v, 4); }
    for (unsigned i = 0; i < 16; i++) ND3D_SET(0xA60u + 4u * i, v);
    G32(RS_TEXTUREFACTOR) = v;
    RET(put_now(dev), 4);
}

/* 0x167900 LineWidth (D3DRS 157, float): LINE_WIDTH = min(trunc(w * dev+0x520 *
 * 8 + 0.5), 0x1FF) as unsigned (6.3 fixed point). */
void n_00167900(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    float f; memcpy(&f, &v, 4);
    float t = (float)((double)f * GF(dev + 0x520) * GF(K_EIGHT) + GF(K_HALF));
    /* ND3D-CHECK: truncation goes through the recompiled cvttss2si helper, so an
     * out-of-range product behaves exactly as in the recompiled build. */
    uint32_t w = nd3d_call(sub_0016DED0, g_ecx, g_edx, 1, fbits(t));
    if (w > 0x1FFu) w = 0x1FFu;
    ND3D_SET(0x380, w);
    G32(RS_LINEWIDTH) = v;
    RET(put_now(dev), 4);
}

/* 0x167970 Dxt1NoiseEnable (D3DRS 159): wanted = value only for a 32-bpp render
 * target; toggles dev+8 bit 0 when it differs. The toggle's software method 8
 * (0x100) has no native equivalent. */
void n_00167970(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    /* ND3D-CHECK: no NULL check on dev+0x21B4, exactly as the original. */
    uint32_t surf = G32(dev + 0x21B4);
    uint32_t want = ((G8(0x173FF8u + G8(surf + 0xDu)) & 0x3Cu) == 0x20u) ? v : 0u;
    uint32_t flags = G32(dev + 8), r = flags;
    if (want != (flags & 1u)) {
        G32(dev + 8) = flags ^ 1u;
        ND3D_SET(0x110, 0);   /* WAIT_FOR_IDLE word kept as plain state; 0x100 dropped */
        r = put_now(dev);
    }
    G32(RS_DXT1NOISEENABLE) = v;
    RET(r, 4);
}

/* 0x167A70 LogicOp (D3DRS 150): LOGIC_OP_ENABLE, LOGIC_OP. */
void n_00167A70(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    if (v == 0) {
        ND3D_SET(0x17BC, 0);
    } else {
        ND3D_SET(0x17BC, 1);
        ND3D_SET(0x17C0, v);
    }
    G32(RS_LOGICOP) = v;
    RET(put_now(dev), 4);
}

/* 0x167AD0 FillMode (D3DRS 139): FRONT_POLYGON_MODE = value; BACK_POLYGON_MODE
 * = TwoSidedLighting ? BackFillMode : value. */
void n_00167AD0(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    uint32_t back = G32(RS_TWOSIDEDLIGHTING) ? G32(RS_BACKFILLMODE) : v;
    ND3D_SET(0x38C, v);
    ND3D_SET(0x390, back);
    G32(RS_FILLMODE) = v;
    RET(put_now(dev), 4);
}

/* 0x167B20 BackFillMode (D3DRS 140): shadow first, then both polygon modes;
 * FillMode is rewritten with its unchanged value. */
void n_00167B20(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    G32(RS_BACKFILLMODE) = v;
    uint32_t fill = G32(RS_FILLMODE);
    uint32_t back = G32(RS_TWOSIDEDLIGHTING) ? G32(RS_BACKFILLMODE) : fill;
    ND3D_SET(0x38C, fill);
    ND3D_SET(0x390, back);
    G32(RS_FILLMODE) = fill;
    RET(put_now(dev), 4);
}

/* 0x167B80 TwoSidedLighting (D3DRS 141): dirty 0x1000, shadow first, then both
 * polygon modes; FillMode is rewritten with its unchanged value. */
void n_00167B80(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    G32(ND3D_DIRTY) |= 0x1000u;
    G32(RS_TWOSIDEDLIGHTING) = v;
    uint32_t fill = G32(RS_FILLMODE);
    uint32_t back = G32(RS_TWOSIDEDLIGHTING) ? G32(RS_BACKFILLMODE) : fill;
    ND3D_SET(0x38C, fill);
    ND3D_SET(0x390, back);
    G32(RS_FILLMODE) = fill;
    RET(put_now(dev), 4);
}

/* 0x167BF0 VertexBlend (D3DRS 137): dirty 0x200; SKIN_MODE. */
void n_00167BF0(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    G32(ND3D_DIRTY) |= 0x200u;
    ND3D_SET(0x328, v);
    G32(RS_VERTEXBLEND) = v;
    RET(put_now(dev), 4);
}

/* ============================================ texture-stage immediate setters */

/* 0x167C40 SetTextureState_TexCoordIndex(stage, value), ret 8. Shadow slot 28;
 * with texgen (value >> 16 != 0) the constant texcoord attribute 9+stage =
 * (0,0,0,1) and the remap byte selects the stage's own slot; TEXGEN_S/T/R;
 * dev+0x514 normal-texgen mask; dirty 0x47F (and 0x200 on the first
 * normal-based texgen). Returns the new dirty word in EAX, as the original. */
void n_00167C40(void)
{
    uint32_t stage = ARG(1), v = ARG(2), dev = nd3d_device();
    G32((stage << 7) + 0x175498u) = v;
    uint32_t tg = v & 0xFFFF0000u, mode = 0, needN = 0, idx = v;
    if (tg) {
        /* SET_VERTEX_DATA4UB(9 + stage) = 0xFF000000, i.e. (0, 0, 0, 1). */
        const float def[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        nd3d_attr(9u + stage, def, 0);
        idx = stage;
        if (tg > 0x30000u) mode = 0x2401u;                         /* OBJECT_LINEAR */
        else if (tg == 0x30000u) { mode = 0x8512u; needN = 1; }    /* REFLECTION_MAP */
        else if (tg == 0x10000u) { mode = 0x8511u; needN = 1; }    /* NORMAL_MAP */
        else mode = 0x2400u;                                       /* EYE_LINEAR */
    }
    G8(stage + 0x1D3561u) = (uint8_t)((idx & 0xFFu) + 9u);
    {
        uint32_t t[3]; t[0] = mode; t[1] = mode; t[2] = mode;
        ND3D_SETN(0x3C0u + 16u * stage, t, 3);                    /* TEXGEN_S/T/R */
    }
    if (G32(dev + 0x514) == 0 && needN) G32(ND3D_DIRTY) |= 0x200u;
    uint32_t sh = stage & 31u;
    G32(dev + 0x514) = (G32(dev + 0x514) & ~(1u << sh)) | (needN << sh);
    uint32_t d = G32(ND3D_DIRTY) | 0x47Fu;
    G32(ND3D_DIRTY) = d;
    ARG(1) = needN;   /* the original reuses its argument slots as locals */
    ARG(2) = idx;
    RET(d, 8);
}

/* 0x167D50 SetTextureState_BumpEnv(stage, type 22..27, value), ret 12. The
 * hardware stage is stage+1 without a pixel shader, stage with one; hardware
 * stages 0 and 4 get no word. Returns `type` in EAX, as the original. */
void n_00167D50(void)
{
    uint32_t stage = ARG(1), type = ARG(2), v = ARG(3), dev = nd3d_device();
    uint32_t hw = G32(dev + 0x370) ? stage : stage + 1u;
    if (hw & 3u) ND3D_SET(0x1AD0u + 4u * ((hw << 4) + type), v);   /* 0x1B28 + 0x40*hw + 4*(type-22) */
    G32(ND3D_TEXSTATE + 4u * ((stage << 5) + type)) = v;
    RET(type, 12);
}

/* 0x167DC0 SetTextureState_BorderColor(stage, value), ret 8. */
void n_00167DC0(void)
{
    uint32_t stage = ARG(1), v = ARG(2), dev = nd3d_device();
    ND3D_SET(0x1B24u + (stage << 6), v);                          /* TEXTURE_BORDER_COLOR */
    G32((stage << 7) + 0x17549Cu) = v;
    RET(put_now(dev), 8);
}

/* 0x167E00 SetTextureState_ColorKeyColor(stage, value), ret 8. */
void n_00167E00(void)
{
    uint32_t stage = ARG(1), v = ARG(2), dev = nd3d_device();
    ND3D_SET(0xAE0u + 4u * stage, v);                             /* COLOR_KEY_COLOR */
    G32((stage << 7) + 0x1754A0u) = v;
    RET(put_now(dev), 8);
}

/* ============================================================ the rest */

/* 0x1681D0 D3D_CommonSetDebugRegisters(), plain ret: the CPU copies of the two
 * PGRAPH debug registers. The original then sends them with software method 9
 * through the 0x1D8C/0x1D90 words (which hold the clear values otherwise). */
void n_001681D0(void)
{
    uint32_t dev = nd3d_device();
    uint32_t a = G32(dev + 0x2BC0) & 0xFFFFFFF7u;
    G32(dev + 0x2BC0) = a;
    if (G32(RS_DONOTCULLUNCOMPRESSED)) G32(dev + 0x2BC0) = a | 8u;
    uint32_t b = G32(dev + 0x2BC4) & 0xE7EFFFFFu;
    G32(dev + 0x2BC4) = b;
    if (G32(RS_ROPZCMPALWAYSREAD)) G32(dev + 0x2BC4) = b | 0x100000u;
    if (G32(RS_ROPZREAD)) G32(dev + 0x2BC4) |= 0x8000000u;
    /* ND3D-CHECK: the words are stored as the hardware latches them (0x110,
     * 0x1D8C, 0x1D90; the 0x100 software-method triggers are dropped). This
     * leaves 0x1D8C/0x1D90 = 0x400B80 / dev+0x2BC4, as on the hardware;
     * D3DDevice_Clear always rewrites both before clearing. */
    ND3D_SET(0x110, 0);
    ND3D_SET(0x1D8C, 0x400094u);
    ND3D_SET(0x1D90, G32(dev + 0x2BC0));
    ND3D_SET(0x1D8C, 0x400B80u);
    ND3D_SET(0x1D90, G32(dev + 0x2BC4));
    RET(put_now(dev), 0);
}

/* 0x1687F0 ZEnable (D3DRS 143): DEPTH_TEST_ENABLE = (value && depth surface),
 * ZMIN_MAX_CONTROL = DEPTHCLIPCONTROL. When the W-buffer mode is entered or
 * left: projection-viewport transform (CPU), pass-through constants, CONTROL0
 * and the viewport/clip helper. EAX = old ZEnable when nothing toggled. */
void n_001687F0(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    ND3D_SET(0x30C, (v && G32(dev + 0x21B8)) ? 1u : 0u);
    ND3D_SET(0x1D78, G32(RS_DEPTHCLIPCONTROL));
    uint32_t old = G32(RS_ZENABLE);
    G32(RS_ZENABLE) = v;
    if (old != 2u && v != 2u) RET(old, 4);
    nd3d_call(sub_001675A0, G32(RS_DEPTHCLIPCONTROL), v, 0);
    nd3d_call(nd3d_h_0016AC90, dev, v, 0);
    uint32_t p = nd3d_call(nd3d_h_00168170, dev, put_now(dev), 0);
    p = nd3d_call(nd3d_h_00167F80, dev, p, 0);
    RET(p, 4);
}

/* 0x168880 StencilEnable (D3DRS 144): 0x1D84 from the old shadows (XDK quirk),
 * STENCIL_TEST_ENABLE = (value && depth surface); shadow last. */
void n_00168880(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    ND3D_SET(0x1D84, occlude_word());
    ND3D_SET(0x32C, (v && G32(dev + 0x21B8)) ? 1u : 0u);
    G32(RS_STENCILENABLE) = v;
    RET(put_now(dev), 4);
}

/* 0x168910 StencilFail (D3DRS 145): 0x1D84 from the old shadows,
 * STENCIL_OP_FAIL; shadow last. */
void n_00168910(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    ND3D_SET(0x1D84, occlude_word());
    ND3D_SET(0x370, v);
    G32(RS_STENCILFAIL) = v;
    RET(put_now(dev), 4);
}

/* 0x168980 YuvEnable (D3DRS 160): shadow, then CONTROL0. */
void n_00168980(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    G32(RS_YUVENABLE) = v;
    uint32_t p = nd3d_call(nd3d_h_00168170, dev, put_now(dev), 0);
    RET(p, 4);
}

/* 0x1689B0 OcclusionCullEnable (D3DRS 161): shadow first, then 0x1D84. */
void n_001689B0(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    G32(RS_OCCLUSIONCULLENABLE) = v;
    ND3D_SET(0x1D84, occlude_word());
    RET(put_now(dev), 4);
}

/* 0x168A20 StencilCullEnable (D3DRS 162): shadow first, then 0x1D84. */
void n_00168A20(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    G32(RS_STENCILCULLENABLE) = v;
    ND3D_SET(0x1D84, occlude_word());
    RET(put_now(dev), 4);
}

/* 0x168B70 MultiSampleAntiAlias (D3DRS 152): shadow first; pass-through
 * constants; ANTI_ALIASING_CONTROL; viewport/clip helper (the -0.5 pixel
 * offset depends on this state). */
void n_00168B70(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    G32(RS_MULTISAMPLEANTIALIAS) = v;
    nd3d_call(nd3d_h_0016AC90, dev, 0, 0);
    ND3D_SET(0x1D7C, aa_word(dev, G32(RS_MULTISAMPLEMASK), G32(RS_SAMPLEALPHA)));
    uint32_t p = nd3d_call(nd3d_h_00167F80, dev, put_now(dev), 0);
    RET(p, 4);
}

/* 0x168BF0 MultiSampleMask (D3DRS 153): shadow, ANTI_ALIASING_CONTROL. */
void n_00168BF0(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    uint32_t alpha = G32(RS_SAMPLEALPHA);
    G32(RS_MULTISAMPLEMASK) = v;
    ND3D_SET(0x1D7C, aa_word(dev, v, alpha));
    RET(put_now(dev), 4);
}

/* 0x168C40 SampleAlpha (D3DRS 158): shadow, ANTI_ALIASING_CONTROL. */
void n_00168C40(void)
{
    uint32_t v = ARG(1), dev = nd3d_device();
    uint32_t mask = G32(RS_MULTISAMPLEMASK);
    G32(RS_SAMPLEALPHA) = v;
    ND3D_SET(0x1D7C, aa_word(dev, mask, v));
    RET(put_now(dev), 4);
}
