/* Exact texture-byte equality. No renderer state or runtime environment policy.
 * Compile ONLY nightfire_texture_equal114_avx2.c with /arch:AVX2; keep this
 * dispatcher's TU and callers at their ordinary baseline architecture. */
#ifndef NIGHTFIRE_TEXTURE_EQUAL114_H
#define NIGHTFIRE_TEXTURE_EQUAL114_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int nf_texture_equal114(const void *left,const void *right,size_t length);
int nf_texture_equal114_baseline(const void *left,const void *right,size_t length);
int nf_texture_equal114_avx2_available(void);
/* Pure predicate: does not execute CPUID, XGETBV, or any AVX instruction. */
int nf_texture_equal114_features(unsigned max_leaf,unsigned leaf1_ecx,
                                uint64_t xcr0,unsigned leaf7_ebx);
#ifdef __cplusplus
}
#endif
#endif
