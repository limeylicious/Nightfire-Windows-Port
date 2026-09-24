#ifndef DRIVING_FINITE328_H
#define DRIVING_FINITE328_H
/* x64/SSE2 integer classification of the existing 16x4 shader output.
 * Optional: the original scalar isfinite loop remains the default. This reads
 * the completed local output only, with no change to guest inputs or handoffs. */
#include <emmintrin.h>
#include <xmmintrin.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
static int driving_finite328_enabled(void){
 static int on=-1;
 if(on<0){DWORD error=GetLastError();int crt=errno;
  const char*v=getenv("DRIVING_FINITE328");on=v&&!strcmp(v,"1");
  errno=crt;SetLastError(error);}
 return on;
}
static int driving_finite328(const float output[16][4]){
 const __m128i exponent=_mm_set1_epi32(0x7f800000);
 for(unsigned k=0;k<16;k++){
  __m128i values=_mm_loadu_si128((const __m128i*)output[k]);
  __m128i invalid=_mm_cmpeq_epi32(_mm_and_si128(values,exponent),exponent);
  if(_mm_movemask_ps(_mm_castsi128_ps(invalid)))return 0;
 }
 return 1;
}
#endif
