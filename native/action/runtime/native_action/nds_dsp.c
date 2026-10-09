/* Copied from nightfire-driving-native runtime/lean/lean_audio_dsp.c (names prefixed nds_). */
/* Native audio DSP helpers (lean build), our own code. Used by lean_dsound.c when
 * LEAN_AUDIO_OWN_DSP=1 (off by default; the APU-file helpers stay in use until the
 * equivalence test, tools/audio_dsp_equiv.c, has been reviewed).
 *
 * Xbox ADPCM block: per channel a 4-byte header (int16 first sample, uint8 step index,
 * reserved byte that must be 0), then the channels' data interleaved in 4-byte groups;
 * each group is 8 nibbles (low nibble first) for one channel. Standard IMA ADPCM step
 * and index-adjust tables. Output is interleaved frames: out[frame * channels + ch].
 *
 * Filter: two-integrator state-variable low-pass (Chamberlin form) with the input scaled
 * by sqrt(q/2 + 0.01) and a cubic soft limit on the band-pass state. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>

static const int16_t ima_step[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};
static const int8_t ima_index_adjust[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

typedef struct { int32_t predictor; int index; } ima_state;

static int16_t ima_expand(ima_state *s, unsigned nibble)
{
    int step = ima_step[s->index];
    int diff = step >> 3;
    if (nibble & 4) diff += step;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 1) diff += step >> 2;
    s->predictor += (nibble & 8) ? -diff : diff;
    if (s->predictor > 32767) s->predictor = 32767;
    else if (s->predictor < -32768) s->predictor = -32768;
    s->index += ima_index_adjust[nibble & 7];
    if (s->index < 0) s->index = 0;
    else if (s->index > 88) s->index = 88;
    return (int16_t)s->predictor;
}

/* Returns the number of frames decoded, or 0 for a short block or a bad header. */
int nds_own_adpcm_decode_block(int16_t *out, const uint8_t *in, size_t n, int channels)
{
    ima_state st[2];
    if (channels < 1 || channels > 2 || n < (size_t)channels * 4) return 0;
    for (int ch = 0; ch < channels; ch++, in += 4, n -= 4) {
        if (in[2] > 88 || in[3] != 0) return 0;
        st[ch].predictor = (int16_t)(in[0] | (in[1] << 8));
        st[ch].index = in[2];
        out[ch] = (int16_t)st[ch].predictor;
    }
    size_t groups = n / ((size_t)channels * 4);
    for (size_t g = 0; g < groups; g++) {
        for (int ch = 0; ch < channels; ch++, in += 4) {
            int16_t *o = out + (1 + g * 8) * channels + ch;
            for (int b = 0; b < 4; b++) {
                o[(2 * b) * channels]     = ima_expand(&st[ch], in[b] & 0x0F);
                o[(2 * b + 1) * channels] = ima_expand(&st[ch], in[b] >> 4);
            }
        }
    }
    return (int)(1 + groups * 8);
}

typedef struct { float fc, q, gain, low, band; } own_svf;

void *nds_own_svf_new(void) { return calloc(1, sizeof(own_svf)); }

void nds_own_svf_setup(void *p, float fc, float q)
{
    own_svf *f = (own_svf *)p;
    f->fc = fc;
    f->q = q;
    f->gain = (float)sqrt(q / 2.0 + 0.01);
}

float nds_own_svf_run(void *p, float x)
{
    own_svf *f = (own_svf *)p;
    float in = f->gain * x;
    f->band = f->band - f->band * f->band * f->band * 0.001f;
    float high = in - f->low - f->q * f->band;
    f->band = f->band + f->fc * high;
    f->low = f->low + f->fc * f->band;
    return f->low;
}
