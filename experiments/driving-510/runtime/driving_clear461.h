#ifndef DRIVING_CLEAR461_H
#define DRIVING_CLEAR461_H
/* Default OFF: exact masked CPU clear only. No GPU API, mapping cache or
 * publication policy. Validated ordinary surfaces use bounded SSE2 row chunks;
 * unusual layouts/descriptor aliasing retain the original scalar implementation. */
#include <windows.h>
#include <stdlib.h>
#include <errno.h>
#include <fenv.h>
#include <emmintrin.h>
#ifndef DRIVING_CLEAR461_ENABLED
static int clear461_setting=-1;
static int clear461_enabled(void){
 if(clear461_setting<0){
  DWORD error=GetLastError();int crt=errno;fenv_t env;unsigned csr=_mm_getcsr();fegetenv(&env);
  const char*v=getenv("DRIVING_CLEAR461");clear461_setting=v&&!strcmp(v,"1");
  fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(error);
 }
 return clear461_setting;
}
#else
static int clear461_enabled(void){return DRIVING_CLEAR461_ENABLED();}
#endif
static int clear461_eligible(void *memory,size_t size,const DrivingSurface145*s,unsigned bytes){
 /* Do not strengthen the original public validator: fallback handles any layout
  * outside the ordinary decoder contract, including unusual overlapping rows. */
 if(!memory||!s||(bytes!=2&&bytes!=4)||!s->width||s->width>65535||!s->height||s->height>65535||
    (s->lanes!=1&&s->lanes!=2)||s->storage_width!=s->width*s->lanes)return 0;
 DrivingSurface145 decoded;
 if(!driving_surface145(&decoded,s->format,s->width<<16,s->height<<16,s->pitch)||
    decoded.lanes!=s->lanes||decoded.storage_width!=s->storage_width||decoded.span!=s->span)return 0;
 size_t row=(size_t)s->width*s->lanes*bytes;
 if(s->pitch<row||s->pitch>65535)return 0;
 size_t span=(size_t)(s->height-1)*s->pitch+row;
 if(s->span!=span||size<span)return 0;
 uintptr_t a=(uintptr_t)memory,b=(uintptr_t)s;
 if(span>UINTPTR_MAX-a||sizeof*s>UINTPTR_MAX-b)return 0;
 if(a<b+sizeof*s&&b<a+span)return 0;
 return 1;
}
static int clear461_rows(void *memory,const DrivingSurface145*s,unsigned x0,unsigned x1,
 unsigned y0,unsigned y1,unsigned bytes,uint32_t mask,uint32_t value){
 __m128i keep=bytes==2?_mm_set1_epi16((short)~mask):_mm_set1_epi32((int)~mask);
 __m128i fill=bytes==2?_mm_set1_epi16((short)(value&mask)):_mm_set1_epi32((int)(value&mask));
 size_t n=(size_t)(x1-x0+1)*s->lanes*bytes,offset=(size_t)x0*s->lanes*bytes;
 for(unsigned y=y0;y<=y1;y++){
  unsigned char*p=(unsigned char*)memory+(size_t)y*s->pitch+offset;size_t left=n;
  while(left>=16){
   __m128i old=_mm_loadu_si128((const __m128i*)p);
   _mm_storeu_si128((__m128i*)p,_mm_or_si128(_mm_and_si128(old,keep),fill));
   p+=16;left-=16;
  }
  while(left){uint32_t old=0;memcpy(&old,p,bytes);old=(old&~mask)|(value&mask);memcpy(p,&old,bytes);p+=bytes;left-=bytes;}
 }
 return 1;
}
#endif
