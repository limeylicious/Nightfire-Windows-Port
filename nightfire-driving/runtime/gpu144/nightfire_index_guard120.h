/* CP120 oversized-stream guard. Pure descriptor math; no guest dereferences,
 * allocation, synchronization, diagnostics, or mutable state. This rejects a
 * new unsupported oversized case, and does not change the small-batch path.
 *
 * The caller supplies RESOLVED guest VAs (not physical DMA offsets), complete
 * pitched output spans including padding, and all potentially read attributes.
 * A valid paired Z attachment remains an output span even with depth disabled:
 * the backend can still upload/read back/pack it. Unknown extents fail closed.
 * Disjointness does not prove equivalence of arbitrary per-chunk CPU fallbacks.
 */
#ifndef NIGHTFIRE_INDEX_GUARD120_H
#define NIGHTFIRE_INDEX_GUARD120_H
#include <stdint.h>

typedef struct { uint32_t address; uint64_t bytes; } NFIndexGuard120Range;
typedef struct {
    unsigned enabled;
    uint32_t address, format;
    unsigned width, height, pitch;
} NFIndexGuard120Texture;
typedef struct {
    NFIndexGuard120Range color, depth; /* depth.bytes==0: no attachment */
    NFIndexGuard120Range vertices[16];
    unsigned vertex_count;
    NFIndexGuard120Texture texture;
} NFIndexGuard120;

/* Half-open ranges. A single range may not straddle a mapping boundary.
 * Contiguous and tiled apertures share storage; low guest RAM does not. */
static int nf_index_guard120_canonical(NFIndexGuard120Range in,
                                      NFIndexGuard120Range *out)
{
    uint64_t begin=in.address, end;
    if (!out || !in.bytes || in.bytes>UINT64_C(0x100000000)-begin) return 0;
    end=begin+in.bytes;
    if ((begin<UINT64_C(0x80000000) && end>UINT64_C(0x80000000)) ||
        (begin<UINT64_C(0xf0000000) && end>UINT64_C(0xf0000000))) return 0;
    if (begin>=UINT64_C(0x80000000) && begin<UINT64_C(0x84000000)) {
        if (end>UINT64_C(0x84000000)) return 0;
    } else if (begin>=UINT64_C(0xf0000000) && begin<UINT64_C(0xf4000000)) {
        if (end>UINT64_C(0xf4000000)) return 0;
        in.address=UINT32_C(0x80000000)+(in.address-UINT32_C(0xf0000000));
    }
    *out=in;
    return 1;
}
static int nf_index_guard120_overlap(NFIndexGuard120Range a,NFIndexGuard120Range b)
{
    return (uint64_t)a.address < (uint64_t)b.address+b.bytes &&
        (uint64_t)b.address < (uint64_t)a.address+a.bytes;
}
/* Conservative envelope used by the existing pre-prepare attribute guard.
 * Includes interleaved raw-record padding, not merely decoded components. */
static int nf_index_guard120_vertex_range(uint32_t address,unsigned stride,
                                          unsigned max_index,NFIndexGuard120Range *out)
{
    NFIndexGuard120Range r, canonical;
    if (!out || !address || max_index>65535) return 0;
    r.address=address;r.bytes=(uint64_t)max_index*stride+64;
    if (!nf_index_guard120_canonical(r,&canonical)) return 0;
    *out=r;return 1;
}
/* Recognized storage matches texture_view_impl and the current CPU sampler.
 * Full mip chains include the minimum whole DXT blocks. Linear storage covers
 * every pitched row, including padding. These are bounds, not sampler changes.
 * Width/height must agree with swizzled metadata so CPU and GPU interpretations
 * cannot silently produce different extents. Unsupported formats reject. */
static int nf_index_guard120_texture_bytes(const NFIndexGuard120Texture *t,uint64_t *out)
{
    unsigned f,levels,logw,logh,w,h,block;
    uint64_t bytes=0;
    if (!t || !out || !t->enabled || !t->width || !t->height ||
        t->width>2048 || t->height>2048) return 0;
    f=(t->format>>8)&255;levels=(t->format>>16)&15;
    if (f==0x12 || f==0x1e || f==0x24 || f==0x25) {
        unsigned bpp=(f==0x24 || f==0x25)?2:4;
        if (levels>1 || t->pitch<t->width*bpp || t->pitch>16384 ||
            (bpp==2 && (t->width&1))) return 0;
        *out=(uint64_t)t->pitch*t->height;return 1;
    }
    block=f==0x0c?8:(f==0x0e || f==0x0f)?16:0;
    logw=(t->format>>20)&15;logh=(t->format>>24)&15;
    if ((!block && f!=6 && f!=7) || (t->format&0xfc)!=0x28 ||
        logw>11 || logh>11 || !levels || levels>(logw>logh?logw:logh)+1 ||
        t->width!=(1u<<logw) || t->height!=(1u<<logh)) return 0;
    w=t->width;h=t->height;
    for (unsigned i=0;i<levels;i++) {
        bytes+=block?(uint64_t)((w+3)/4)*((h+3)/4)*block:(uint64_t)w*h*4;
        if(w>1)w/=2;if(h>1)h/=2;
    }
    *out=bytes;return 1;
}
static int nf_index_guard120_safe(const NFIndexGuard120 *g)
{
    NFIndexGuard120Range color,depth={0,0},input;
    unsigned has_depth;
    if (!g || g->vertex_count>16 || !nf_index_guard120_canonical(g->color,&color)) return 0;
    has_depth=g->depth.bytes!=0;
    if (has_depth && (!nf_index_guard120_canonical(g->depth,&depth) ||
                     nf_index_guard120_overlap(color,depth))) return 0;
    for (unsigned i=0;i<g->vertex_count;i++) {
        if (!nf_index_guard120_canonical(g->vertices[i],&input) ||
            nf_index_guard120_overlap(input,color) ||
            (has_depth && nf_index_guard120_overlap(input,depth))) return 0;
    }
    if (g->texture.enabled) {
        NFIndexGuard120Range raw;
        raw.address=g->texture.address;
        if (!nf_index_guard120_texture_bytes(&g->texture,&raw.bytes) ||
            !nf_index_guard120_canonical(raw,&input) ||
            nf_index_guard120_overlap(input,color) ||
            (has_depth && nf_index_guard120_overlap(input,depth))) return 0;
    }
    return 1;
}
#endif
