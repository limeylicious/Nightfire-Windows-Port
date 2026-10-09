/* NIGHTFIRE_NATIVE_SOUND glue: answers the XDK DirectSound entry points the Action
 * game and the XMV player call (survey: native-driving/action-audio). Generated code
 * reaches this through one hook in RECOMP_ABI_CALL (src/recomp/gen/recomp_types.h,
 * added by scripts/nds_action_hook.py), before the per-call checkpoint, so the older
 * audio layers never see these calls. Indirect calls through the stream vtable
 * 0x16264C arrive here too, with their run-time VA. Every entry point is stdcall: the
 * guest stack holds the return address, then the arguments; we read them, run the
 * native implementation, set eax and pop like the routine's `ret N`. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
#define NIGHTFIRE_MEMORY_TRACE_IMPL
#include "recomp_types.h"
#include "recomp_funcs.h"
#include "nds_action.h"

#define ARG(i)  MEM32(g_esp + 4u * (i))
#define ARGP(i) (g_esp + 4u * (i))

/* Guest callback on the calling guest thread: save every guest register (the
 * callback may clobber caller-saved ones, and FP/MMX/SSE state), push
 * (status, packet context, stream context) and a return address inside DirectSound
 * (0x113525, as the existing movie-audio path does), call, restore. */
void nds_guest_callback(uint32_t fn_va, uint32_t stream_ctx, uint32_t packet_ctx, uint32_t status)
{
    recomp_func_t fn = recomp_lookup(fn_va);
    if (!fn) { fprintf(stderr, "[NDS] unresolved stream callback %08X\n", fn_va); return; }
    uint32_t *gp[] = { &g_eax, &g_ebx, &g_ecx, &g_edx, &g_esi, &g_edi, &g_esp, &g_ebp, &g_seh_ebp, &g_fs_base };
    uint32_t saved[10];
    RecompMmx *mp[] = { &g_mm0, &g_mm1, &g_mm2, &g_mm3, &g_mm4, &g_mm5, &g_mm6, &g_mm7 }, ms[8];
    RecompXmm *xp[] = { &g_xmm0, &g_xmm1, &g_xmm2, &g_xmm3, &g_xmm4, &g_xmm5, &g_xmm6, &g_xmm7 }, xs[8];
    double fs[8]; int ft = g_fp_top, fc = g_fp_cmp, df = g_df; uint16_t cw = g_fp_control_word;
    for (unsigned i = 0; i < 10; i++) saved[i] = *gp[i];
    for (unsigned i = 0; i < 8; i++) { ms[i] = *mp[i]; xs[i] = *xp[i]; fs[i] = g_fp_stack[i]; }
    PUSH32(g_esp, status); PUSH32(g_esp, packet_ctx); PUSH32(g_esp, stream_ctx);
    PUSH32(g_esp, 0x00113525u);
    fn();
    if (g_esp != saved[6]) {
        static unsigned warned;
        if (warned++ < 4) fprintf(stderr, "[NDS] stream callback %08X left the stack unbalanced (%08X vs %08X)\n", fn_va, g_esp, saved[6]);
    }
    for (unsigned i = 0; i < 10; i++) *gp[i] = saved[i];
    for (unsigned i = 0; i < 8; i++) { *mp[i] = ms[i]; *xp[i] = xs[i]; g_fp_stack[i] = fs[i]; }
    g_fp_top = ft; g_fp_cmp = fc; g_fp_control_word = cw; g_df = df;
}

/* Call ring (no printing, so timing is unchanged): last 256 calls, polling excluded.
 * NIGHTFIRE_NATIVE_SOUND_LOG=1 also prints the first 400 of them as they happen. */
static struct { ULONGLONG t; uint32_t va, a1, a2, a3, a4, r; DWORD tid; } ring[256];
static volatile LONG ring_n;
void nds_ring_dump(void)
{
    LONG n = ring_n;
    fprintf(stderr, "[NDS-RING] last DirectSound calls (newest last, polling excluded), %ld total\n", n);
    for (LONG k = n > 256 ? n - 256 : 0; k < n; k++) {
        int i = k & 255;
        fprintf(stderr, "[NDS-RING] t=%llu tid=%lu va=%08X args %08X %08X %08X %08X -> %08X\n",
                ring[i].t, ring[i].tid, ring[i].va, ring[i].a1, ring[i].a2, ring[i].a3, ring[i].a4, ring[i].r);
    }
}

int nds_override(uint32_t va)
{
    if (!nds_on()) return 0;
    uint32_t r; int n;
    switch (va) {
    /* DirectSound object and listener */
    case 0x001148B8u: r = nds_DirectSoundCreate(ARG(1), ARG(2), ARG(3)); n = 3; break;
    case 0x0011275Fu: r = nds_UseFullHRTF(); n = 0; break;
    case 0x0011338Bu: r = nds_DownloadEffectsImage(ARG(1), ARG(2), ARG(3), ARG(4), ARG(5)); n = 5; break;
    case 0x00112733u: r = nds_ReleaseDS(ARG(1)); n = 1; break;
    case 0x001133B2u: r = nds_SynchPlayback(ARG(1)); n = 1; break;
    case 0x00113C13u: r = nds_CommitDeferredSettings(ARG(1)); n = 1; break;
    case 0x00114334u: r = nds_SetOrientation(ARGP(2), ARG(8)); n = 8; break;
    case 0x0011437Eu: r = nds_ListenerSetPosition(ARGP(2), ARG(5)); n = 5; break;
    case 0x001143B3u: r = nds_ListenerSetVelocity(ARGP(2), ARG(5)); n = 5; break;
    case 0x001134FDu: r = nds_DoWork(); n = 0; break;
    /* buffers */
    case 0x001146FEu: r = nds_CreateSoundBuffer(ARG(1), ARG(2), ARG(3), ARG(4)); n = 4; break;
    case 0x00112749u: r = nds_ReleaseBuffer(ARG(1)); n = 1; break;
    case 0x001143E8u: r = nds_SetBufferData(ARG(1), ARG(2), ARG(3)); n = 3; break;
    case 0x001133CAu: r = nds_SetVolume(ARG(1), ARG(2)); n = 2; break;
    case 0x001133E6u: r = nds_SetHeadroom(ARG(1), ARG(2)); n = 2; break;
    case 0x00113402u: r = nds_SetMixBins(ARG(1), ARG(2)); n = 2; break;
    case 0x0011341Eu: r = nds_SetMixBinVolumes(ARG(1), ARG(2)); n = 2; break;
    case 0x0011343Au: r = nds_Play(ARG(1), ARG(2), ARG(3), ARG(4)); n = 4; break;
    case 0x0011345Eu: r = nds_Stop(ARG(1)); n = 1; break;
    case 0x00113476u: r = nds_SetLoopRegion(ARG(1), ARG(2), ARG(3)); n = 3; break;
    case 0x00113496u: r = nds_GetStatus(ARG(1), ARG(2)); n = 2; break;
    case 0x001134B2u: r = nds_GetCurrentPosition(ARG(1), ARG(2), ARG(3)); n = 3; break;
    case 0x001134D2u: r = nds_SetCurrentPosition(ARG(1), ARG(2)); n = 2; break;
    case 0x00113C2Bu: r = nds_SetFrequency(ARG(1), ARG(2)); n = 2; break;
    case 0x00113C47u: r = nds_Buffer3D(ARG(1), 0, ARGP(2), 0, ARG(3)); n = 3; break;   /* SetMaxDistance */
    case 0x00113C6Bu: r = nds_Buffer3D(ARG(1), 1, ARGP(2), 0, ARG(3)); n = 3; break;   /* SetMinDistance */
    case 0x00113C8Fu: r = nds_Buffer3D(ARG(1), 2, ARGP(2), 0, ARG(5)); n = 5; break;   /* SetPosition */
    case 0x00113CC4u: r = nds_Buffer3D(ARG(1), 3, ARGP(2), 0, ARG(5)); n = 5; break;   /* SetVelocity */
    case 0x00113CF9u: r = nds_Buffer3D(ARG(1), 4, ARGP(2), ARG(3), ARG(4)); n = 4; break;   /* SetRolloffCurve */
    case 0x00113D1Du: r = nds_SetI3DL2Source(ARG(1), ARG(2), ARG(3)); n = 3; break;
    /* streams: C wrappers (tail jumps to 1132E7/113339/113135) and vtable 0x16264C slots */
    case 0x001148FFu: r = nds_CreateStream(ARG(1), ARG(2)); n = 2; break;
    case 0x001134EEu: case 0x001132E7u: r = nds_SetVolume(ARG(1), ARG(2)); n = 2; break;
    case 0x001134F3u: case 0x00113339u: r = nds_SetMixBins(ARG(1), ARG(2)); n = 2; break;
    case 0x001134F8u: case 0x00113135u: r = nds_StreamPause(ARG(1), ARG(2)); n = 2; break;
    case 0x00112ED7u: r = nds_StreamAddRef(ARG(1)); n = 1; break;
    case 0x00112F1Eu: r = nds_StreamRelease(ARG(1)); n = 1; break;
    case 0x00112F6Cu: r = nds_StreamGetInfo(ARG(1), ARG(2)); n = 2; break;
    case 0x0011306Du: r = nds_StreamGetStatus(ARG(1), ARG(2)); n = 2; break;
    case 0x001130BEu: r = nds_StreamProcess(ARG(1), ARG(2), ARG(3)); n = 3; break;
    case 0x00112FD3u: r = nds_StreamDiscontinuity(ARG(1)); n = 1; break;
    case 0x00113020u: r = nds_StreamFlush(ARG(1)); n = 1; break;
    default: return 0;
    }
    if (va != 0x00113496u && va != 0x001134B2u && va != 0x001134FDu) {   /* polling calls are not recorded */
        LONG k = InterlockedIncrement(&ring_n) - 1; int i = k & 255;
        ring[i].t = GetTickCount64(); ring[i].tid = GetCurrentThreadId(); ring[i].va = va;
        ring[i].a1 = n > 0 ? ARG(1) : 0; ring[i].a2 = n > 1 ? ARG(2) : 0; ring[i].a3 = n > 2 ? ARG(3) : 0; ring[i].a4 = n > 3 ? ARG(4) : 0; ring[i].r = r;
        static int log = -1; if (log < 0) { const char *e = getenv("NIGHTFIRE_NATIVE_SOUND_LOG"); log = e && e[0] == '1'; }
        if (log && k < 400) fprintf(stderr, "[NDS-CALL] tid=%lu va=%08X args %08X %08X %08X %08X -> %08X\n",
                                    ring[i].tid, va, ring[i].a1, ring[i].a2, ring[i].a3, ring[i].a4, r);
    }
    g_eax = r;
    g_esp += 4u + 4u * (uint32_t)n;   /* stdcall: return address + arguments */
    return 1;
}
