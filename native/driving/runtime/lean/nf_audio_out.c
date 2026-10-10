/* Our own final-mix output (2026-10-09). The mixer (Driving lean_dsound.c,
 * Action nds_action.c) hands over 48 kHz stereo 16-bit blocks and this file
 * plays them through XAudio2. Written from Microsoft's XAudio2 documentation
 * only; it replaces lean_audio_out.c / nds_out.c, which came with the
 * toolkit's emulated sound chip. One file for both games: Driving builds it as
 * is (xa2_*, lean_audio_*), Action through native_action/nds_out.c, which
 * defines NF_AO_ACTION first (nds_xa2_*, nds_audio_*).
 *
 * Pacing: the mixer calls queued_now() and mixes another block while fewer
 * than its target are queued (8 x 128 frames, about 21 ms). Each block is
 * copied into one slot of a 64-slot ring, because XAudio2 reads a buffer's
 * memory until that buffer has played; with at most 63 queued, the next slot
 * is always free.
 *
 * No sound device, or the device is lost (XAudio2 reports a critical error,
 * e.g. the headphones it played to were unplugged): queued_now() then counts
 * blocks against the clock, so the mixer and the game keep real time instead
 * of spinning or stalling, and the output is reopened after 1 s (then 2, 4,
 * 8 s while it keeps failing).
 *
 * Every function may be called from any thread (the mixer, the stats and F9
 * threads); the device is only touched under s_dev.
 *
 * LEAN_AUDIO_WAV=<file> (Driving) / NDS_AUDIO_WAV=<file> (Action): also write
 * everything submitted to that WAV file (diagnostic). */
#include <windows.h>
#include <objbase.h>
#include <xaudio2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _MSC_VER
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")
#endif

#ifdef NF_AO_ACTION
#define AO(n)        nds_##n
#define AO_TAG       "[NDS-OUT]"
#define AO_WAV_ENV   "NDS_AUDIO_WAV"
#define AO_PEAK      nds_audio_peak
#define AO_RMS       nds_audio_rms
#define AO_NONSILENT nds_audio_nonsilent
#else
#define AO(n)        n
#define AO_TAG       "[LEAN-OUT]"
#define AO_WAV_ENV   "LEAN_AUDIO_WAV"
#define AO_PEAK      lean_audio_peak
#define AO_RMS       lean_audio_rms
#define AO_NONSILENT lean_audio_nonsilent
#endif

#define AO_RATE    48000
#define AO_SLOTS   64                /* XAUDIO2_MAX_QUEUED_BUFFERS */
#define AO_FRAMES  2048              /* largest piece one slot holds */

static SRWLOCK s_dev = SRWLOCK_INIT;
static IXAudio2 *s_xa;
static IXAudio2MasteringVoice *s_master;
static IXAudio2SourceVoice *s_src;
static int16_t s_ring[AO_SLOTS][AO_FRAMES * 2];
static int s_next;
static volatile LONG s_lost;             /* set by the engine's critical-error callback */
static volatile LONG s_queued = -1;      /* last BuffersQueued seen */
static volatile LONG s_min_queued = -1;  /* lowest queue seen at a submit since the last health read */
static volatile LONG s_submitted, s_dropped;
static UINT32 s_glitch_base, s_glitch_done; /* glitch count at open; glitches of closed engines */
static int s_last_frames = 128;
static LONG64 s_clock_end;               /* clock mode: when the blocks "played" so far would end */
static ULONGLONG s_retry_at;
static DWORD s_retry_ms = 1000;
static int s_open_fails;
static FILE *s_wav; static uint32_t s_wav_bytes, s_wav_mark;

/* ---- engine callback: only the critical error matters ---------------------- */
static void STDMETHODCALLTYPE cb_pass_start(IXAudio2EngineCallback *t) { (void)t; }
static void STDMETHODCALLTYPE cb_pass_end(IXAudio2EngineCallback *t) { (void)t; }
static void STDMETHODCALLTYPE cb_error(IXAudio2EngineCallback *t, HRESULT e)
{
    (void)t;
    InterlockedExchange(&s_lost, 1);
    fprintf(stderr, AO_TAG " sound device error %08lX; keeping time without sound until it reopens\n", (unsigned long)e);
}
static IXAudio2EngineCallbackVtbl s_cb_vtbl = { cb_pass_start, cb_pass_end, cb_error };
static IXAudio2EngineCallback s_cb = { &s_cb_vtbl };

static LONG64 qpc(void) { LARGE_INTEGER c; QueryPerformanceCounter(&c); return c.QuadPart; }
static LONG64 qpf(void) { static LARGE_INTEGER f; if (!f.QuadPart) QueryPerformanceFrequency(&f); return f.QuadPart; }

static void wav_header(void)
{
    uint8_t h[44];
    uint32_t riff = 36 + s_wav_bytes, fmtlen = 16, rate = AO_RATE, bps = AO_RATE * 4;
    uint16_t fmt = 1, ch = 2, align = 4, bits = 16;
    memcpy(h, "RIFF", 4); memcpy(h + 4, &riff, 4); memcpy(h + 8, "WAVEfmt ", 8); memcpy(h + 16, &fmtlen, 4);
    memcpy(h + 20, &fmt, 2); memcpy(h + 22, &ch, 2); memcpy(h + 24, &rate, 4); memcpy(h + 28, &bps, 4);
    memcpy(h + 32, &align, 2); memcpy(h + 34, &bits, 2); memcpy(h + 36, "data", 4); memcpy(h + 40, &s_wav_bytes, 4);
    fseek(s_wav, 0, SEEK_SET); fwrite(h, 1, sizeof h, s_wav); fseek(s_wav, 0, SEEK_END);
}

static UINT32 glitches_now(void)
{
    XAUDIO2_PERFORMANCE_DATA pd;
    memset(&pd, 0, sizeof pd);
    if (s_xa) s_xa->lpVtbl->GetPerformanceData(s_xa, &pd);
    return pd.GlitchesSinceEngineStarted;
}

static void close_device(void)
{
    if (s_xa) s_glitch_done += glitches_now() - s_glitch_base;
    if (s_src) { s_src->lpVtbl->DestroyVoice(s_src); s_src = NULL; }
    if (s_master) { s_master->lpVtbl->DestroyVoice(s_master); s_master = NULL; }
    if (s_xa) { s_xa->lpVtbl->UnregisterForCallbacks(s_xa, &s_cb); s_xa->lpVtbl->Release(s_xa); s_xa = NULL; }
    InterlockedExchange(&s_queued, -1);
}

static int open_device(void)
{
    const char *why = NULL;
    HRESULT hr = XAudio2Create(&s_xa, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) { s_xa = NULL; why = "XAudio2Create"; goto fail; }
    s_xa->lpVtbl->RegisterForCallbacks(s_xa, &s_cb);
    hr = s_xa->lpVtbl->CreateMasteringVoice(s_xa, &s_master, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE,
                                            0, NULL, NULL, AudioCategory_GameEffects);
    if (FAILED(hr)) { s_master = NULL; why = "no sound device"; goto fail; }
    WAVEFORMATEX wf;
    memset(&wf, 0, sizeof wf);
    wf.wFormatTag = WAVE_FORMAT_PCM; wf.nChannels = 2; wf.nSamplesPerSec = AO_RATE;
    wf.wBitsPerSample = 16; wf.nBlockAlign = 4; wf.nAvgBytesPerSec = AO_RATE * 4;
    hr = s_xa->lpVtbl->CreateSourceVoice(s_xa, &s_src, &wf, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);
    if (FAILED(hr)) { s_src = NULL; why = "source voice"; goto fail; }
    hr = s_src->lpVtbl->Start(s_src, 0, XAUDIO2_COMMIT_NOW);
    if (FAILED(hr)) { why = "start"; goto fail; }
    s_glitch_base = glitches_now();
    InterlockedExchange(&s_lost, 0);
    s_next = 0;
    s_open_fails = 0; s_retry_ms = 1000;
    return 1;
fail:
    if (++s_open_fails <= 3) fprintf(stderr, AO_TAG " output not opened: %s (%08lX)%s\n", why, (unsigned long)hr,
                                     s_open_fails == 3 ? "; retrying quietly" : "");
    close_device();
    InterlockedExchange(&s_lost, 0);
    s_retry_at = GetTickCount64() + s_retry_ms;
    if (s_retry_ms < 8000) s_retry_ms *= 2;
    return 0;
}

/* Lost or missing device (called under s_dev): drop the old engine, try again when due.
 * The callback cannot do this itself: XAudio2 must not be released from its own thread. */
static void recover(void)
{
    if (s_lost && s_xa) { close_device(); InterlockedExchange(&s_lost, 0); s_retry_at = GetTickCount64() + 1000; s_retry_ms = 1000; }
    if (!s_src && GetTickCount64() >= s_retry_at && open_device()) fprintf(stderr, AO_TAG " sound device (re)opened\n");
}

int AO(xa2_init)(void)
{
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    BOOL pending;
    if (!InitOnceBeginInitialize(&once, 0, &pending, NULL) || !pending) return 1;
    /* XAudio2 runs in the multithreaded apartment; keep it alive without moving any thread into it. */
    CO_MTA_USAGE_COOKIE cookie;
    if (FAILED(CoIncrementMTAUsage(&cookie))) fprintf(stderr, AO_TAG " CoIncrementMTAUsage failed\n");
    const char *w = getenv(AO_WAV_ENV);
    AcquireSRWLockExclusive(&s_dev);
    int ok = open_device();
    if (w && *w && (s_wav = fopen(w, "wb")) != NULL) { wav_header(); fprintf(stderr, AO_TAG " also writing the output to %s\n", w); }
    ReleaseSRWLockExclusive(&s_dev);
    fprintf(stderr, AO_TAG " own XAudio2 output %s (%d Hz stereo, %d-slot ring)\n",
            ok ? "ready" : "has no device yet, keeping time without sound", AO_RATE, AO_SLOTS);
    InitOnceComplete(&once, 0, NULL);
    return 1;   /* even without a device the mixer runs, on the clock */
}

void AO(xa2_shutdown)(void)
{
    AcquireSRWLockExclusive(&s_dev);
    if (s_src) s_src->lpVtbl->Stop(s_src, 0, XAUDIO2_COMMIT_NOW);
    close_device();
    s_retry_at = (ULONGLONG)-1;   /* stay closed */
    if (s_wav) { wav_header(); fclose(s_wav); s_wav = NULL; }
    ReleaseSRWLockExclusive(&s_dev);
}

int AO(xa2_is_active)(void) { return s_src != NULL && !s_lost; }

int AO(xa2_queued_now)(void)
{
    int q = -1;
    AcquireSRWLockExclusive(&s_dev);
    if (s_lost || !s_src) recover();
    if (s_src && !s_lost) {
        XAUDIO2_VOICE_STATE st;
        s_src->lpVtbl->GetState(s_src, &st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        q = (int)st.BuffersQueued;
        InterlockedExchange(&s_queued, q);
    } else {   /* clock mode: blocks still "playing", rounded up */
        LONG64 left = s_clock_end - qpc(), block = (LONG64)s_last_frames * qpf() / AO_RATE;
        q = left <= 0 || block <= 0 ? 0 : (int)((left + block - 1) / block);
    }
    ReleaseSRWLockExclusive(&s_dev);
    return q;
}

int AO(xa2_submit_samples)(const int16_t *samples, int num_samples)
{
    if (!samples || num_samples <= 0) return 0;
    int ok = 1;
    AcquireSRWLockExclusive(&s_dev);
    if (s_wav) {
        fwrite(samples, 4, (size_t)num_samples, s_wav); s_wav_bytes += (uint32_t)num_samples * 4;
        if (s_wav_bytes - s_wav_mark >= AO_RATE * 4) { s_wav_mark = s_wav_bytes; wav_header(); }
    }
    s_last_frames = num_samples;
    if (!s_src || s_lost) {   /* clock mode */
        LONG64 now = qpc(), start = s_clock_end > now ? s_clock_end : now;
        s_clock_end = start + (LONG64)num_samples * qpf() / AO_RATE;
    } else {
        s_clock_end = 0;
        for (int done = 0; done < num_samples && ok; ) {
            int n = num_samples - done; if (n > AO_FRAMES) n = AO_FRAMES;
            XAUDIO2_VOICE_STATE st;
            s_src->lpVtbl->GetState(s_src, &st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
            LONG q = (LONG)st.BuffersQueued;
            if (s_min_queued < 0 || q < s_min_queued) InterlockedExchange(&s_min_queued, q);
            if (q >= AO_SLOTS - 1) { InterlockedIncrement(&s_dropped); ok = 0; break; }
            int16_t *slot = s_ring[s_next];
            memcpy(slot, samples + done * 2, (size_t)n * 4);
            XAUDIO2_BUFFER b;
            memset(&b, 0, sizeof b);
            b.AudioBytes = (UINT32)n * 4; b.pAudioData = (const BYTE *)slot;
            if (FAILED(s_src->lpVtbl->SubmitSourceBuffer(s_src, &b, NULL))) { InterlockedIncrement(&s_dropped); ok = 0; break; }
            s_next = (s_next + 1) % AO_SLOTS;
            InterlockedIncrement(&s_submitted);
            InterlockedExchange(&s_queued, q + 1);
            done += n;
        }
    }
    ReleaseSRWLockExclusive(&s_dev);
    return ok;
}

int AO(xa2_get_buffer_size)(void) { return s_last_frames; }

void AO(xa2_get_stats)(int *queued, int *dropped, int *submitted)
{
    if (queued) *queued = (int)s_queued;
    if (dropped) *dropped = (int)s_dropped;
    if (submitted) *submitted = (int)s_submitted;
}

/* glitches: XAudio2's count of processing passes that missed their deadline,
 * since the game started; min_queued: the lowest queue seen at a submit since
 * the last call (-1 if none), so a value near 0 means the mixer nearly ran dry. */
void AO(xa2_get_health)(int *glitches, int *min_queued)
{
    AcquireSRWLockExclusive(&s_dev);
    UINT32 g = s_glitch_done + (s_xa ? glitches_now() - s_glitch_base : 0);
    ReleaseSRWLockExclusive(&s_dev);
    if (glitches) *glitches = (int)g;
    if (min_queued) *min_queued = (int)InterlockedExchange(&s_min_queued, -1);
}

/* ---- level meter for logs and stats.csv ------------------------------------
 * Called by the mixer thread with every 256 output frames (about 188 times a
 * second). Over the last 188 calls: peak = largest |sample|, rms = root mean
 * square (both in 16-bit sample units), nonsilent = how many of those calls
 * had a peak above 16 (about -66 dBFS). */
volatile int AO_PEAK, AO_NONSILENT;
volatile float AO_RMS;
#define AO_WINDOWS 188
static struct { int peak; double sumsq; int frames; } s_win[AO_WINDOWS];
static int s_win_at;
static void ao_stats(const int16_t (*buf)[2], int n)
{
    int peak = 0; double sq = 0;
    for (int i = 0; i < n; i++) for (int c = 0; c < 2; c++) {
        int v = buf[i][c];
        sq += (double)v * v;
        if (v < 0) v = -v;
        if (v > peak) peak = v;
    }
    s_win[s_win_at].peak = peak; s_win[s_win_at].sumsq = sq; s_win[s_win_at].frames = n;
    s_win_at = (s_win_at + 1) % AO_WINDOWS;
    int p = 0, loud = 0; double total = 0; long frames = 0;
    for (int i = 0; i < AO_WINDOWS; i++) {
        if (s_win[i].peak > p) p = s_win[i].peak;
        if (s_win[i].peak > 16) loud++;
        total += s_win[i].sumsq; frames += s_win[i].frames;
    }
    AO_PEAK = p; AO_NONSILENT = loud; AO_RMS = frames ? (float)sqrt(total / (2.0 * frames)) : 0.0f;
}
#ifdef NF_AO_ACTION
void nds_audio_stats_native(const int16_t (*buf)[2], int n) { ao_stats(buf, n); }
#else
void lean_audio_stats(const int16_t (*buf)[2], int n) { ao_stats(buf, n); }
void lean_audio_stats_native(const int16_t (*buf)[2], int n) { ao_stats(buf, n); }
#endif
