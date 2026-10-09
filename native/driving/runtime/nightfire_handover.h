/* Engine hand-over (combined launcher, native-driving/play-nightfire-native.py).
 *
 * On the Xbox, default.xbe (Action) starts a driving mission by XLaunchNewImage:
 * the title's own XAPI code fills a 4 KB launch data page (type, title id, target
 * path at +8, the game's 0xA50-byte payload at +0x400; see
 * nightfire-port/analysis/driving-launch142.md) and reboots with
 * HalReturnToFirmware(2); Driving.xbe reads the page with XGetLaunchInfo and
 * returns to default.xbe the same way. Here each XBE is its own program, so:
 *
 *  NIGHTFIRE_HANDOVER_DIR=dir   at HalReturnToFirmware with a launch page, write
 *                               the page verbatim to dir\next-page.bin and its
 *                               target path to dir\next-target.txt; the launcher
 *                               then starts the other program.
 *  NIGHTFIRE_LAUNCH_PAGE=file   at start, publish that page (4096 bytes, as saved
 *                               by the other program) as the kernel's
 *                               LaunchDataPage, exactly as the reboot would.
 *
 * Nothing is interpreted or changed here: the games read and write their own data.
 * The same header is used by the Action and Driving runtimes. */
#ifndef NIGHTFIRE_HANDOVER_H
#define NIGHTFIRE_HANDOVER_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NF_HANDOVER_PAGE_BYTES 0x1000u

/* Returns a guest page holding the saved launch page, or 0 when none was given. */
static uint32_t nf_handover_load_page(uint8_t *(*guest)(uint32_t), uint32_t (*alloc)(uint32_t, uint32_t))
{
    const char *path = getenv("NIGHTFIRE_LAUNCH_PAGE");
    if (!path || !*path) return 0;
    uint8_t bytes[NF_HANDOVER_PAGE_BYTES];
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "[HANDOVER] cannot open launch page %s\n", path); return 0; }
    size_t n = fread(bytes, 1, sizeof bytes, f);
    fclose(f);
    if (n != sizeof bytes) { fprintf(stderr, "[HANDOVER] launch page %s is %u bytes, not 4096\n", path, (unsigned)n); return 0; }
    uint32_t page = alloc(NF_HANDOVER_PAGE_BYTES, 4096);
    if (!page) { fprintf(stderr, "[HANDOVER] launch page allocation failed\n"); return 0; }
    memcpy(guest(page), bytes, sizeof bytes);
    char target[64] = { 0 };
    memcpy(target, bytes + 8, sizeof target - 1);
    uint32_t type, title; memcpy(&type, bytes, 4); memcpy(&title, bytes + 4, 4);
    fprintf(stderr, "[HANDOVER] launch page from %s: type %u title %08X path '%s' -> guest %08X\n", path, type, title, target, page);
    return page;
}

/* Saves the page the title just wrote for its next image. */
static void nf_handover_save(const uint8_t *page_bytes)
{
    const char *dir = getenv("NIGHTFIRE_HANDOVER_DIR");
    if (!dir || !*dir || !page_bytes) return;
    char p[1024], t[1024], target[521];
    snprintf(p, sizeof p, "%s\\next-page.bin", dir);
    snprintf(t, sizeof t, "%s\\next-target.txt", dir);
    memcpy(target, page_bytes + 8, 520); target[520] = 0;
    FILE *f = fopen(p, "wb");
    if (f) { fwrite(page_bytes, 1, NF_HANDOVER_PAGE_BYTES, f); fclose(f); }
    FILE *g = fopen(t, "w");
    if (g) { fprintf(g, "%s\n", target); fclose(g); }
    fprintf(stderr, "[HANDOVER] saved launch page for '%s' to %s (%s)\n", target, p, f && g ? "ok" : "FAILED");
    fflush(stderr);
}
#endif
