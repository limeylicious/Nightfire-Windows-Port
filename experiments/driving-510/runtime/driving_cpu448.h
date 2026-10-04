/* Checkpoint448: exact CPU replacements for two hot loops named by the 447 ETW
 * profile of the /O2 445 build (vehicle, heavy scene):
 *   - nf_material_white242: byte-at-a-time "are all 524288 bytes 0xFF?" scan
 *     (~45 ms/frame, inlined into material_preflight221 and called from road242);
 *   - font348 inline HUD path: per-pixel 4-byte memcpy lane split/join of the
 *     640x480 interleaved pair (~20 ms/frame) and a 5 MiB malloc/free per call.
 * Replacements compute the same bytes/answers with SSE2 (x64 baseline).
 * Runtime switch DRIVING_CPU448: unset/0 = original code, 1 = new code,
 * 2 = verify (run BOTH, compare, count, and always use the ORIGINAL result).
 * No caching of content, no skipped check, no change of what is read or written.
 */
#ifndef DRIVING_CPU448_H
#define DRIVING_CPU448_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emmintrin.h>
#ifdef _WIN32
#include <windows.h>
#endif

static __inline int cpu448_mode(void)
{
    static int mode = -1;
    if (mode < 0) {
#ifdef _WIN32
        DWORD saved = GetLastError();
#endif
        const char *v = getenv("DRIVING_CPU448");
        int m = v && !strcmp(v, "1") ? 1 : v && !strcmp(v, "2") ? 2 : 0;
        if (m) fprintf(stderr, "[CPU448] mode=%d (%s) white-scan and font-lane loops\n", m, m == 2 ? "verify, original results used" : "new loops");
        mode = m;
#ifdef _WIN32
        SetLastError(saved);
#endif
    }
    return mode;
}

/* 1 when every byte of p[0..n) equals 0xFF, else 0. Reads only p[0..n).
 * Checks in 1 KiB blocks: may read up to one block past the first non-0xFF
 * byte (still inside the buffer) before answering 0. */
static __inline int cpu448_all_ff(const uint8_t *p, size_t n)
{
    const __m128i ones = _mm_set1_epi8((char)0xFF);
    size_t i = 0;
    for (; i + 1024 <= n; i += 1024) {
        __m128i acc = ones;
        for (size_t k = 0; k < 1024; k += 64) {
            __m128i a = _mm_and_si128(_mm_loadu_si128((const __m128i *)(p + i + k)), _mm_loadu_si128((const __m128i *)(p + i + k + 16)));
            __m128i b = _mm_and_si128(_mm_loadu_si128((const __m128i *)(p + i + k + 32)), _mm_loadu_si128((const __m128i *)(p + i + k + 48)));
            acc = _mm_and_si128(acc, _mm_and_si128(a, b));
        }
        if (_mm_movemask_epi8(_mm_cmpeq_epi8(acc, ones)) != 0xFFFF) return 0;
    }
    for (; i < n; i++) if (p[i] != 255) return 0;
    return 1;
}

/* Original loop, kept verbatim for mode 0 and for verify mode. */
static __inline int cpu448_all_ff_original(const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++) if (p[i] != 255) return 0;
    return 1;
}

static struct { long long white_calls, white_true, white_mismatch, font_calls, font_mismatch; } cpu448_counts;
static __inline void cpu448_report(void)
{
    long long c = cpu448_counts.white_calls + cpu448_counts.font_calls;
    if (c == 1 || !(c & 0xFFFF))
        fprintf(stderr, "[CPU448] mode=%d file=%s white_calls=%lld white_true=%lld white_mismatch=%lld font_calls=%lld font_mismatch=%lld\n",
                cpu448_mode(), __FILE__, cpu448_counts.white_calls, cpu448_counts.white_true, cpu448_counts.white_mismatch,
                cpu448_counts.font_calls, cpu448_counts.font_mismatch);
}

static __inline int cpu448_white(const uint8_t *p, size_t n)
{
    int mode = cpu448_mode();
    if (!mode) return cpu448_all_ff_original(p, n);
    int fast = cpu448_all_ff(p, n);
    if (mode == 2) {
        int slow = cpu448_all_ff_original(p, n);
        cpu448_counts.white_calls++; cpu448_counts.white_true += slow;
        if (fast != slow) {
            cpu448_counts.white_mismatch++;
            fprintf(stderr, "[CPU448] WHITE MISMATCH data=%p fast=%d original=%d\n", (const void *)p, fast, slow);
        }
        cpu448_report();
        return slow;
    }
    return fast;
}

/* Deinterleave `pixels` 8-byte pairs: out0[i] = dword 2i, out1[i] = dword 2i+1. */
static __inline void cpu448_split(uint8_t *out0, uint8_t *out1, const uint8_t *in, size_t pixels)
{
    size_t i = 0;
    for (; i + 4 <= pixels; i += 4) {
        __m128i a = _mm_loadu_si128((const __m128i *)(in + i * 8));        /* d0 d1 d2 d3 */
        __m128i b = _mm_loadu_si128((const __m128i *)(in + i * 8 + 16));   /* d4 d5 d6 d7 */
        a = _mm_shuffle_epi32(a, _MM_SHUFFLE(3, 1, 2, 0));                /* d0 d2 d1 d3 */
        b = _mm_shuffle_epi32(b, _MM_SHUFFLE(3, 1, 2, 0));                /* d4 d6 d5 d7 */
        _mm_storeu_si128((__m128i *)(out0 + i * 4), _mm_unpacklo_epi64(a, b)); /* d0 d2 d4 d6 */
        _mm_storeu_si128((__m128i *)(out1 + i * 4), _mm_unpackhi_epi64(a, b)); /* d1 d3 d5 d7 */
    }
    for (; i < pixels; i++) { memcpy(out0 + i * 4, in + i * 8, 4); memcpy(out1 + i * 4, in + i * 8 + 4, 4); }
}

/* Interleave: out dword 2i = in0[i], dword 2i+1 = in1[i]. */
static __inline void cpu448_join(uint8_t *out, const uint8_t *in0, const uint8_t *in1, size_t pixels)
{
    size_t i = 0;
    for (; i + 4 <= pixels; i += 4) {
        __m128i a = _mm_loadu_si128((const __m128i *)(in0 + i * 4));
        __m128i b = _mm_loadu_si128((const __m128i *)(in1 + i * 4));
        _mm_storeu_si128((__m128i *)(out + i * 8), _mm_unpacklo_epi32(a, b));
        _mm_storeu_si128((__m128i *)(out + i * 8 + 16), _mm_unpackhi_epi32(a, b));
    }
    for (; i < pixels; i++) { memcpy(out + i * 8, in0 + i * 4, 4); memcpy(out + i * 8 + 4, in1 + i * 4, 4); }
}
#endif
