/* NIGHTFIRE_NATIVE_SOUND=1 (off by default): native DirectSound for the Action engine.
 *
 * The game and the XMV movie player reach the statically linked XDK DirectSound
 * library through 33 C-API wrappers and 7 stream vtable slots (survey:
 * native-driving/action-audio/ACTION-AUDIO-SURVEY.md). With this switch on, those
 * calls are answered here (nds_action_glue.c, hooked at the call site), so the
 * DirectSound internals, the emulated MCPX APU/DSP/AC97 and the older native layers
 * (nightfire_game_audio.h, nightfire_video_audio.c) never run.
 *
 * Derived from the Driving native mixer (nightfire-driving-native
 * runtime/lean/lean_dsound.c), extended for Action:
 *  - stereo voices: slot b of a voice takes channel b % channels, as the voice
 *    processor does (left on even slots, right on odd);
 *  - 3D buffers: listener/buffer position, distance curve, pan and doppler, with
 *    deferred settings applied on CommitDeferredSettings (nds_3d_* below);
 *  - streams (XMV movie audio): packets queue on the stream and are mixed in order;
 *    finished packets complete, and the guest callback runs, in DirectSoundDoWork on
 *    the calling guest thread, as the XDK does; Discontinuity lets queued audio play
 *    out; Pause modes 0-3 and SynchPlayback.
 * Speaker bins fold to stereo exactly as the chip path does (apu_dsp.c); LFE (3) and
 * sends (10+, I3DL2 reverb) are not played. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nds_action.h"

int nds_own_adpcm_decode_block(int16_t *out, const uint8_t *in, size_t n, int channels);
int nds_xa2_init(void);
int nds_xa2_is_active(void);
int nds_xa2_submit_samples(const int16_t *samples, int num_samples);
int nds_xa2_queued_now(void);
void nds_audio_stats_native(const int16_t (*buf)[2], int n);

extern ptrdiff_t g_xbox_mem_offset;
uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment);

#define G8(a)  (*(volatile uint8_t  *)(g_xbox_mem_offset + (uintptr_t)(a)))
#define G16(a) (*(volatile uint16_t *)(g_xbox_mem_offset + (uintptr_t)(a)))
#define G32(a) (*(volatile uint32_t *)(g_xbox_mem_offset + (uintptr_t)(a)))
static float GF(uint32_t a) { uint32_t u = G32(a); float f; memcpy(&f, &u, 4); return f; }

#define DS_OK            0u
#define DSERR_GENERIC    0x80004005u
#define DSERR_OUTOFMEM   0x8007000Eu
#define DSERR_INVALIDCALL 0x88780032u
#define PKT_PENDING      0x8000000Au   /* XMEDIAPACKET_STATUS_PENDING */
#define PKT_FLUSHED      0x80004004u   /* XMEDIAPACKET_STATUS_FLUSHED */

#define NDS_RATE         48000
#define NDS_BLOCK        128           /* samples per mixed block (2.67 ms) */
#define NDS_QUEUE        8             /* XAudio2 blocks queued (~21 ms) */
#define NDS_MAX_VOICES   256
#define NDS_BINS         32
#define NDS_MAX_PACKETS  32
#define STREAM_VTABLE    0x0016264Cu   /* PAL DirectSound stream vtable (XMV calls through it) */

int nds_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("NIGHTFIRE_NATIVE_SOUND");
        on = e && e[0] == '1';
        if (on) fprintf(stderr, "[NDS] native DirectSound on: no APU/DSP/AC97, older audio layers off\n");
    }
    return on;
}

typedef struct { float pos[3], vel[3], mind, maxd; unsigned ncurve; float curve[64]; int32_t i3d_direct, i3d_room; } nds_3d;
typedef struct { float pos[3], vel[3], front[3], top[3]; } nds_listener;
typedef struct { uint32_t addr, size, completed, status, context; } nds_packet;

typedef struct {
    uint32_t handle;            /* guest object handed to the game (0 = free) */
    int      stream;            /* DirectSoundCreateStream object */
    int      adpcm, channels;
    uint32_t rate, freq, flags;
    uint32_t data, bytes;       /* buffer sample data in guest memory */
    uint32_t loop_start, loop_len;
    double   pos;               /* samples; for streams, within the head packet */
    int      playing, looping;
    int32_t  lvol, headroom;
    int      nbins;
    uint32_t bin[8];
    int32_t  binmb[8];
    uint32_t dec_addr;          /* guest address of the cached decoded ADPCM block */
    int16_t  dec[65 * 2];
    uint32_t serial;
    int32_t  pf;                /* frequency pitch, 1/4096 octave re 48 kHz (XAudioCalculatePitch) */
    /* 3D (ACTION-3D-CALC.md) */
    int      is3d, center;
    nds_3d   cur;               /* written at once; DS3D_DEFERRED only postpones the recalculation */
    int32_t  t3d[8];            /* per-slot 3D attenuation term, mB (<= 0) */
    int32_t  p3;                /* Doppler pitch, 1/4096 octave */
    float    az, el, dist;
    uint32_t hrtf_first; int hrtf_swap, itd;   /* HRIR pair (guest address of the first entry) */
    float    hl[31], hr[31], ol[31], or_[31]; int oitd, xfade, hrtf_loaded;
    float    hist[128]; unsigned hpos;          /* dry history for the HRIR filters */
    /* stream */
    uint32_t callback, context, max_packets;
    LONG     refs;
    nds_packet q[NDS_MAX_PACKETS]; int qn;     /* queued, head first */
    nds_packet done[NDS_MAX_PACKETS]; int dn;  /* finished, awaiting DoWork */
    int      paused, sync_wait, discontinuity;
    uint32_t submitted, completed_n, starved_blocks;
} nds_voice;

static nds_voice s_v[NDS_MAX_VOICES];
static CRITICAL_SECTION s_lock;
static INIT_ONCE s_once = INIT_ONCE_STATIC_INIT;
static volatile LONG s_thread_started;
static uint32_t s_ds_handle;
static LONG s_ds_refs;
static float s_master = 1.0f;
static volatile LONG64 s_mixed_blocks;
static nds_listener s_lis = { {0,0,0}, {0,0,0}, {0,0,1}, {0,1,0} };   /* listener defaults 0x12F1D8 */

/* Per-bin submix headroom DirectSound programs at init (same XDK library as Driving,
 * read from the chip path there: 1 on bins 0-30, 0 on bin 31). */
static int s_submix_hr[32] = { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0 };
static BOOL CALLBACK nds_init(PINIT_ONCE o, PVOID p, PVOID *c)
{
    (void)o; (void)p; (void)c;
    InitializeCriticalSectionAndSpinCount(&s_lock, 4000);
    const char *g = getenv("NIGHTFIRE_NATIVE_SOUND_GAIN");
    if (g && *g) s_master = (float)atof(g);
    if (!nds_xa2_init()) fprintf(stderr, "[NDS] XAudio2 output could not be started\n");
    return TRUE;
}
static void nds_lock(void)   { InitOnceExecuteOnce(&s_once, nds_init, NULL, NULL); EnterCriticalSection(&s_lock); }
static void nds_unlock(void) { LeaveCriticalSection(&s_lock); }

static nds_voice *nds_find(uint32_t handle)
{
    if (!handle) return NULL;
    for (int i = 0; i < NDS_MAX_VOICES; i++) if (s_v[i].handle == handle) return &s_v[i];
    return NULL;
}

/* ---- 3D --------------------------------------------------------------------- */
/* The XDK full-HRTF calculation (DirectSoundUseFullHRTF), decoded from the PAL XBE and
 * checked by running the XBE's own routines under an emulator against a model:
 * native-driving/action-audio/ACTION-3D-CALC.md. Section numbers below refer to it.
 * Defaults the game never changes are folded in: distance, rolloff and Doppler factors
 * 1, mode NORMAL, cone angles 360/360 (cone attenuation 0), I3DL2 listener RoomRolloff 0. */

static float dot3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static int32_t trunc32(float x) { return (int32_t)x; }
static int32_t mb_amp(float g) { return g <= 0.0f ? -10000 : g >= 1.0f ? 0 : trunc32(2000.0f * log10f(g)); }   /* 0x114C4F */
static int32_t mb_pow(float g) { return g <= 0.0f ? -10000 : g >= 1.0f ? 0 : trunc32(1000.0f * log10f(g)); }   /* 0x114C08 */
static float zeta(float t) { return t <= 1.0f ? 45.0f * t : 90.0f - 45.0f / t; }   /* the library's atan, degrees (0x115490) */

/* Surround flag (listener object +0x78, section 9): a stock stereo console gives 0, which
 * mutes the back bins and uses the true rear filters. NIGHTFIRE_NATIVE_SOUND_SURROUND=1
 * gives the surround behaviour. */
static int surround_flag(void)
{
    static int s = -1;
    if (s < 0) { const char *e = getenv("NIGHTFIRE_NATIVE_SOUND_SURROUND"); s = e && e[0] == '1'; }
    return s;
}
/* HRIR taps are sign-magnitude bytes (section 4). Their scale on the hardware is not known
 * from the library; 1/256 puts the front filter near unity gain (DC +1.3 dB, energy -2.6 dB).
 * NIGHTFIRE_NATIVE_SOUND_HRIR_SCALE overrides it (e.g. 0.0078125 for 1/128). */
static float hrir_scale(void)
{
    static float s = -1.0f;
    if (s < 0.0f) { const char *e = getenv("NIGHTFIRE_NATIVE_SOUND_HRIR_SCALE"); s = e && *e ? (float)atof(e) : 1.0f / 256.0f; }
    return s;
}
/* NIGHTFIRE_NATIVE_SOUND_HRTF=1: filter 3D sounds with the decoded head filters (HRIRs).
 * Off by default: as decoded, low-pitched sounds come out louder in the far ear (for a source
 * on the right, the left-ear filter has about 23 dB more bass) and nearby vehicles clip, which
 * suggests their format or scale is not yet understood. Until that is settled, a plain
 * left/right pan with the decoded ear delay stands in for them; every volume rule stays. */
static int hrtf_filters(void)
{
    static int h = -1;
    if (h < 0) { const char *e = getenv("NIGHTFIRE_NATIVE_SOUND_HRTF"); h = e && e[0] == '1'; }
    return h;
}
static float hrir_tap(uint8_t b) { return (b & 0x80 ? -(float)(b & 0x7F) : (float)b) * hrir_scale(); }

static int32_t pitch_of(double hz)   /* XAudioCalculatePitch 0x112787 */
{
    if (hz == 48000.0 || hz <= 0.0) return 0;
    return (int32_t)lround(4096.0 * log2(hz / 48000.0));
}
static double voice_rate(const nds_voice *v)
{
    int32_t p = v->pf + (v->is3d ? v->p3 : 0);
    if (p < -32767) p = -32767;
    if (p > 8191) p = 8191;
    return 48000.0 * pow(2.0, p / 4096.0);
}

static void nds_3d_update(nds_voice *v)
{
    const nds_3d *s = &v->cur;
    /* 1: listener-relative direction and distance */
    float d[3] = { s->pos[0] - s_lis.pos[0], s->pos[1] - s_lis.pos[1], s->pos[2] - s_lis.pos[2] };
    float dist = sqrtf(dot3(d, d));
    if (dist > 0.0f) { d[0] /= dist; d[1] /= dist; d[2] /= dist; } else d[0] = d[1] = d[2] = 0.0f;
    const float *f = s_lis.front, *t = s_lis.top;
    float right[3] = { t[1] * f[2] - t[2] * f[1], t[2] * f[0] - t[0] * f[2], t[0] * f[1] - t[1] * f[0] };
    float F = dot3(f, d), U = dot3(t, d), R = dot3(right, d), az = 0.0f, el = 0.0f;
    if (dist > 0.0f) {
        float h = sqrtf(R * R + F * F);
        el = h > 0.0f ? zeta(fabsf(U) / h) : (U != 0.0f ? 90.0f : 0.0f);
        if (U < 0.0f) el = -el;
        if (fabsf(F) > fabsf(R)) az = zeta(fabsf(R) / fabsf(F));
        else if (R == 0.0f && F == 0.0f) az = 0.0f;
        else az = 90.0f - 45.0f * fabsf(F) / fabsf(R);
        if (F < 0.0f) az = 180.0f - az;
        if (R < 0.0f) az = -az;
    }
    v->az = az; v->el = el; v->dist = dist;
    /* 2: distance attenuation D (mB, may be below -10000) */
    int32_t D = 0;
    float m = s->mind, M = s->maxd, x = dist;
    if (x > m) {
        if (x > M) x = M;
        if (s->ncurve) {
            float step = (M - m) / (float)s->ncurve, tt = x - m;
            int32_t i = trunc32(tt * (1.0f / step));
            if (i > (int32_t)s->ncurve - 1) i = (int32_t)s->ncurve - 1;
            if (i < 0) i = 0;
            float L = i == 0 ? 1.0f : s->curve[i - 1], Rr = s->curve[i];
            D = mb_amp(L + (Rr - L) * ((tt - (float)i * step) * (1.0f / step)));
        } else D = trunc32(-2000.0f * log10f(1.0f + (x / m - 1.0f)));
    }
    /* 3: front/back split and centre */
    int sur = surround_flag();
    int32_t FBf = 0, FBb = -10000;
    if (sur) {
        float B = (fabsf(az) / 90.0f - 1.0f) * (1.0f - fabsf(el) / 90.0f) + 0.5f;
        B = B < 0.0f ? 0.0f : B > 1.0f ? 1.0f : B;
        if (dist < 0.5f) B = 0.5f * ((B - 0.5f) * dist + 1.0f);
        FBf = mb_pow(1.0f - B); FBb = mb_pow(B);
    }
    int32_t CT = -10000, CF = 0;
    if (v->center) {
        int32_t a = trunc32(fabsf(az)), b = trunc32(fabsf(el));
        if (a < 45 && b < 45) {   /* tables T1 0x11CA20, T2 0x11CAD8 in the XBE */
            CT = mb_amp(GF(0x0011CA20u + 4u * a) * GF(0x0011CA20u + 4u * b));
            CF = mb_amp(1.0f - GF(0x0011CAD8u + 4u * a) * GF(0x0011CAD8u + 4u * b));
        }
    }
    /* 10: I3DL2 direct and room (the game sets only lRoom) */
    int32_t Id = s->i3d_direct, Ir = s->i3d_room;
    /* per-slot terms (0x116E53) */
    for (int k = 0; k < v->nbins; k++) {
        int32_t term;
        switch (v->bin[k]) {
        case 6: case 7: term = D + Id + FBf + CF; break;
        case 8: case 9: term = D + Id + FBb; break;
        case 10: term = D + Ir; break;
        case 2: term = (v->center && k == 4) ? D + Id + FBf + CT : D; break;
        default: term = D; break;
        }
        v->t3d[k] = term > 0 ? 0 : term;
    }
    /* 5: Doppler (speed of sound 342), velocity along the direction, positive = apart */
    float vel[3] = { s->vel[0] - s_lis.vel[0], s->vel[1] - s_lis.vel[1], s->vel[2] - s_lis.vel[2] };
    float X = dot3(vel, d);
    v->p3 = X == 0.0f ? 0 : X >= 342.0f ? -32767 : X <= -342.0f ? 4096 : (int32_t)lround(4096.0 * log2(1.0 - X / 342.0));
    /* 4: HRIR pair */
    float e = el >= 0.0f ? el + 3.0f : el - 3.0f;
    int32_t ie = trunc32(e), e6 = 6 * (ie / 6), q;
    float Az = fabsf(az);
    if (abs(e6) == 90) q = 0;
    else if (abs(e6) > 60) q = 12 * (trunc32(Az + 6.0f) / 12);
    else if (abs(e6) > 30) q = 6 * (trunc32(Az + 3.0f) / 6);
    else q = 3 * (trunc32(Az + 1.5f) / 3);
    if (sur && q > 90) q = 180 - q;
    int32_t ai = q / 3, ei = (e6 + 90) / 6;
    if (ai < 0 || ai > 60) ai = 0;
    if (ei < 0 || ei > 30) ei = 0;
    uint32_t id = G16(0x0012E2C0u + 2u * (uint32_t)(ai * 31 + ei));
    uint32_t first = 0x0011CD00u + 32u * id;
    int swap = az < 0.0f;
    int itd = (int)G8(first + 31);
    if (swap) itd = -itd;
    float nl[31], nr[31];
    memset(nl, 0, sizeof nl); memset(nr, 0, sizeof nr);
    if (hrtf_filters()) {
        uint32_t la = swap ? first + 32 : first, ra = swap ? first : first + 32;
        for (int k = 0; k < 31; k++) { nl[k] = hrir_tap(G8(la + k)); nr[k] = hrir_tap(G8(ra + k)); }
    } else {
        /* Default: the decoded filters are replaced by a plain left/right pan (one tap per ear,
         * constant power, unity per ear straight ahead) with the decoded ear delay. */
        float p = sinf(az * 3.14159265f / 180.0f);
        nl[0] = sqrtf(1.0f - p); nr[0] = sqrtf(1.0f + p);
    }
    if (!v->hrtf_loaded || itd != v->itd || memcmp(nl, v->hl, sizeof nl) || memcmp(nr, v->hr, sizeof nr)) {
        if (v->hrtf_loaded) { memcpy(v->ol, v->hl, sizeof v->ol); memcpy(v->or_, v->hr, sizeof v->or_); v->oitd = v->itd; v->xfade = 1; }
        memcpy(v->hl, nl, sizeof nl); memcpy(v->hr, nr, sizeof nr);
        v->hrtf_first = first; v->hrtf_swap = swap; v->itd = itd; v->hrtf_loaded = 1;
    }
}
static void nds_3d_apply_all(void)
{
    for (int i = 0; i < NDS_MAX_VOICES; i++) if (s_v[i].handle && s_v[i].is3d) nds_3d_update(&s_v[i]);
}

/* ---- sample fetch ------------------------------------------------------- */

static uint32_t blk_bytes(const nds_voice *v) { return v->adpcm ? 36u * v->channels : 2u * v->channels; }
static uint32_t bytes_to_samples(const nds_voice *v, uint32_t b)
{
    if (v->adpcm) return (b / (36u * v->channels)) * 64u;
    return b / (2u * v->channels);
}
static uint32_t samples_to_bytes(const nds_voice *v, uint32_t s)
{
    if (v->adpcm) return (s / 64u) * 36u * v->channels;
    return s * 2u * v->channels;
}
static void fetch(nds_voice *v, uint32_t base, uint32_t idx, float out[2])
{
    if (v->adpcm) {
        uint32_t bsz = 36u * v->channels, a = base + (idx / 64u) * bsz, off = idx % 64u;
        if (a != v->dec_addr) {
            uint8_t raw[72];
            for (uint32_t k = 0; k < bsz; k++) raw[k] = G8(a + k);
            if (!nds_own_adpcm_decode_block(v->dec, raw, bsz, v->channels)) memset(v->dec, 0, sizeof v->dec);
            v->dec_addr = a;
        }
        out[0] = v->dec[off * v->channels] * (1.0f / 32768.0f);
        out[1] = v->channels == 2 ? v->dec[off * 2 + 1] * (1.0f / 32768.0f) : out[0];
        return;
    }
    uint32_t a = base + idx * 2u * v->channels;
    out[0] = (int16_t)G16(a) * (1.0f / 32768.0f);
    out[1] = v->channels == 2 ? (int16_t)G16(a + 2) * (1.0f / 32768.0f) : out[0];
}

/* ---- mixer ---------------------------------------------------------------- */

static float s_bins[NDS_BINS][NDS_BLOCK];
static long s_mixed, s_skipped;
static double s_cpu_us, s_cpu_max; static long s_cpu_blocks;

/* 12-bit attenuation for slot k (0x116E53): att = headroom - lVolume - binVolume - T3D,
 * programmed as min(att*64/100, 0xFFF) with an unsigned divide (negative wraps to mute). */
static int slot_val(const nds_voice *v, int k)
{
    int32_t att = v->headroom - v->lvol - v->binmb[k] - (v->is3d ? v->t3d[k] : 0);
    if (att < 0) return 0xFFF;
    uint32_t q = (uint32_t)att * 64u / 100u;
    return q > 0xFFF ? 0xFFF : (int)q;
}
/* Slots 0-3 of a 3D voice carry the HRTF output and use the HRTF headroom (0, so x1);
 * every other slot is divided by its bin's submix headroom (section 8). */
static int hrtf_slot(const nds_voice *v, int k) { return v->is3d && k < 4; }
static float slot_gain(const nds_voice *v, int k)
{
    int q = slot_val(v, k);
    if (q >= 0xFFF) return 0.0f;
    float hr = hrtf_slot(v, k) ? 1.0f : (float)(1 << s_submix_hr[v->bin[k] & 31]);
    return powf(10.0f, (float)q / (64.0f * -20.0f)) / hr;
}
static int voice_gains(nds_voice *v, float g[8])
{
    int audible = 0;
    for (int b = 0; b < v->nbins; b++) {
        g[b] = slot_gain(v, b) * s_master;
        if (g[b] > 0.0f) audible = 1;
    }
    return audible;
}
static inline void put(nds_voice *v, const float g[8], int i, const float s[2])
{
    for (int b = 0; b < v->nbins; b++) s_bins[v->bin[b] & (NDS_BINS - 1)][i] += s[b % v->channels] * g[b];
}
static float fir31(const float *h, const float *hist, unsigned pos, int delay)
{
    float acc = 0.0f;
    for (int j = 0; j < 31; j++) acc += h[j] * hist[(pos - (unsigned)delay - (unsigned)j) & 127u];
    return acc;
}
/* 3D voice: the dry mono goes through the left and right HRIRs, the far ear delayed by the
 * ITD (samples at 48 kHz); left-ear output to bins 6/8, right-ear to 7/9 (the HRTF submixes
 * 6, 8, 7, 9), the dry mono to every other slot (centre 2, I3DL2 send 10). After a filter
 * change the old and new filter outputs are cross-faded over one block. */
static inline void put3d(nds_voice *v, const float g[8], int i, float s)
{
    v->hist[v->hpos & 127u] = s;
    float yl = fir31(v->hl, v->hist, v->hpos, v->itd > 0 ? v->itd : 0);
    float yr = fir31(v->hr, v->hist, v->hpos, v->itd < 0 ? -v->itd : 0);
    if (v->xfade) {
        float w = (float)(i + 1) / NDS_BLOCK;
        float ol = fir31(v->ol, v->hist, v->hpos, v->oitd > 0 ? v->oitd : 0);
        float orr = fir31(v->or_, v->hist, v->hpos, v->oitd < 0 ? -v->oitd : 0);
        yl = ol + (yl - ol) * w; yr = orr + (yr - orr) * w;
    }
    v->hpos++;
    for (int b = 0; b < v->nbins; b++) {
        float x = s;
        if (b < 4) { uint32_t bin = v->bin[b]; x = (bin == 6 || bin == 8) ? yl : (bin == 7 || bin == 9) ? yr : s; }
        s_bins[v->bin[b] & (NDS_BINS - 1)][i] += x * g[b];
    }
}

static void mix_buffer(nds_voice *v)
{
    uint32_t total = bytes_to_samples(v, v->bytes);
    if (!total || !v->data) { v->playing = 0; return; }
    uint32_t lstart = v->looping ? bytes_to_samples(v, v->loop_start) : 0;
    uint32_t lend = (v->looping && v->loop_len) ? lstart + bytes_to_samples(v, v->loop_len) : total;
    if (lend > total || lend <= lstart) lend = total;
    double step = voice_rate(v) / NDS_RATE;
    float g[8];
    if (!voice_gains(v, g)) {   /* silent: advance time exactly, skip decode and mix */
        s_skipped++;
        double end = v->pos + step * NDS_BLOCK;
        if (end >= lend) {
            if (v->looping) v->pos = lstart + fmod(end - lend, (double)(lend - lstart));
            else { v->playing = 0; v->pos = total; }
        } else v->pos = end;
        return;
    }
    s_mixed++;
    for (int i = 0; i < NDS_BLOCK; i++) {
        uint32_t i0 = (uint32_t)v->pos;
        if (i0 >= lend) {
            if (v->looping) { v->pos = lstart + fmod(v->pos - lend, (double)(lend - lstart)); i0 = (uint32_t)v->pos; }
            else { v->playing = 0; v->pos = total; return; }
        }
        uint32_t i1 = i0 + 1;
        if (i1 >= lend) i1 = v->looping ? lstart : i0;
        float f = (float)(v->pos - i0), a[2], b[2], s[2];
        fetch(v, v->data, i0, a);
        if (f > 0.0f) { fetch(v, v->data, i1, b); s[0] = a[0] + (b[0] - a[0]) * f; s[1] = a[1] + (b[1] - a[1]) * f; }
        else { s[0] = a[0]; s[1] = a[1]; }
        if (v->is3d) put3d(v, g, i, s[0]); else put(v, g, i, s);
        v->pos += step;
    }
    v->xfade = 0;
}

/* Streams: the head packet plays from pos; a finished packet moves to the done list
 * (completed by DoWork on a guest thread). No packet: starved, silence. */
static uint32_t pkt_samples(const nds_voice *v, const nds_packet *p) { return bytes_to_samples(v, p->size); }
static void stream_pop(nds_voice *v)
{
    if (v->dn < NDS_MAX_PACKETS) v->done[v->dn++] = v->q[0];
    memmove(&v->q[0], &v->q[1], (size_t)(v->qn - 1) * sizeof v->q[0]);
    v->qn--;
}
static void mix_stream(nds_voice *v)
{
    if (v->paused || !v->qn) { if (!v->paused && !v->qn) v->starved_blocks++; return; }
    double step = voice_rate(v) / NDS_RATE;
    float g[8];
    int audible = voice_gains(v, g);
    if (audible) s_mixed++; else s_skipped++;
    for (int i = 0; i < NDS_BLOCK; i++) {
        while (v->qn && (uint32_t)v->pos >= pkt_samples(v, &v->q[0])) {
            v->pos -= pkt_samples(v, &v->q[0]);
            stream_pop(v);
        }
        if (!v->qn) { v->pos = 0; v->starved_blocks++; return; }
        if (!audible) { v->pos += step; continue; }
        uint32_t i0 = (uint32_t)v->pos, n0 = pkt_samples(v, &v->q[0]);
        float f = (float)(v->pos - i0), a[2], b[2], s[2];
        fetch(v, v->q[0].addr, i0, a);
        if (f > 0.0f) {
            if (i0 + 1 < n0) fetch(v, v->q[0].addr, i0 + 1, b);
            else if (v->qn > 1) fetch(v, v->q[1].addr, 0, b);
            else { b[0] = a[0]; b[1] = a[1]; }
            s[0] = a[0] + (b[0] - a[0]) * f; s[1] = a[1] + (b[1] - a[1]) * f;
        } else { s[0] = a[0]; s[1] = a[1]; }
        put(v, g, i, s);
        v->pos += step;
    }
    while (v->qn && (uint32_t)v->pos >= pkt_samples(v, &v->q[0])) { v->pos -= pkt_samples(v, &v->q[0]); stream_pop(v); }
}

static void fold(float out[NDS_BLOCK][2])
{
    for (int i = 0; i < NDS_BLOCK; i++) {
        out[i][0] = s_bins[0][i] + s_bins[6][i] + 0.7071f * (s_bins[2][i] + s_bins[4][i] + s_bins[8][i]);
        out[i][1] = s_bins[1][i] + s_bins[7][i] + 0.7071f * (s_bins[2][i] + s_bins[5][i] + s_bins[9][i]);
    }
}

static DWORD WINAPI nds_thread(LPVOID p)
{
    (void)p;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    HANDLE t = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    int16_t out[NDS_BLOCK][2];
    static int16_t stat_buf[256][2]; int stat_n = 0;
    static float f_stream[NDS_BLOCK][2], f_2d[NDS_BLOCK][2], f_all[NDS_BLOCK][2];
    static double e_stream, e_2d, e_3d; static long clipped, frames5; static ULONGLONG t5;
    for (;;) {
        int q = nds_xa2_queued_now();
        if (q >= NDS_QUEUE) {
            LARGE_INTEGER due; due.QuadPart = -5000;   /* 0.5 ms */
            if (t && SetWaitableTimer(t, &due, 0, NULL, NULL, FALSE)) WaitForSingleObject(t, 20); else Sleep(1);
            continue;
        }
        LARGE_INTEGER c0, c1; QueryPerformanceCounter(&c0);
        memset(s_bins, 0, sizeof s_bins);
        nds_lock();
        /* streams (movies) first, then 2D buffers, then 3D buffers, so their levels can be reported separately */
        for (int i = 0; i < NDS_MAX_VOICES; i++) if (s_v[i].handle && s_v[i].stream) mix_stream(&s_v[i]);
        fold(f_stream);
        for (int i = 0; i < NDS_MAX_VOICES; i++) if (s_v[i].handle && !s_v[i].stream && !s_v[i].is3d && s_v[i].playing) mix_buffer(&s_v[i]);
        fold(f_2d);
        for (int i = 0; i < NDS_MAX_VOICES; i++) if (s_v[i].handle && !s_v[i].stream && s_v[i].is3d && s_v[i].playing) mix_buffer(&s_v[i]);
        nds_unlock();
        fold(f_all);
        for (int i = 0; i < NDS_BLOCK; i++) {
            float l = f_all[i][0], r = f_all[i][1];
            e_stream += f_stream[i][0] * f_stream[i][0] + f_stream[i][1] * f_stream[i][1];
            e_2d += (f_2d[i][0] - f_stream[i][0]) * (f_2d[i][0] - f_stream[i][0]) + (f_2d[i][1] - f_stream[i][1]) * (f_2d[i][1] - f_stream[i][1]);
            e_3d += (l - f_2d[i][0]) * (l - f_2d[i][0]) + (r - f_2d[i][1]) * (r - f_2d[i][1]);
            if (l > 1.0f || l < -1.0f) clipped++;
            if (r > 1.0f || r < -1.0f) clipped++;
            l = l > 1.0f ? 1.0f : l < -1.0f ? -1.0f : l;
            r = r > 1.0f ? 1.0f : r < -1.0f ? -1.0f : r;
            out[i][0] = (int16_t)(l * 32767.0f); out[i][1] = (int16_t)(r * 32767.0f);
        }
        frames5 += NDS_BLOCK;
        { static LARGE_INTEGER hz; if (!hz.QuadPart) QueryPerformanceFrequency(&hz); QueryPerformanceCounter(&c1);
          double us = (c1.QuadPart - c0.QuadPart) * 1e6 / hz.QuadPart; s_cpu_us += us; if (us > s_cpu_max) s_cpu_max = us; s_cpu_blocks++; }
        if (GetTickCount64() - t5 >= 5000) {
            if (t5) {
                int nb = 0, n3 = 0, ns = 0; uint32_t starved = 0;
                nds_lock();
                for (int i = 0; i < NDS_MAX_VOICES; i++) if (s_v[i].handle) {
                    if (s_v[i].stream) { ns++; starved += s_v[i].starved_blocks; s_v[i].starved_blocks = 0; }
                    else if (s_v[i].playing) { if (s_v[i].is3d) n3++; else nb++; }
                }
                nds_unlock();
                fprintf(stderr, "[NDS] 5s: rms streams %.0f 2d %.0f 3d %.0f, clipped %ld, playing 2d %d 3d %d, streams %d (starved blocks %u), mixer cpu per block avg %.0f us max %.0f us (budget %.0f us), voices mixed %ld skipped %ld per block\n",
                        32767.0 * sqrt(e_stream / (2.0 * frames5)), 32767.0 * sqrt(e_2d / (2.0 * frames5)), 32767.0 * sqrt(e_3d / (2.0 * frames5)),
                        clipped, nb, n3, ns, starved, s_cpu_blocks ? s_cpu_us / s_cpu_blocks : 0.0, s_cpu_max, NDS_BLOCK * 1e6 / NDS_RATE,
                        s_cpu_blocks ? s_mixed / s_cpu_blocks : 0L, s_cpu_blocks ? s_skipped / s_cpu_blocks : 0L);
                s_cpu_us = 0; s_cpu_max = 0; s_cpu_blocks = 0; s_mixed = s_skipped = 0;
            }
            t5 = GetTickCount64(); e_stream = e_2d = e_3d = 0; clipped = 0; frames5 = 0;
        }
        {   /* NIGHTFIRE_NATIVE_SOUND_VOICES=1 (diagnostic): every half second, each playing voice with the
             * per-slot attenuation in the chip's units (12-bit, 1/64 dB), to compare with [APU-VOICE] */
            static int vd = -1; static ULONGLONG tv; static unsigned tick;
            if (vd < 0) { const char *e = getenv("NIGHTFIRE_NATIVE_SOUND_VOICES"); vd = e && e[0] == '1'; }
            if (vd && GetTickCount64() - tv >= 500) {
                tv = GetTickCount64(); tick++;
                nds_lock();
                for (int k = 0; k < NDS_MAX_VOICES; k++) {
                    nds_voice *v = &s_v[k];
                    if (!v->handle || !(v->playing || (v->stream && v->qn))) continue;
                    char line[400]; int n = snprintf(line, sizeof line, "[NDS-VOICE] t=%u k=%d %s%s%s rate=%.4f",
                        tick, k, v->stream ? "stream " : "", v->is3d ? "3d " : "", v->channels == 2 ? "st" : "mo",
                        voice_rate(v) / NDS_RATE);
                    if (v->is3d)
                        n += snprintf(line + n, sizeof line - n, " dist=%.1f az=%.0f el=%.0f min=%.1f max=%.1f itd=%d p3=%d",
                                      v->dist, v->az, v->el, v->cur.mind, v->cur.maxd, v->itd, v->p3);
                    n += snprintf(line + n, sizeof line - n, " bins");
                    for (int b = 0; b < v->nbins && n < 380; b++) {
                        n += snprintf(line + n, sizeof line - n, " %u:%03X", v->bin[b], slot_val(v, b));
                    }
                    fprintf(stderr, "%s\n", line);
                }
                nds_unlock();
            }
        }
        memcpy(stat_buf[stat_n], out, sizeof out); stat_n += NDS_BLOCK;
        if (stat_n >= 256) { nds_audio_stats_native((const int16_t (*)[2])stat_buf, 256); stat_n = 0; }
        nds_xa2_submit_samples(&out[0][0], NDS_BLOCK);
        InterlockedIncrement64(&s_mixed_blocks);
    }
}

static void nds_start_thread(void)
{
    if (InterlockedCompareExchange(&s_thread_started, 1, 0) == 0) {
        HANDLE h = CreateThread(NULL, 0, nds_thread, NULL, 0, NULL);
        if (h) CloseHandle(h);
        fprintf(stderr, "[NDS] mixer started (%d-sample blocks, %d queued, XAudio2 %s)\n",
                NDS_BLOCK, NDS_QUEUE, nds_xa2_is_active() ? "ready" : "NOT ready");
    }
}

/* ---- object helpers ------------------------------------------------------ */

static uint32_t nds_new_handle(void)
{
    uint32_t h = xbox_HeapAlloc(0x40, 16);
    if (h) for (uint32_t k = 0; k < 0x40; k += 4) G32(h + k) = 0;
    return h;
}
static void set_mixbins(nds_voice *v, uint32_t pmix)
{
    /* DSMIXBINS { DWORD dwMixBinCount; DSMIXBINVOLUMEPAIR *pairs }; pair { DWORD bin; LONG vol } */
    if (!pmix) return;
    uint32_t n = G32(pmix), pairs = G32(pmix + 4);
    if (n > 8) n = 8;
    v->nbins = 0;
    for (uint32_t i = 0; i < n && pairs; i++) {
        v->bin[v->nbins] = G32(pairs + i * 8);
        v->binmb[v->nbins] = (int32_t)G32(pairs + i * 8 + 4);
        v->nbins++;
    }
    v->center = v->nbins > 4 && v->bin[4] == 2;   /* 0x116D78: centre term only for bin 2 at index 4 */
    if (v->is3d) nds_3d_update(v);
}
static void set_mixbin_volumes(nds_voice *v, uint32_t pmix)
{
    if (!pmix) return;   /* only updates bins already routed (XDK) */
    uint32_t n = G32(pmix), pairs = G32(pmix + 4);
    if (n > 8) n = 8;
    for (uint32_t i = 0; i < n && pairs; i++) {
        uint32_t bin = G32(pairs + i * 8);
        for (int b = 0; b < v->nbins; b++) if (v->bin[b] == bin) v->binmb[b] = (int32_t)G32(pairs + i * 8 + 4);
    }
}
static void default_bins(nds_voice *v)
{
    v->nbins = 2; v->bin[0] = 0; v->bin[1] = 1; v->binmb[0] = v->binmb[1] = 0;
    if (v->is3d) {   /* default 3D list 0x12F28C: 6, 8, 7, 9, 10 */
        static const uint32_t d3[5] = { 6, 8, 7, 9, 10 };
        v->nbins = 5; for (int b = 0; b < 5; b++) { v->bin[b] = d3[b]; v->binmb[b] = 0; }
    }
    v->center = 0;
}
static nds_voice *new_voice(uint32_t wfx, uint32_t flags, uint32_t *handle)
{
    nds_voice *v = NULL;
    for (int i = 0; i < NDS_MAX_VOICES; i++) if (!s_v[i].handle) { v = &s_v[i]; break; }
    uint32_t h = v ? nds_new_handle() : 0;
    if (!v || !h) return NULL;
    uint32_t serial = v->serial + 1;
    memset(v, 0, sizeof *v);
    v->serial = serial; v->handle = h; v->flags = flags; v->dec_addr = 0;
    v->is3d = (flags & 0x10) != 0;
    v->headroom = v->is3d ? 0 : 600;   /* XDK default headroom: 600 for 2D, 0 for 3D */
    v->adpcm = wfx && G16(wfx) == 0x69;
    v->channels = wfx ? (G16(wfx + 2) ? G16(wfx + 2) : 1) : 1;
    if (v->channels > 2) v->channels = 2;
    v->rate = wfx ? G32(wfx + 4) : NDS_RATE;
    if (!v->rate) v->rate = NDS_RATE;
    v->pf = pitch_of(v->rate);
    v->cur.mind = 1.0f; v->cur.maxd = 1000000000.0f;   /* DS3DBUFFER defaults 0x12F240 */
    default_bins(v);
    if (v->is3d) nds_3d_update(v);
    *handle = h;
    return v;
}

/* ---- entry points ---------------------------------------------------------- */

uint32_t nds_DirectSoundCreate(uint32_t guid, uint32_t ppds, uint32_t unk)
{
    (void)guid; (void)unk;
    InitOnceExecuteOnce(&s_once, nds_init, NULL, NULL);
    nds_lock();
    if (!s_ds_handle) { s_ds_handle = nds_new_handle(); fprintf(stderr, "[NDS] DirectSoundCreate -> %08X\n", s_ds_handle); }
    s_ds_refs++;
    nds_unlock();
    if (ppds) G32(ppds) = s_ds_handle;
    nds_start_thread();
    return s_ds_handle ? DS_OK : DSERR_OUTOFMEM;
}
uint32_t nds_UseFullHRTF(void) { return DS_OK; }
uint32_t nds_DownloadEffectsImage(uint32_t ds, uint32_t img, uint32_t size, uint32_t ploc, uint32_t ppdesc)
{
    (void)ds; (void)img; (void)ploc;
    if (ppdesc) G32(ppdesc) = 0;
    fprintf(stderr, "[NDS] DownloadEffectsImage (%u bytes) accepted; reverb/crosstalk effects not run yet\n", size);
    return DS_OK;
}
uint32_t nds_ReleaseDS(uint32_t ds)
{
    (void)ds;   /* the singleton stays alive (the game keeps its pointer at 0x2AE598) */
    nds_lock(); LONG r = s_ds_refs > 1 ? --s_ds_refs : 1; nds_unlock();
    return (uint32_t)r;
}
uint32_t nds_SynchPlayback(uint32_t ds)
{
    (void)ds;
    nds_lock();
    int n = 0;
    for (int i = 0; i < NDS_MAX_VOICES; i++) if (s_v[i].handle && s_v[i].stream && s_v[i].sync_wait) { s_v[i].sync_wait = 0; s_v[i].paused = 0; n++; }
    nds_unlock();
    static unsigned logged; if (n && logged++ < 8) fprintf(stderr, "[NDS] SynchPlayback started %d stream(s)\n", n);
    return DS_OK;
}
uint32_t nds_CommitDeferredSettings(uint32_t ds)
{
    (void)ds;
    nds_lock();
    nds_3d_apply_all();
    nds_unlock();
    return DS_OK;
}
static void listener_set(int what, uint32_t pargs, int n, uint32_t apply)
{
    nds_lock();
    float *dst = what == 0 ? s_lis.pos : what == 1 ? s_lis.vel : s_lis.front;
    for (int k = 0; k < n; k++) dst[k] = GF(pargs + 4u * k);   /* front then top are contiguous */
    if (!(apply & 1)) nds_3d_apply_all();   /* DS3D_IMMEDIATE: recalculate now (0x113541) */
    nds_unlock();
}
/* argument addresses are the guest stack slots, so floats are read in place */
uint32_t nds_SetOrientation(uint32_t pargs, uint32_t apply) { listener_set(2, pargs, 6, apply); return DS_OK; }
uint32_t nds_ListenerSetPosition(uint32_t pargs, uint32_t apply) { listener_set(0, pargs, 3, apply); return DS_OK; }
uint32_t nds_ListenerSetVelocity(uint32_t pargs, uint32_t apply) { listener_set(1, pargs, 3, apply); return DS_OK; }

uint32_t nds_CreateSoundBuffer(uint32_t ds, uint32_t pdesc, uint32_t ppbuf, uint32_t unk)
{
    (void)ds; (void)unk;
    /* DSBUFFERDESC { dwSize, dwFlags, dwBufferBytes, lpwfxFormat, lpMixBins, dwInputMixBin } */
    if (!pdesc || !ppbuf) return DSERR_GENERIC;
    uint32_t flags = G32(pdesc + 4), wfx = G32(pdesc + 0xC), mix = G32(pdesc + 0x10), h = 0;
    nds_lock();
    nds_voice *v = new_voice(wfx, flags, &h);
    if (v && mix) set_mixbins(v, mix);
    nds_unlock();
    if (!v) return DSERR_OUTOFMEM;
    G32(ppbuf) = h;
    nds_start_thread();
    return DS_OK;
}
uint32_t nds_ReleaseBuffer(uint32_t buf)
{
    nds_lock(); nds_voice *v = nds_find(buf); if (v) { v->playing = 0; v->handle = 0; } nds_unlock();
    return 0;
}
uint32_t nds_Play(uint32_t buf, uint32_t r1, uint32_t r2, uint32_t flags)
{
    (void)r1; (void)r2;
    nds_lock(); nds_voice *v = nds_find(buf);
    if (v) {
        v->looping = (flags & 1) != 0;              /* already playing: only the loop mode changes */
        if (flags & 2) { v->pos = 0; v->dec_addr = 0; }   /* DSBPLAY_FROMSTART */
        if (!v->playing && v->pos >= bytes_to_samples(v, v->bytes)) v->pos = 0;
        v->playing = 1;
    }
    nds_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_Stop(uint32_t buf)
{
    nds_lock(); nds_voice *v = nds_find(buf); if (v) v->playing = 0; nds_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_SetBufferData(uint32_t buf, uint32_t data, uint32_t bytes)
{
    nds_lock(); nds_voice *v = nds_find(buf);
    if (v && !(v->data == data && v->bytes == bytes)) {
        v->playing = 0; v->data = data; v->bytes = bytes; v->dec_addr = 0;
        v->pos = 0; v->loop_start = 0; v->loop_len = 0;
    }
    nds_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_SetLoopRegion(uint32_t buf, uint32_t start, uint32_t len)
{
    nds_lock(); nds_voice *v = nds_find(buf); uint32_t r = DSERR_GENERIC;
    if (v) {
        if ((uint64_t)start + len > v->bytes) r = DSERR_INVALIDCALL;
        else { v->loop_start = start; v->loop_len = len; r = DS_OK; }
    }
    nds_unlock();
    return r;
}
uint32_t nds_SetCurrentPosition(uint32_t buf, uint32_t pos)
{
    nds_lock(); nds_voice *v = nds_find(buf);
    if (v) {
        v->pos = bytes_to_samples(v, pos); v->dec_addr = 0;
        if (v->playing && v->looping) {
            uint32_t lend = v->loop_len ? v->loop_start + v->loop_len : v->bytes;
            if (pos >= lend) v->looping = 0;
        }
    }
    nds_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_GetCurrentPosition(uint32_t buf, uint32_t pplay, uint32_t pwrite)
{
    nds_lock(); nds_voice *v = nds_find(buf);
    uint32_t play = 0, write = 0;
    if (v) {
        play = samples_to_bytes(v, (uint32_t)v->pos);
        if (v->playing && v->bytes) {
            uint32_t ba = blk_bytes(v);
            uint32_t ahead = v->adpcm ? ba : 32u * ba;
            write = play + ahead;
            uint32_t lend = v->loop_len ? v->loop_start + v->loop_len : v->bytes;
            uint32_t llen = lend - v->loop_start;
            if (v->looping && llen && play >= v->loop_start && play < lend) write = v->loop_start + (write % llen);
            else write %= v->bytes;
        } else write = play;
    }
    nds_unlock();
    if (pplay) G32(pplay) = play;
    if (pwrite) G32(pwrite) = write;
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_SetMixBins(uint32_t obj, uint32_t pmix)
{
    nds_lock(); nds_voice *v = nds_find(obj); if (v) set_mixbins(v, pmix); nds_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_SetMixBinVolumes(uint32_t buf, uint32_t pmix)
{
    nds_lock(); nds_voice *v = nds_find(buf); if (v) set_mixbin_volumes(v, pmix); nds_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_SetVolume(uint32_t obj, uint32_t mb)
{
    nds_lock(); nds_voice *v = nds_find(obj); if (v) v->lvol = (int32_t)mb; nds_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_SetHeadroom(uint32_t buf, uint32_t hr)
{
    nds_lock(); nds_voice *v = nds_find(buf); if (v) v->headroom = (int32_t)hr; nds_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_SetFrequency(uint32_t buf, uint32_t hz)
{
    nds_lock(); nds_voice *v = nds_find(buf);
    if (v) v->pf = pitch_of(hz ? (double)hz : (double)v->rate);   /* 0 = the format rate; clamped with Doppler in voice_rate */
    nds_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_GetStatus(uint32_t buf, uint32_t pstatus)
{
    nds_lock(); nds_voice *v = nds_find(buf);
    uint32_t st = v && v->playing ? (1u | (v->looping ? 4u : 0u)) : 0u;
    nds_unlock();
    if (pstatus) G32(pstatus) = st;
    return v ? DS_OK : DSERR_GENERIC;
}
/* 3D buffer settings: kind 0 max distance, 1 min distance, 2 position, 3 velocity, 4 rolloff curve */
uint32_t nds_Buffer3D(uint32_t buf, int kind, uint32_t pargs, uint32_t count, uint32_t apply)
{
    nds_lock(); nds_voice *v = nds_find(buf);
    if (v) {
        switch (kind) {
        case 0: v->cur.maxd = GF(pargs); break;
        case 1: v->cur.mind = GF(pargs); break;
        case 2: for (int k = 0; k < 3; k++) v->cur.pos[k] = GF(pargs + 4u * k); break;
        case 3: for (int k = 0; k < 3; k++) v->cur.vel[k] = GF(pargs + 4u * k); break;
        case 4: {   /* the library keeps the game's pointer; the game's curve (0x19A9A8) is constant */
            uint32_t pc = G32(pargs);
            v->cur.ncurve = count > 64 ? 64 : count;
            for (unsigned k = 0; k < v->cur.ncurve; k++) v->cur.curve[k] = pc ? GF(pc + 4u * k) : 0.0f;
            if (!pc) v->cur.ncurve = 0;
            break; }
        }
        if (!(apply & 1) && v->is3d) nds_3d_update(v);   /* DS3D_IMMEDIATE: this voice now (0x112AEF) */
    }
    nds_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_SetI3DL2Source(uint32_t buf, uint32_t params, uint32_t apply)
{
    /* DSI3DL2BUFFER { lDirect, lDirectHF, lRoom, lRoomHF, flRoomRolloff, Obstruction {lHFLevel, flLFRatio},
     * Occlusion {lHFLevel, flLFRatio} }; direct and room per 0x11AB29 (listener RoomRolloff 0: no distance
     * term). The send (bin 10) is not played yet and the HF filters are not applied (the game's give 0). */
    nds_lock(); nds_voice *v = nds_find(buf);
    if (v && params) {
        float obs = (float)(int32_t)G32(params + 0x14) * GF(params + 0x18), occ = (float)(int32_t)G32(params + 0x1C) * GF(params + 0x20);
        v->cur.i3d_direct = (int32_t)G32(params) + trunc32(obs + occ);
        v->cur.i3d_room = (int32_t)G32(params + 8) + trunc32(occ);
        if (!(apply & 1) && v->is3d) nds_3d_update(v);
    }
    nds_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}

/* ---- streams ----------------------------------------------------------------- */

uint32_t nds_CreateStream(uint32_t pdesc, uint32_t ppstream)
{
    /* DSSTREAMDESC { dwFlags, dwMaxAttachedPackets, lpwfxFormat, lpfnCallback, lpvContext, lpMixBins } */
    if (!pdesc || !ppstream) return DSERR_GENERIC;
    uint32_t flags = G32(pdesc), maxp = G32(pdesc + 4), wfx = G32(pdesc + 8), h = 0;
    nds_lock();
    nds_voice *v = new_voice(wfx, flags, &h);
    if (v) {
        v->stream = 1; v->refs = 1; v->playing = 1;
        v->max_packets = maxp ? maxp : 1;
        v->callback = G32(pdesc + 12); v->context = G32(pdesc + 16);
        if (G32(pdesc + 20)) set_mixbins(v, G32(pdesc + 20));
        G32(h) = STREAM_VTABLE;   /* XMV calls through the guest vtable; its slots are hooked */
    }
    nds_unlock();
    if (!v) return DSERR_OUTOFMEM;
    G32(ppstream) = h;
    fprintf(stderr, "[NDS] stream %08X: %s %u ch %u Hz, max %u packets, callback %08X\n",
            h, v->adpcm ? "ADPCM" : "PCM", v->channels, v->rate, v->max_packets, v->callback);
    nds_start_thread();
    return DS_OK;
}
uint32_t nds_StreamPause(uint32_t s, uint32_t mode)
{
    nds_lock(); nds_voice *v = nds_find(s);
    if (v) {   /* 0 resume, 1 pause, 2 wait for SynchPlayback, 3 pause without activating */
        v->paused = mode != 0;
        v->sync_wait = mode == 2;
    }
    nds_unlock();
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_StreamAddRef(uint32_t s)
{
    nds_lock(); nds_voice *v = nds_find(s); LONG r = v ? ++v->refs : 0; nds_unlock();
    return (uint32_t)r;
}
uint32_t nds_StreamProcess(uint32_t s, uint32_t in, uint32_t outp)
{
    (void)outp;
    if (!in) return DSERR_GENERIC;
    /* XMEDIAPACKET { pvBuffer, dwMaxSize, pdwCompletedSize, pdwStatus, pContext, prtTimestamp } */
    nds_packet p = { G32(in), G32(in + 4), G32(in + 8), G32(in + 12), G32(in + 16) };
    nds_lock(); nds_voice *v = nds_find(s); uint32_t r = DSERR_GENERIC;
    if (v && v->stream) {
        if (v->qn + v->dn >= NDS_MAX_PACKETS) r = DSERR_INVALIDCALL;
        else {
            p.size -= p.size % blk_bytes(v);
            if (p.completed) G32(p.completed) = 0;
            if (p.status) G32(p.status) = PKT_PENDING;
            v->q[v->qn++] = p; v->submitted++; v->discontinuity = 0; r = DS_OK;
            if (v->submitted <= 4 || v->submitted % 256 == 0)
                fprintf(stderr, "[NDS] stream %08X packet %u: %u bytes queued %d\n", s, v->submitted, p.size, v->qn);
        }
    }
    nds_unlock();
    return r;
}
uint32_t nds_StreamDiscontinuity(uint32_t s)
{
    nds_lock(); nds_voice *v = nds_find(s); if (v) v->discontinuity = 1; nds_unlock();   /* queued audio plays out */
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_StreamGetStatus(uint32_t s, uint32_t pst)
{
    nds_lock(); nds_voice *v = nds_find(s);
    uint32_t st = v && v->qn + v->dn < (int)v->max_packets ? 1u : 0u;   /* XMO_STATUSF_ACCEPT_INPUT_DATA */
    nds_unlock();
    if (pst) G32(pst) = st;
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_StreamGetInfo(uint32_t s, uint32_t pinfo)
{
    nds_lock(); nds_voice *v = nds_find(s); uint32_t ba = v ? blk_bytes(v) : 0; nds_unlock();
    if (pinfo) { G32(pinfo) = 0; G32(pinfo + 4) = ba; G32(pinfo + 8) = 0; G32(pinfo + 12) = 0; }   /* XMEDIAINFO */
    return v ? DS_OK : DSERR_GENERIC;
}

/* Completed packets, collected under the lock and reported on the calling guest thread. */
typedef struct { uint32_t callback, sctx, pctx, status, completed_ptr, size, status_ptr; } nds_done;
static int collect(nds_voice *v, nds_done *out, int max, int flush)
{
    int n = 0;
    for (int k = 0; k < v->dn && n < max; k++, n++) {
        out[n] = (nds_done){ v->callback, v->context, v->done[k].context, 0, v->done[k].completed, v->done[k].size, v->done[k].status };
        v->completed_n++;
    }
    v->dn = 0;
    if (flush) {
        for (int k = 0; k < v->qn && n < max; k++, n++)
            out[n] = (nds_done){ v->callback, v->context, v->q[k].context, PKT_FLUSHED, v->q[k].completed, 0, v->q[k].status };
        v->qn = 0; v->pos = 0;
    }
    return n;
}
static void report(const nds_done *d, int n)
{
    for (int k = 0; k < n; k++) {
        if (d[k].completed_ptr) G32(d[k].completed_ptr) = d[k].status ? 0 : d[k].size;
        if (d[k].status_ptr) G32(d[k].status_ptr) = d[k].status;
        if (d[k].callback) nds_guest_callback(d[k].callback, d[k].sctx, d[k].pctx, d[k].status);
    }
}
uint32_t nds_StreamFlush(uint32_t s)
{
    nds_done d[2 * NDS_MAX_PACKETS]; int n = 0;
    nds_lock(); nds_voice *v = nds_find(s); if (v) n = collect(v, d, 2 * NDS_MAX_PACKETS, 1); nds_unlock();
    report(d, n);
    return v ? DS_OK : DSERR_GENERIC;
}
uint32_t nds_StreamRelease(uint32_t s)
{
    nds_done d[2 * NDS_MAX_PACKETS]; int n = 0; LONG r = 0;
    nds_lock(); nds_voice *v = nds_find(s);
    if (v) {
        r = --v->refs;
        if (r <= 0) {   /* last release: pending packets are flushed, then the stream goes */
            n = collect(v, d, 2 * NDS_MAX_PACKETS, 1);
            fprintf(stderr, "[NDS] stream %08X released: %u packets submitted, %u completed\n", s, v->submitted, v->completed_n + n);
            v->handle = 0; v->stream = 0; r = 0;
        }
    }
    nds_unlock();
    report(d, n);
    return (uint32_t)r;
}
static volatile LONG s_dowork_calls;
uint32_t nds_DoWork(void)
{
    static __declspec(thread) int busy;
    if (busy) return DS_OK;
    busy = 1;
    InterlockedIncrement(&s_dowork_calls);
    nds_done d[2 * NDS_MAX_PACKETS]; int n = 0;
    nds_lock();
    for (int i = 0; i < NDS_MAX_VOICES && n < NDS_MAX_PACKETS; i++)
        if (s_v[i].handle && s_v[i].stream && s_v[i].dn) n += collect(&s_v[i], d + n, 2 * NDS_MAX_PACKETS - n, 0);
    nds_unlock();
    report(d, n);
    busy = 0;
    return DS_OK;
}
