/* LEAN_AUDIO_NATIVE glue: replaces the 19 XDK DirectSound C-API wrappers the
 * game calls (see lean_dsound.c). Generated code reaches this through one hook in
 * RECOMP_ABI_CALL (src/recomp/gen/recomp_types.h, added by
 * scripts/apply_native_audio_hook.py). Each wrapper is stdcall: on entry the
 * guest stack holds the return address, then the arguments; we read them, run
 * the native implementation, set eax and pop like the wrapper's `ret N`. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#  define GLUE_TLS __declspec(thread)
#else
#  define GLUE_TLS _Thread_local
#endif
extern GLUE_TLS uint32_t g_eax, g_esp;
extern ptrdiff_t g_xbox_mem_offset;
#define ARG(i) (*(volatile uint32_t *)(g_xbox_mem_offset + (uintptr_t)(g_esp + 4u * (i))))

int lean_audio_native_on(void);
uint32_t lean_ds_DirectSoundCreate(uint32_t, uint32_t, uint32_t);
uint32_t lean_ds_DownloadEffectsImage(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t lean_ds_SetI3DL2Listener(uint32_t, uint32_t, uint32_t);
uint32_t lean_ds_CreateSoundBuffer(uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t lean_ds_DirectSoundCreateBuffer(uint32_t, uint32_t);
uint32_t lean_ds_ReleaseDS(uint32_t);
uint32_t lean_ds_ReleaseBuffer(uint32_t);
uint32_t lean_ds_Play(uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t lean_ds_Stop(uint32_t);
uint32_t lean_ds_SetBufferData(uint32_t, uint32_t, uint32_t);
uint32_t lean_ds_SetLoopRegion(uint32_t, uint32_t, uint32_t);
uint32_t lean_ds_SetCurrentPosition(uint32_t, uint32_t);
uint32_t lean_ds_GetCurrentPosition(uint32_t, uint32_t, uint32_t);
uint32_t lean_ds_SetMixBinsA(uint32_t, uint32_t);
uint32_t lean_ds_SetMixBinsB(uint32_t, uint32_t);
uint32_t lean_ds_SetVolume(uint32_t, uint32_t);
uint32_t lean_ds_SetFrequency(uint32_t, uint32_t);
uint32_t lean_ds_GetStatus(uint32_t, uint32_t);
uint32_t lean_ds_OutCall2(uint32_t, uint32_t);

/* LEAN_AUDIO_DSLOG=1 (diagnostic, both modes): track what the game asks of each
 * buffer and log every Play with its data, volume, frequency and the per-bin
 * attenuation the XDK would program (headroom - lVolume - binVolume, 1/64 dB
 * units like the chip's volume registers). */
#include <stdio.h>
#include <windows.h>
extern volatile LONG lean_present_count;
static int dslog = -1;
static struct { uint32_t buf, data, bytes, freq; int32_t lvol; uint32_t nb, bin[8]; int32_t mb[8]; uint32_t created_by; } trk[256];
static int trk_find(uint32_t buf, int make)
{
    for (int i = 0; i < 256; i++) if (trk[i].buf == buf) return i;
    if (!make) return -1;
    for (int i = 0; i < 256; i++) if (!trk[i].buf) { memset(&trk[i], 0, sizeof trk[i]); trk[i].buf = buf; return i; }
    return -1;
}
#define GM32(a) (*(volatile uint32_t *)(g_xbox_mem_offset + (uintptr_t)(a)))
volatile uint32_t lean_ds_last_play_buf;   /* read by the chip voice log to tag VOICE_ON */
static void dslog_att(int i)
{
    char line[400]; int len = snprintf(line, sizeof line, "[LEAN-DSATT] f=%ld buf %08X", lean_present_count, trk[i].buf);
    for (uint32_t b = 0; b < trk[i].nb && len < 360; b++) {
        int32_t att = 600 - trk[i].lvol - trk[i].mb[b];
        uint32_t q = att < 0 ? 0xFFFu : ((uint32_t)att * 64u / 100u > 0xFFF ? 0xFFF : (uint32_t)att * 64u / 100u);
        len += snprintf(line + len, sizeof line - len, " %u:%03X", trk[i].bin[b], q);
    }
    fprintf(stderr, "%s\n", line);
}
static void dslog_pre(uint32_t va)
{
    int i;
    switch (va) {
    case 0x0017BE3Bu: if ((i = trk_find(ARG(1), 1)) >= 0) { trk[i].data = ARG(2); trk[i].bytes = ARG(3); } break;
    case 0x0017B5C4u: if ((i = trk_find(ARG(1), 1)) >= 0) { trk[i].lvol = (int32_t)ARG(2); dslog_att(i); } break;
    case 0x0017B996u: if ((i = trk_find(ARG(1), 1)) >= 0) trk[i].freq = ARG(2); break;
    case 0x0017B5FCu: case 0x0017B618u:
        if ((i = trk_find(ARG(1), 1)) >= 0 && ARG(2)) {
            uint32_t pm = ARG(2), n = GM32(pm), pr = GM32(pm + 4); if (n > 8) n = 8;
            if (va == 0x0017B5FCu) { trk[i].nb = 0; for (uint32_t k = 0; k < n && pr; k++) { trk[i].bin[k] = GM32(pr + 8 * k); trk[i].mb[k] = (int32_t)GM32(pr + 8 * k + 4); } trk[i].nb = n; }
            else for (uint32_t k = 0; k < n && pr; k++) for (uint32_t b = 0; b < trk[i].nb; b++) if (trk[i].bin[b] == GM32(pr + 8 * k)) trk[i].mb[b] = (int32_t)GM32(pr + 8 * k + 4);
            dslog_att(i);
        }
        break;
    case 0x0017B634u:
        lean_ds_last_play_buf = ARG(1);
        if ((i = trk_find(ARG(1), 1)) >= 0) {
            char line[512]; int len = snprintf(line, sizeof line, "[LEAN-DSPLAY] f=%ld buf %08X data %08X bytes %u freq %u lvol %d flags %u bins",
                                               lean_present_count, trk[i].buf, trk[i].data, trk[i].bytes, trk[i].freq, trk[i].lvol, ARG(4));
            for (uint32_t b = 0; b < trk[i].nb && len < 480; b++) {
                int32_t att = 600 - trk[i].lvol - trk[i].mb[b];
                uint32_t q = att < 0 ? 0xFFFu : ((uint32_t)att * 64u / 100u > 0xFFF ? 0xFFF : (uint32_t)att * 64u / 100u);
                len += snprintf(line + len, sizeof line - len, " %u:%03X", trk[i].bin[b], q);
            }
            fprintf(stderr, "%s\n", line);
        }
        {   /* LEAN_AUDIO_PLAY_CHAIN=from,to (diagnostic): call chain of each Play in that game-frame range */
            static int init; static long cf = -1, ct = -1; static unsigned chains;
            if (!init) { const char *e = getenv("LEAN_AUDIO_PLAY_CHAIN"); if (e) sscanf(e, "%ld,%ld", &cf, &ct); init = 1; }
            if (cf >= 0 && lean_present_count >= cf && lean_present_count <= ct && chains++ < 600) {
                extern void lean_print_host_chain(const char *tag);
                char tag[96]; snprintf(tag, sizeof tag, "play buf %08X data %08X f=%ld", ARG(1), i >= 0 ? trk[i].data : 0, lean_present_count);
                lean_print_host_chain(tag);
            }
        }
        break;
    }
}

/* Native call ring (always on with LEAN_AUDIO_NATIVE, no printing so timing is
 * unchanged): last 256 DirectSound calls with thread, arguments, result and the
 * values written back through out-pointers; printed by the freeze monitor. */
static struct { ULONGLONG t; uint32_t va, a1, a2, a3, a4, r, out1, out2; DWORD tid; } ring[256];
static volatile LONG ring_n;
void lean_ds_ring_dump(void)
{
    LONG n = ring_n;
    fprintf(stderr, "[LEAN-DSRING] last DirectSound calls (newest last, polling excluded), %ld total\n", n);
    for (LONG k = n > 256 ? n - 256 : 0; k < n; k++) {
        int i = k & 255;
        fprintf(stderr, "[LEAN-DSRING] t=%llu tid=%lu va=%08X args %08X %08X %08X %08X -> %08X out %08X %08X\n",
                ring[i].t, ring[i].tid, ring[i].va, ring[i].a1, ring[i].a2, ring[i].a3, ring[i].a4, ring[i].r, ring[i].out1, ring[i].out2);
    }
}

int lean_ds_override(uint32_t va)
{
    uint32_t r; int n;
    {   /* LEAN_DSRING_DUMP_AT=seconds (diagnostic): dump the call ring once at that time */
        static long at = -1; static ULONGLONG t0; static volatile LONG done;
        if (at < 0) { const char *e = getenv("LEAN_DSRING_DUMP_AT"); at = e ? atol(e) : 0; t0 = GetTickCount64(); }
        if (at > 0 && !done && GetTickCount64() - t0 >= (ULONGLONG)at * 1000 && !InterlockedExchange(&done, 1)) lean_ds_ring_dump();
    }
    if (dslog < 0) { const char *e = getenv("LEAN_AUDIO_DSLOG"); dslog = e && e[0] == '1'; }
    if (dslog) dslog_pre(va);
    if (!lean_audio_native_on()) return 0;
    {   /* LEAN_NATIVE_CALL_DELAY_US (diagnostic only): spin this long in every native call,
         * to test whether a start-up race depends on DirectSound call latency */
        static long delay = -1; static LARGE_INTEGER hz;
        if (delay < 0) { const char *e = getenv("LEAN_NATIVE_CALL_DELAY_US"); delay = e ? atol(e) : 0; QueryPerformanceFrequency(&hz); }
        static char only[256] = "?"; if (only[0] == '?') { const char *e2 = getenv("LEAN_NATIVE_CALL_DELAY_VA"); snprintf(only, sizeof only, "%s", e2 ? e2 : ""); }
        char key[16]; snprintf(key, sizeof key, "%X", va);
        if (delay > 0 && (!only[0] || strstr(only, key))) { LARGE_INTEGER a0, a1; QueryPerformanceCounter(&a0); do QueryPerformanceCounter(&a1); while ((a1.QuadPart - a0.QuadPart) * 1000000 / hz.QuadPart < delay); }
    }
    switch (va) {
    case 0x0017C259u: r = lean_ds_DirectSoundCreate(ARG(1), ARG(2), ARG(3)); n = 3; break;
    case 0x0017B59Du: r = lean_ds_DownloadEffectsImage(ARG(1), ARG(2), ARG(3), ARG(4), ARG(5)); n = 5; break;
    case 0x0017BE1Bu: r = lean_ds_SetI3DL2Listener(ARG(1), ARG(2), ARG(3)); n = 3; break;
    case 0x0017C09Fu: r = lean_ds_CreateSoundBuffer(ARG(1), ARG(2), ARG(3), ARG(4)); n = 4; break;
    case 0x0017C2A0u: r = lean_ds_DirectSoundCreateBuffer(ARG(1), ARG(2)); n = 2; break;
    case 0x0017AD34u: r = lean_ds_ReleaseDS(ARG(1)); n = 1; break;
    case 0x0017AD4Au: r = lean_ds_ReleaseBuffer(ARG(1)); n = 1; break;
    case 0x0017B634u: r = lean_ds_Play(ARG(1), ARG(2), ARG(3), ARG(4)); n = 4; break;
    case 0x0017B658u: r = lean_ds_Stop(ARG(1)); n = 1; break;
    case 0x0017BE3Bu: r = lean_ds_SetBufferData(ARG(1), ARG(2), ARG(3)); n = 3; break;
    case 0x0017B670u: r = lean_ds_SetLoopRegion(ARG(1), ARG(2), ARG(3)); n = 3; break;
    case 0x0017B6CCu: r = lean_ds_SetCurrentPosition(ARG(1), ARG(2)); n = 2; break;
    case 0x0017B6ACu: r = lean_ds_GetCurrentPosition(ARG(1), ARG(2), ARG(3)); n = 3; break;
    case 0x0017B5FCu: r = lean_ds_SetMixBinsA(ARG(1), ARG(2)); n = 2; break;
    case 0x0017B618u: r = lean_ds_SetMixBinsB(ARG(1), ARG(2)); n = 2; break;
    case 0x0017B5C4u: r = lean_ds_SetVolume(ARG(1), ARG(2)); n = 2; break;
    case 0x0017B996u: r = lean_ds_SetFrequency(ARG(1), ARG(2)); n = 2; break;
    case 0x0017B5E0u: r = lean_ds_GetStatus(ARG(1), ARG(2)); n = 2; break;
    case 0x0017B690u: r = lean_ds_OutCall2(ARG(1), ARG(2)); n = 2; break;
    default: return 0;
    }
    if (va != 0x0017B690u && va != 0x0017B6ACu) {   /* polling calls are not recorded */
        LONG k = InterlockedIncrement(&ring_n) - 1; int i = k & 255;
        ring[i].t = GetTickCount64(); ring[i].tid = GetCurrentThreadId(); ring[i].va = va;
        ring[i].a1 = ARG(1); ring[i].a2 = n > 1 ? ARG(2) : 0; ring[i].a3 = n > 2 ? ARG(3) : 0; ring[i].a4 = n > 3 ? ARG(4) : 0; ring[i].r = r;
        ring[i].out1 = (va == 0x0017B6ACu && ARG(2)) ? GM32(ARG(2)) : (va == 0x0017B690u && ARG(2)) ? GM32(ARG(2)) : 0;
        ring[i].out2 = (va == 0x0017B6ACu && ARG(3)) ? GM32(ARG(3)) : 0; }
    g_eax = r;
    g_esp += 4u + 4u * (uint32_t)n;   /* stdcall: return address + arguments */
    return 1;
}
