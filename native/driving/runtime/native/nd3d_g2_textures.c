/* Native D3D, group 2: textures, palettes, the lazy state-flush helpers and the
 * vertex-array part of CDevice_SetStateVB.
 *
 * Spec: native-driving/phase2/specs/G2-textures-lazy-state.md (G4 §1.6/Table D for
 * SetStateVB). Reference: the recompiled originals in src/recomp/gen/recomp_0017.c.
 *
 * Every routine keeps the original's CPU-side effects (device fields, resource
 * refcounts/Lock, dirty flags 0x175424, dev+8 flag bits, return value, stack pops)
 * and stores the state words the original computed with ND3D_SET. Push-buffer
 * mechanics (space checks, MakeSpace, headers, the put pointer at dev+0) are gone.
 * None of the words written here is a special slot of nd3d_api.h, so all of them
 * are plain state.
 *
 * Conventions used below:
 *  - Arithmetic the original did on the x87 is done in double; a float is stored
 *    exactly where the original executed fstp.
 *  - A float the original only moved through the x87 (fld + fstp, no arithmetic)
 *    is copied with fl(), which reproduces the one change such a move makes:
 *    a signalling NaN becomes quiet.
 *  - Constants the original read from .rdata/.data are read from the same guest
 *    addresses (values noted in comments, checked against the PAL Driving.xbe).
 *  - Jump tables the original indexed (0x170B38/0x170D44, 0x173F78, 0x1739E4) are
 *    read from guest memory exactly as the original indexed them, so every valid
 *    and every out-of-design input selects the same case. Entries that are not a
 *    case label (the original jumps into garbage) are logged once.
 *  - Stack scratch of the originals (their locals, and the few places where they
 *    reuse their own already-dead argument slots as scratch) is not reproduced.
 *    Where a recompiled CPU-only helper needs guest pointers, a 16-byte aligned
 *    scratch area is reserved on the guest stack below g_esp and released before
 *    returning, as the original's frame was. */
#include "nd3d_internal.h"

/* Recompiled CPU-only library routines called through the guest ABI. */
void sub_001690D0(void);   /* D3D_DestroyResource(res), ret 4 */
void sub_0016BD60(void);   /* vec3 scale (out, in, float s), ret 12 */
void sub_0016BD90(void);   /* vec3 add (out, a, b), ret 12 */
void sub_0016BDC0(void);   /* vec3 x 4x4 (out, in, float w, matrix), ret 16 */
void sub_0016BE20(void);   /* 4x4 multiply (out, a, b), ret 12 */
void sub_0016BF20(void);   /* specular power fit (float p, float *a, float *b), ret 12 */
void sub_0016C070(void);   /* vec3 normalise in place (v), ret 4 */
void sub_0016C0C0(void);   /* inverse model-view (out, in, normalize), ret 12 */
void sub_00170280(void);   /* colour-material word, cdecl, no arguments */
void sub_00170E80(void);   /* material/scene ambient (glue -> n_00170E80 below) */

/* ------------------------------------------------------------- helpers */
static inline float u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static inline uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static inline double gd(uint32_t a) { return (double)GF(a); }
static inline uint32_t fd(double d) { return f2u((float)d); }

/* fld dword + fstp dword: bit-exact except a signalling NaN is made quiet. */
static inline uint32_t fl(uint32_t a)
{
    uint32_t v = G32(a);
    if ((v & 0x7F800000u) == 0x7F800000u && (v & 0x007FFFFFu) && !(v & 0x00400000u)) v |= 0x00400000u;
    return v;
}

/* 0x16DED0 is cvttss2si: NaN and out-of-range give 0x80000000. */
static int32_t cvtt(float f)
{
    if (!(f >= -2147483648.0f && f < 2147483648.0f)) return INT32_MIN;
    return (int32_t)f;
}

static void table_miss(volatile int *seen, const char *where, uint32_t value, uint32_t target)
{
    if (*seen) return;
    *seen = 1;
    nd3d_log("ND3D-CHECK %s: input %08X selects jump-table entry %08X, which is not a case "
             "label (the original jumps into garbage here)\n", where, value, target);
}

/* Guest-stack scratch for helpers that take guest pointers. */
static inline uint32_t scratch_begin(uint32_t bytes, uint32_t *saved)
{
    *saved = g_esp;
    g_esp = (g_esp - bytes) & ~0xFu;
    return g_esp;
}

/* ===================================================== 0x166830 SetTexture */
/* void __stdcall D3DDevice_SetTexture(DWORD Stage, D3DBaseTexture *pTex), ret 8. */
void n_00166830(void)
{
    uint32_t dev = nd3d_device();
    uint32_t s = ARG(1), tex = ARG(2);
    uint32_t old = G32(dev + 0xB38u + 4u * s);
    if (old) {
        uint32_t c = G32(old) + 0xFFF80000u;     /* internal (device) refcount - 1 */
        G32(old + 8) = G32(dev + 0x30);          /* Lock = current GPU time */
        G32(old) = c;
        if (!(c & 0x78FFFFu)) nd3d_call(sub_001690D0, c, c, 1, old);
    }
    G32(dev + 0xB38u + 4u * s) = tex;
    if (!tex) {
        ND3D_SET(0x1B0Cu + 0x40u * s, 0);        /* TEXTURE_CONTROL0: stage disabled */
        G32(dev + 0xCu + 4u * s) = 0x80000000u;  /* format key: no texture */
        G32(ND3D_DIRTY) |= 0x4800u;
        RETV(8);
    }
    G32(tex) += 0x80000u;
    uint32_t data = G32(tex + 4), fmt = G32(tex + 0xC), size = G32(tex + 0x10);
    ND3D_SET(0x1B00u + 0x40u * s, data);         /* TEXTURE_OFFSET (physical) */
    ND3D_SET(0x1B04u + 0x40u * s, fmt);          /* TEXTURE_FORMAT */
    if (size) {                                  /* linear texture: pitch and size */
        uint32_t pitch = ((size >> 24) + 1u) << 6;
        uint32_t w = (size & 0xFFFu) + 1u, h = ((size >> 12) & 0xFFFu) + 1u;
        ND3D_SET(0x1B10u + 0x40u * s, pitch << 16);       /* TEXTURE_CONTROL1 */
        ND3D_SET(0x1B1Cu + 0x40u * s, (w << 16) | h);     /* TEXTURE_IMAGE_RECT */
    }
    uint32_t k = fmt & 0x20F4u;
    if (k == G32(dev + 0xCu + 4u * s)) RETV(8);
    if (k & 0x2000u) {
        uint32_t code = fmt & 0xFF00u;
        k &= ~0x2000u;
        if (code >= 0x2A00u && code <= 0x3100u) k |= 0x40000000u;   /* depth formats */
    }
    G32(dev + 0xCu + 4u * s) = k;
    G32(ND3D_DIRTY) |= 0x4000u;
    if (!old) {                                  /* NULL -> texture: re-enable the stage */
        ND3D_SET(0x1B0Cu + 0x40u * s, G32(dev + 0x360u + 4u * s));
        G32(ND3D_DIRTY) |= 0x800u;
    }
    RETV(8);
}

/* ===================================================== 0x1669E0 SetPalette */
/* void __stdcall D3DDevice_SetPalette(DWORD Stage, D3DPalette *pPal), ret 8. */
void n_001669E0(void)
{
    uint32_t dev = nd3d_device();
    uint32_t s = ARG(1), pal = ARG(2);
    uint32_t old = G32(dev + 0xB48u + 4u * s);
    if (old) {
        G32(old + 8) = G32(dev + 0x30);
        uint32_t c = G32(old) + 0xFFF80000u;
        G32(old) = c;
        if (!(c & 0x78FFFFu)) nd3d_call(sub_001690D0, c, c, 1, old);
    }
    G32(dev + 0xB48u + 4u * s) = pal;
    if (pal) {
        uint32_t c = G32(pal) + 0x80000u;
        G32(pal) = c;
        ND3D_SET(0x1B20u + 0x40u * s, G32(pal + 4) | (c >> 28));   /* TEXTURE_PALETTE */
    }
    RETV(8);
}

/* ============================================== 0x170320 LazySetPointParams */
/* void __stdcall(CDevice *dev), ret 4. */
void n_00170320(void)
{
    uint32_t dev = ARG(1);
    uint32_t mn = G32(0x1757FC), mx = G32(0x175814), sz = G32(0x1757F8);
    uint32_t e = G32(0x175804);                  /* POINTSCALEENABLE */
    double s;
    if (!e) {
        s = (double)u2f(sz) * gd(dev + 0x520);   /* POINTSIZE x super-sample scale */
        /* x87 fcom: an unordered (NaN) compare keeps the value. */
        if (s < (double)u2f(mn)) s = (double)u2f(mn);
        if (s > (double)u2f(mx)) s = (double)u2f(mx);
        if (s > gd(0x175314)) s = gd(0x175314);  /* 64.0 */
    } else {
        uint32_t w[8];
        float dr = (float)((double)u2f(mx) - (double)u2f(mn));
        uint32_t hi = G32(dev + 0xA9C);          /* viewport height, unsigned */
        double h = (double)(int32_t)hi;
        if ((int32_t)hi < 0) h += gd(0x18A4A8);  /* 2^32 */
        double k = (double)dr / (h * (double)u2f(sz));
        double k2 = k * k;
        w[0] = fd((double)u2f(G32(0x175808)) * k2);   /* POINTSCALE_A */
        w[1] = fd((double)u2f(G32(0x17580C)) * k2);   /* POINTSCALE_B */
        w[2] = fd(k2 * (double)u2f(G32(0x175810)));   /* POINTSCALE_C */
        w[3] = w[4] = w[5] = f2u(dr);
        w[6] = fd(-((double)u2f(mn) / (double)dr));
        w[7] = mn;
        ND3D_SETN(0xA30, w, 8);                  /* SET_POINT_PARAMS */
        s = (double)u2f(sz);
    }
    uint32_t px = (uint32_t)cvtt((float)(s * gd(0x18B5DC) + gd(0x189EB0)));   /* x8.0 + 0.5 */
    if (px > 0x1FFu) px = 0x1FFu;
    ND3D_SET(0x318, e);                          /* POINT_PARAMS_ENABLE */
    ND3D_SET(0x31C, G32(0x175800));              /* POINT_SMOOTH_ENABLE = POINTSPRITEENABLE */
    ND3D_SET(0x43C, px);                         /* POINT_SIZE, 1/8 pixel units */
    RETV(4);
}

/* ======================================= 0x170490 LazySetShaderStageProgram */
static uint32_t stage_mode(uint32_t key)
{
    if (key & 4u) return 3;                      /* cube map */
    if (key & 0x40000000u) return 2;             /* depth texture: 3-D projective */
    if ((key & 0xF0u) == 0x30u) return 2;        /* volume */
    return 1;                                    /* 2-D projective */
}

/* void __stdcall(CDevice *dev), ret 4. */
void n_00170490(void)
{
    uint32_t dev = ARG(1), v = 0;
    if (!G32(dev + 0x370)) {                     /* fixed function */
        for (int s = 3; s >= 0; s--) {
            uint32_t key = G32(dev + 0xCu + 4u * (uint32_t)s), m;
            if ((int32_t)key < 0) m = 0;
            else {
                uint32_t prev = s ? G32(0x175458u + 0x80u * (uint32_t)(s - 1)) : 0;   /* previous COLOROP */
                if (s && prev == 0x19u) m = 6;          /* BUMPENVMAP */
                else if (s && prev == 0x1Au) m = 7;     /* BUMPENVMAP_LUMINANCE */
                else m = stage_mode(key);
            }
            v = (v << 5) | m;
        }
    } else if (!G32(dev + 0x378)) {
        v = G32(dev + 0x37C);                    /* PSTextureModes verbatim */
    } else {                                     /* PS with TEXMODE_ADJUST */
        uint32_t modes = G32(dev + 0x37C);
        for (int s = 3; s >= 0; s--) {
            uint32_t key = G32(dev + 0xCu + 4u * (uint32_t)s);
            uint32_t m = (modes >> (5u * (uint32_t)s)) & 0x1Fu;
            if ((int32_t)key < 0 && m != 4u && m != 5u && m != 10u && m != 17u) m = 0;
            else if (m >= 1u && m <= 3u) m = stage_mode(key);
            else if (m == 13u || m == 14u) m = (key & 4u) ? 14u : 13u;
            v = (v << 5) | m;
        }
    }
    ND3D_SET(0x1E70, v);                         /* SET_SHADER_STAGE_PROGRAM */
    RETV(4);
}

/* ================================== 0x1705F0 LazySetTextureState (sampler) */
/* void __stdcall(CDevice *dev, DWORD dirty), ret 8. Per dirty stage: ADDRESS,
 * CONTROL0 and FILTER, latched now (dev+0x524 and Textures[] read at flush time). */
void n_001705F0(void)
{
    uint32_t dev = ARG(1), dirty = ARG(2);
    for (uint32_t s = 0; s < 4u; s++, dirty >>= 1) {
        if (!(dirty & 1u)) continue;
        uint32_t T = 0x175428u + 0x80u * s;      /* deferred texture state, stage s */
        uint32_t addr = G32(0x1757B0u + 4u * (G32(T + 0x70) & 0xFFFFu))      /* WRAPn of TCI */
                      | (((G32(T + 8) << 8) | G32(T + 4)) << 8) | G32(T + 0);
        uint32_t c0 = (G32(T + 0x1C) << 26) | G32(T + 0x2C) | G32(T + 0x24) | 0x3FFC0u;
        if (G32(dev + 0xB38u + 4u * s)) c0 |= 0x40000000u;
        uint32_t mn = G32(T + 0x10), mg = G32(T + 0x0C), kern = 0x2000u;
        if (mn < 3u && mg < 3u) {
            /* point/linear: unchanged */
        } else if (mn > 3u || mg > 3u) {         /* quincunx / gaussian cubic */
            kern = (mn == 5u || mg == 5u) ? 0x4000u : 0x2000u;
            kern |= 0x70000u;
            mn = 2; mg = 4;
        } else {                                 /* anisotropic */
            uint32_t a = G32(T + 0x20);
            if (!a) { mn = mg = 1; }
            else { mn = mg = 2; c0 |= (a - 1u) << 4; }
        }
        G32(dev + 0x360u + 4u * s) = c0 | 0x40000000u;   /* cached CONTROL0 with ENABLE */
        int32_t bias = cvtt((float)((gd(T + 0x18) + gd(dev + 0x524)) * gd(0x192C04) + gd(0x189EB0)));  /* x256 + 0.5 */
        if (bias < -4096) bias = -4096; else if (bias > 0xFFF) bias = 0xFFF;
        uint32_t sign = G32(T + 0x30) >= 0x19u ? 0xC0000000u : G32(T + 0x28);
        uint32_t filt = G32(0x1746A4u + 4u * (G32(T + 0x14) + mn * 3u))   /* MIN code table */
                      | ((uint32_t)bias & 0x1FFFu) | (mg << 24) | sign | kern;
        ND3D_SET(0x1B08u + 0x40u * s, addr);     /* TEXTURE_ADDRESS */
        ND3D_SET(0x1B0Cu + 0x40u * s, c0);       /* TEXTURE_CONTROL0 */
        ND3D_SET(0x1B14u + 0x40u * s, filt);     /* TEXTURE_FILTER */
    }
    RETV(8);
}

/* ===================================== 0x1707E0 LazySetFog + final combiner */
/* void __stdcall(CDevice *dev), ret 4. */
void n_001707E0(void)
{
    uint32_t dev = ARG(1);
    nd3d_call(nd3d_h_0016AC90, dev, g_edx, 0);   /* pass-through program constants (group 1) */
    if (G32(0x175798)) {                         /* FOGENABLE */
        uint32_t tm = G32(0x17579C), start = G32(0x1757A0), end = G32(0x1757A4), dens = G32(0x1757A8);
        uint32_t gen = G32(0x1757AC) ? 1u : 2u, mode, bias, scale;
        if (tm == 0) {
            bias = scale = 0x3F800000u; mode = 0x2601; gen = 0;
        } else if (tm == 3) {                    /* LINEAR */
            double q;
            if ((double)u2f(end) == (double)u2f(start)) q = gd(0x175318);   /* 8192.0 */
            else q = gd(0x189DE8) / ((double)u2f(end) - (double)u2f(start)); /* 1.0 / (END-START) */
            bias = fd((double)u2f(end) * q + gd(0x189DE8));
            scale = fd(-q);
            mode = 0x2601;
        } else {
            bias = 0x3FC00000u;                  /* 1.5 */
            if (tm == 1) { scale = fd((double)u2f(dens) * gd(0x1A1E7C)); mode = 0x800; }   /* EXP */
            else         { scale = fd((double)u2f(dens) * gd(0x1A1E78)); mode = 0x801; }   /* EXP2 */
        }
        ND3D_SET(0x2A0, gen);                    /* FOG_GEN_MODE */
        ND3D_SET(0x2A4, 1);                      /* FOG_ENABLE */
        ND3D_SET(0x29C, mode);                   /* FOG_MODE */
        ND3D_SET(0x9C0, bias);                   /* FOG_PARAMS */
        ND3D_SET(0x9C4, scale);
        ND3D_SET(0x9C8, 0);
        if (G32(dev + 0x370) && G32(dev + 0x374)) RETV(4);   /* PS supplies its final combiner */
        ND3D_SET(0x288, (G32(0x1757C4) ? 0x20000u : 0u) + 0x130C0300u);
        ND3D_SET(0x28C, 0x1C80u);
    } else {
        ND3D_SET(0x2A4, 0);
        if (G32(dev + 0x370) && G32(dev + 0x374)) RETV(4);
        ND3D_SET(0x288, G32(0x1757C4) ? 0x0Eu : 0x0Cu);
        ND3D_SET(0x28C, 0x1C80u);
    }
    RETV(4);
}

/* ======================================= 0x1709C0 LazySetTextureTransform */
/* void __stdcall(CDevice *dev), ret 4. TEXTURE_MATRIX_ENABLE and TEXTURE_MATRIX
 * per stage. ND3D-CHECK: follows the original x86 at 0x170C41, where `je` uses
 * the flags of `dec edx` (key == 0x331), not the reloaded edx the recompiled C
 * tests (G2 §8.1). */
void n_001709C0(void)
{
    static volatile int miss;
    uint32_t dev = ARG(1);
    uint32_t vs = G32(dev + 0x380);
    if (G8(vs + 4) & 0x12u) RETV(4);
    if ((G32(dev + 0x20D8) & ~0x10u) == 1u) RETV(4);
    for (uint32_t s = 0; s < 4u; s++) {
        uint32_t ttf = G32(0x17547Cu + 0x80u * s);
        if (!ttf) { ND3D_SET(0x420u + 4u * s, 0); continue; }
        uint32_t tci = G32(0x175498u + 0x80u * s), in;
        if (tci & 0xFFFF0000u) in = 3;
        else {
            in = (G32(vs + 0x10) >> (((tci & 0xFFFFu) << 3) & 31u)) & 0xFFu;   /* x86 shr count & 31 */
            if (!in) in = 2;
        }
        uint32_t key = ((((in << 4) | (ttf & 0xFFu)) << 4) | ((ttf >> 8) & 1u));
        ND3D_SET(0x420u + 4u * s, 1);
        const uint32_t m = dev + 0x890u + 0x40u * s;
#define MTX(r, c) (m + 16u * (r) + 4u * (c))
        uint32_t w[16];
        int cs;                                  /* 0..3 byte-table cases, 4 = 0x320, 5 = > 0x320 */
        if (key > 0x320u) cs = 5;
        else if (key == 0x320u) cs = 4;
        else {
            uint32_t t = G32(0x170D44u + 4u * G8(0x170B38u + key));
            if (t == 0x00170AC7u) cs = 0;
            else if (t == 0x00170B02u) cs = 1;
            else if (t == 0x00170B43u) cs = 2;
            else if (t == 0x00170B93u) cs = 3;
            else { table_miss(&miss, "0x1709C0 texture-matrix key", key, t); continue; }
        }
        if (cs <= 3) {                           /* 2-D texcoord input */
            w[0] = G32(MTX(0, 0)); w[1] = fl(MTX(1, 0)); w[2] = 0; w[3] = fl(MTX(2, 0));
            w[4] = fl(MTX(0, 1));  w[5] = fl(MTX(1, 1)); w[6] = 0; w[7] = fl(MTX(2, 1));
            w[8] = w[9] = w[10] = w[11] = 0;
            w[12] = w[13] = w[14] = 0; w[15] = 0x3F800000u;
            if (cs == 1) {                       /* 0x230 */
                w[8] = fl(MTX(0, 2)); w[9] = fl(MTX(1, 2)); w[11] = fl(MTX(2, 2));
            } else if (cs == 2) {                /* 0x231 */
                w[12] = fl(MTX(0, 2)); w[13] = fl(MTX(1, 2)); w[15] = fl(MTX(2, 2));
            } else if (cs == 3) {                /* 0x241 */
                w[8] = fl(MTX(0, 2));  w[9] = fl(MTX(1, 2));  w[11] = fl(MTX(2, 2));
                w[12] = fl(MTX(0, 3)); w[13] = fl(MTX(1, 3)); w[15] = fl(MTX(2, 3));
            }
        } else {                                 /* 3-D input: transposed rows */
            w[0] = G32(MTX(0, 0)); w[1] = fl(MTX(1, 0)); w[2] = fl(MTX(2, 0)); w[3] = fl(MTX(3, 0));
            w[4] = fl(MTX(0, 1));  w[5] = fl(MTX(1, 1)); w[6] = fl(MTX(2, 1)); w[7] = fl(MTX(3, 1));
            if (cs == 4 || key == 0x330u) {      /* 0x320 / 0x330 */
                if (cs == 4) { w[8] = w[9] = w[10] = w[11] = 0; }
                else { w[8] = fl(MTX(0, 2)); w[9] = fl(MTX(1, 2)); w[10] = fl(MTX(2, 2)); w[11] = fl(MTX(3, 2)); }
                w[12] = w[13] = w[14] = 0; w[15] = 0x3F800000u;
            } else if (key - 0x330u - 1u == 0u) { /* ZF of `dec edx`: 0x331 */
                w[8] = w[9] = w[10] = w[11] = 0;
                w[12] = fl(MTX(0, 2)); w[13] = fl(MTX(1, 2)); w[14] = fl(MTX(2, 2)); w[15] = fl(MTX(3, 2));
            } else {                             /* full transpose */
                w[8] = fl(MTX(0, 2));  w[9] = fl(MTX(1, 2));  w[10] = fl(MTX(2, 2)); w[11] = fl(MTX(3, 2));
                w[12] = fl(MTX(0, 3)); w[13] = fl(MTX(1, 3)); w[14] = fl(MTX(2, 3)); w[15] = fl(MTX(3, 3));
            }
        }
#undef MTX
        ND3D_SETN(0x6C0u + 0x40u * s, w, 16);    /* TEXTURE_MATRIX(s) */
    }
    RETV(4);
}

/* ============================================ 0x170E80 material / ambient */
/* One side: scene ambient (3), material emission (3), material alpha (1). */
static void ambient_side(uint32_t mat, uint32_t amb, uint32_t f,
                         uint32_t m_ambient, uint32_t m_emission, uint32_t m_alpha)
{
    double c = gd(0x189FCC);                     /* 1/255 */
    double rn = (double)((amb >> 16) & 0xFFu) * c;           /* stays in an x87 register */
    float gf = (float)((double)((amb >> 8) & 0xFFu) * c);
    float bf = (float)((double)(amb & 0xFFu) * c);
    uint32_t a[3], e[3];
    if (f & 0x0Cu) {                             /* ambient from the vertex */
        a[0] = fl(mat + 0x30); a[1] = fl(mat + 0x34); a[2] = G32(mat + 0x38);
        e[0] = fd(rn); e[1] = f2u(gf); e[2] = f2u(bf);
    } else if (f & 0x03u) {                      /* emissive from the vertex */
        a[0] = fd(rn * gd(mat + 0x10));
        a[1] = fd((double)gf * gd(mat + 0x14));
        a[2] = fd((double)bf * gd(mat + 0x18));
        e[0] = e[1] = e[2] = 0x3F800000u;
    } else {
        a[0] = fd(rn * gd(mat + 0x10) + gd(mat + 0x30));
        a[1] = fd((double)gf * gd(mat + 0x14) + gd(mat + 0x34));
        a[2] = fd((double)bf * gd(mat + 0x18) + gd(mat + 0x38));
        e[0] = e[1] = e[2] = 0;
    }
    ND3D_SETN(m_ambient, a, 3);
    ND3D_SETN(m_emission, e, 3);
    ND3D_SET(m_alpha, G32(mat + 0xC));           /* Diffuse.a */
}

/* Front side, then the back side TWOSIDEDLIGHTING times (each back pass shifts the
 * colour-material flags another 8 bits; passes 4.. are identical, so stopping at 4
 * gives the same final state). */
static void material_ambient(uint32_t cm)
{
    uint32_t dev = nd3d_device(), two = G32(0x17585C);
    uint32_t nback = two > 4u ? 4u : two;
    ambient_side(dev + 0xAB0, G32(0x1757F4), cm, 0xA10, 0x3A8, 0x3B4);
    for (uint32_t p = 1; p <= nback; p++)
        ambient_side(dev + 0xAF4, G32(0x1757F0), p < 4u ? cm >> (8u * p) : 0u, 0x17A0, 0x17B0, 0x17AC);
}

/* DWORD *__stdcall(DWORD *push, DWORD cm), ret 8. ND3D-CHECK: the original returns
 * push advanced past the words it wrote into the caller's space; nothing is written
 * there now, so (nd3d_internal.h convention) the pointer is returned unchanged. */
void n_00170E80(void)
{
    uint32_t put = ARG(1), cm = ARG(2);
    material_ambient(cm);
    RET(put, 8);
}

/* ============================================== 0x171160 LazySetLights */
/* 0x171020: SPECULAR_PARAMS from material Power (6 floats). */
static void specular_fit(uint32_t power, uint32_t scr, uint32_t w[6])
{
    double one = gd(0x189DE8);
    nd3d_call(sub_0016BF20, g_ecx, g_edx, 3, power, scr, scr + 4u);
    uint32_t r1 = G32(scr), r2 = G32(scr + 4u);
    uint32_t half = fd((double)u2f(power) * gd(0x189EB0));   /* x 0.5 */
    nd3d_call(sub_0016BF20, g_ecx, g_edx, 3, half, scr + 8u, scr + 12u);
    uint32_t r3 = G32(scr + 8u), r4 = G32(scr + 12u);
    w[0] = r1; w[1] = r2; w[2] = fd((double)u2f(r1) + one - (double)u2f(r2));
    w[3] = r3; w[4] = r4; w[5] = fd((double)u2f(r3) + one - (double)u2f(r4));
}
static void specular_params(uint32_t scr)
{
    uint32_t dev = nd3d_device(), w[6];
    specular_fit(G32(dev + 0xAF0), scr, w);
    ND3D_SETN(0x9E0, w, 6);
    if (G32(0x17585C)) {                         /* back side; every repeat is identical */
        specular_fit(G32(dev + 0xB34), scr, w);
        ND3D_SETN(0x1E28, w, 6);
    }
}

/* 0x170D80: light i colours (ambient, diffuse, specular), each pre-multiplied by
 * the material unless that component comes from the vertex. Back side as above. */
static void light_side(uint32_t mat, uint32_t light, uint32_t f, uint32_t method)
{
    uint32_t w[9];
    if (!(f & 0x0Cu)) {
        w[0] = fd(gd(mat + 0x10) * gd(light + 0x24));
        w[1] = fd(gd(mat + 0x14) * gd(light + 0x28));
        w[2] = fd(gd(mat + 0x18) * gd(light + 0x2C));
    } else { w[0] = G32(light + 0x24); w[1] = G32(light + 0x28); w[2] = G32(light + 0x2C); }
    if (!(f & 0x30u)) {
        w[3] = fd(gd(light + 0x4) * gd(mat + 0x0));
        w[4] = fd(gd(mat + 0x4) * gd(light + 0x8));
        w[5] = fd(gd(mat + 0x8) * gd(light + 0xC));
    } else { w[3] = G32(light + 0x4); w[4] = G32(light + 0x8); w[5] = G32(light + 0xC); }
    if (!(f & 0xC0u)) {
        w[6] = fd(gd(mat + 0x20) * gd(light + 0x14));
        w[7] = fd(gd(mat + 0x24) * gd(light + 0x18));
        w[8] = fd(gd(mat + 0x28) * gd(light + 0x1C));
    } else { w[6] = G32(light + 0x14); w[7] = G32(light + 0x18); w[8] = G32(light + 0x1C); }
    ND3D_SETN(method, w, 9);
}
static void light_colours(uint32_t i, uint32_t light, uint32_t cm)
{
    uint32_t dev = nd3d_device(), two = G32(0x17585C);
    uint32_t nback = two > 4u ? 4u : two;
    light_side(dev + 0xAB0, light, cm, 0x1000u + 0x80u * i);
    for (uint32_t p = 1; p <= nback; p++)
        light_side(dev + 0xAF4, light, p < 4u ? cm >> (8u * p) : 0u, 0x0C00u + 0x40u * i);
}

/* void __stdcall(CDevice *dev), ret 4. ND3D-CHECK: 0x170D80 and 0x171020 write
 * through a push-buffer pointer and are not in the replace set; they are only
 * called from here, so their logic lives in the static helpers above. */
void n_00171160(void)
{
    uint32_t dev = ARG(1);
    uint32_t spec = (G32(0x1757C4) || (G8(dev + 8) & 0x40u)) ? 1u : 0u;
    uint32_t vs = G32(dev + 0x380);
    if ((G8(vs + 4) & 0x12u) || !G32(0x1757C0) || (G32(dev + 0x20D8) & ~0x10u) == 1u) {
        ND3D_SET(0x314, 0);                      /* LIGHTING_ENABLE */
        ND3D_SET(0x3B8, spec);                   /* SPECULAR_ENABLE */
        ND3D_SET(0x294, 0x20001u);               /* LIGHT_CONTROL */
        ND3D_SET(0x17C4, G32(0x17585C));         /* TWO_SIDE_LIGHT_EN */
        RETV(4);
    }
    uint32_t saved, scr = scratch_begin(0x80, &saved);
    uint32_t A = scr, B = scr + 0x10u, C = scr + 0x20u;
    uint32_t ctl = 1;
    if (spec) {
        if (G32(0x1757C8) && G32(dev + 0x398)) ctl = 0x10001u;   /* LOCALVIEWER with lights */
        specular_params(scr + 0x40u);
    }
    ND3D_SET(0x294, ctl);
    ND3D_SET(0x314, 1);
    ND3D_SET(0x17C4, G32(0x17585C));
    ND3D_SET(0x3B8, 1);
    uint32_t cm = nd3d_call_cdecl(sub_00170280, 0);
    ND3D_SET(0x298, cm);                         /* COLOR_MATERIAL */
    nd3d_call(sub_00170E80, g_ecx, G32(dev), 2, G32(dev), cm);
    uint32_t mask = 0, light = G32(dev + 0x398);
    for (uint32_t i = 0; light; ) {
        light_colours(i, light, cm);
        if (G32(light) == 3u) {                  /* directional */
            mask |= 1u << (2u * i);
            nd3d_call(sub_0016BDC0, g_ecx, g_edx, 4, A, light + 0x6Cu, 0u, dev + 0x810u);
            nd3d_call(sub_0016C070, g_ecx, g_edx, 1, A);
            nd3d_call(sub_0016BD90, g_ecx, g_edx, 3, B, A, 0x17531Cu);   /* + (0,0,-1) */
            nd3d_call(sub_0016C070, g_ecx, g_edx, 1, B);
            uint32_t w[6] = { G32(B), G32(B + 4u), G32(B + 8u), G32(A), G32(A + 4u), G32(A + 8u) };
            ND3D_SET(0x1024u + 0x80u * i, 0x7149F2CAu);           /* LOCAL_RANGE = 1e30 */
            ND3D_SETN(0x1028u + 0x80u * i, w, 6);                 /* half vector, direction */
        } else {
            ND3D_SET(0x1024u + 0x80u * i, fl(light + 0x4Cu));     /* Range */
            nd3d_call(sub_0016BDC0, g_ecx, g_edx, 4, C, light + 0x34u, 0x3F800000u, dev + 0x810u);
            uint32_t w[6] = { G32(C), G32(C + 4u), G32(C + 8u),
                              G32(light + 0x54u), G32(light + 0x58u), G32(light + 0x5Cu) };
            ND3D_SETN(0x105Cu + 0x80u * i, w, 6);                 /* position, attenuation */
            if (G32(light) == 1u) mask |= 2u << (2u * i);         /* point */
            else {                                                 /* spot */
                mask |= 3u << (2u * i);
                nd3d_call(sub_0016BDC0, g_ecx, g_edx, 4, A, light + 0x6Cu, 0u, dev + 0x810u);
                nd3d_call(sub_0016C070, g_ecx, g_edx, 1, A);
                nd3d_call(sub_0016BD60, g_ecx, g_edx, 3, A, A, G32(light + 0x84u));
                uint32_t f[7] = { G32(light + 0x78u), G32(light + 0x7Cu), G32(light + 0x80u),
                                  G32(A), G32(A + 4u), G32(A + 8u), G32(light + 0x88u) };
                ND3D_SETN(0x1040u + 0x80u * i, f, 7);
            }
        }
        if (++i == 8u) break;
        light = G32(light + 0x8Cu);
    }
    ND3D_SET(0x3BC, mask);                       /* LIGHT_ENABLE_MASK */
    g_esp = saved;
    RETV(4);
}

/* ============================================== 0x1714A0 LazySetTransform */
/* 16 words of a 4x4 guest matrix, transposed (as 0x16C640 writes them). */
static void set_transposed(uint32_t method, uint32_t mat)
{
    for (uint32_t k = 0; k < 16u; k++)
        ND3D_SET(method + 4u * k, G32(mat + 16u * (k & 3u) + 4u * (k >> 2)));
}

/* void __stdcall(CDevice *dev, DWORD dirty), ret 8. Returns at once in this game's
 * 192-constant mode and for programmable shaders. ND3D-CHECK: the fixed-function
 * matrix methods (0x480.., 0x580.., 0x680, like the light/material words of
 * 0x171160 and the texture matrices of 0x1709C0) are not in the vertex-program
 * constant range 0xB80..0xBFC, so they are stored as plain state, as the old
 * decoder (lean_gpu.c kelvin()) did; nothing here goes to nd3d_vp_constants. */
void n_001714A0(void)
{
    uint32_t dev = ARG(1), dirty = ARG(2);
    if ((int32_t)dirty < 0) RETV(8);
    if (G8(G32(dev + 0x380) + 4) & 0x12u) RETV(8);
    if ((G32(dev + 0x20D8) & ~0x10u) == 1u) RETV(8);
    uint32_t saved, scr = scratch_begin(0x140, &saved);   /* 16-byte aligned (movaps) */
    uint32_t mv = scr, inv = scr + 0x40u, comp = scr + 0x80u, mvi = scr + 0xC0u, invi = scr + 0x100u;
    nd3d_call(sub_0016BE20, g_ecx, g_edx, 3, mv, dev + 0x990u, dev + 0x810u);   /* WORLD0 x VIEW */
    set_transposed(0x480, mv);                   /* MODEL_VIEW_MATRIX0 */
    if (G32(dev + 0x514) || G32(0x1757C0)) {     /* normal texgen or LIGHTING */
        nd3d_call(sub_0016C0C0, g_ecx, g_edx, 3, inv, mv, (uint32_t)(G32(0x175860) == 0));
        for (uint32_t k = 0; k < 12u; k++) ND3D_SET(0x580u + 4u * k, G32(inv + 4u * k));
    }
    if (!G32(0x17584C)) {                        /* VERTEXBLEND off */
        nd3d_call(sub_0016BE20, g_ecx, g_edx, 3, comp, mv, dev + 0x530u);
        set_transposed(0x680, comp);             /* COMPOSITE_MATRIX */
    } else {
        set_transposed(0x680, dev + 0x530u);
        for (uint32_t i = 1; i <= 3u; i++) {
            nd3d_call(sub_0016BE20, g_ecx, g_edx, 3, mvi, dev + 0x990u + 0x40u * i, dev + 0x810u);
            set_transposed(0x480u + 0x40u * i, mvi);              /* 0x4C0 / 0x500 / 0x540 */
            if (G32(dev + 0x514) || G32(0x1757C0)) {
                nd3d_call(sub_0016C0C0, g_ecx, g_edx, 3, invi, mvi, (uint32_t)(G32(0x175860) == 0));
                for (uint32_t k = 0; k < 12u; k++)
                    ND3D_SET(0x580u + 0x40u * i + 4u * k, G32(invi + 4u * k));   /* 0x5C0 / 0x600 / 0x640 */
            }
        }
    }
    g_esp = saved;
    RETV(8);
}

/* =============================================== 0x171720 CDevice_SetStateVB */
/* void __thiscall CDevice::SetStateVB(DWORD BaseVertexIndex): ECX = dev, ret 4. */
void n_00171720(void)
{
    uint32_t dev = g_ecx, ib = ARG(1);
    uint32_t d = G32(ND3D_DIRTY);
    G32(ND3D_DIRTY) = d & 0xFFFFFFAFu;           /* clear 0x10 and 0x40 now */
    g_eax = d & 0xFFFFFFAFu;
    if (d & 0x3FFFFF8Fu) nd3d_call(sub_001716A0, dev, g_edx, 0);   /* lazy flush */
    if (d & 0x40000000u) RETV(4);
    if (!(d & 0x40u) && G32(dev + 0x20) == ib) RETV(4);
    G32(dev + 0x20) = ib;
    uint32_t vs = G32(dev + 0x380);
    uint32_t map = (G32(vs + 4) & 0x10u) + 0x1D3558u;     /* 0x1D3568 for programs */
    if (d & 0x10u)
        for (uint32_t s = 0; s < 16u; s++) {
            uint32_t rec = vs + 16u * G8(map + s);        /* attribute record at rec+0x14 */
            uint32_t stride = G32(0x178500u + 12u * G32(rec + 0x14));
            ND3D_SET(0x1760u + 4u * s, (stride << 8) + G32(rec + 0x1C));   /* ARRAY_FORMAT */
        }
    for (uint32_t s = 0; s < 16u; s++) {         /* skipped slots keep their old offset */
        uint32_t rec = vs + 16u * G8(map + s);
        if (G32(rec + 0x1C) == 2u) continue;
        uint32_t st = 0x178500u + 12u * G32(rec + 0x14);
        uint32_t vb = G32(st + 8);
        if (!vb) continue;
        uint32_t off = G32(vb + 4) + G32(rec + 0x18) + G32(st + 4);
        if (ib) off += G32(st) * ib;
        ND3D_SET(0x1720u + 4u * s, off);         /* ARRAY_OFFSET (physical) */
    }
    g_eax = dev;
    RETV(4);
}

/* ============================================= 0x173A00 LazySetCombiners */
/* 0x173960, the combiner argument mapper (custom register ABI: EDI = D3DTA value,
 * EDX = stage flags, ECX = input slot | modifiers; returns EAX). Done natively
 * because its argument arrives in EDI, which native code must not change. The
 * register comes from the original's jump table at 0x1739E4. */
static uint32_t ff_arg(uint32_t arg, uint32_t fl_, uint32_t mods)
{
    static volatile int miss;
    uint32_t reg, t = G32(0x1739E4u + 4u * (arg & 0xFu));
    switch (t) {
    case 0x001739B3u: reg = 4; break;                                  /* DIFFUSE -> V0 */
    case 0x001739A9u: reg = (fl_ & 0x10u) ? 4u : 0xCu; break;          /* CURRENT -> R0, V0 first */
    case 0x0017396Du: {                                                /* TEXTURE -> T(s) */
        uint32_t s = fl_ & 3u;
        reg = G32(0x176408u + 4u * s) ? s + 8u : 0xFFFFFFFFu;
        break; }
    case 0x00173987u: reg = 1; break;                                  /* TFACTOR -> C0 */
    case 0x0017398Eu: G32(0x1758D8) |= 0x40u; reg = 5; break;          /* SPECULAR -> V1 */
    case 0x001739A2u: reg = 0xD; break;                                /* TEMP -> R1 */
    default:
        /* ND3D-CHECK: D3DTA register 6..15 overruns the table in the original. */
        table_miss(&miss, "0x173960 combiner argument", arg, t);
        reg = 0xFFFFFFFFu;
    }
    uint32_t v = ((mods | fl_ | arg) >> 1) & 0x10u;   /* alpha */
    v |= ((mods ^ arg) & 0x10u) << 1;                 /* invert -> UNSIGNED_INVERT */
    v |= mods & 0x40u;                                /* EXPAND_NORMAL */
    v |= reg;
    return v << (((mods >> 13) & 0x78u) & 31u);
}

/* DWORD __stdcall(CDevice *dev, DWORD dirty), ret 8: fixed-function register
 * combiners from the texture-stage state, returns the new dirty word. */
void n_00173A00(void)
{
    static volatile int miss;
    uint32_t dev = ARG(1), dirty = ARG(2);
    if (G32(dev + 0x370)) RET(dirty, 8);         /* pixel shader bound */
    uint32_t oldf = G32(dev + 8);
    G32(dev + 8) = oldf & 0xFFFEFFBFu;
    uint32_t s0 = G32(0x175800) ? 3u : 0u;       /* point sprites use T3 */
    /* Same layout as the original's local block: header + 8 slots for colour ICW
     * [0..8], colour OCW [9..17], alpha ICW [18..26], alpha OCW [27..35]. */
    uint32_t buf[36];
    uint32_t c = 0, stage = s0, fl_ = s0 | 0x10u, T = 0x175428u + (s0 << 7);
    uint32_t op = G32(T + 0x30);
    for (;;) {                                   /* one texture stage */
        uint32_t a0 = G32(T + 0x34), a1 = G32(T + 0x38), a2 = G32(T + 0x3C);
        uint32_t O = G32(T + 0x50) != 5u ? 0xC00u : 0xD00u;   /* sum -> R0, or R1 for TEMP */
        for (;;) {                               /* colour pass, then alpha pass */
            uint32_t icw, ocw = O, x, y;
#define ARGM(arg, mods) ff_arg((arg), fl_, (mods))
            switch (G32(0x173F78u + 4u * (op - 1u))) {
            case 0x00173ABEu:                    /* 1 DISABLE */
                if (fl_ & 0x10u) {
                    uint32_t al = fl_ & 0x20u;
                    icw = (al << 23) | 0x4200000u;           /* V0 x ONE */
                    if (!al) {                               /* first stage: colour and alpha, end */
                        buf[1 + c] = icw; buf[10 + c] = O;
                        buf[19 + c] = icw | 0x10000000u; buf[28 + c] = O;
                        c++; stage++;
                        goto chain_end;
                    }
                } else { icw = 0; ocw = 0; }
                break;
            case 0x00173B08u: icw = ARGM(a1, 0x30000u) | 0x200000u; break;              /* 2 SELECTARG1 */
            case 0x00173B20u: icw = ARGM(a2, 0) | 0x2000u; break;                       /* 3 SELECTARG2 */
            case 0x00173B35u: ocw = O | 0x20000u;                                    /* 6 MODULATE4X */
                x = ARGM(a1, 0x30000u); icw = ARGM(a2, 0x20000u) | x; break;
            case 0x00173B5Au: ocw = O | 0x10000u;                                    /* 5 MODULATE2X */
                x = ARGM(a1, 0x30000u); icw = ARGM(a2, 0x20000u) | x; break;
            case 0x00173B65u:                                                        /* 4 MODULATE */
                x = ARGM(a1, 0x30000u); icw = ARGM(a2, 0x20000u) | x; break;
            case 0x00173B7Fu: ocw = O | 0x18000u | 0x8000u;                          /* 9 ADDSIGNED2X */
                x = ARGM(a1, 0x30000u) | 0x202000u; icw = ARGM(a2, 0) | x; break;
            case 0x00173B8Au: ocw = O | 0x8000u;                                     /* 8 ADDSIGNED */
                x = ARGM(a1, 0x30000u) | 0x202000u; icw = ARGM(a2, 0) | x; break;
            case 0x00173B92u:                                                        /* 7 ADD */
                x = ARGM(a1, 0x30000u) | 0x202000u; icw = ARGM(a2, 0) | x; break;
            case 0x00173BADu:                                                        /* 10 SUBTRACT */
                x = ARGM(a1, 0x30000u) | 0x204000u; icw = ARGM(a2, 0) | x; break;
            case 0x00173BC8u:                                                        /* 11 ADDSMOOTH */
                x = ARGM(a1, 0x30000u) | 0x200000u; x |= ARGM(a1, 0x10010u);
                icw = ARGM(a2, 0) | x; break;
            case 0x00173BF4u: {                                                      /* 12-15 BLEND*ALPHA */
                uint32_t src = op - 12u;
                x = ARGM(a1, 0x30000u); y = ARGM(src, 0x20020u); x |= y; x |= ARGM(src, 0x10030u);
                icw = ARGM(a2, 0) | x; break; }
            case 0x00173C25u:                                                        /* 16 BLENDTEXTUREALPHAPM */
                x = ARGM(a1, 0x30000u) | 0x200000u; x |= ARGM(2, 0x10030u);
                icw = ARGM(a2, 0) | x; break;
            case 0x00173C4Au:                                                        /* 17 PREMODULATE */
                x = ARGM(a1, 0x30000u);
                icw = (fl_ & 0x10u) ? (x | ARGM(2, 0x20000u)) : (x | 0x200000u); break;
            case 0x00173C86u:                                                        /* 18 MODULATEALPHA_ADDCOLOR */
                x = ARGM(a1, 0x30000u) | 0x200000u; x |= ARGM(a1, 0x10020u);
                icw = ARGM(a2, 0) | x; break;
            case 0x00173CB2u:                                                        /* 19 MODULATECOLOR_ADDALPHA */
                x = ARGM(a1, 0x30000u); x |= ARGM(a2, 0x20000u);
                icw = ARGM(a1, 0x10020u) | x | 0x20u; break;
            case 0x00173CB9u:                                                        /* 20 MODULATEINVALPHA_ADDCOLOR */
                x = ARGM(a1, 0x30000u) | 0x200000u; x |= ARGM(a1, 0x10030u);
                icw = ARGM(a2, 0) | x; break;
            case 0x00173CE5u:                                                        /* 21 MODULATEINVCOLOR_ADDALPHA */
                x = ARGM(a1, 0x30010u); x |= ARGM(a2, 0x20000u);
                icw = ARGM(a1, 0x10020u) | x | 0x20u; break;
            case 0x00173D51u:                                                        /* 22 DOTPRODUCT3 */
                x = ARGM(a1, 0x30040u); y = ARGM(a2, 0x20040u);
                ocw = (O | 0x820000u) >> 4;
                buf[19 + c] = 0; buf[28 + c] = 0;    /* this stage's alpha: nothing */
                icw = y | x;
                fl_ |= 0x80u;                        /* skip the alpha pass */
                break;
            case 0x00173D96u:                                                        /* 23 MULTIPLYADD */
                x = ARGM(a0, 0x30000u) | 0x200000u; x |= ARGM(a1, 0x10000u);
                icw = ARGM(a2, 0) | x; break;
            case 0x00173DBFu:                                                        /* 24 LERP */
                x = ARGM(a0, 0x30000u); x |= ARGM(a1, 0x20000u); x |= ARGM(a0, 0x10010u);
                icw = ARGM(a2, 0) | x; break;
            case 0x00173D23u:                                                        /* 25/26 BUMPENVMAP(LUM) */
                icw = ARGM(1, 0x30000u) | 0x200000u;
                G32(dev + 8) |= 0x10000u;
                break;
            default:
                /* ND3D-CHECK: D3DTOP 0 or > 26 overruns the table in the original. */
                table_miss(&miss, "0x173A00 texture-stage op", op, G32(0x173F78u + 4u * (op - 1u)));
                icw = 0; ocw = 0;
            }
#undef ARGM
            /* An argument named TEXTURE on a stage without a texture: pass CURRENT. */
            if ((icw & 0xFF000000u) == 0xFF000000u)
                icw = ((~fl_ & 0x10u) << 23) | 0x4200000u | ((fl_ & 0x20u) << 23);
            {   uint32_t at = 1u + c + ((fl_ & 0x48u) >> 2);
                buf[at] = icw; buf[at + 9u] = ocw; }
            if (fl_ & 0x80u) break;
            a0 = G32(T + 0x44); a1 = G32(T + 0x48); a2 = G32(T + 0x4C);   /* alpha pass */
            op = G32(T + 0x40);
            fl_ |= 0xE8u;
        }
        c++; stage++; T += 0x80u;
        if (stage == 4u) break;
        op = G32(T + 0x30);
        fl_ = stage;
        if (op == 1u) break;                     /* next stage disabled: chain ends */
    }
chain_end:;
    uint32_t n = stage - s0;
    for (uint32_t k = n; k < 8u; k++) buf[1 + k] = buf[10 + k] = buf[19 + k] = buf[28 + k] = 0;
    ND3D_SET(0x1E60, n);                         /* COMBINER_CONTROL */
    ND3D_SETN(0xAC0, buf + 1, 8);                /* colour ICW */
    ND3D_SETN(0x1E40, buf + 10, 8);              /* colour OCW */
    ND3D_SETN(0x260, buf + 19, 8);               /* alpha ICW */
    ND3D_SETN(0xAA0, buf + 28, 8);               /* alpha OCW */
    uint32_t nf = G32(dev + 8);
    if (((nf ^ oldf) & 0x40u) && !G32(0x1757C4)) ND3D_SET(0x3B8, (nf >> 6) & 1u);
    if ((G32(dev + 8) | oldf) & 0x10000u) dirty |= 0x400Fu;
    RET(dirty, 8);
}
