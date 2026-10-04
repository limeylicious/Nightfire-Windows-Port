/* Default-OFF integer classification of a fully preflighted29-float material
 * vertex. All later color/projective/clip arithmetic and rejection order stay
 * original. Full vertex readability precedes this call; no extra word read. */
#ifndef NIGHTFIRE_FINITE357_H
#define NIGHTFIRE_FINITE357_H
#include <windows.h>
#include <stdint.h>
#include <emmintrin.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
static int nf_finite357_enabled(void){
 static int on=-1;
 if(on<0){DWORD error=GetLastError();int crt=errno;
  const char*v=getenv("DRIVING_MATERIAL_FINITE357");on=v&&!strcmp(v,"1");
  errno=crt;SetLastError(error);}
 return on;
}
static int nf_finite29_357(const float*values){
 const __m128i exponent=_mm_set1_epi32(0x7f800000);
 for(unsigned k=0;k<28;k+=4){
  __m128i bits=_mm_loadu_si128((const __m128i*)(values+k));
  __m128i invalid=_mm_cmpeq_epi32(_mm_and_si128(bits,exponent),exponent);
  if(_mm_movemask_epi8(invalid))return 0;
 }
 uint32_t tail;memcpy(&tail,values+28,sizeof tail);
 return (tail&0x7f800000u)!=0x7f800000u;
}
#endif
