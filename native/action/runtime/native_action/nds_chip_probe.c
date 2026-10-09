/* NIGHTFIRE_APU_VOICE_DUMP=1 (diagnostic, emulated-chip runs only): reference data
 * for native sound. Every half second, prints each voice the emulated voice processor
 * mixed since the last print: voice index (0-63 are the 3D/HRTF voices), stereo, the
 * eight bin/attenuation pairs DirectSound programmed (12-bit, 1/64 dB; 0xFFF silent),
 * and the playback rate. Once, prints the submix and HRTF headroom
 * and the HRTF submix bins. Nothing is changed in the APU. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "apu.h"
#include "apu_state.h"
#include "apu_debug.h"

extern MCPXAPUState *g_apu_state;

static DWORD WINAPI probe(LPVOID p)
{
    (void)p;
    int once = 0;
    for (unsigned tick = 0;; tick++) {
        Sleep(500);
        MCPXAPUState *d = g_apu_state;
        if (!d) continue;
        if (!once) {
            char line[256]; int n = snprintf(line, sizeof line, "[APU-PROBE] hrtf headroom %u, hrtf submix bins %u %u %u %u, submix headroom",
                                             d->vp.hrtf_headroom, d->vp.hrtf_submix[0], d->vp.hrtf_submix[1], d->vp.hrtf_submix[2], d->vp.hrtf_submix[3]);
            for (int b = 0; b < 32 && n < 240; b++) n += snprintf(line + n, sizeof line - n, " %u", d->vp.submix_headroom[b]);
            fprintf(stderr, "%s\n", line);
            if (tick > 20) once = 1;   /* repeat for the first ten seconds, until DirectSound has set them */
        }
        for (int v = 0; v < 256; v++) {
            struct McpxApuDebugVoice *x = &g_dbg.vp.v[v];
            if (!x->active) continue;
            x->active = false;
            if (x->paused) continue;
            char line[320]; int n = snprintf(line, sizeof line, "[APU-VOICE] t=%u v=%d %s rate=%.4f bins",
                                             tick, v, x->stereo ? "st" : "mo", x->rate);
            for (int b = 0; b < 8; b++) n += snprintf(line + n, sizeof line - n, " %u:%03X", x->bin[b], x->vol[b]);
            fprintf(stderr, "%s\n", line);
        }
    }
}

void nds_chip_probe_start(void)
{
    const char *e = getenv("NIGHTFIRE_APU_VOICE_DUMP");
    if (!e || e[0] != '1') return;
    HANDLE h = CreateThread(NULL, 0, probe, NULL, 0, NULL);
    if (h) CloseHandle(h);
    fprintf(stderr, "[APU-PROBE] voice dump on (NIGHTFIRE_APU_VOICE_DUMP)\n");
}
