/* LEAN_AUDIO_NATIVE=1 (off by default): native audio for the Driving build.
 *
 * The game talks to its statically linked Xbox DirectSound library through 19
 * XDK C-API wrappers (surveyed 2026-10-06). With this switch on, those wrappers
 * call the functions below instead (one-line prologues in
 * src/recomp/gen/recomp_0017.c / recomp_0018.c), so the DSOUND internals, the
 * MCPX front end / voice processor emulation and the APU frame thread never run:
 * DirectSound init is what starts the APU, and it no longer happens.
 *
 * Sound is mixed here: every DirectSound buffer is a voice reading its sample
 * data (mono PCM16 or Xbox ADPCM) straight from guest memory, resampled to
 * 48 kHz, scaled by its volume and spread over its mixbins (7 bin/volume pairs),
 * then the speaker bins are folded to stereo exactly as the chip path does
 * (apu_dsp.c) and handed to XAudio2 in small blocks, paced by the XAudio2 queue.
 * Buffer positions advance per mixed block, so GetCurrentPosition reports what
 * we have actually mixed: the game's own software mixer (six looping output
 * buffers) sees a cursor that moves smoothly by at most a few blocks.
 *
 * Send bin 0xB (reverb send) is accumulated but not yet played: I3DL2 reverb via
 * XAudio2's reverb effect is phase 2. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
int lean_adpcm_decode_block(int16_t *out, const uint8_t *in, size_t n, int channels);   /* apu_vp.c */
void *lean_svf_new(void); void lean_svf_setup(void *f, float fc, float q); float lean_svf_run(void *f, float x);   /* apu_vp.c */
/* LEAN_AUDIO_OWN_DSP=1 (off by default): our own decoder and filter (lean_audio_dsp.c) instead of
 * the APU-file helpers above; bit-exact on the tools/audio_dsp_equiv.c sweep (2026-10-07). */
int lean_own_adpcm_decode_block(int16_t *out, const uint8_t *in, size_t n, int channels);
void *lean_own_svf_new(void); void lean_own_svf_setup(void *f, float fc, float q); float lean_own_svf_run(void *f, float x);
static int own_dsp(void)
{
    static int on = -1;
    if (on < 0) { const char *e = getenv("LEAN_AUDIO_OWN_DSP"); on = e && e[0] == '1';
        if (on) fprintf(stderr, "[LEAN-NATIVE] own ADPCM decoder and filter (LEAN_AUDIO_OWN_DSP)\n"); }
    return on;
}
static int nat_adpcm(int16_t *out, const uint8_t *in, size_t n, int ch) { return own_dsp() ? lean_own_adpcm_decode_block(out, in, n, ch) : lean_adpcm_decode_block(out, in, n, ch); }
static void *nat_svf_new(void) { return own_dsp() ? lean_own_svf_new() : lean_svf_new(); }
static void nat_svf_setup(void *f, float fc, float q) { if (own_dsp()) lean_own_svf_setup(f, fc, q); else lean_svf_setup(f, fc, q); }
static float nat_svf_run(void *f, float x) { return own_dsp() ? lean_own_svf_run(f, x) : lean_svf_run(f, x); }

extern ptrdiff_t g_xbox_mem_offset;
uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment);
int xa2_is_active(void);
int xa2_submit_samples(const int16_t *samples, int num_samples);
int xa2_queued_now(void);

#define G8(a)  (*(volatile uint8_t  *)(g_xbox_mem_offset + (uintptr_t)(a)))
#define G16(a) (*(volatile uint16_t *)(g_xbox_mem_offset + (uintptr_t)(a)))
#define G32(a) (*(volatile uint32_t *)(g_xbox_mem_offset + (uintptr_t)(a)))

#define DS_OK            0u
#define DSERR_GENERIC    0x80004005u
#define DSERR_OUTOFMEM   0x8007000Eu

#define NAT_RATE         48000
#define NAT_BLOCK        128           /* samples per mixed block (2.67 ms) */
#define NAT_QUEUE        8             /* XAudio2 blocks queued (~21 ms) */
#define NAT_MAX_VOICES   256
#define NAT_BINS         32

int lean_audio_native = -1;
int lean_audio_native_on(void)
{
    if (lean_audio_native < 0) {
        const char *e = getenv("LEAN_AUDIO_NATIVE");
        lean_audio_native = e && e[0] == '1';
        if (lean_audio_native) fprintf(stderr, "[LEAN-NATIVE] native DirectSound replacement on\n");
    }
    return lean_audio_native;
}

/* LEAN_AUDIO_NO_CHIP=1 (with LEAN_AUDIO_NATIVE=1; off by default): don't create the emulated
 * MCPX APU at all (src/main.c). Any later access to its registers, the DSP start-up handshake
 * or the AC97 reset is logged as [LEAN-NOCHIP], so we can see whether anything still needs it. */
int lean_audio_no_chip(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("LEAN_AUDIO_NO_CHIP");
        on = e && e[0] == '1' && lean_audio_native_on();
        if (e && e[0] == '1' && !on) fprintf(stderr, "[LEAN-NOCHIP] ignored: needs LEAN_AUDIO_NATIVE=1\n");
        if (on) fprintf(stderr, "[LEAN-NOCHIP] emulated APU/DSP/AC97 not started (native audio only)\n");
    }
    return on;
}

typedef struct {
    uint32_t handle;            /* guest interface pointer handed to the game (0 = free) */
    int      adpcm;             /* Xbox ADPCM (tag 0x69) else PCM16 */
    int      channels;
    uint32_t rate;              /* format rate */
    uint32_t freq;              /* playback frequency, Hz */
    uint32_t flags;             /* DSBUFFERDESC flags */
    uint32_t data, bytes;       /* sample data in guest memory */
    uint32_t loop_start, loop_len; /* bytes */
    double   pos;               /* current position, in samples */
    int      playing, looping;
    int32_t  lvol;              /* SetVolume, mB */
    int32_t  headroom;          /* mB: 600 for 2D buffers, 0 for 3D (XDK 0x17B465) */
    int      nbins;
    uint32_t bin[8];
    int32_t  binmb[8];          /* mixbin volumes, mB */
    int      dec_block;         /* cached ADPCM block index */
    int16_t  dec[65 * 2];
    uint32_t serial;
    int      is_output;         /* created by IDirectSound_CreateSoundBuffer: one of the six output buffers */
    int      lpf;               /* SetFilter: low-pass on (as the voice processor decides it) */
    float    fc, q;             /* filter cutoff / resonance, voice-processor units */
    void    *svf;               /* filter state (voice processor's own state-variable filter) */
} nat_voice;

static nat_voice s_v[NAT_MAX_VOICES];
static CRITICAL_SECTION s_lock;
static INIT_ONCE s_once = INIT_ONCE_STATIC_INIT;
static volatile LONG s_thread_started;
static uint32_t s_ds_handle;
static float s_master = 1.0f;
static volatile LONG64 s_mixed_blocks;

/* XDK applies volume as attenuation = headroom - lVolume - binVolume (mB) and
 * programs min(0xFFF, att * 64 / 100) with an unsigned divide, so a negative
 * attenuation wraps to silence. The voice processor then divides each bin by
 * 1 << submix headroom (s_submix_hr, programmed by DirectSound at init). */
/* Per-bin submix headroom DirectSound programs at init (read from the chip path,
 * [LEAN-HEADROOM] log 2026-10-06): 1 on bins 0-30, 0 on bin 31. */
static int s_submix_hr[32] = { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0 };
static float nat_gain(int32_t headroom, int32_t lvol, int32_t binmb, uint32_t bin)
{
    int32_t att = headroom - lvol - binmb;
    if (att < 0) return 0.0f;
    uint32_t q = (uint32_t)att * 64u / 100u;
    if (q >= 0xFFF) return 0.0f;
    return powf(10.0f, (float)q / (64.0f * -20.0f)) / (float)(1 << s_submix_hr[bin & 31]);
}

static BOOL CALLBACK nat_init(PINIT_ONCE o, PVOID p, PVOID *c)
{
    (void)o; (void)p; (void)c;
    InitializeCriticalSectionAndSpinCount(&s_lock, 4000);
    const char *g = getenv("LEAN_AUDIO_NATIVE_GAIN");
    if (g && *g) s_master = (float)atof(g);
    /* XAudio2 output: the emulated chip starts it when present; start it here too, so
     * native audio also works without the chip (LEAN_AUDIO_NO_CHIP). No-op if already up. */
    { extern int xa2_init(void); if (!xa2_init()) fprintf(stderr, "[LEAN-NATIVE] XAudio2 output could not be started\n"); }
    return TRUE;
}
static void nat_lock(void)   { InitOnceExecuteOnce(&s_once, nat_init, NULL, NULL); EnterCriticalSection(&s_lock); }
static void nat_unlock(void) { LeaveCriticalSection(&s_lock); }

static nat_voice *nat_find(uint32_t handle)
{
    if (!handle) return NULL;
    for (int i = 0; i < NAT_MAX_VOICES; i++) if (s_v[i].handle == handle) return &s_v[i];
    return NULL;
}

/* ---- sample fetch ------------------------------------------------------- */

static uint32_t nat_total_samples(const nat_voice *v)
{
    if (v->adpcm) return (v->bytes / (36u * v->channels)) * 64u;
    return v->bytes / (2u * v->channels);
}
static uint32_t nat_bytes_to_samples(const nat_voice *v, uint32_t b)
{
    if (v->adpcm) return (b / (36u * v->channels)) * 64u + ((b % (36u * v->channels)) ? 0u : 0u);
    return b / (2u * v->channels);
}
static uint32_t nat_samples_to_bytes(const nat_voice *v, uint32_t s)
{
    if (v->adpcm) return (s / 64u) * 36u * v->channels;
    return s * 2u * v->channels;
}

static float nat_sample(nat_voice *v, uint32_t idx)   /* mono mixdown of one sample */
{
    if (!v->data) return 0.0f;
    if (v->adpcm) {
        int blk = (int)(idx / 64u), off = (int)(idx % 64u);
        if (blk != v->dec_block) {
            uint8_t raw[72];
            uint32_t bsz = 36u * v->channels, a = v->data + (uint32_t)blk * bsz;
            for (uint32_t k = 0; k < bsz; k++) raw[k] = G8(a + k);
            if (!nat_adpcm(v->dec, raw, bsz, v->channels)) memset(v->dec, 0, sizeof v->dec);
            v->dec_block = blk;
        }
        if (v->channels == 2) return (v->dec[off * 2] + v->dec[off * 2 + 1]) * (0.5f / 32768.0f);
        return v->dec[off] * (1.0f / 32768.0f);
    }
    uint32_t a = v->data + idx * 2u * v->channels;
    if (v->channels == 2) return ((int16_t)G16(a) + (int16_t)G16(a + 2)) * (0.5f / 32768.0f);
    return (int16_t)G16(a) * (1.0f / 32768.0f);
}

/* ---- mixer ---------------------------------------------------------------- */

static float s_bins[NAT_BINS][NAT_BLOCK];
static long s_mixed, s_skipped;   /* voices per 5 s: mixed vs silent-skipped */
static double s_cpu_us, s_cpu_max; static long s_cpu_blocks;

static void nat_mix_voice(nat_voice *v)
{
    uint32_t total = nat_total_samples(v);
    if (!total) { v->playing = 0; return; }
    uint32_t lstart = v->looping ? nat_bytes_to_samples(v, v->loop_start) : 0;
    uint32_t lend = (v->looping && v->loop_len) ? lstart + nat_bytes_to_samples(v, v->loop_len) : total;
    if (lend > total || lend <= lstart) lend = total;
    double step = (double)(v->freq ? v->freq : v->rate) / NAT_RATE;
    float g[8]; int nb = v->nbins, audible = 0;
    for (int b = 0; b < nb; b++) { g[b] = nat_gain(v->headroom, v->lvol, v->binmb[b], v->bin[b]) * s_master; if (g[b] > 0.0f) audible = 1; }
    if (!audible) {   /* silent: advance time exactly (loop/end), skip decode, filter and mix */
        s_skipped++;
        double end = v->pos + step * NAT_BLOCK;
        if (end >= lend) {
            if (v->looping) v->pos = lstart + fmod(end - lend, (double)(lend - lstart));
            else { v->playing = 0; v->pos = total; }
        } else v->pos = end;
        return;
    }
    s_mixed++;
    for (int i = 0; i < NAT_BLOCK; i++) {
        uint32_t i0 = (uint32_t)v->pos;
        if (i0 >= lend) {
            if (v->looping) { v->pos = lstart + fmod(v->pos - lend, (double)(lend - lstart)); i0 = (uint32_t)v->pos; }
            else { v->playing = 0; v->pos = total; return; }
        }
        uint32_t i1 = i0 + 1;
        if (i1 >= lend) i1 = v->looping ? lstart : i0;
        float f = (float)(v->pos - i0);
        float s = nat_sample(v, i0) * (1.0f - f) + (f > 0.0f ? nat_sample(v, i1) * f : 0.0f);
        if (v->lpf && v->svf) { s = nat_svf_run(v->svf, s); s = s > 1.0f ? 1.0f : s < -1.0f ? -1.0f : s; }
        for (int b = 0; b < nb; b++) s_bins[v->bin[b] & (NAT_BINS - 1)][i] += s * g[b];
        v->pos += step;
    }
}

static DWORD WINAPI nat_thread(LPVOID p)
{
    (void)p;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    HANDLE t = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    int16_t out[NAT_BLOCK][2];
    static int16_t stat_buf[256][2]; int stat_n = 0;
    for (;;) {
        int q = xa2_queued_now();
        if (q >= NAT_QUEUE) {
            LARGE_INTEGER due; due.QuadPart = -5000;   /* 0.5 ms */
            if (t && SetWaitableTimer(t, &due, 0, NULL, NULL, FALSE)) WaitForSingleObject(t, 20); else Sleep(1);
            continue;
        }
        LARGE_INTEGER c0, c1; QueryPerformanceCounter(&c0);
        memset(s_bins, 0, sizeof s_bins);
        float out_only[NAT_BLOCK][2];
        nat_lock();
        /* the six output buffers first (music, dialogue, movies), then effect voices,
         * so their levels can be reported separately */
        for (int i = 0; i < NAT_MAX_VOICES; i++) if (s_v[i].handle && s_v[i].playing && s_v[i].is_output) nat_mix_voice(&s_v[i]);
        for (int i = 0; i < NAT_BLOCK; i++) {
            out_only[i][0] = s_bins[0][i] + s_bins[6][i] + 0.7071f * (s_bins[2][i] + s_bins[4][i] + s_bins[8][i]);
            out_only[i][1] = s_bins[1][i] + s_bins[7][i] + 0.7071f * (s_bins[2][i] + s_bins[5][i] + s_bins[9][i]);
        }
        for (int i = 0; i < NAT_MAX_VOICES; i++) if (s_v[i].handle && s_v[i].playing && !s_v[i].is_output) nat_mix_voice(&s_v[i]);
        nat_unlock();
        /* same fold as apu_dsp.c: speaker bins 0 FL, 1 FR, 2 C, 4 BL, 5 BR and the
         * crosstalk bins 6..9; LFE (3) and sends (10+) are not played. Saturate on
         * the way out (no wrap-around) and count clipped samples. */
        static double e_out, e_fx; static long clipped, frames5; static ULONGLONG t5;
        for (int i = 0; i < NAT_BLOCK; i++) {
            float l = s_bins[0][i] + s_bins[6][i] + 0.7071f * (s_bins[2][i] + s_bins[4][i] + s_bins[8][i]);
            float r = s_bins[1][i] + s_bins[7][i] + 0.7071f * (s_bins[2][i] + s_bins[5][i] + s_bins[9][i]);
            e_out += out_only[i][0] * out_only[i][0] + out_only[i][1] * out_only[i][1];
            e_fx += (l - out_only[i][0]) * (l - out_only[i][0]) + (r - out_only[i][1]) * (r - out_only[i][1]);
            if (l > 1.0f || l < -1.0f) clipped++;
            if (r > 1.0f || r < -1.0f) clipped++;
            l = l > 1.0f ? 1.0f : l < -1.0f ? -1.0f : l;
            r = r > 1.0f ? 1.0f : r < -1.0f ? -1.0f : r;
            out[i][0] = (int16_t)(l * 32767.0f); out[i][1] = (int16_t)(r * 32767.0f);
        }
        frames5 += NAT_BLOCK;
        { static LARGE_INTEGER hz; if (!hz.QuadPart) QueryPerformanceFrequency(&hz); QueryPerformanceCounter(&c1);
          double us = (c1.QuadPart - c0.QuadPart) * 1e6 / hz.QuadPart; s_cpu_us += us; if (us > s_cpu_max) s_cpu_max = us; s_cpu_blocks++; }
        if (GetTickCount64() - t5 >= 5000) {
            if (t5) {
                int nv = 0; for (int i = 0; i < NAT_MAX_VOICES; i++) if (s_v[i].handle && s_v[i].playing) nv++;
                fprintf(stderr, "[LEAN-NATIVE] 5s: rms output-buffers %.0f effects %.0f, clipped samples %ld, voices playing %d, mixer cpu per %d-sample block avg %.0f us max %.0f us (budget %.0f us), voices mixed %ld skipped %ld per block\n",
                        32767.0 * sqrt(e_out / (2.0 * frames5)), 32767.0 * sqrt(e_fx / (2.0 * frames5)), clipped, nv,
                        NAT_BLOCK, s_cpu_blocks ? s_cpu_us / s_cpu_blocks : 0.0, s_cpu_max, NAT_BLOCK * 1e6 / NAT_RATE,
                        s_cpu_blocks ? s_mixed / s_cpu_blocks : 0L, s_cpu_blocks ? s_skipped / s_cpu_blocks : 0L);
                s_cpu_us = 0; s_cpu_max = 0; s_cpu_blocks = 0; s_mixed = s_skipped = 0;
            }
            t5 = GetTickCount64(); e_out = e_fx = 0; clipped = 0; frames5 = 0;
        }
        memcpy(stat_buf[stat_n], out, sizeof out); stat_n += NAT_BLOCK;
        if (stat_n >= 256) { extern void lean_audio_stats_native(const int16_t (*buf)[2], int n); lean_audio_stats_native((const int16_t (*)[2])stat_buf, 256); stat_n = 0; }
        xa2_submit_samples(&out[0][0], NAT_BLOCK);
        InterlockedIncrement64(&s_mixed_blocks);
    }
}

static void nat_start_thread(void)
{
    if (InterlockedCompareExchange(&s_thread_started, 1, 0) == 0) {
        HANDLE h = CreateThread(NULL, 0, nat_thread, NULL, 0, NULL);
        if (h) CloseHandle(h);
        fprintf(stderr, "[LEAN-NATIVE] mixer started (%d-sample blocks, %d queued, XAudio2 %s)\n",
                NAT_BLOCK, NAT_QUEUE, xa2_is_active() ? "ready" : "NOT ready");
    }
}

/* ---- object helpers ------------------------------------------------------ */

static uint32_t nat_new_handle(void)
{
    uint32_t h = xbox_HeapAlloc(0x40, 16);
    if (h) for (uint32_t k = 0; k < 0x40; k += 4) G32(h + k) = 0;
    return h;
}

static void nat_set_mixbins(nat_voice *v, uint32_t pmix)
{
    /* DSMIXBINS { DWORD dwMixBinCount; DSMIXBINVOLUMEPAIR *pairs } ; pair { DWORD bin; LONG vol } */
    if (!pmix) return;
    uint32_t n = G32(pmix), pairs = G32(pmix + 4);
    if (n > 8) n = 8;
    v->nbins = 0;
    for (uint32_t i = 0; i < n && pairs; i++) {
        v->bin[v->nbins] = G32(pairs + i * 8);
        v->binmb[v->nbins] = (int32_t)G32(pairs + i * 8 + 4);
        v->nbins++;
    }
}

static void nat_set_mixbin_volumes(nat_voice *v, uint32_t pmix)
{
    /* update volumes of bins already routed; route new bins if absent */
    if (!pmix) return;
    uint32_t n = G32(pmix), pairs = G32(pmix + 4);
    if (n > 8) n = 8;
    for (uint32_t i = 0; i < n && pairs; i++) {
        /* SetMixBinVolumes (XDK 0x17B54F) only updates bins already routed */
        uint32_t bin = G32(pairs + i * 8);
        for (int b = 0; b < v->nbins; b++) if (v->bin[b] == bin) v->binmb[b] = (int32_t)G32(pairs + i * 8 + 4);
    }
}

static uint32_t nat_create(uint32_t pdesc, uint32_t ppbuf)
{
    /* DSBUFFERDESC { dwSize, dwFlags, dwBufferBytes, lpwfxFormat, lpMixBins, dwInputMixBin } */
    if (!pdesc || !ppbuf) return DSERR_GENERIC;
    uint32_t flags = G32(pdesc + 4), bytes = G32(pdesc + 8), wfx = G32(pdesc + 0xC), mix = G32(pdesc + 0x10);
    nat_lock();
    nat_voice *v = NULL;
    for (int i = 0; i < NAT_MAX_VOICES; i++) if (!s_v[i].handle) { v = &s_v[i]; break; }
    uint32_t h = v ? nat_new_handle() : 0;
    if (!v || !h) { nat_unlock(); return DSERR_OUTOFMEM; }
    uint32_t serial = v->serial + 1;
    void *keep_svf = v->svf;   /* filter state survives slot reuse, as on the voice processor */
    memset(v, 0, sizeof *v);
    v->svf = keep_svf;
    v->serial = serial; v->handle = h; v->flags = flags; v->dec_block = -1; v->lvol = 0;
    v->headroom = (flags & 0x10) ? 0 : 600;   /* DSBCAPS_CTRL3D buffers have no headroom */
    v->adpcm = wfx && G16(wfx) == 0x69;
    v->channels = wfx ? (G16(wfx + 2) ? G16(wfx + 2) : 1) : 1;
    v->rate = wfx ? G32(wfx + 4) : NAT_RATE;
    if (!v->rate) v->rate = NAT_RATE;
    v->freq = v->rate;
    (void)bytes; /* data arrives via SetBufferData */
    v->nbins = 2; v->bin[0] = 0; v->bin[1] = 1; v->binmb[0] = v->binmb[1] = 0;   /* default routing until mixbins arrive */
    if (mix) nat_set_mixbins(v, mix);
    nat_unlock();
    G32(ppbuf) = h;
    nat_start_thread();
    return DS_OK;
}

/* ---- the 19 entry points (called from the wrapper prologues) ------------- */

uint32_t lean_ds_DirectSoundCreate(uint32_t guid, uint32_t ppds, uint32_t unk)
{
    (void)guid; (void)unk;
    InitOnceExecuteOnce(&s_once, nat_init, NULL, NULL);
    if (!s_ds_handle) s_ds_handle = nat_new_handle();
    if (ppds) G32(ppds) = s_ds_handle;
    fprintf(stderr, "[LEAN-NATIVE] DirectSoundCreate -> %08X\n", s_ds_handle);
    return s_ds_handle ? DS_OK : DSERR_OUTOFMEM;
}
uint32_t lean_ds_DownloadEffectsImage(uint32_t ds, uint32_t img, uint32_t size, uint32_t ploc, uint32_t ppdesc)
{
    (void)ds; (void)img; (void)size; (void)ploc;
    if (ppdesc) G32(ppdesc) = 0;
    fprintf(stderr, "[LEAN-NATIVE] DownloadEffectsImage (%u bytes) accepted; effects not run yet\n", size);
    return DS_OK;
}
uint32_t lean_ds_SetI3DL2Listener(uint32_t ds, uint32_t params, uint32_t apply) { (void)ds; (void)params; (void)apply; return DS_OK; }
uint32_t lean_ds_CreateSoundBuffer(uint32_t ds, uint32_t pdesc, uint32_t ppbuf, uint32_t unk)
{
    (void)ds; (void)unk;
    uint32_t r = nat_create(pdesc, ppbuf);
    if (r == DS_OK && ppbuf) { nat_lock(); nat_voice *v = nat_find(G32(ppbuf)); if (v) v->is_output = 1; nat_unlock(); }
    return r;
}
uint32_t lean_ds_DirectSoundCreateBuffer(uint32_t pdesc, uint32_t ppbuf) { return nat_create(pdesc, ppbuf); }
uint32_t lean_ds_ReleaseDS(uint32_t ds) { (void)ds; return 0; }
uint32_t lean_ds_ReleaseBuffer(uint32_t buf)
{
    nat_lock(); nat_voice *v = nat_find(buf); if (v) { v->playing = 0; v->handle = 0; } nat_unlock();
    return 0;
}
/* Behaviour below follows the XDK library's own code (see the 2026-10-06 notes):
 * positions are bytes; ADPCM converts as floor(bytes/36)*64 samples. */
uint32_t lean_ds_Play(uint32_t buf, uint32_t r1, uint32_t r2, uint32_t flags)
{
    (void)r1; (void)r2;
    nat_lock(); nat_voice *v = nat_find(buf);
    if (v) {
        v->looping = (flags & 1) != 0;              /* already playing: only the loop mode changes */
        if (flags & 2) { v->pos = 0; v->dec_block = -1; }   /* DSBPLAY_FROMSTART */
        if (!v->playing && v->pos >= nat_total_samples(v)) v->pos = 0;  /* finished voice restarts */
        v->playing = 1;                             /* stopped: resume from the saved position */
    }
    nat_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t lean_ds_Stop(uint32_t buf)                 /* keeps the position */
{
    nat_lock(); nat_voice *v = nat_find(buf); if (v) v->playing = 0; nat_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t lean_ds_SetBufferData(uint32_t buf, uint32_t data, uint32_t bytes)
{
    nat_lock(); nat_voice *v = nat_find(buf);
    if (v && !(v->data == data && v->bytes == bytes)) {   /* same data: no-op */
        v->playing = 0; v->data = data; v->bytes = bytes; v->dec_block = -1;
        v->pos = 0; v->loop_start = 0; v->loop_len = 0;   /* stopped, position 0, loop = whole buffer */
    }
    nat_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t lean_ds_SetLoopRegion(uint32_t buf, uint32_t start, uint32_t len)
{
    nat_lock(); nat_voice *v = nat_find(buf); uint32_t r = DSERR_GENERIC;
    if (v) {
        if ((uint64_t)start + len > v->bytes) r = 0x88780032u;   /* region past the buffer */
        else { v->loop_start = start; v->loop_len = len; r = DS_OK; }   /* len 0 = to the end */
    }
    nat_unlock();
    return r;
}
uint32_t lean_ds_SetCurrentPosition(uint32_t buf, uint32_t pos)
{
    nat_lock(); nat_voice *v = nat_find(buf);
    if (v) {
        v->pos = nat_bytes_to_samples(v, pos); v->dec_block = -1;
        if (v->playing && v->looping) {             /* at/past the loop end: stops looping */
            uint32_t lend = v->loop_len ? v->loop_start + v->loop_len : v->bytes;
            if (pos >= lend) v->looping = 0;
        }
    }
    nat_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t lean_ds_GetCurrentPosition(uint32_t buf, uint32_t pplay, uint32_t pwrite)
{
    nat_lock(); nat_voice *v = nat_find(buf);
    uint32_t play = 0, write = 0;
    if (v) {
        play = nat_samples_to_bytes(v, (uint32_t)v->pos);   /* ADPCM moves in whole 36-byte blocks */
        if (v->playing && v->bytes) {
            uint32_t ba = v->adpcm ? 36u * v->channels : 2u * v->channels;
            uint32_t ahead = v->adpcm ? ba : 32u * ba;      /* max(32 samples in bytes, blockAlign) */
            if (ahead < ba) ahead = ba;
            write = play + ahead;
            uint32_t lend = v->loop_len ? v->loop_start + v->loop_len : v->bytes;
            uint32_t llen = lend - v->loop_start;
            if (v->looping && llen && play >= v->loop_start && play < lend) write = v->loop_start + (write % llen);  /* as the XDK computes it */
            else write %= v->bytes;
        } else write = play;                        /* stopped: both report the saved position */
    }
    nat_unlock();
    if (pplay) G32(pplay) = play;
    if (pwrite) G32(pwrite) = write;
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t lean_ds_SetMixBinsA(uint32_t buf, uint32_t pmix)   /* 0x17B5FC = SetMixBins (rebuilds the list) */
{
    nat_lock(); nat_voice *v = nat_find(buf); if (v) nat_set_mixbins(v, pmix); nat_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t lean_ds_SetMixBinsB(uint32_t buf, uint32_t pmix)   /* 0x17B618 = SetMixBinVolumes */
{
    nat_lock(); nat_voice *v = nat_find(buf); if (v) nat_set_mixbin_volumes(v, pmix); nat_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t lean_ds_SetVolume(uint32_t buf, uint32_t mb)
{
    nat_lock(); nat_voice *v = nat_find(buf); if (v) v->lvol = (int32_t)mb; nat_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t lean_ds_SetFrequency(uint32_t buf, uint32_t hz)
{
    nat_lock(); nat_voice *v = nat_find(buf);
    if (v) {   /* XDK 0x17B948: pitch = round(4096*log2(Hz/48000)) clamped to [-32767, 8191] */
        double f = hz ? (double)hz : (double)v->rate;
        long pitch = lround(4096.0 * log2(f / 48000.0));
        if (pitch < -32767) pitch = -32767; if (pitch > 8191) pitch = 8191;
        v->freq = (uint32_t)lround(48000.0 * pow(2.0, pitch / 4096.0));
    }
    nat_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t lean_ds_GetStatus(uint32_t buf, uint32_t pfilter)  /* 0x17B5E0 = SetFilter */
{
    /* XDK 0x17B4B3/0x17E53F: struct {mode, Q, coef[4]}; FMODE = mode & 3 (MISC bits 16-17),
     * FCA = coef1<<16 | coef0. The voice processor low-passes a mono 2D voice when
     * FMODE bit 0 is set (stereo: FMODE == 1), cutoff 2^(int16 FC0/4096) clamped to
     * [0.003906, 1], resonance FC1/0x8000 clamped to [0.079407, 1]. */
    nat_lock(); nat_voice *v = nat_find(buf);
    if (v && pfilter) {
        uint32_t mode = G32(pfilter) & 3u;
        int16_t fc0 = (int16_t)(G32(pfilter + 8) & 0xFFFF);
        uint16_t fc1 = (uint16_t)(G32(pfilter + 12) & 0xFFFF);
        v->lpf = v->channels == 2 ? (mode == 1) : ((mode & 1) != 0);
        float fc = powf(2.0f, fc0 / 4096.0f); v->fc = fc < 0.003906f ? 0.003906f : fc > 1.0f ? 1.0f : fc;
        float q = fc1 / (float)0x8000; v->q = q < 0.079407f ? 0.079407f : q > 1.0f ? 1.0f : q;
        if (v->lpf) { if (!v->svf) v->svf = nat_svf_new(); if (v->svf) nat_svf_setup(v->svf, v->fc, v->q); }
    }
    nat_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t lean_ds_OutCall2(uint32_t buf, uint32_t pstatus)   /* 0x17B690 = GetStatus: 1 playing, +4 looping */
{
    nat_lock(); nat_voice *v = nat_find(buf);
    uint32_t st = v && v->playing ? (1u | (v->looping ? 4u : 0u)) : 0u;
    nat_unlock();
    if (pstatus) G32(pstatus) = st;
    return v ? DS_OK : DSERR_GENERIC;
}

/* blocks mixed so far (for diagnostics) */
long long lean_ds_mixed_blocks(void) { return s_mixed_blocks; }
