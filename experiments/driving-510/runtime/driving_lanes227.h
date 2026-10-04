/* Exact byte permutation for two independent 32-bit sample lanes. SSE2 is
 * baseline on the x64 Windows target. Unaligned loads retain arbitrary owned
 * byte-buffer alignment; no floating-point or depth conversion occurs here. */
#ifndef DRIVING_LANES227_H
#define DRIVING_LANES227_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <emmintrin.h>
#ifdef DRIVING_ACCESS321
#include "driving_access321.h"
#endif
static void driving_split227(const uint8_t *pair,uint8_t *a,uint8_t *b,size_t pixels)
{
#ifdef DRIVING_ACCESS321
 if(pixels<=SIZE_MAX/8)driving_access321_note(pair,pixels*8,DA321_SPLIT_READ);
#endif
 size_t i=0;
 for(;i+4<=pixels;i+=4){
  __m128i x=_mm_loadu_si128((const __m128i*)(pair+i*8));
  __m128i y=_mm_loadu_si128((const __m128i*)(pair+i*8+16));
  x=_mm_shuffle_epi32(x,_MM_SHUFFLE(3,1,2,0));
  y=_mm_shuffle_epi32(y,_MM_SHUFFLE(3,1,2,0));
  _mm_storeu_si128((__m128i*)(a+i*4),_mm_unpacklo_epi64(x,y));
  _mm_storeu_si128((__m128i*)(b+i*4),_mm_unpackhi_epi64(x,y));
 }
 for(;i<pixels;i++){memcpy(a+i*4,pair+i*8,4);memcpy(b+i*4,pair+i*8+4,4);}
}
/*276: freshly load/permutate/store every input byte, with optional comparison
 * to the previous complete lane before overwriting it. No FP arithmetic. */
static unsigned driving_split_equal276(const uint8_t *pair,uint8_t *a,uint8_t *b,size_t pixels,unsigned eligible)
{
#ifdef DRIVING_ACCESS321
 if(pixels<=SIZE_MAX/8)driving_access321_note(pair,pixels*8,DA321_SPLIT_READ);
#endif
 size_t i=0;__m128i different_a=_mm_setzero_si128(),different_b=_mm_setzero_si128();
 for(;i+4<=pixels;i+=4){
  __m128i x=_mm_loadu_si128((const __m128i*)(pair+i*8));
  __m128i y=_mm_loadu_si128((const __m128i*)(pair+i*8+16));
  x=_mm_shuffle_epi32(x,_MM_SHUFFLE(3,1,2,0));y=_mm_shuffle_epi32(y,_MM_SHUFFLE(3,1,2,0));
  __m128i na=_mm_unpacklo_epi64(x,y),nb=_mm_unpackhi_epi64(x,y);
  if(eligible&1)different_a=_mm_or_si128(different_a,_mm_xor_si128(na,_mm_loadu_si128((const __m128i*)(a+i*4))));
  if(eligible&2)different_b=_mm_or_si128(different_b,_mm_xor_si128(nb,_mm_loadu_si128((const __m128i*)(b+i*4))));
  _mm_storeu_si128((__m128i*)(a+i*4),na);_mm_storeu_si128((__m128i*)(b+i*4),nb);
 }
 if(_mm_movemask_epi8(_mm_cmpeq_epi8(different_a,_mm_setzero_si128()))!=65535)eligible&=~1u;
 if(_mm_movemask_epi8(_mm_cmpeq_epi8(different_b,_mm_setzero_si128()))!=65535)eligible&=~2u;
 for(;i<pixels;i++){
  if((eligible&1)&&memcmp(a+i*4,pair+i*8,4))eligible&=~1u;
  if((eligible&2)&&memcmp(b+i*4,pair+i*8+4,4))eligible&=~2u;
  memcpy(a+i*4,pair+i*8,4);memcpy(b+i*4,pair+i*8+4,4);
 }return eligible&3;
}
static void driving_join227(uint8_t *pair,const uint8_t *a,const uint8_t *b,size_t pixels)
{
#ifdef DRIVING_ACCESS321
 if(pixels<=SIZE_MAX/8)driving_access321_note(pair,pixels*8,DA321_JOIN_WRITE);
#endif
 size_t i=0;
 for(;i+4<=pixels;i+=4){
  __m128i x=_mm_loadu_si128((const __m128i*)(a+i*4));
  __m128i y=_mm_loadu_si128((const __m128i*)(b+i*4));
  _mm_storeu_si128((__m128i*)(pair+i*8),_mm_unpacklo_epi32(x,y));
  _mm_storeu_si128((__m128i*)(pair+i*8+16),_mm_unpackhi_epi32(x,y));
 }
 for(;i<pixels;i++){memcpy(pair+i*8,a+i*4,4);memcpy(pair+i*8+4,b+i*4,4);}
}
#endif
