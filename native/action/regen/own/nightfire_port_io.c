#include <stdint.h>
#include <stdio.h>
#include "../../../runtime/nightfire_video_io.h"
#include "../../nightfire_diagnostics.h"
/* This diagnostic model has no scanout synchronization; see its header. */
static NightfireVideoIo video_io;
#ifdef _WIN32
#include <windows.h>
static SRWLOCK video_io_lock = SRWLOCK_INIT;
#define VIDEO_LOCK() AcquireSRWLockExclusive(&video_io_lock)
#define VIDEO_UNLOCK() ReleaseSRWLockExclusive(&video_io_lock)
#else
/* Non-Windows test harness invokes this wrapper on one thread. */
#define VIDEO_LOCK() ((void)0)
#define VIDEO_UNLOCK() ((void)0)
#endif
uint8_t nightfire_port_read8(uint16_t port)
{
    uint8_t value;
    VIDEO_LOCK();
    int handled = nightfire_video_in8(&video_io, port, &value);
    VIDEO_UNLOCK();
    if (!handled) {
        fprintf(stderr, "Unsupported byte port read: 0x%04X\n", port);
        nightfire_diagnostic_stop(__func__, "unsupported port");
        return 0;
    }
    return value;
}
void nightfire_port_write8(uint16_t port, uint8_t value)
{
    VIDEO_LOCK();
    int handled = nightfire_video_out8(&video_io, port, value);
    VIDEO_UNLOCK();
    if (!handled) {
        fprintf(stderr, "Unsupported byte port write: 0x%04X\n", port);
        nightfire_diagnostic_stop(__func__, "unsupported port");
    }
}
