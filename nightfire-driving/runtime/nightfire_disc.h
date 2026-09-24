#ifndef NIGHTFIRE_DISC_H
#define NIGHTFIRE_DISC_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

/* Exact bare optical device only; do not swallow paths like CdRom01. */
static int nightfire_disc_root(const char *path)
{
    const char *expected = "\\Device\\CdRom0";
    if (!path) return 0;
    while (*expected) {
        if (tolower((unsigned char)*path) != tolower((unsigned char)*expected)) return 0;
        ++path; ++expected;
    }
    return *path == 0;
}

/* MODE SENSE(10), page 0x3e: virtual mounted extracted Xbox-media state.
 * 8-byte mode header + 20-byte Xbox page, documented at:
 * https://xboxdevwiki.net/DVD_Drive#MODE_SENSE_and_MODE_SELECT
 * No physical drive commands or authentication exchange are implemented.
 */
static int nightfire_disc_mode_page(uint8_t *out, size_t capacity,
                                    uint8_t opcode, uint8_t page)
{
    if (!out || capacity < 28 || opcode != 0x5a || page != 0x3e) return 0;
    memset(out, 0, 28);
    out[1] = 26; /* big-endian mode data length, excluding first 2 bytes */
    out[8] = 0x3e;
    out[9] = 18;
    out[10] = 1; /* Xbox data partition selected */
    out[11] = 1; /* Xbox media type */
    out[12] = 1; /* extracted media already available to the host */
    out[13] = 0xd0; /* Xbox book type */
    return 28;
}
#endif
