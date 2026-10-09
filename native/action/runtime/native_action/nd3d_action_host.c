/* Native graphics for the Action engine: what the shared native D3D library
 * (runtime/native, generated from the Driving sources) and the Direct3D 11
 * renderer (runtime/lean) expect from their host program.
 *
 * In Driving these come from the lean runtime (lean_gpu.c, lean_hang.c,
 * driving_present201.c, kernel_bridge.c). Here they are mapped onto the Action
 * runtime: the game window is fb_present.c's window, frames shown are counted
 * there (window title, crash-capture frame counter), and guest memory is the
 * same flat mapping. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ptrdiff_t xbox_GetMemoryOffset(void);
extern uint32_t xbox_ContiguousAllocatedBytes(void);
extern HWND nightfire_window_hwnd(void);
extern void nightfire_window_direct(int on);
extern void nightfire_window_frame_shown(void);

volatile LONG lean_present_count;      /* game frames presented */
volatile LONG lean_mark_request;       /* no F9 marks in the Action runtime */
volatile long long lean_vblank_due;    /* 0: in-between frames time frames by arrival */
volatile long long lean_vblank_next;

/* Guest pointer for a guest address range, or NULL when it is not mapped
 * (same rules as Driving's lean_gpu.c). */
uint8_t *lean_guest(uint32_t va, uint32_t bytes)
{
    uint32_t allocated = xbox_ContiguousAllocatedBytes();
    if (va >= 0x80000000u) {
        uint64_t off = (uint64_t)va - 0x80000000u;
        if (off + bytes > allocated) {
            if (off + bytes > 0x04000000u) return NULL;
            uintptr_t at = (uintptr_t)xbox_GetMemoryOffset() + va, end = at + bytes;
            while (at < end) {
                MEMORY_BASIC_INFORMATION m;
                if (!VirtualQuery((void *)at, &m, sizeof m) || m.State != MEM_COMMIT || (m.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return NULL;
                at = (uintptr_t)m.BaseAddress + m.RegionSize;
            }
        }
    } else if ((uint64_t)va + bytes > 0x04000000u) return NULL;
    return (uint8_t *)((uintptr_t)xbox_GetMemoryOffset() + va);
}

/* The Action game steps its logic at 60 Hz (NIGHTFIRE_GAME_RATE94). */
int lean_vblank_hz(void) { return 60; }

void lean_hang_frame(void) { InterlockedIncrement(&lean_present_count); }
void lean_session_test_tick(void) {}
void lean_session_mark_picture(long n, const uint32_t *pix) { (void)n; (void)pix; }

HWND driving_present201_window(void) { return nightfire_window_hwnd(); }
void driving_present201_direct(int on) { nightfire_window_direct(on); }
int driving_present201_capture_due(void) { return 0; }
/* lean_d3d_sync_guest (Driving's LEAN_SYNC_VERTEX_RT copy-back, off by default) now
 * comes with the shared renderer copy (re-synchronised 2026-10-08, exchange-shadows). */

/* A frame was shown: by the swap chain (pixels NULL) or, before the swap chain
 * is up, as CPU pixels, which go to the GDI window. */
void driving_present201(const uint8_t *pixels, uint32_t physical)
{
    (void)physical;
    if (pixels) {
        extern void nightfire_window_show_pixels(const uint32_t *pixels);
        nightfire_window_show_pixels((const uint32_t *)pixels);
    }
    nightfire_window_frame_shown();
}
