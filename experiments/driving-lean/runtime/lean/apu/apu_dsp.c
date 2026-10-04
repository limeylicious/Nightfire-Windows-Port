/*
 * MCPX APU DSP (GP/EP) - Stub implementation
 *
 * The DSP Global Processor (GP) and Encode Processor (EP) handle effects
 * processing (reverb, chorus, etc.) and final output encoding. The full
 * DSP is ~3000 lines of DSP56300 emulation code.
 *
 * For initial audio, we bypass the DSP entirely:
 * - VP mixbins are passed directly to the EP output
 * - GP effects processing is skipped
 * - The EP just copies mixbin 0/1 (front L/R) to the monitor buffer
 *
 * This gives us basic voice playback without effects. The DSP can be
 * connected later for reverb, EQ, and other processing.
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2019-2025 Matt Borgerson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include "apu_state.h"
#include "fpconv.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "apu_debug.h"
#include <string.h>

/* ── DSP command doorbell acknowledgement ────────────────────────────────
 *
 * DirectSound does not stop at creating the device. It hands the audio DSP a
 * command block in guest RAM, writes a command word, and spins until the DSP
 * writes zero back. On real hardware the GP runs a DSP56300 program that does
 * that. Here the DSP is a passthrough stub, so the word never changes and the
 * title hangs inside DirectSound initialisation -- which on Wreckless gates the
 * entire engine, not just audio.
 *
 * RECOMP_APU_DSP_ACK=<addr>[,<addr>...] clears those guest dwords once per APU
 * frame, which is what "the command completed" looks like to the title.
 *
 * ponytail: this is a handshake acknowledgement, not a DSP. It says every
 * command succeeded instantly and computes nothing, so anything whose *result*
 * the title reads back will still be wrong. The real fix is DSP56300 emulation
 * in the GP/EP; this exists so audio init stops blocking everything behind it.
 *
 * The address is not derivable from the APU registers: GPSADDR/GPFADDR/
 * EPSADDR/EPFADDR point at the DSP's own scratch and frame memory, while the
 * command block is a DirectSound heap allocation. On Wreckless the registers
 * read 0x01504000 / 0x014EC000 / 0x0151C000 / 0x014F0000 and the doorbell is at
 * 0x014F8810 -- inside none of them. So it has to be observed: run with
 * RECOMP_WATCHDOG_SECS and the spin shows up as ebx plus the poll offset.
 */
#define APU_DSP_ACK_MAX 8
static uint32_t s_dsp_ack[APU_DSP_ACK_MAX];
static int s_dsp_ack_count = -1;

static void dsp_ack_init(void)
{
    const char *spec = getenv("RECOMP_APU_DSP_ACK");
    char buf[256], *p, *end;

    s_dsp_ack_count = 0;
    if (!spec || !*spec)
        return;
    strncpy(buf, spec, sizeof buf - 1);
    buf[sizeof buf - 1] = 0;
    for (p = buf; *p && s_dsp_ack_count < APU_DSP_ACK_MAX; ) {
        unsigned long v = strtoul(p, &end, 0);
        if (end == p)
            break;
        if (v)
            s_dsp_ack[s_dsp_ack_count++] = (uint32_t)v;
        p = (*end == ',') ? end + 1 : end;
    }
    if (s_dsp_ack_count)
        fprintf(stderr, "[APU] DSP doorbell ack: %d address(es), first 0x%08X\n",
                s_dsp_ack_count, s_dsp_ack[0]);
}

static void dsp_ack_frame(MCPXAPUState *d)
{
    int i;

    if (s_dsp_ack_count < 0)
        dsp_ack_init();
    if (!d->ram_ptr)
        return;
    for (i = 0; i < s_dsp_ack_count; i++) {
        uint32_t *slot = (uint32_t *)(d->ram_ptr + s_dsp_ack[i]);
        if (*slot) {
            static int shown[APU_DSP_ACK_MAX];
            if (shown[i]++ < 3)
                fprintf(stderr, "[APU] DSP doorbell 0x%08X: command 0x%08X"
                                " acknowledged\n", s_dsp_ack[i], *slot);
            *slot = 0;
        }
    }
}

void mcpx_apu_dsp_init(MCPXAPUState *d)
{
    /* Allocate minimal DSP state for GP and EP.
     * We need these to exist so reset doesn't crash,
     * but they won't actually run DSP programs. */
    d->gp.dsp = (DSPState *)calloc(1, sizeof(DSPState));
    d->ep.dsp = (DSPState *)calloc(1, sizeof(DSPState));

    if (d->gp.dsp) d->gp.dsp->is_gp = true;
    if (d->ep.dsp) d->ep.dsp->is_gp = false;

    d->gp.realtime = false;
    d->ep.realtime = false;

    fprintf(stderr, "[APU] DSP GP/EP initialized (STUBBED - passthrough mode)\n");
}

void mcpx_apu_update_dsp_preference(MCPXAPUState *d)
{
    /* In the real xemu, this reads settings to decide whether
     * GP/EP should run in realtime or cached mode. We ignore it. */
    (void)d;
}

void mcpx_apu_dsp_frame(MCPXAPUState *d,
                         float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME])
{
    /* Bypass DSP: take mixbin 0 (front-left) and mixbin 1 (front-right)
     * and write them directly to the monitor frame buffer as the final
     * EP output.
     *
     * The Xbox DirectSound typically routes:
     *   Mixbin 0 = Front Left
     *   Mixbin 1 = Front Right
     *   Mixbin 2 = Center (often unused in stereo)
     *   Mixbin 3 = LFE
     *   Mixbin 4-5 = Rear L/R
     *
     * For stereo output, bins 0 and 1 are what we want.
     */

    int off = (d->ep_frame_div % 8) * NUM_SAMPLES_PER_FRAME;
    /* LEAN: once-per-second census of live voices and mixbin energy
     * (LEAN_AUDIO_CENSUS=0 disables). */
    {
        static int frames, on = -1, maxv, peakv;
        static double bin_e[NUM_MIXBINS];
        if (on < 0) { const char *e = getenv("LEAN_AUDIO_CENSUS"); on = !(e && e[0] == '0'); }
        if (on) {
            int nv = 0;
            for (int v = 0; v < 256; v++) if (g_dbg.vp.v[v].active) { nv++; peakv = v; }
            if (nv > maxv) maxv = nv;
            for (int b = 0; b < NUM_MIXBINS; b++)
                for (int i = 0; i < NUM_SAMPLES_PER_FRAME; i++)
                    bin_e[b] += (double)mixbins[b][i] * mixbins[b][i];
            if (++frames >= 1500) {
                char line[1024]; int n = 0;
                for (int b = 0; b < NUM_MIXBINS; b++)
                    if (bin_e[b] > 0) n += snprintf(line + n, sizeof(line) - n, " b%d=%.4f", b, sqrt(bin_e[b] / (1500.0 * NUM_SAMPLES_PER_FRAME)));
                line[n] = 0;
                fprintf(stderr, "[LEAN-VP] voices max=%d last=%d hrtf_submix=%d,%d,%d,%d mon=%d bins:%s\n", maxv, peakv,
                        d->vp.hrtf_submix[0], d->vp.hrtf_submix[1], d->vp.hrtf_submix[2], d->vp.hrtf_submix[3], (int)d->monitor.point, n ? line : " none");
                for (int v = 0, shown = 0; v < 256 && shown < 3; v++) {
                    struct McpxApuDebugVoice *x = &g_dbg.vp.v[v];
                    if (!x->active) continue;
                    shown++;
                    fprintf(stderr, "[LEAN-VP]   v%d paused=%d stereo=%d mp=%d rate=%.3f bins=%d,%d,%d,%d,%d,%d,%d,%d vol=%03X,%03X,%03X,%03X,%03X,%03X\n", v, x->paused, x->stereo, x->multipass, x->rate,
                            x->bin[0], x->bin[1], x->bin[2], x->bin[3], x->bin[4], x->bin[5], x->bin[6], x->bin[7],
                            x->vol[0], x->vol[1], x->vol[2], x->vol[3], x->vol[4], x->vol[5]);
                }
                frames = maxv = 0; memset(bin_e, 0, sizeof(bin_e));
            }
        }
    }


    dsp_ack_frame(d);

    if (d->monitor.point != MCPX_APU_DEBUG_MON_VP) {
        for (int i = 0; i < NUM_SAMPLES_PER_FRAME; i++) {
            /* Clamp to [-1, 1] range */
            /* LEAN: fold the DirectSound speaker mixbins (0 FL, 1 FR, 2 C,
             * 4 BL, 5 BR) and the crosstalk bins the HRTF submixes feed
             * (6 XTLK FL, 7 XTLK FR, 8 XTLK BL, 9 XTLK BR) to stereo. The GP
             * DSP would normally mix those down; LFE, I3DL2 reverb and FX
             * sends (3, 10+) need DSP effects and are left out. */
            float left = mixbins[0][i] + mixbins[6][i] + 0.7071f * (mixbins[2][i] + mixbins[4][i] + mixbins[8][i]);
            float right = mixbins[1][i] + mixbins[7][i] + 0.7071f * (mixbins[2][i] + mixbins[5][i] + mixbins[9][i]);
            if (left > 1.0f) left = 1.0f;
            if (left < -1.0f) left = -1.0f;
            if (right > 1.0f) right = 1.0f;
            if (right < -1.0f) right = -1.0f;

            /* Convert to 16-bit and write (not accumulate) into frame buffer.
             * Each of the 8 sub-frames writes its own 32-sample slice. */
            d->monitor.frame_buf[off + i][0] = (int16_t)(left * 32767.0f);
            d->monitor.frame_buf[off + i][1] = (int16_t)(right * 32767.0f);
        }
    }

    g_dbg.gp.cycles = 0;
    g_dbg.ep.cycles = 0;
}
