/*418 CPU packing only. No GPU comparison/quantization or publication changes.
 * For b=bits(float z), 0.5<=z<=1: round(z*16777215) is b-0x3e800001,
 * except z==0.5 yields0x800000. Original cvttpd normally sets precision;
 * require that flag already sticky and all exceptions masked. Other groups,
 * modes and scalar tails run the literal original conversion. */
#ifndef NIGHTFIRE_PACK418_H
#define NIGHTFIRE_PACK418_H
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef NF_PACK418_TEST
static uint64_t pack418_fast_groups_test;
#define PACK418_VISIT() (++pack418_fast_groups_test)
#else
#define PACK418_VISIT() ((void)0)
#endif
static int pack418_setting=-1;
static int pack418_enabled(void){
 if(pack418_setting<0){int c=errno;
#ifdef _WIN32
  DWORD e=GetLastError();
#endif
  const char*v=getenv("DRIVING_PACK418");pack418_setting=v&&!strcmp(v,"1");errno=c;
#ifdef _WIN32
  SetLastError(e);
#endif
 }return pack418_setting;
}
static void nf_depth_pack(uint32_t*dst,const float*src,unsigned count){
#ifdef NF_DEPTH_SSE2
 if(pack418_enabled()&&((_mm_getcsr()&0x7fa0u)==0x1fa0u)){
  unsigned i=0;const __m128i lower=_mm_set1_epi32(0x3effffff),upper=_mm_set1_epi32(0x3f800000),half=_mm_set1_epi32(0x3f000000);
  const __m128i bias=_mm_set1_epi32(0x3e800001),stencil=_mm_set1_epi32(255);
  for(;i+4<=count;i+=4){
   __m128i b=_mm_loadu_si128((const __m128i*)(src+i));
   if(_mm_movemask_epi8(_mm_cmpeq_epi32(b,upper))==65535){
    PACK418_VISIT();
    _mm_storeu_si128((__m128i*)(dst+i),_mm_or_si128(_mm_loadu_si128((const __m128i*)(dst+i)),_mm_set1_epi32(-256)));continue;
   }
   if(_mm_movemask_epi8(_mm_cmpeq_epi32(b,_mm_setzero_si128()))==65535){
    PACK418_VISIT();
    _mm_storeu_si128((__m128i*)(dst+i),_mm_and_si128(_mm_loadu_si128((const __m128i*)(dst+i)),stencil));continue;
   }
   __m128i valid=_mm_andnot_si128(_mm_cmpgt_epi32(b,upper),_mm_cmpgt_epi32(b,lower));
   if(_mm_movemask_epi8(valid)==65535){
    PACK418_VISIT();
    __m128i q=_mm_sub_epi32(_mm_sub_epi32(b,bias),_mm_cmpeq_epi32(b,half));
    q=_mm_or_si128(_mm_slli_epi32(q,8),_mm_and_si128(_mm_loadu_si128((const __m128i*)(dst+i)),stencil));
    _mm_storeu_si128((__m128i*)(dst+i),q);
   }else {nf_depth_pack_original418(dst+i,src+i,count-i);return;}
  }
  if(i<count)nf_depth_pack_original418(dst+i,src+i,count-i);
  return;
 }
#endif
 nf_depth_pack_original418(dst,src,count);
}
#endif
