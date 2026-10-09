/* Native D3D: switch, diagnostics, and the library's synchronisation routines.
 *
 * With the native library every draw is finished by the time the routine that
 * issued it returns (D3D11 orders the work; guest memory is read when the draw
 * is recorded). So fences are complete as soon as they are inserted, nothing is
 * ever busy, and waiting never blocks. The bookkeeping the rest of the library
 * reads (fence counter, completed-time word, kick-off pointer) is kept
 * consistent with that. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "nd3d_api.h"

int nd3d_on = -1;
void nd3d_init_switch(void)
{
    const char *v = getenv("LEAN_NATIVE_D3D");
    nd3d_on = v && v[0] == '1';
    if (nd3d_on) {
        extern const unsigned nd3d_native_count, nd3d_routine_count;
        fprintf(stderr, "[ND3D] native D3D library on: %u of %u replaced routines native\n",
                nd3d_native_count, nd3d_routine_count);
        fflush(stderr);
    }
}

void nd3d_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("[ND3D] ", stderr);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
}

/* Bring-up aid: an unported routine ran. Once per routine, with the caller. */
void nd3d_unported(uint32_t va)
{
    static volatile LONG seen[0x10000];
    unsigned slot = (va - 0x160000u) & 0xFFFFu;
    if (InterlockedExchange(&seen[slot], 1)) return;
    nd3d_log("not native yet: sub_%08X (called from %08X)\n", va, G32(g_esp));
}

/* ------------------------------------------------------------------ sync */

/* CPU writes to textures and vertex data: the renderer re-checks guest memory
 * of a resource once per "epoch". The library's own synchronisation points
 * (kick-off, fence waits, waits before a resource is locked or reused) are
 * exactly where the game may write such memory next, so each starts a new epoch. */
void lean_d3d_epoch(void);

/* Every fence is complete: publish the current counter as the completed time. */
static void complete_all(uint32_t dev)
{
    uint32_t done = G32(dev + DEV_DONE_PTR);
    if (done) G32(done) = G32(dev + DEV_TIME);
}

/* Push-buffer space. Only an unported routine still writes chip commands; its
 * words are never executed, so the buffer is simply reused from the start. */
static uint32_t rewind_push(uint32_t dev, uint32_t caller)
{
    static volatile LONG warned;
    uint32_t start = G32(dev + DEV_PB_START), end = G32(dev + DEV_PB_END);
    if (!InterlockedExchange(&warned, 1))
        nd3d_log("push buffer reused (an unported routine wrote chip commands; caller %08X)\n", caller);
    G32(dev + DEV_PUT) = start;
    G32(dev + DEV_LIMIT) = end - 0x204u;
    return start;
}

/* 0x16CC20 D3D_MakeRequestedSpace(min, wanted), stdcall, returns the write pointer. */
void n_0016CC20(void) { uint32_t dev = nd3d_device(); RET(rewind_push(dev, G32(g_esp)), 8); }

/* 0x16CDA0 D3DDevice_MakeSpace(), returns the write pointer. */
void n_0016CDA0(void) { uint32_t dev = nd3d_device(); RET(rewind_push(dev, G32(g_esp)), 0); }

/* 0x16CDB0 (ECX = device, dwords): make room for `dwords` more words. */
void n_0016CDB0(void)
{
    uint32_t dev = g_ecx, need = ARG(1);
    if (G32(dev + DEV_PUT) + need * 4u >= G32(dev + DEV_LIMIT) + 0x200u) rewind_push(dev, G32(g_esp));
    RET(G32(dev + DEV_PUT), 4);
}

/* 0x16C940 CDevice_KickOff (ECX = device): nothing to submit. */
void n_0016C940(void)
{
    uint32_t dev = g_ecx;
    G32(dev + DEV_KICKED) = G32(dev + DEV_PUT);
    G32(dev + DEV_FLAGS) |= 0x2000u;
    lean_d3d_epoch();
    complete_all(dev);
    RETV(0);
}

/* 0x16CA30 D3D_SetFence(flags): returns the fence time; it is already complete. */
void n_0016CA30(void)
{
    uint32_t dev = nd3d_device(), t = G32(dev + DEV_TIME);
    G32(dev + DEV_TIME) = t + 2u;
    complete_all(dev);
    RET(t, 4);
}

/* 0x16CAE0 D3D_BlockOnTime(time, flags): never waits. */
void n_0016CAE0(void) { lean_d3d_epoch(); complete_all(nd3d_device()); RETV(8); }

/* 0x166B00 D3DDevice_IsBusy(): never busy. */
void n_00166B00(void) { RET(0, 0); }

/* 0x16CDF0 D3D_KickOffAndWaitForIdle(): idle already. */
void n_0016CDF0(void) { lean_d3d_epoch(); complete_all(nd3d_device()); RETV(0); }

/* 0x16CE10 D3D_BlockOnResource(resource) and 0x16CE90 (vertex-buffer wait): no wait. */
void n_0016CE10(void) { lean_d3d_epoch(); RETV(4); }
void n_0016CE90(void) { lean_d3d_epoch(); RETV(4); }

/* 0x16EC10 CMiniport_IsFlipPending (ECX = miniport): reads device fields only. */
void orig_sub_0016EC10(void);
void n_0016EC10(void) { orig_sub_0016EC10(); }

/* ------------------------------------------------- guest ABI calls */
static void push_args(unsigned n, va_list ap)
{
    uint32_t a[16];
    for (unsigned i = 0; i < n && i < 16; i++) a[i] = va_arg(ap, uint32_t);
    for (unsigned i = n; i-- > 0;) { g_esp -= 4; G32(g_esp) = a[i]; }
    g_esp -= 4; G32(g_esp) = 0xFEEDF00Du;   /* return address (never used) */
}
uint32_t nd3d_call(void (*fn)(void), uint32_t ecx, uint32_t edx, unsigned nargs, ...)
{
    va_list ap; va_start(ap, nargs); push_args(nargs, ap); va_end(ap);
    g_ecx = ecx; g_edx = edx;
    fn();
    return g_eax;
}
uint32_t nd3d_call_cdecl(void (*fn)(void), unsigned nargs, ...)
{
    va_list ap; va_start(ap, nargs); push_args(nargs, ap); va_end(ap);
    fn();
    g_esp += 4u * nargs;
    return g_eax;
}

/* 0x166030 D3DDevice_BlockUntilVerticalBlank(): with the emulated vblank
 * interrupt on, the original waits for the event its interrupt routine sets.
 * With LEAN_NO_VBLANK=1 there is no such interrupt: wait for the next
 * display-refresh boundary (LEAN_VBLANK_HZ, default 50) on our own clock. */
void orig_sub_00166030(void);
void n_00166030(void)
{
    static int novb = -1; static HANDLE ht; static LARGE_INTEGER f, t0;
    if (novb < 0) { const char *e = getenv("LEAN_NO_VBLANK"); novb = e && e[0] == '1';
        if (novb) { QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0);
            ht = CreateWaitableTimerExW(NULL, NULL, 0x00000002 /* HIGH_RESOLUTION */, TIMER_ALL_ACCESS); } }
    if (!novb) { orig_sub_00166030(); return; }
    {   const char *v = getenv("LEAN_VBLANK_HZ"); long long hz = v ? atoi(v) : 50; if (hz < 25 || hz > 240) hz = 50;
        LARGE_INTEGER q; QueryPerformanceCounter(&q);
        long long period = f.QuadPart / hz, k = (q.QuadPart - t0.QuadPart) / period + 1;
        long long wait_q = t0.QuadPart + k * period - q.QuadPart;
        G32(nd3d_device() + 0x2558) = 0;   /* the event the original reset */
        if (ht) { LARGE_INTEGER d; d.QuadPart = -(long long)((double)wait_q * 1e7 / (double)f.QuadPart); if (!d.QuadPart) d.QuadPart = -1;
                  if (SetWaitableTimer(ht, &d, 0, NULL, NULL, FALSE)) WaitForSingleObject(ht, 100); else Sleep(1); }
        else Sleep((DWORD)(wait_q * 1000 / f.QuadPart));
    }
    RETV(0);
}
