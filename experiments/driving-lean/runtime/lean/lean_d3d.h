/* Lean Driving renderer: GPU-resident D3D11 backend interface.
 * Colour/depth surfaces live on the GPU keyed by guest address; guest RAM is
 * only read for inputs (vertices, textures, initial surface contents) and only
 * written/read back where the title or the window genuinely needs pixels. */
#ifndef LEAN_D3D_H
#define LEAN_D3D_H
#include <stdint.h>

typedef struct LeanTarget {
    uint32_t color, zeta;          /* guest VAs (0x8xxxxxxx); 0 = none */
    unsigned width, height;        /* logical clip size */
    unsigned aa_x, aa_y;           /* storage multiplier */
    unsigned color_fmt, zeta_fmt;  /* NV097 surface format fields */
    unsigned color_pitch, zeta_pitch;
    unsigned swizzled;
} LeanTarget;

typedef struct LeanDraw {
    LeanTarget t;
    const uint32_t *K;             /* Kelvin method registers, K[method/4] */
    const void *vp;                /* NFVertexProgram */
    unsigned topo;                 /* 0 triangle list, 1 line list, 2 point list */
    const float *verts;            /* popcount(mask) float4 per vertex */
    unsigned nverts, mask;
    const uint32_t *idx;
    unsigned nidx;
    uint32_t tex_addr[4], pal_addr[4];
} LeanDraw;

int  lean_d3d_init(void);
void lean_d3d_epoch(void);
void lean_d3d_vp_dirty(void);
void lean_d3d_draw(const LeanDraw *d);
void lean_d3d_clear(const LeanTarget *t, uint32_t flags, unsigned x0, unsigned x1,
                    unsigned y0, unsigned y1, uint32_t color, uint32_t zs);
int  lean_d3d_blit(uint32_t src, unsigned spitch, uint32_t dst, unsigned dpitch,
                   unsigned sx, unsigned sy, unsigned dx, unsigned dy, unsigned w, unsigned h);
int  lean_d3d_readback(uint32_t addr, unsigned w, unsigned h, uint32_t *out);
void lean_d3d_forget(uint32_t addr, uint32_t bytes);
void lean_d3d_report(void);

/* Supplied by the front end. */
uint8_t *lean_guest(uint32_t va, uint32_t bytes);
#endif
