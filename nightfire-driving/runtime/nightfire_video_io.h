#ifndef NIGHTFIRE_VIDEO_IO_H
#define NIGHTFIRE_VIDEO_IO_H
#include <stdint.h>
/* Narrow diagnostic model of the TV-encoder field input at port 80C0.
 * The alternating bit-5 approximation follows the behavior documented in
 * xemu hw/xbox/acpi_xbox.c (fdfb5a8f481b2f870c57080e74ec8d3a31a47053).
 * It is NOT synchronized to scanout and is not a finished video-timing model.
 * Writes do not echo into the independent field input.
 */
typedef struct NightfireVideoIo { uint8_t field, last_output; } NightfireVideoIo;
static inline int nightfire_video_in8(NightfireVideoIo *s, uint16_t port, uint8_t *value)
{
    if (port != 0x80C0u || !value) return 0;
    s->field ^= 1u;
    *value = (uint8_t)(s->field << 5);
    return 1;
}
static inline int nightfire_video_out8(NightfireVideoIo *s, uint16_t port, uint8_t value)
{
    if (port != 0x80C0u) return 0;
    s->last_output = value;
    return 1;
}
#endif
