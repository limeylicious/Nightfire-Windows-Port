/* Diagnostic TV-encoder output compatibility; no host hardware-port access. */
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include "../runtime/nightfire_video_io.h"
extern void driving_translation_stop(const char *, const char *);
static NightfireVideoIo driving_video_io;
static SRWLOCK driving_video_lock = SRWLOCK_INIT;
uint8_t driving_port_read8(uint16_t port)
{
    uint8_t value=0;
    AcquireSRWLockExclusive(&driving_video_lock);
    int handled=nightfire_video_in8(&driving_video_io,port,&value);
    ReleaseSRWLockExclusive(&driving_video_lock);
    if(!handled)driving_translation_stop(__func__,"unsupported input port");
    return value;
}
void driving_port_write8(uint16_t port, uint8_t value)
{
    AcquireSRWLockExclusive(&driving_video_lock);
    int handled = nightfire_video_out8(&driving_video_io, port, value);
    ReleaseSRWLockExclusive(&driving_video_lock);
    if (!handled) {
        fprintf(stderr, "[DRIVING PORT] unsupported write %04X=%02X\n", port, value);
        driving_translation_stop(__func__, "unsupported output port");
    }
    fprintf(stderr, "[DRIVING PORT] TV-encoder output %04X=%02X (diagnostic latch)\n", port, value);
}
