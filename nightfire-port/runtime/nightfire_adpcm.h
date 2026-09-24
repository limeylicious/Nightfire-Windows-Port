/*
 * MCPX APU State Structures (adapted from xemu)
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2018-2019 Jannik Vogel
 * Copyright (c) 2019-2025 Matt Borgerson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
/* Decoder extracted unchanged from pinned xboxrecomp apu_state.h. */
#ifndef NIGHTFIRE_ADPCM_H
#define NIGHTFIRE_ADPCM_H
#include <stdint.h>
#include <stddef.h>
static inline int adpcm_decode_block(int16_t *outbuf, const uint8_t *inbuf,
                                      size_t inbufsize, int channels) {
    #define ADPCM_CLIP(data, mn, mx) \
        if ((data) > (mx)) data = mx; \
        else if ((data) < (mn)) data = mn;

    static const uint16_t step_table[89] = {
        7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,
        34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,
        157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,
        724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,
        3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,
        15289,16818,18500,20350,22385,24623,27086,29794,32767
    };
    static const int index_table[] = { -1, -1, -1, -1, 2, 4, 6, 8 };

    int samples = 1, chunks;
    int32_t pcmdata[2];
    int8_t index[2];

    if (inbufsize < (uint32_t)channels * 4) return 0;

    for (int ch = 0; ch < channels; ch++) {
        *outbuf++ = pcmdata[ch] = (int16_t)(inbuf[0] | (inbuf[1] << 8));
        index[ch] = inbuf[2];
        if (index[ch] < 0 || index[ch] > 88 || inbuf[3]) return 0;
        inbufsize -= 4;
        inbuf += 4;
    }

    chunks = (int)(inbufsize / (channels * 4));
    samples += chunks * 8;

    while (chunks--) {
        for (int ch = 0; ch < channels; ++ch) {
            for (int i = 0; i < 4; ++i) {
                int step = step_table[index[ch]], delta = step >> 3;
                if (*inbuf & 1) delta += (step >> 2);
                if (*inbuf & 2) delta += (step >> 1);
                if (*inbuf & 4) delta += step;
                if (*inbuf & 8) delta = -delta;
                pcmdata[ch] += delta;
                index[ch] += index_table[*inbuf & 0x7];
                ADPCM_CLIP(index[ch], 0, 88);
                ADPCM_CLIP(pcmdata[ch], -32768, 32767);
                outbuf[i * 2 * channels] = (int16_t)pcmdata[ch];

                step = step_table[index[ch]]; delta = step >> 3;
                if (*inbuf & 0x10) delta += (step >> 2);
                if (*inbuf & 0x20) delta += (step >> 1);
                if (*inbuf & 0x40) delta += step;
                if (*inbuf & 0x80) delta = -delta;
                pcmdata[ch] += delta;
                index[ch] += index_table[(*inbuf >> 4) & 0x7];
                ADPCM_CLIP(index[ch], 0, 88);
                ADPCM_CLIP(pcmdata[ch], -32768, 32767);
                outbuf[(i * 2 + 1) * channels] = (int16_t)pcmdata[ch];
                inbuf++;
            }
            outbuf++;
        }
        outbuf += channels * 7;
    }
    #undef ADPCM_CLIP
    return samples;
}

#endif
