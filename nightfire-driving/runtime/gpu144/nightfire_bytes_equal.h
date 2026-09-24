/* Exact equality only; every byte is checked, with no read past length.
 * Unlike memcmp, the texture cache does not need lexicographic ordering. */
#ifndef NIGHTFIRE_BYTES_EQUAL_H
#define NIGHTFIRE_BYTES_EQUAL_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
#include <emmintrin.h>
#endif
static int nf_bytes_equal(const void *left,const void *right,size_t length){
 const uint8_t *a=left,*b=right;
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
 const __m128i zero=_mm_setzero_si128();
 while(length>=64){
  __m128i d0=_mm_xor_si128(_mm_loadu_si128((const __m128i*)a),_mm_loadu_si128((const __m128i*)b));
  __m128i d1=_mm_xor_si128(_mm_loadu_si128((const __m128i*)(a+16)),_mm_loadu_si128((const __m128i*)(b+16)));
  __m128i d2=_mm_xor_si128(_mm_loadu_si128((const __m128i*)(a+32)),_mm_loadu_si128((const __m128i*)(b+32)));
  __m128i d3=_mm_xor_si128(_mm_loadu_si128((const __m128i*)(a+48)),_mm_loadu_si128((const __m128i*)(b+48)));
  if(_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_or_si128(_mm_or_si128(d0,d1),_mm_or_si128(d2,d3)),zero))!=65535)return 0;
  a+=64;b+=64;length-=64;
 }
 while(length>=16){
  if(_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128((const __m128i*)a),_mm_loadu_si128((const __m128i*)b)))!=65535)return 0;
  a+=16;b+=16;length-=16;
 }
#endif
 return !memcmp(a,b,length);
}
#endif
