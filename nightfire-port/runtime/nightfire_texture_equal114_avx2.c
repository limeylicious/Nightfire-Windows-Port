/* The only CP114 translation unit compiled with /arch:AVX2. */
#include "nightfire_texture_equal114.h"
#if defined(_M_X64) || defined(_M_IX86) || defined(__AVX2__)
#include <immintrin.h>
int nf_texture_equal114_avx2_kernel(const void *left,const void *right,size_t length){
    const uint8_t *a=(const uint8_t*)left,*b=(const uint8_t*)right;
    int equal=1;
    while(length>=128){
        __m256i d0=_mm256_xor_si256(_mm256_loadu_si256((const __m256i*)a),_mm256_loadu_si256((const __m256i*)b));
        __m256i d1=_mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(a+32)),_mm256_loadu_si256((const __m256i*)(b+32)));
        __m256i d2=_mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(a+64)),_mm256_loadu_si256((const __m256i*)(b+64)));
        __m256i d3=_mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(a+96)),_mm256_loadu_si256((const __m256i*)(b+96)));
        __m256i d=_mm256_or_si256(_mm256_or_si256(d0,d1),_mm256_or_si256(d2,d3));
        if(!_mm256_testz_si256(d,d)){equal=0;goto done;}
        a+=128;b+=128;length-=128;
    }
    while(length>=32){
        __m256i d=_mm256_xor_si256(_mm256_loadu_si256((const __m256i*)a),_mm256_loadu_si256((const __m256i*)b));
        if(!_mm256_testz_si256(d,d)){equal=0;goto done;}
        a+=32;b+=32;length-=32;
    }
    /* Scalar tail is deliberately bounded; no speculative full-vector tail. */
    while(length){if(*a++!=*b++){equal=0;goto done;}--length;}
done:
    _mm256_zeroupper();
    return equal;
}
#else
int nf_texture_equal114_avx2_kernel(const void *left,const void *right,size_t length){
    return nf_texture_equal114_baseline(left,right,length);
}
#endif
