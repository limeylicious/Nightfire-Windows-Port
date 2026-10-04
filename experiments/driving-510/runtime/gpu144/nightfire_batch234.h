/* Private implementation after initialize/surfaces and quant130 definitions.
 * No packet/guest ownership is implied. Caller supplies a stable owned lane. */
#ifndef NIGHTFIRE_BATCH234_H
#define NIGHTFIRE_BATCH234_H
#ifdef NIGHTFIRE_BATCH234
static int batch_nearest236(void){
#ifdef NF_DEPTH_SSE2
 return (_mm_getcsr()&_MM_ROUND_MASK)==_MM_ROUND_NEAREST;
#else
 return 0;
#endif
}
static unsigned prepared_width234,prepared_height234;
static uint64_t boundaries234;
#ifdef NF_BATCH234_TEST
static int force_unsupported234;
#endif
#endif
#ifdef NIGHTFIRE_BATCH_TIMING244
#define nf_hw_batch234_prepare nf_hw_batch234_prepare_impl244
#endif
int nf_hw_batch234_prepare(unsigned w,unsigned h)
{if(resident313_blocked()){return 0;}
#ifdef NIGHTFIRE_BATCH234
    /* A refused preparation must never complete or change a pending draw. */
    if(pending||!batch_nearest236())return 0;
    prepared_width234=prepared_height234=0;
    if(w!=640||h!=480)return 0;
#ifdef NF_BATCH234_TEST
    if(force_unsupported234)return 0;
#endif
    if(!initialize()||ID3D11Device_GetFeatureLevel(dev)<D3D_FEATURE_LEVEL_11_0||
       !quant130_prepare(w,h))return 0;
    prepared_width234=w;prepared_height234=h;return 1;
#else
    (void)w;(void)h;return 0;
#endif
}
#ifdef NIGHTFIRE_BATCH_TIMING244
#undef nf_hw_batch234_prepare
int nf_hw_batch234_prepare(unsigned w,unsigned h){if(resident313_blocked()){return 0;}uint64_t t=bt244_start(0);int ok=nf_hw_batch234_prepare_impl244(w,h);bt244_end(BT244_PREPARE,t);return ok;}
#endif
#include "nightfire_depth_clean269.h"
#include "nightfire_depth_ping272.h"
int nf_hw_batch234_boundary(const NFHardwareState *next)
{if(resident313_blocked()){return 0;}
#ifdef NIGHTFIRE_BATCH234
    if(!batch_nearest236()||!next||!pending||initialized<=0||!active.material221||!next->material221||
       prepared_width234!=640||prepared_height234!=480||width!=640||height!=480||
       active.color_only||next->color_only||active.color_layout||next->color_layout||
       !active.color||!active.depth||next->color!=active.color||next->depth!=active.depth||
       active.pitch!=2560||active.depth_pitch!=2560||next->pitch!=2560||next->depth_pitch!=2560||
       next->width!=640||next->height!=480||!depth||!dsv||
       quant130_width!=640||quant130_height!=480||!quant130_shader||!quant130_input||!quant130_output)
        return 0;
    /* Equivalent to previous draw's D32->Z24 publication followed by the next
     * draw's Z24->D32 upload. Stencil stays in the untouched owned depth bytes.
     * quant130_apply unbinds OM targets and clears bound.targets; next begin
     * restores graphics. No allocation, CPU mapping or guest copy happens here. */
    BT244_START(bt_boundary244);int canonical302=native302_boundary(next);if(canonical302<0)return 0;int canonical291=canonical302?1:fragment291_boundary(next);if(canonical291<0)return 0;if(!canonical291){int converted272=depth272_apply();if(converted272<0)return 0;if(!converted272&&!depth269_skip(depth)){quant130_apply(depth);depth269_quantized_now(depth);}}BT244_END(bt_boundary244,BT244_BOUNDARY);boundaries234++;return 1;
#else
    (void)next;return 0;
#endif
}
#endif
