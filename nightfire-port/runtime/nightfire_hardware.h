#ifndef NIGHTFIRE_HARDWARE_H
#define NIGHTFIRE_HARDWARE_H
#include <stdint.h>
#include <stddef.h>
#include "nightfire_vertex_program.h"
#include "nightfire_swizzled_shadow131.h"
#ifdef NIGHTFIRE_SHADOW_LAYOUT131_DIAGNOSTIC
#include "nightfire_shadow_layout131.h"
#endif
typedef struct {float attributes[16][4];} NFHardwareInputVertex;
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
    unsigned texture_format,texture_filter,texture_address;
    unsigned texture_width,texture_height,texture_pitch; /* linear packed video */
    int filtered;
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
} NFHardwareState;
int nf_hw_fallback96_enabled(void);
int nf_hw_blend97_enabled(void);
int nf_hw_begin(const NFHardwareState *state);
int nf_hw_draw(const NFHardwareVertex *vertices,unsigned count);
int nf_hw_program_active(void);
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
