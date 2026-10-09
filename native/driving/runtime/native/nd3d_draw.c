/* Native D3D: the draw, clear, copy and present calls of nd3d_api.h.
 *
 * Draws are recorded straight into D3D11 through the D3D11 backend
 * (runtime/lean/lean_d3d.c): the current state block nd3d_reg[] and vertex
 * program, the bound surfaces, and the vertices read from guest memory at the
 * moment of the call. No command stream is produced or read. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "nd3d_api.h"
#include "../lean/lean_d3d.h"
#include "../gpu144/nightfire_vertex_program.h"
unsigned lean_d3d_vp_mask(const void *vp);   /* lean_d3d.c */

uint32_t nd3d_reg[ND3D_REGS];
static NFVertexProgram vp;

/* ------------------------------------------------------------- backend */
static int backend_ready;
static void backend(void)
{
    if (backend_ready) return;
    backend_ready = 1;
    extern void lean_sampler_start(void);
    lean_sampler_start();
    if (!lean_d3d_init()) { nd3d_log("STOP: D3D11 initialisation failed\n"); abort(); }
    nd3d_log("native D3D11 renderer active\n");
}

/* Guest memory: lean_guest() (lean_d3d.h, defined beside the backend). */
/* Physical address (as written into state words) to guest address. */
static uint32_t phys(uint32_t p) { return 0x80000000u + (p & 0x07FFFFFFu); }

/* ------------------------------------------------------- vertex program */
void nd3d_vp_program_words(unsigned slot, const uint32_t *w, unsigned n)
{
    for (unsigned i = 0; i < n && slot + i < 136; i++) {
        memcpy(vp.code[slot + i], w + 4 * i, 16);
        vp.valid[slot + i] = 15; vp.decoded[slot + i].ready = 0;
    }
    lean_d3d_vp_dirty();
}
void nd3d_vp_constants(unsigned slot, const uint32_t *w, unsigned n)
{
    for (unsigned i = 0; i < n && slot + i < 192; i++) {
        memcpy(vp.constant_words[slot + i], w + 4 * i, 16);
        vp.constant_valid[slot + i] = 15;
    }
    lean_d3d_vp_dirty();
}
void nd3d_vp_start(unsigned slot) { vp.start = slot; lean_d3d_vp_dirty(); }
void nd3d_vp_mode(uint32_t mode) { vp.mode = mode; lean_d3d_vp_dirty(); }

/* ------------------------------------------------------------ vertices */
static float cur[16][4];                       /* current attribute values */
static float *vbuf; static size_t vcap;
static uint32_t *ibuf; static size_t icap;
static uint32_t *src_idx; static size_t sicap;
static float *imm; static size_t imcap; static unsigned nimm;
static unsigned imm_prim; static int imm_active;
static uint64_t ndraws, nrefused;

static void *grow(void *p, size_t *cap, size_t need, size_t elem)
{
    if (need <= *cap) return p;
    size_t n = *cap ? *cap : 4096;
    while (n < need) n *= 2;
    void *q = realloc(p, n * elem);
    if (!q) { nd3d_log("STOP: out of memory\n"); abort(); }
    *cap = n; return q;
}

/* 64-bit hash of guest bytes for the vertex cache (8 bytes per step). */
static uint64_t vh64(const uint8_t *p, size_t n, uint64_t h)
{
    h ^= 0x9E3779B97F4A7C15ull * (n + 1);
    size_t i = 0;
    uint64_t a = h, b = ~h, c = h * 3u, d = h ^ 0x5555u;   /* four independent lanes */
    for (; i + 32 <= n; i += 32) {
        uint64_t w[4]; memcpy(w, p + i, 32);
        a = (a ^ w[0]) * 0xFF51AFD7ED558CCDull; b = (b ^ w[1]) * 0xC4CEB9FE1A85EC53ull;
        c = (c ^ w[2]) * 0xFF51AFD7ED558CCDull; d = (d ^ w[3]) * 0xC4CEB9FE1A85EC53ull;
        a ^= a >> 29; b ^= b >> 31; c ^= c >> 27; d ^= d >> 33;
    }
    h = a ^ (b * 7u) ^ (c * 13u) ^ (d * 31u);
    for (; i + 8 <= n; i += 8) { uint64_t w; memcpy(&w, p + i, 8); h = (h ^ w) * 0xFF51AFD7ED558CCDull; h ^= h >> 29; }
    for (; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h ^ (h >> 31);
}

static void decode_attr(float *o, const uint8_t *p, uint32_t fmt)
{
    unsigned type = fmt & 15, size = (fmt >> 4) & 15;
    o[0] = o[1] = o[2] = 0; o[3] = 1;
    switch (type) {
    case 0: /* D3DCOLOR: B,G,R,A in memory */
        if (size >= 3) { o[0] = p[2] / 255.0f; o[1] = p[1] / 255.0f; o[2] = p[0] / 255.0f; if (size == 4) o[3] = p[3] / 255.0f; }
        else for (unsigned i = 0; i < size; i++) o[i] = p[i] / 255.0f;
        break;
    case 4: for (unsigned i = 0; i < size && i < 4; i++) o[i] = p[i] / 255.0f; break;
    case 2:
        if (size == 7) { float f[3]; memcpy(f, p, 12); o[0] = f[0]; o[1] = f[1]; o[2] = 0; o[3] = f[2]; }  /* FLOAT2H */
        else memcpy(o, p, 4 * (size > 4 ? 4 : size));
        break;
    case 1: for (unsigned i = 0; i < size && i < 4; i++) { int16_t v; memcpy(&v, p + 2 * i, 2); o[i] = v < 0 ? v / 32768.0f : v / 32767.0f; } break;
    case 5: for (unsigned i = 0; i < size && i < 4; i++) { int16_t v; memcpy(&v, p + 2 * i, 2); o[i] = (float)v; } break;
    case 6: { uint32_t v; memcpy(&v, p, 4); int x = (int)(v << 21) >> 21, y = (int)((v >> 11) << 21) >> 21, z = (int)v >> 22;
              o[0] = x / 1023.0f; o[1] = y / 1023.0f; o[2] = z / 511.0f; } break;
    default: { static unsigned w; if (w++ < 8) nd3d_log("vertex attribute type %u unsupported\n", type); }
    }
}
static unsigned attr_bytes(uint32_t fmt)
{
    unsigned type = fmt & 15, size = (fmt >> 4) & 15;
    switch (type) {
    case 0: case 4: return size ? (type == 4 ? size : 4) : 0;
    case 2: return size == 7 ? 12 : size * 4;
    case 1: case 5: return size * 2;
    case 6: return size ? 4 : 0;
    }
    return 0;
}

static int target(LeanTarget *t)
{
    memset(t, 0, sizeof *t);
    uint32_t fmt = ND3D_GET(0x208), pitch = ND3D_GET(0x20C);
    t->width = ND3D_GET(0x200) >> 16; t->height = ND3D_GET(0x204) >> 16;
    unsigned aa = (fmt >> 12) & 15; t->aa_x = aa ? 2 : 1; t->aa_y = aa == 2 ? 2 : 1;
    t->color_fmt = fmt & 15; t->zeta_fmt = (fmt >> 4) & 15; t->swizzled = ((fmt >> 8) & 15) == 2;
    if (t->swizzled) { t->width = 1u << ((fmt >> 16) & 255); t->height = 1u << (fmt >> 24); }
    t->color_pitch = pitch & 0xFFFF; t->zeta_pitch = pitch >> 16;
    if (!t->width || !t->height) return 0;
    if (t->swizzled) { t->color_pitch = t->width * 4; t->zeta_pitch = t->width * 4; }
    if (t->color_fmt) t->color = phys(ND3D_GET(0x210));
    if (t->zeta_fmt && ND3D_GET(0x214)) t->zeta = phys(ND3D_GET(0x214));
    return 1;
}

/* Expand an Xbox primitive into list indices (topology 0 tri, 1 line, 2 point). */
static size_t expand(unsigned prim, const uint32_t *si, unsigned count, unsigned *topo)
{
    #define SRC(i) (si ? si[i] : (uint32_t)(i))
    size_t n = 0;
    ibuf = grow(ibuf, &icap, (size_t)count * 6 + 16, 4);
    *topo = 0;
    switch (prim) {
    case 1: *topo = 2; for (unsigned i = 0; i < count; i++) ibuf[n++] = SRC(i); break;
    case 2: *topo = 1; for (unsigned i = 0; i + 1 < count; i += 2) { ibuf[n++] = SRC(i); ibuf[n++] = SRC(i + 1); } break;
    case 3: case 4: *topo = 1;
        for (unsigned i = 0; i + 1 < count; i++) { ibuf[n++] = SRC(i); ibuf[n++] = SRC(i + 1); }
        if (prim == 3 && count > 2) { ibuf[n++] = SRC(count - 1); ibuf[n++] = SRC(0); }
        break;
    case 5: for (unsigned i = 0; i + 2 < count; i += 3) { ibuf[n++] = SRC(i); ibuf[n++] = SRC(i + 1); ibuf[n++] = SRC(i + 2); } break;
    case 6: for (unsigned i = 0; i + 2 < count; i++) {
            if (i & 1) { ibuf[n++] = SRC(i + 1); ibuf[n++] = SRC(i); ibuf[n++] = SRC(i + 2); }
            else { ibuf[n++] = SRC(i); ibuf[n++] = SRC(i + 1); ibuf[n++] = SRC(i + 2); } }
        break;
    case 7: case 10: for (unsigned i = 1; i + 1 < count; i++) { ibuf[n++] = SRC(0); ibuf[n++] = SRC(i); ibuf[n++] = SRC(i + 1); } break;
    case 8: for (unsigned i = 0; i + 3 < count; i += 4) { ibuf[n++] = SRC(i); ibuf[n++] = SRC(i + 1); ibuf[n++] = SRC(i + 2);
                                                         ibuf[n++] = SRC(i); ibuf[n++] = SRC(i + 2); ibuf[n++] = SRC(i + 3); } break;
    case 9: for (unsigned i = 0; i + 3 < count; i += 2) { ibuf[n++] = SRC(i); ibuf[n++] = SRC(i + 1); ibuf[n++] = SRC(i + 3);
                                                         ibuf[n++] = SRC(i); ibuf[n++] = SRC(i + 3); ibuf[n++] = SRC(i + 2); } break;
    default: { static unsigned w; if (w++ < 8) nd3d_log("primitive %u unsupported\n", prim); }
    }
    #undef SRC
    return n;
}

/* Submit one draw: verts (nslot float4 per vertex, slots in `slots`), list indices in ibuf. */
static void *submit_vb;   /* cached vertex buffer for the next submit (NULL: vbuf) */
static void submit(unsigned prim, unsigned nv, unsigned mask, const uint32_t *si, unsigned count)
{
    unsigned topo;
    size_t n = expand(prim, si, count, &topo);
    if (!n || !nv) return;
    LeanDraw d; memset(&d, 0, sizeof d);
    if (!target(&d.t)) { nrefused++; return; }
    d.K = nd3d_reg; d.vp = &vp; d.topo = topo; d.verts = vbuf; d.nverts = nv; d.mask = mask; d.idx = ibuf; d.nidx = (unsigned)n;
    d.gpu_vb = submit_vb; submit_vb = NULL;
    uint32_t prog = ND3D_GET(0x1E70);
    for (unsigned i = 0; i < 4; i++) {
        unsigned mode = (prog >> (5 * i)) & 31;
        if (!mode || mode == 4 || mode == 5) continue;
        uint32_t fmt = ND3D_GET(0x1B04 + i * 64);
        d.tex_addr[i] = phys(ND3D_GET(0x1B00 + i * 64));
        if (((fmt >> 8) & 255) == 0x0B) d.pal_addr[i] = phys(ND3D_GET(0x1B20 + i * 64) & ~63u);
    }
    {   /* LEAN_KDUMP_TEX=hex (diagnostic): save the state block at the first draw using that stage-0 texture. */
        static long want = -1; static int done;
        if (want < 0) { const char *e = getenv("LEAN_KDUMP_TEX"); want = e ? (long)strtoul(e, NULL, 16) : 0; }
        if (want && !done && (d.tex_addr[0] & 0x0FFFFFFFu) == (uint32_t)want) { const char *dir = getenv("DRIVING_CAPTURE_DIR"); done = 1;
            if (dir) { char path[MAX_PATH]; snprintf(path, sizeof path, "%s/kdump-on.bin", dir); FILE *f = fopen(path, "wb"); if (f) { fwrite(nd3d_reg, 4, ND3D_REGS, f); fclose(f); } } }
    }
    lean_d3d_draw(&d);
    ndraws++;
    if (!(ndraws % 200000)) { nd3d_log("draws=%llu refused=%llu\n", (unsigned long long)ndraws, (unsigned long long)nrefused); lean_d3d_report(); }
}

/* Array/indexed draws: gather the vertices [lo, hi] from the bound vertex arrays. */
static void draw_from_arrays(unsigned prim, const uint32_t *si, unsigned count, uint32_t lo, uint32_t hi)
{
    backend();
    if ((vp.mode & 3) != 2) { static unsigned w; if (w++ < 8) nd3d_log("non-program transform mode %X skipped\n", vp.mode); nrefused++; return; }
    unsigned mask = lean_d3d_vp_mask(&vp);
    if (!mask) { nrefused++; return; }
    unsigned slots[16], nslot = 0;
    for (unsigned k = 0; k < 16; k++) if (mask & (1u << k)) slots[nslot++] = k;
    unsigned nv = hi - lo + 1;
    if (nv > (1u << 20)) { nd3d_log("vertex span %u too large\n", nv); nrefused++; return; }
    /* Vertex cache: same source description and same guest bytes as before ->
     * the decoded vertices already sit in a GPU buffer (lean_d3d_vcache_*).
     * LEAN_VCACHE=0 decodes every draw as before. */
    /* Vertex streams in memory a resident surface rendered into: copy the GPU's
     * pixels back first (lean_d3d_sync_guest; the Paris light-glare points). */
    for (unsigned s = 0; s < nslot; s++) {
        unsigned k = slots[s];
        uint32_t fmt = ND3D_GET(0x1760 + 4 * k), off = ND3D_GET(0x1720 + 4 * k);
        unsigned size = (fmt >> 4) & 15, stride = fmt >> 8, eb = attr_bytes(fmt);
        if (size && eb) lean_d3d_sync_guest(0x80000000u + ((off + lo * stride) & 0x7FFFFFFFu), (nv - 1) * stride + eb);
    }
    static int vc_on = -1; if (vc_on < 0) { const char *e = getenv("LEAN_VCACHE"); vc_on = !(e && e[0] == '0'); }
    uint64_t vkey = 0, vhash = 0;
    if (vc_on && nv >= 4) {
        uint32_t desc[4 + 16 * 4]; unsigned nd = 0, ok = 1;
        uint32_t umin = 0xFFFFFFFFu, umax = 0; size_t sum = 0;
        desc[nd++] = mask; desc[nd++] = lo; desc[nd++] = nv; desc[nd++] = nslot;
        for (unsigned s = 0; s < nslot; s++) {
            unsigned k = slots[s];
            uint32_t fmt = ND3D_GET(0x1760 + 4 * k), off = ND3D_GET(0x1720 + 4 * k);
            desc[nd++] = fmt; desc[nd++] = off;
            unsigned size = (fmt >> 4) & 15, stride = fmt >> 8, eb = attr_bytes(fmt);
            if (!size || !eb) { memcpy(&desc[nd], cur[k], 16); nd += 4; continue; }   /* constant attribute value */
            uint32_t va = 0x80000000u + ((off + lo * stride) & 0x7FFFFFFFu), span = (nv - 1) * stride + eb;
            desc[nd++] = span;
            if (va < umin) umin = va; if (va + span > umax) umax = va + span; sum += span;
        }
        vkey = vh64((const uint8_t *)desc, nd * 4u, 0x51ED27u);
        if (umax > umin && (size_t)(umax - umin) <= 4 * sum + 4096) {
            const uint8_t *g = lean_guest(umin, umax - umin);
            if (g) vhash = vh64(g, umax - umin, vkey); else ok = 0;
        } else ok = 0;
        if (ok) { void *b = NULL; int r = lean_d3d_vcache_find(vkey, vhash, &b);
            if (r == 1) { submit_vb = b; submit(prim, nv, mask, si, count); return; }
            if (r < 0) vkey = 0; }
        else vkey = 0;
    }
    vbuf = grow(vbuf, &vcap, (size_t)nv * nslot * 4, 4);
    for (unsigned s = 0; s < nslot; s++) {
        unsigned k = slots[s];
        uint32_t fmt = ND3D_GET(0x1760 + 4 * k), off = ND3D_GET(0x1720 + 4 * k);
        unsigned size = (fmt >> 4) & 15, stride = fmt >> 8, eb = attr_bytes(fmt);
        if (!size || !eb) { for (unsigned v = 0; v < nv; v++) memcpy(&vbuf[((size_t)v * nslot + s) * 4], cur[k], 16); continue; }
        uint32_t va = 0x80000000u + ((off + lo * stride) & 0x7FFFFFFFu);
        uint32_t span = (nv - 1) * stride + eb;
        const uint8_t *g = lean_guest(va, span);
        if (!g) { nd3d_log("vertex stream %08X+%u outside RAM\n", va, span); nrefused++; return; }
        for (unsigned v = 0; v < nv; v++) decode_attr(&vbuf[((size_t)v * nslot + s) * 4], g + (size_t)v * stride, fmt);
    }
    if (vkey) submit_vb = lean_d3d_vcache_put(vkey, vhash, vbuf, nv * nslot * 16u);
    submit(prim, nv, mask, si, count);
}

void nd3d_draw_arrays(unsigned prim, unsigned start, unsigned count)
{
    if (!count) return;
    src_idx = grow(src_idx, &sicap, count, 4);
    for (unsigned i = 0; i < count; i++) src_idx[i] = i;
    draw_from_arrays(prim, src_idx, count, start, start + count - 1);
}

void nd3d_draw_indexed16(unsigned prim, uint32_t indices, unsigned count)
{
    if (!count) return;
    const uint8_t *g = lean_guest(indices, count * 2u);
    if (!g) { nd3d_log("index data %08X+%u outside RAM\n", indices, count * 2u); return; }
    src_idx = grow(src_idx, &sicap, count, 4);
    uint32_t lo = 0xFFFFFFFFu, hi = 0;
    for (unsigned i = 0; i < count; i++) {
        uint16_t v; memcpy(&v, g + 2 * i, 2);
        src_idx[i] = v; if (v < lo) lo = v; if (v > hi) hi = v;
    }
    for (unsigned i = 0; i < count; i++) src_idx[i] -= lo;
    draw_from_arrays(prim, src_idx, count, lo, hi);
}

/* Inline vertex data (Action's DrawVerticesUP): `nv` vertices of `dpv` dwords each,
 * packed as the chip received them: the enabled attributes (format size not 0) in
 * slot order, each rounded up to whole dwords. Driving does not use it. */
void nd3d_draw_inline(unsigned prim, const uint32_t *packed, unsigned nv, unsigned dpv)
{
    if (!nv || !dpv) return;
    backend();
    if ((vp.mode & 3) != 2) { static unsigned w; if (w++ < 8) nd3d_log("non-program transform mode %X skipped (inline)\n", vp.mode); nrefused++; return; }
    unsigned mask = lean_d3d_vp_mask(&vp);
    if (!mask) { nrefused++; return; }
    unsigned slots[16], nslot = 0, offs[16], o = 0;
    for (unsigned k = 0; k < 16; k++) {
        uint32_t fmt = ND3D_GET(0x1760 + 4 * k);
        unsigned eb = ((fmt >> 4) & 15) ? attr_bytes(fmt) : 0;
        offs[k] = eb ? o : ~0u;
        o += (eb + 3) / 4;
    }
    if (o != dpv) { static unsigned w; if (w++ < 8) nd3d_log("inline vertex is %u dwords but the formats describe %u\n", dpv, o); }
    for (unsigned k = 0; k < 16; k++) if (mask & (1u << k)) slots[nslot++] = k;
    vbuf = grow(vbuf, &vcap, (size_t)nv * nslot * 4, 4);
    for (unsigned v = 0; v < nv; v++)
        for (unsigned s = 0; s < nslot; s++) {
            unsigned k = slots[s];
            float *d = &vbuf[((size_t)v * nslot + s) * 4];
            if (offs[k] == ~0u || offs[k] >= dpv) memcpy(d, cur[k], 16);
            else decode_attr(d, (const uint8_t *)(packed + (size_t)v * dpv + offs[k]), ND3D_GET(0x1760 + 4 * k));
        }
    submit(prim, nv, mask, NULL, nv);
}

void nd3d_imm_begin(unsigned prim) { imm_prim = prim; imm_active = 1; nimm = 0; }
void nd3d_attr(unsigned slot, const float v[4], int emit)
{
    if (slot >= 16) return;
    memcpy(cur[slot], v, 16);
    if (slot == 0 && emit && imm_active) {
        imm = grow(imm, &imcap, (size_t)(nimm + 1) * 64, 4);
        memcpy(imm + (size_t)nimm * 64, cur, sizeof cur);
        nimm++;
    }
}
void nd3d_imm_end(void)
{
    if (!imm_active) return;
    imm_active = 0;
    if (!nimm) return;
    backend();
    if ((vp.mode & 3) != 2) { nrefused++; return; }
    unsigned mask = lean_d3d_vp_mask(&vp);
    if (!mask) { nrefused++; return; }
    unsigned slots[16], nslot = 0;
    for (unsigned k = 0; k < 16; k++) if (mask & (1u << k)) slots[nslot++] = k;
    vbuf = grow(vbuf, &vcap, (size_t)nimm * nslot * 4, 4);
    for (unsigned v = 0; v < nimm; v++)
        for (unsigned s = 0; s < nslot; s++) memcpy(&vbuf[((size_t)v * nslot + s) * 4], imm + (size_t)v * 64 + slots[s] * 4, 16);
    {   /* LEAN_ND3D_IMM_LOG=n (diagnostic): describe the first n immediate-mode draws. */
        static int lim = -1; static int seen;
        if (lim < 0) { const char *e = getenv("LEAN_ND3D_IMM_LOG"); lim = e ? atoi(e) : 0; }
        static long ftex = -1; if (ftex < 0) { const char *e = getenv("LEAN_ND3D_IMM_TEX"); ftex = e ? (long)strtoul(e, NULL, 16) : 0; }
        if (seen < lim && imm_prim != 5 && (!ftex || ND3D_GET(0x1B00) == (uint32_t)ftex)) { seen++;
            nd3d_log("imm draw prim=%u n=%u mask=%04X start=%u prog=%05X tex0=%08X fmt0=%08X rt=%08X\n", imm_prim, nimm, mask, vp.start,
                     ND3D_GET(0x1E70), ND3D_GET(0x1B00), ND3D_GET(0x1B04), ND3D_GET(0x210));
            for (unsigned v = 0; v < nimm && v < 4; v++) {
                char line[512]; int len = 0;
                for (unsigned s = 0; s < nslot; s++) { const float *x = &vbuf[((size_t)v * nslot + s) * 4];
                    len += snprintf(line + len, sizeof line - len, " a%u=(%.3g,%.3g,%.3g,%.3g)", slots[s], x[0], x[1], x[2], x[3]); }
                nd3d_log("   v%u:%s\n", v, line);
            }
        }
    }
    submit(imm_prim, nimm, mask, NULL, nimm);
    nimm = 0;
}

/* ------------------------------------------------------ clear / copy */
void nd3d_clear(uint32_t flags, unsigned x0, unsigned x1, unsigned y0, unsigned y1, uint32_t color, uint32_t zs)
{
    LeanTarget t;
    backend();
    if (!target(&t)) return;
    if (x1 >= t.width) x1 = t.width - 1;
    if (y1 >= t.height) y1 = t.height - 1;
    lean_d3d_clear(&t, flags, x0, x1, y0, y1, color, zs);
}

void nd3d_copy_rect(uint32_t src, unsigned spitch, uint32_t dst, unsigned dpitch,
                    unsigned sx, unsigned sy, unsigned dx, unsigned dy, unsigned w, unsigned h)
{
    backend();
    if (!w || !h) return;
    if (lean_d3d_blit(src, spitch, dst, dpitch, sx, sy, dx, dy, w, h)) return;
    uint8_t *gs = lean_guest(src, spitch * (sy + h)), *gd = lean_guest(dst, dpitch * (dy + h));
    if (!gs || !gd) return;
    for (unsigned y = 0; y < h; y++) memmove(gd + (size_t)(dy + y) * dpitch + dx * 4, gs + (size_t)(sy + y) * spitch + sx * 4, (size_t)w * 4);
    lean_d3d_forget(dst, dpitch * (dy + h));
}

/* ------------------------------------------------------- visibility */
/* Occlusion queries recorded around the draws between begin and end
 * (lean_d3d.c); the result is the number of samples that passed. */
void nd3d_visibility_begin(unsigned index) { (void)index; backend(); lean_d3d_vis_begin(); }
void nd3d_visibility_end(unsigned index) { backend(); lean_d3d_vis_end(index); }
int nd3d_visibility_result(unsigned index, uint32_t *count) { backend(); return lean_d3d_vis_result(index, count); }

/* ----------------------------------------------------------- present */
extern void driving_present201(const uint8_t *, uint32_t);
extern int driving_present201_capture_due(void);
extern int lean_d3d_present(uint32_t va, const uint32_t *pixels);
void nd3d_present(uint32_t front)
{
    static uint32_t *pix;
    backend();
    {   /* Bring-up check: no routine may write chip commands any more. If the
         * write pointer moved, something not yet native wrote some; report how
         * much (LEAN_COVERAGE shows which routines ran) and reuse the space. */
        uint32_t dev = nd3d_device(), put = G32(dev + DEV_PUT), start = G32(dev + DEV_PB_START);
        if (dev && put != start) {
            static unsigned reports;
            if (reports < 20) { reports++; nd3d_log("chip commands written since last frame: %u bytes (unported routine)\n", put - start);
                /* which methods: names the routine that wrote them */
                char line[400]; int len = 0; uint32_t at = start, end = put; unsigned n = 0;
                while (at < end && n < 12 && len < 360) {
                    uint32_t h = G32(at); unsigned cnt = (h >> 18) & 0x7FF, m = h & 0x1FFC, sc = (h >> 13) & 7;
                    len += snprintf(line + len, sizeof line - len, " %X/%u:%04X", sc, cnt, m);
                    if (!h || (h & 0xE0000003u)) break;   /* not a plain method header */
                    at += 4u * (cnt + 1u); n++;
                }
                nd3d_log("  methods (subchannel/count:method):%s\n", line); }
            G32(dev + DEV_PUT) = start; G32(dev + DEV_LIMIT) = G32(dev + DEV_PB_END) - 0x204u;
        }
    }
    { extern void lean_hang_frame(void); lean_hang_frame(); }   /* frame counter + freeze monitor */
    lean_d3d_epoch();   /* new frame: re-check guest memory of textures the CPU may have rewritten */
    if (!pix) pix = malloc(640 * 480 * 4);
    if (!pix) return;
    /* LEAN_CAPTURE_FROM_MS=n: first wall capture at n ms; LEAN_CAPTURE_COUNT=c: at
     * most c captures. With LEAN_CAPTURE_EVERY_MS=1 this saves consecutive frames. */
    static long long every = -1, next_at, left; static ULONGLONG t0;
    if (every < 0) { const char *v = getenv("LEAN_CAPTURE_EVERY_MS"); every = v ? atoll(v) : 0; t0 = GetTickCount64(); next_at = every;
        v = getenv("LEAN_CAPTURE_FROM_MS"); if (v) next_at = atoll(v); v = getenv("LEAN_CAPTURE_COUNT"); left = v ? atoll(v) : -1; }
    const char *dir = getenv("DRIVING_CAPTURE_DIR");
    ULONGLONG wall_now = GetTickCount64() - t0;
    int wall_due = every > 0 && dir && left != 0 && (long long)wall_now >= next_at;
    if (wall_due && left > 0) left--;
    extern volatile LONG lean_mark_request; LONG mark_due = lean_mark_request;   /* F9 marks */
    { extern void lean_session_test_tick(void); lean_session_test_tick(); }
    int shown = lean_d3d_present(front, NULL), have = 0;
    {   /* LEAN_ND3D_PRESENT_LOG=1 (diagnostic): each change of the shown surface. */
        static int on = -1; static uint32_t last; static unsigned n;
        if (on < 0) { const char *v = getenv("LEAN_ND3D_PRESENT_LOG"); on = v && v[0] == '1'; }
        if (on && front != last && n < 400) { n++; last = front;
            extern volatile LONG lean_present_count;
            nd3d_log("present f=%ld surface %08X shown=%d draws=%llu refused=%llu rt=%08X/%08X fmt=%08X clip=%08X/%08X\n", lean_present_count, front, shown,
                     (unsigned long long)ndraws, (unsigned long long)nrefused, ND3D_GET(0x210), ND3D_GET(0x214), ND3D_GET(0x208), ND3D_GET(0x200), ND3D_GET(0x204)); }
    }
    if (shown != 1 || wall_due || mark_due || driving_present201_capture_due()) {
        int resident = lean_d3d_readback(front, 640, 480, pix);
        if (!resident) { const uint8_t *g = lean_guest(front, 2560 * 480); if (g) memcpy(pix, g, 2560 * 480); have = g != NULL; }
        else have = 1;
        if (shown == -1 && have) shown = lean_d3d_present(front, pix);
    }
    { static unsigned frames; static ULONGLONG last;
      if (!(++frames % 300)) { ULONGLONG now = GetTickCount64(); nd3d_log("frames=%u ms/frame=%.2f draws=%llu\n", frames, (now - last) / 300.0, (unsigned long long)ndraws); last = now; lean_d3d_report(); } }
    /* Frame boundary for the shared renderer's per-frame diagnostics (LEAN_PICK,
     * LEAN_DRAWLOG frame numbers) and LEAN_SURFACE_GUARD, as the chip path does. */
    { extern void lean_d3d_guard_surfaces(void); lean_d3d_guard_surfaces(); }
    if (mark_due && have) { extern void lean_session_mark_picture(long, const uint32_t *); lean_session_mark_picture(mark_due, pix);
        InterlockedCompareExchange(&lean_mark_request, 0, mark_due); }
    if (!have && shown != 1) return;
    driving_present201(have ? (const uint8_t *)pix : NULL, front & 0x07FFFFFFu);
    if (wall_due && have) {
        next_at = (long long)wall_now + every;
        char path[MAX_PATH]; snprintf(path, sizeof path, "%s/wall-%06llu.bmp", dir, (unsigned long long)wall_now);
        { extern unsigned lean_d3d_frame_no(void); nd3d_log("[LEAN-WALL] %06llu frame=%u\n", (unsigned long long)wall_now, lean_d3d_frame_no()); }
        FILE *f = fopen(path, "wb");
        if (f) { unsigned char h[54] = {0}; uint32_t size = 54 + 2560 * 480, off = 54, dib = 40, w = 640; int32_t ht = -480; uint16_t planes = 1, bits = 32;
            memcpy(h, "BM", 2); memcpy(h + 2, &size, 4); memcpy(h + 10, &off, 4); memcpy(h + 14, &dib, 4); memcpy(h + 18, &w, 4); memcpy(h + 22, &ht, 4);
            memcpy(h + 26, &planes, 2); memcpy(h + 28, &bits, 2); fwrite(h, 1, 54, f); fwrite(pix, 1, 2560 * 480, f); fclose(f); }
    }
}
