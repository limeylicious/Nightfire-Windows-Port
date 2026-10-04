#ifndef NIGHTFIRE_HARDWARE_H
#define NIGHTFIRE_HARDWARE_H
#include <stdint.h>
#include <stddef.h>
#include "nf_pixel221.h"
#include "nightfire_vertex_program.h"
#include "../driving_cpu448.h"
#include "nightfire_swizzled_shadow131.h"
#ifdef NIGHTFIRE_SHADOW_LAYOUT131_DIAGNOSTIC
#include "nightfire_shadow_layout131.h"
#endif
typedef struct {float attributes[16][4];} NFHardwareInputVertex;
/* Explicit218 window XYZ/original homogeneous W, projective UVRQ, float RGBA.
 * Fog is supplied as the final unsaturated interpolation factor. */
typedef struct {float position[4],uv[4],color[4],fog;} NFHardwareTransformed218;
typedef struct {float position[4],uv0[4],uv1[4],color[4],fog;} NFHardwareDual220;
/* Explicit bounded raw Driving material; each UV preserves original UVRQ. */
typedef struct {float position[4],uv[4][4],color[4],specular[4],fog;} NFHardwareMaterialVertex221;
typedef struct {
 const uint8_t *data;size_t available;
 /* Optional opaque source identity; never dereferenced. Bytes remain authoritative. */
 const void *identity228;
 unsigned format,filter,address,anisotropy,bias215;
} NFHardwareMaterialTexture221;
typedef struct {NFPixel221 pixel;NFHardwareMaterialTexture221 textures[4];unsigned integer_depth291;unsigned integer_depth302; /* Derived eligibility, never activation. */} NFHardwareMaterial221;
/* Caller owns/validates the complete readable snapshot before this content
 * check. No identity/cache shortcut: every byte belongs to this submission. */
static int nf_material_white242(const NFHardwareMaterial221 *m){
 const NFHardwareMaterialTexture221 *t=&m->textures[2];
 if((!nf_pixel_white_contract242(&m->pixel)&&!nf_pixel_white_contract292(&m->pixel))||!t->data||t->available!=524288||
    t->format!=0x09910529||t->address!=0x00030303||t->filter!=0x02063f01||t->anisotropy!=1||t->bias215!=1)return 0;
 /*448: same all-0xFF answer; DRIVING_CPU448 selects SSE2 scan (1) or verify (2).*/
 return cpu448_white(t->data,524288);
}
enum {NF_DIRECT90_F28=1,NF_DIRECT90_1F32=2,NF_DIRECT90_7F9F32=4};
enum {NF_HW_TRIANGLE_LIST=0,NF_HW_TRIANGLE_STRIP=1};
typedef struct {
    float position[4],uv[2];uint32_t color;float fog;
} NFHardwareVertex;
typedef struct {
    uint8_t *color,*depth;
    unsigned width,height,pitch,depth_pitch;
    unsigned left,top,right,bottom;
    unsigned depth_enable,depth_func,depth_write;
    unsigned alpha_enable,alpha_func,alpha_ref;
    unsigned blend_enable,blend_src,blend_dst;
    unsigned combiner,scale;
    unsigned fog_enable,fog_color;float fog_bias,fog_slope;
    const uint8_t *texture;size_t texture_available;
    const uint8_t *texture_palette;size_t texture_palette_available; /* Selected175 palette span, independently validated. */
    unsigned texture_format,texture_filter,texture_address;
    unsigned texture_width,texture_height,texture_pitch; /* linear packed video */
    int filtered;
    /* Explicit182: stage0/3 UVRQ; linear11 uses texel coordinates. Default0 retains legacy. */
    unsigned selected_coordinates182,texture_stage182,cpu_vertex182;
    /* Off0; experimental193 center lane0=1/lane1=2. Not verified NV2A AA. */
    unsigned experimental_resolve193;
    /* Off0; experimental200 two-lane movie coverage, center lane0=1/lane1=2. */
    unsigned experimental_movie200;
    const NFVertexProgram *program;
    unsigned cull_enable,cull_face,front_face;
    unsigned direct28; /* Validated original float3/BGRA8/float2 record request. */
    unsigned direct90; /* Allowed original-record/default-stream layouts. */
    /* CP96 requests are rejected unless its compile/runtime opt-in is active.
     * color_only requires depth disabled, depth=NULL and depth_pitch=0; the
     * disabled write-mask bit has no effect and no guest Z bytes are touched.
     * compat_point_clamp preserves the current CPU fallback for one-level
     * format6/7 textures with U/V address4; it is not a border-mode oracle. */
    unsigned color_only,compat_point_clamp;
    /* Explicit captured dst - src*srcAlpha pass; ordinary blend stays ADD. */
    unsigned reverse_subtract97;
    unsigned color_layout; /* Default0 pitched;1 Morton256;2 Morton128 (exact color-only). */
    /* Driving212: zero retains legacy RGBA writes; 0x10|RGBA bits is explicit.
     * Bit0=R,1=G,2=B,3=A. Xbox RGB-only passes must preserve target alpha. */
    unsigned color_write_mask212;
    /* Driving213 explicit one-level RGBA sampler, zero keeps legacy settings.
     * Values1/2/4/8 request that anisotropy limit and zero mip bias.
     * Explicit transformed218 BC1/BC2 instead permits the original mip chain. */
    unsigned sampler_anisotropy213;
    /* Driving215: explicit captured -255/256 bias with the one-level sampler.
     * Requires anisotropy213 and exact filter02063F01; default0 keeps213.
     * Also selects this bias for the separate transformed218 BC mip samplers. */
    unsigned sampler_bias215;
    unsigned transformed218; /* Off0. Explicit single-BC1/BC2 pretransformed path. */
    unsigned dual220; /* Analysis-only exact original48 two-texture combiner. */
    const uint8_t *texture1_220;size_t texture1_available220;
    unsigned texture1_format220,texture1_filter220,texture1_address220;
    const NFHardwareMaterial221 *material221; /* Requires NIGHTFIRE_MATERIAL221. Descriptor copied at begin. */
} NFHardwareState;
/*352 owned generic compute result. Return1 means a completed private result,
 * including per-vertex ISA failures;0 unsupported/not idle;-1 actual backend
 * failure. Output is unchanged on0/-1. No guest RAM publication or handoff.
 * Program is immutable; caller owns complete input/output nonoverlapping spans. */
typedef struct NFHardwareVertexResult352 {
 float output[16][4];uint32_t valid,pc,constant,reserved;
} NFHardwareVertexResult352;
int nf_hw_vertex_compute352(const NFVertexProgram *,const NFHardwareInputVertex *,
 unsigned,NFHardwareVertexResult352 *,uint32_t *failure);
int nf_hw_fallback96_enabled(void);
int nf_hw_blend97_enabled(void);
int nf_hw_begin(const NFHardwareState *state);
int nf_hw_draw(const NFHardwareVertex *vertices,unsigned count);
int nf_hw_draw_transformed218(const NFHardwareTransformed218 *vertices,unsigned count);
int nf_hw_draw_dual220(const NFHardwareDual220 *vertices,unsigned count);
int nf_hw_draw_material221(const NFHardwareMaterialVertex221 *vertices,unsigned count);
int nf_hw_program_active(void);
/* Selected CPU execution accepts dense triangle-list input through nf_hw_draw_input. */
int nf_hw_selected_cpu182(void);
unsigned nf_hw_program_inputs(void);
int nf_hw_draw_input(const NFHardwareInputVertex *vertices,unsigned count);
int nf_hw_draw_indexed_input(const NFHardwareInputVertex *vertices,unsigned count,const uint16_t *indices,unsigned index_count);
/* Packed float4 attributes in ascending mask-bit order, one dummy float4 if0. */
int nf_hw_draw_packed_input(const float *vertices,unsigned count,unsigned inputs,const uint16_t *indices,unsigned index_count);
int nf_hw_draw_packed_topology(const float *vertices,unsigned count,unsigned inputs,const uint16_t *indices,unsigned index_count,unsigned topology);
int nf_hw_direct28_active(void);
int nf_hw_draw_direct28(const void *source,unsigned vertices,const uint16_t *indices,unsigned index_count);
unsigned nf_hw_direct90_stride(void);
int nf_hw_draw_direct90(const void *source,unsigned vertices,const uint16_t *indices,unsigned index_count);
/* Raw layout must already be selected and its stride must match. R16 cut
 * index0xffff is excluded; topology is explicit and never inherited. */
int nf_hw_draw_raw_indexed(const void *source,unsigned vertices,unsigned stride,const uint16_t *indices,unsigned index_count,unsigned topology);
/* Gather only the referenced original records directly into the mapped upload
 * ring. source_indices are relative to the validated source span, in first-use
 * order; draw indices address this compact order. No guest data is cached. */
int nf_hw_draw_raw_compact_indexed(const void *source,unsigned source_vertices,unsigned stride,const uint16_t *source_indices,unsigned unique_vertices,const uint16_t *indices,unsigned index_count,unsigned topology);
int nf_hw_sync(void);
int nf_hw_font_sync469(const NFHardwareState *expected); /*469 exact font caller only*/
#ifdef NIGHTFIRE_BATCH_TIMING244
void nf_hw_batch244_mark(unsigned phase,unsigned count);
#endif
int nf_hw_batch234_prepare(unsigned,unsigned);
int nf_hw_batch234_boundary(const NFHardwareState *);
/* Two owned 640x480 lanes of one original draw. Serialized immediate context.
 * 1: both outputs published; 0: refused before drawing; -1: fatal backend
 * failure, no CPU output publication. No deferred work survives the call. */
int nf_hw_material_pair234(const NFHardwareState states[2],const NFHardwareMaterialVertex221 *vertices[2],unsigned count);
/* Bounded owned draw sequence; return 0 only before draw, -1 after beginning. */
typedef struct NFHardwareBatchDraw248 {
 NFHardwareState states[2];const NFHardwareMaterialVertex221 *vertices[2];unsigned count;
} NFHardwareBatchDraw248;
int nf_hw_material_batch248(const NFHardwareBatchDraw248 *draws,unsigned count);
/*315 opt-in retained backend, entirely inside the existing owned flush. */
int nf_hw_material_retained315(const NFHardwareBatchDraw248 *draws,unsigned count);
/*450 PC mode (default OFF): drain-scoped retained session of the main pair. */
int nf_hw_pc450_open(void);
int nf_hw_pc450_begin(const NFHardwareBatchDraw248 *list,unsigned count);
int nf_hw_pc450_append(const NFHardwareBatchDraw248 *list,unsigned count);
int nf_hw_pc450_finish(void);
int nf_hw_pc450_clear(unsigned x0,unsigned x1,unsigned y0,unsigned y1,unsigned flags,uint32_t argb,uint32_t zs);
void nf_hw_pc451_kick(void); /*451: submit queued GPU work while the pair stays resident between drains*/
/*276 internal consumer/backend protocol. Equality is computed by the backend
 * during a fresh split, never asserted by a caller flag. Token is one-use. */
typedef struct NFHardwareColorKey276 {uintptr_t mapped[2];uint32_t surface[7],span[2][5],reserved;} NFHardwareColorKey276;
int nf_hw_color_seed276_enabled(void);
void nf_hw_color_seed276_revoke(void);
int nf_hw_color_seed276_split(const NFHardwareColorKey276 *key,uint64_t allocation,
 const uint8_t *source,uint8_t *storage,uint64_t *token);
int nf_hw_material_batch_seed276(const NFHardwareBatchDraw248 *draws,unsigned count,uint64_t token);
void nf_hw_color_seed276_publish(uint64_t token);
/*278 depth token is independent; full fresh packed words are compared. */
int nf_hw_depth_seed278_enabled(void);
void nf_hw_depth_seed278_revoke(void);
int nf_hw_depth_seed278_split(const NFHardwareColorKey276 *key,uint64_t allocation,
 const uint8_t *source,uint8_t *storage,uint64_t *token);
int nf_hw_material_batch_seeds278(const NFHardwareBatchDraw248 *draws,unsigned count,uint64_t color_token,uint64_t depth_token);
void nf_hw_depth_seed278_publish(uint64_t token);

#ifdef NF_PAIR234_TEST
void nf_hw_pair234_test_fail(unsigned stage);
#endif

#ifdef NF_RESIDENT130_TEST
void nf_hw_resident130_test_set(int on);
void nf_hw_resident130_test_stats(uint64_t out[6]);
int nf_hw_resident130_test_quant(uint32_t *bits,unsigned width,unsigned height);
#endif
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
/* Exact opt-in: queue the pending paired target's readback before a validated
 * disjoint 256x256 color-only CPU clear. One record, within this command drain.
 * Return0 leaves normal completion required; return1 keeps source guards armed.
 * nf_hw_sync always publishes every retained image in submission order. */
int nf_hw_defer_clear128(const NFHardwareState *clear);
#ifdef NF_DEFER128_TEST
void nf_hw_defer128_test_set(int enabled);
void nf_hw_defer128_test_stats(uint64_t stats[6]);
#endif
#endif
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
/* Diagnostic label only; preserves the ordinary synchronization operation. */
int nf_hw_sync_tag109(const char *reason);
/* Read-only clear command evidence, under the existing109 trigger/window. */
typedef struct {
    uint32_t mask,color_value,z_value,raw_color,raw_depth,resolved_color,resolved_depth;
    uint32_t format,control0,clear_x,clear_y;
    unsigned clip_x,clip_y,width,height,pitch,depth_pitch;
    unsigned depth_valid,gpu_clear_enabled;
    uintptr_t color_host,depth_host;
} NFHardwareClear111;
int nf_hw_clear111_wants(void);
void nf_hw_clear111_observe(const NFHardwareClear111 *clear);
#endif
/* Full matching pending surface only: depth=1.0. Caller clears guest Z24S8.
 * Rejection has no side effects. Existing completion boundaries stay required. */
int nf_hw_clear_depth(const NFHardwareState *state);
/* Pending surface reads need an alias barrier: CP81 clear, or a CP96 pass
 * previously published immediately by the CPU. This is not clear eligibility. */
extern int nf_hw_clear_pending;
int nf_hw_clear_read(const void *address,size_t length);
void nf_hw_report(void);
#endif

/*399 Serialized diagnostic hooks; no rendering or ownership effect. */
void nf_hw_pair_profile399(unsigned profile);
void nf_hw_pair_report399(void);

/*400 Two captured original scope profiles only; no ownership extension. */
void nf_hw_pair_import_profile400(unsigned profile);

/*401 Diagnostic published-output snapshots; never skip GPU work. */
void nf_hw_pair_publish401(unsigned profile,const NFHardwareState states[2],int result);
void nf_hw_pair_report401(void);
