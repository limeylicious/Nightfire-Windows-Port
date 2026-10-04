/*417 Exact Morton byte permutation, no cached game contents or GPU ownership.
 * Only disjoint complete128/256 planes with nonoverlapping rows enter SIMD.
 * Every other case retains the original row-major access and overwrite order.
 * Both header consumers use x64/SSE2; only integer moves/unpacks occur. */
#ifndef NIGHTFIRE_LAYOUT417_H
#define NIGHTFIRE_LAYOUT417_H
#include <emmintrin.h>
#include <errno.h>
static int layout417_setting=-1;
static int layout417_enabled(void){
 if(layout417_setting<0){
  int c=errno;
#ifdef _WIN32
  DWORD e=GetLastError();
#endif
  const char*v=getenv("DRIVING_LAYOUT417");layout417_setting=v&&!strcmp(v,"1");
  errno=c;
#ifdef _WIN32
  SetLastError(e);
#endif
 }return layout417_setting;
}
static int layout417_disjoint(const void*linear,unsigned pitch,const void*guest,unsigned size){
 if((size!=128&&size!=256)||pitch<size*4||!linear||!guest)return 0;
 size_t ln=(size_t)(size-1)*pitch+(size_t)size*4,gn=(size_t)size*size*4;
 uintptr_t a=(uintptr_t)linear,b=(uintptr_t)guest;
 if(ln>UINTPTR_MAX-a||gn>UINTPTR_MAX-b)return 0;
 return a+ln<=b||b+gn<=a;
}
static void layout417_import(void*linear,unsigned pitch,const void*guest,unsigned size){
 for(unsigned y=0;y<size;y+=4){
  unsigned by=nf_swizzled131_spread(y)<<1;
  uint8_t*d=(uint8_t*)linear+(size_t)y*pitch;
  for(unsigned x=0;x<size;x+=4){
   const uint8_t*s=(const uint8_t*)guest+4*(by|nf_swizzled131_spread(x));
   __m128i a=_mm_loadu_si128((const __m128i*)(s)),b=_mm_loadu_si128((const __m128i*)(s+16));
   __m128i c=_mm_loadu_si128((const __m128i*)(s+32)),e=_mm_loadu_si128((const __m128i*)(s+48));
   _mm_storeu_si128((__m128i*)(d+x*4),_mm_unpacklo_epi64(a,b));
   _mm_storeu_si128((__m128i*)(d+pitch+x*4),_mm_unpackhi_epi64(a,b));
   _mm_storeu_si128((__m128i*)(d+(size_t)2*pitch+x*4),_mm_unpacklo_epi64(c,e));
   _mm_storeu_si128((__m128i*)(d+(size_t)3*pitch+x*4),_mm_unpackhi_epi64(c,e));
  }
 }
}
static void layout417_export(void*guest,const void*linear,unsigned pitch,unsigned size){
 for(unsigned y=0;y<size;y+=4){
  unsigned by=nf_swizzled131_spread(y)<<1;
  const uint8_t*s=(const uint8_t*)linear+(size_t)y*pitch;
  for(unsigned x=0;x<size;x+=4){
   uint8_t*d=(uint8_t*)guest+4*(by|nf_swizzled131_spread(x));
   __m128i a=_mm_loadu_si128((const __m128i*)(s+x*4)),b=_mm_loadu_si128((const __m128i*)(s+pitch+x*4));
   __m128i c=_mm_loadu_si128((const __m128i*)(s+(size_t)2*pitch+x*4)),e=_mm_loadu_si128((const __m128i*)(s+(size_t)3*pitch+x*4));
   _mm_storeu_si128((__m128i*)d,_mm_unpacklo_epi64(a,b));
   _mm_storeu_si128((__m128i*)(d+16),_mm_unpackhi_epi64(a,b));
   _mm_storeu_si128((__m128i*)(d+32),_mm_unpacklo_epi64(c,e));
   _mm_storeu_si128((__m128i*)(d+48),_mm_unpackhi_epi64(c,e));
  }
 }
}
static void nf_swizzled131_import_size(void*linear,unsigned pitch,const void*guest,unsigned size){
 if(layout417_enabled()&&layout417_disjoint(linear,pitch,guest,size))layout417_import(linear,pitch,guest,size);
 else nf_swizzled131_import_original417(linear,pitch,guest,size);
}
static void nf_swizzled131_export_size(void*guest,const void*linear,unsigned pitch,unsigned size){
 if(layout417_enabled()&&layout417_disjoint(linear,pitch,guest,size))layout417_export(guest,linear,pitch,size);
 else nf_swizzled131_export_original417(guest,linear,pitch,size);
}
#endif
