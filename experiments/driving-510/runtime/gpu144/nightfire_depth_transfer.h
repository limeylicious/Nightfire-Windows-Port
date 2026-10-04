/* Exact host depth conversion for D3D11/guest Z24S8 transfers. */
#ifndef NIGHTFIRE_DEPTH_TRANSFER_H
#define NIGHTFIRE_DEPTH_TRANSFER_H
#include <stdint.h>
#include <math.h>
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
#include <emmintrin.h>
#define NF_DEPTH_SSE2 1
#endif
/* Only visible depth words participate; callers handle rows and pitch padding.
 * Stencil is guest-owned and does not affect the D32 initialization value. */
static int nf_depth_is_max(const uint32_t *src,unsigned count){
 unsigned i=0;
#ifdef NF_DEPTH_SSE2
 const __m128i stencil=_mm_set1_epi32(255),ones=_mm_set1_epi32(-1);
 for(;i+4<=count;i+=4){
  __m128i z=_mm_or_si128(_mm_loadu_si128((const __m128i*)(src+i)),stencil);
  if(_mm_movemask_epi8(_mm_cmpeq_epi32(z,ones))!=65535)return 0;
 }
#endif
 for(;i<count;i++)if((src[i]>>8)!=0xffffff)return 0;
 return 1;
}
static void nf_depth_unpack(float *dst,const uint32_t *src,unsigned count){
 unsigned i=0;
#ifdef NF_DEPTH_SSE2
 const __m128d scale=_mm_set1_pd(16777215.0);
 for(;i+4<=count;i+=4){
  __m128i z=_mm_srli_epi32(_mm_loadu_si128((const __m128i*)(src+i)),8);
  __m128d lo=_mm_div_pd(_mm_cvtepi32_pd(z),scale),hi=_mm_div_pd(_mm_cvtepi32_pd(_mm_srli_si128(z,8)),scale);
  _mm_storeu_ps(dst+i,_mm_movelh_ps(_mm_cvtpd_ps(lo),_mm_cvtpd_ps(hi)));
 }
#endif
 for(;i<count;i++)dst[i]=(float)((double)(src[i]>>8)/16777215.0);
}
static void nf_depth_pack_original418(uint32_t *dst,const float *src,unsigned count){
 unsigned i=0;
#ifdef NF_DEPTH_SSE2
 const __m128d scale=_mm_set1_pd(16777215.0),half=_mm_set1_pd(.5);
 const __m128i stencil=_mm_set1_epi32(255);
 for(;i+4<=count;i+=4){
  __m128 z=_mm_min_ps(_mm_max_ps(_mm_loadu_ps(src+i),_mm_setzero_ps()),_mm_set1_ps(1));
  __m128d lo=_mm_add_pd(_mm_mul_pd(_mm_cvtps_pd(z),scale),half),hi=_mm_add_pd(_mm_mul_pd(_mm_cvtps_pd(_mm_movehl_ps(z,z)),scale),half);
  __m128i packed=_mm_unpacklo_epi64(_mm_cvttpd_epi32(lo),_mm_cvttpd_epi32(hi));
  packed=_mm_or_si128(_mm_slli_epi32(packed,8),_mm_and_si128(_mm_loadu_si128((const __m128i*)(dst+i)),stencil));
  _mm_storeu_si128((__m128i*)(dst+i),packed);
 }
#endif
 for(;i<count;i++){double z=fmin(16777215.0,fmax(0.0,(double)src[i]*16777215.0));dst[i]=((uint32_t)(z+.5)<<8)|(dst[i]&255);}
}
#include "nightfire_pack418.h"
#endif
