/* LEAN_HOOK_PROBE=1 (diagnostic, Paris crane hook; notes: hires\hook-20261009\AIM-PATH.md).
 * Installed into src/recomp/gen/ by scripts/hook_probe_patch.py, which also adds the calls
 * marked LEAN_HOOK_PROBE in recomp_0003.c, recomp_0004.c and recomp_0007.c.
 * Without LEAN_HOOK_PROBE=1 every entry point returns after one cached test.
 *
 * Lines, all prefixed [HOOK]:
 *   shot   one per sniper bullet record (71D80): start, direction, length, camera lock state
 *   walk   one per D0A30 test of that record: segment, grid, walked cells, hook cell visited?
 *   cand   one per region the walk reaches (loc_000D0BA2) and the D0A30 filter result
 *   cf520  result of the shape test for regions that passed the filter (loc_000D0BFA)
 *   flags  the hook region's +0x10..0x13 changed (checked once per step at D0C90 entry)
 *   write  an instrumented writer cleared +0x12 bit 0 of the hook region (+ host call chain)
 * Options: LEAN_HOOK_PROBE_INDEX=n   hook region index (default 711 = 0x2C7, 8065E960 in the
 *                                    10-10 sessions); LEAN_HOOK_PROBE_TYPE=hex  bullet type (1A). */
#include "recomp_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern volatile long lean_present_count;
extern void lean_print_host_chain(const char *tag);

static int hp_on = -1;
static uint32_t hp_index = 0x2C7u, hp_type = 0x1Au;
static uint8_t hp_rec_type[64];          /* bullet type per record slot (0x1E8570 + i*0x30) */
static uint32_t hp_hook_addr;            /* last resolved hook region address */
static uint32_t hp_flags_last; static long hp_flags_f; static int hp_flags_valid;
static uint32_t hp_wr_func, hp_wr_occ; static long hp_wr_f = -1000;
static unsigned hp_chains;
static long hp_cand_f = -1; static uint32_t hp_cand_rec = 0xFFFFFFFFu; static unsigned hp_cand_n, hp_cand_drop;
static uint32_t hp_grid_sig;

static int hp_enabled(void)
{
    if (hp_on < 0) {
        const char *e = getenv("LEAN_HOOK_PROBE"), *i = getenv("LEAN_HOOK_PROBE_INDEX"), *t = getenv("LEAN_HOOK_PROBE_TYPE");
        hp_on = e && e[0] == '1';
        if (i && *i) hp_index = (uint32_t)strtoul(i, NULL, 0);
        if (t && *t) hp_type = (uint32_t)strtoul(t, NULL, 16);
        if (hp_on) fprintf(stderr, "[HOOK] probe on (LEAN_HOOK_PROBE): hook region index %u, bullet type %02X\n", hp_index, hp_type);
    }
    return hp_on;
}
static int hp_ptr_ok(uint32_t a) { return (a >= 0x10000u && a < 0x04000000u) || (a >= 0x80000000u && a < 0x84000000u); }
static float hp_f(uint32_t a) { uint32_t u = MEM32(a); float f; memcpy(&f, &u, 4); return f; }

/* Hook region = [[0x23E260]+4] + index*64; 0 while the map rules are not loaded. */
static uint32_t hp_hook(void)
{
    uint32_t mgr = MEM32(0x23E260u), n, base, h;
    if (!hp_ptr_ok(mgr)) return 0;
    n = MEM32(mgr); base = MEM32(mgr + 4);
    if (!hp_ptr_ok(base) || hp_index >= n || n > 100000u) return 0;
    h = base + hp_index * 0x40u;
    if (h != hp_hook_addr) {
        float cx = hp_f(h), cy = hp_f(h + 4), cz = hp_f(h + 8), r = hp_f(h + 0xC);
        int looks = fabsf(cx + 316.26f) < 1.0f && fabsf(cz - 143.89f) < 1.0f;
        hp_hook_addr = h; hp_flags_valid = 0;
        fprintf(stderr, "[HOOK] f=%ld hook region %08X (index %u of %u) centre %.3f %.3f %.3f r %.2f +10..13 %08X%s\n",
                lean_present_count, h, hp_index, n, cx, cy, cz, r, MEM32(h + 0x10),
                looks ? "" : "  (not the 10-10 hook centre: check LEAN_HOOK_PROBE_INDEX)");
    }
    return h;
}

/* sub_00071D80 at loc_00071DAF: record slot idx is about to be filled from bullet b
 * (arg dist = segment length bits). */
void lean_hook_probe_record(uint32_t idx, uint32_t b, uint32_t dist_bits)
{
    uint32_t type, cam, cv;
    float dist;
    if (!hp_enabled()) return;
    type = MEM32(b + 0x5C);
    hp_rec_type[idx & 63u] = (uint8_t)type;
    if (type != hp_type) return;
    memcpy(&dist, &dist_bits, 4);
    cv = MEM32(0x1EC488u); cam = hp_ptr_ok(cv) ? MEM32(cv + 4) : 0;
    if (!hp_ptr_ok(cam)) cam = 0;
    fprintf(stderr, "[HOOK] f=%ld shot rec %u type %02X owner %08X start %.3f %.3f %.3f dir %.5f %.5f %.5f len %.1f",
            lean_present_count, idx, type, MEM32(b + 0x64), hp_f(b + 0x10), hp_f(b + 0x14), hp_f(b + 0x18),
            hp_f(b + 0x30), hp_f(b + 0x34), hp_f(b + 0x38), dist);
    if (cam)
        fprintf(stderr, " | cam %08X lock %08X blend %d/%d aimoff %.5f %.5f eye %.3f %.3f %.3f aim %.5f %.5f %.5f fwd %.5f %.5f %.5f\n",
                cam, MEM32(cam + 0x150), (int)MEM32(cam + 0x170), (int)MEM32(cam + 0x174), hp_f(cam + 0x1E0), hp_f(cam + 0x1E4),
                hp_f(cam + 0x40), hp_f(cam + 0x44), hp_f(cam + 0x48), hp_f(cam + 0x180), hp_f(cam + 0x184), hp_f(cam + 0x188),
                hp_f(cam + 0x30), hp_f(cam + 0x34), hp_f(cam + 0x38));
    else fputc('\n', stderr);
}

static int hp_cell(float v, float o, float inv) { int t = (int)(float)((double)v - (double)o); return (int)(float)((double)t * inv); }   /* as C64B0 (cvttss2si) */

/* sub_000D0A30 at loc_000D0B45 (after the C64B0 walk): record idx at rec, grid, cell list [b, e). */
void lean_hook_probe_walk(uint32_t idx, uint32_t rec, uint32_t grid, uint32_t b, uint32_t e)
{
    uint32_t h, n, cols, rows, i, sig;
    float x0, z0, x1, z1, ox, oz, c, inv;
    int hx0, hx1, hz0, hz1, hit = 0, nx, nz;
    char list[640]; int ll = 0;
    if (!hp_enabled() || hp_rec_type[idx & 63u] != (uint8_t)hp_type) return;
    hp_cand_f = lean_present_count; hp_cand_rec = idx; hp_cand_n = 0; hp_cand_drop = 0;
    if (!hp_ptr_ok(rec) || !hp_ptr_ok(grid)) return;
    x0 = hp_f(rec + 0x10); z0 = hp_f(rec + 0x18); x1 = hp_f(rec + 0x20); z1 = hp_f(rec + 0x28);
    ox = hp_f(grid); oz = hp_f(grid + 8); c = hp_f(grid + 0x10); inv = hp_f(grid + 0x14);
    rows = MEM32(grid + 0x18); cols = MEM32(grid + 0x1C);
    sig = MEM32(grid) ^ (MEM32(grid + 8) * 3u) ^ (MEM32(grid + 0x10) * 7u) ^ (rows << 16) ^ cols;
    if (sig != hp_grid_sig) {
        hp_grid_sig = sig;
        fprintf(stderr, "[HOOK] grid %08X origin x %.3f z %.3f cell %.4f inv %.6f rows(z) %u cols(x) %u\n", grid, ox, oz, c, inv, rows, cols);
    }
    n = (e >= b && e - b < 0x10000u) ? (e - b) / 4u : 0;
    h = hp_hook();
    hx0 = hx1 = hz0 = hz1 = -1;
    if (h) {
        float cx = hp_f(h), cz = hp_f(h + 8), r = hp_f(h + 0xC);
        hx0 = hp_cell(cx - r, ox, inv); hx1 = hp_cell(cx + r, ox, inv);
        hz0 = hp_cell(cz - r, oz, inv); hz1 = hp_cell(cz + r, oz, inv);
    }
    for (i = 0; i < n; i++) {
        uint32_t cell = MEM32(b + i * 4u);
        int cx = cols ? (int)(cell % cols) : -1, cz = cols ? (int)(cell / cols) : -1;
        if (cx >= hx0 && cx <= hx1 && cz >= hz0 && cz <= hz1) hit = 1;
        if (ll < (int)sizeof list - 24) ll += snprintf(list + ll, sizeof list - ll, " %d:%d", cx, cz);
    }
    list[ll] = 0;
    {   /* first-boundary rounding as lifted (frndint = nearest) versus the CRT intent (ceil/floor) */
        double ax = (double)x0 * inv, az = (double)z0 * inv;
        nx = (x1 - x0 >= 0.0f) ? (ceil(ax) != rint(ax)) : (floor(ax) != rint(ax));
        nz = (z1 - z0 >= 0.0f) ? (ceil(az) != rint(az)) : (floor(az) != rint(az));
    }
    fprintf(stderr, "[HOOK] f=%ld walk rec %u seg %.3f,%.3f -> %.3f,%.3f cells %u hookcells x%d..%d z%d..%d %s | first boundary rounded %s/%s (x/z) | cells x:z%s%s\n",
            lean_present_count, idx, x0, z0, x1, z1, n, hx0, hx1, hz0, hz1, hit ? "VISITED" : "not visited",
            nx ? "differs" : "same", nz ? "differs" : "same", list, ll >= (int)sizeof list - 24 ? " ..." : "");
}

/* sub_000D0A30 at loc_000D0BA2: region reached by the walk, before the D0A30 filter.
 * maskown = [esp+0x1C] mask (0x40/0xC0) | owner-is-player byte [esp+0x1B] << 8; gate = byte [mgr+8]. */
void lean_hook_probe_cand(uint32_t idx, uint32_t reg, uint32_t stamp_now, uint32_t maskown, uint32_t gate)
{
    uint32_t lo, hi, stamp, mask = maskown & 0xFFu, own = (maskown >> 8) & 0xFFu, h;
    const char *why;
    if (!hp_enabled() || hp_rec_type[idx & 63u] != (uint8_t)hp_type) return;
    if (hp_cand_f != lean_present_count || hp_cand_rec != idx) { hp_cand_f = lean_present_count; hp_cand_rec = idx; hp_cand_n = 0; hp_cand_drop = 0; }
    lo = MEM8(reg + 0x12); hi = MEM8(reg + 0x13); stamp = MEM32(reg + 0x1C); h = hp_hook();
    if (stamp == stamp_now) why = "seen (already tested this walk)";
    else if (!(lo & 1u)) why = "rejected: +12 bit0 clear (disabled)";
    else if (hi & 0x20u) why = "rejected: +13 bit 0x20";
    else if ((hi & 4u) && !(gate & 0xFFu)) why = "rejected: +13 bit 4 and [mgr+8]=0";
    else if (!(mask & ((hi << 8) | lo))) why = "rejected: mask";
    else if (!own && (lo & 0x80u)) why = "rejected: player-only";
    else why = "pass -> CF520";
    if (reg != h && stamp == stamp_now) return;              /* duplicates of other regions: not logged */
    if (reg != h && hp_cand_n >= 40u) { hp_cand_drop++; return; }
    hp_cand_n++;
    fprintf(stderr, "[HOOK] f=%ld cand rec %u region %08X%s +12 %02X +13 %02X stamp %08X now %08X mask %02X own %u gate %u -> %s%s\n",
            lean_present_count, idx, reg, reg == h ? " HOOK" : "", lo, hi, stamp, stamp_now, mask, own, gate & 0xFFu, why,
            hp_cand_n == 40u ? " (further non-hook candidates of this shot not logged)" : "");
}

/* sub_000D0A30 at loc_000D0BFA: CF520 result (al) for a region that passed the filter. */
void lean_hook_probe_cf520(uint32_t idx, uint32_t reg, uint32_t al)
{
    uint32_t h;
    if (!hp_enabled() || hp_rec_type[idx & 63u] != (uint8_t)hp_type) return;
    h = hp_hook();
    fprintf(stderr, "[HOOK] f=%ld cf520 rec %u region %08X%s -> %u\n", lean_present_count, idx, reg, reg == h ? " HOOK" : "", al & 0xFFu);
}

/* sub_000D0C90 entry (map rules, once per game step): watch the hook region's +0x10..0x13. */
void lean_hook_probe_step(void)
{
    uint32_t h, v;
    if (!hp_enabled() || !(h = hp_hook())) return;
    v = MEM32(h + 0x10);
    if (!hp_flags_valid) { hp_flags_valid = 1; hp_flags_last = v; hp_flags_f = lean_present_count; return; }
    if (v == hp_flags_last) return;
    {
        char wr[64];
        if (hp_wr_f >= hp_flags_f) snprintf(wr, sizeof wr, "sub_%08X#%u at f=%ld", hp_wr_func, hp_wr_occ, hp_wr_f);
        else snprintf(wr, sizeof wr, "none since then (writer not instrumented)");
        fprintf(stderr, "[HOOK] f=%ld flags hook region %08X +10..13 %08X -> %08X (unchanged since f=%ld); last instrumented write: %s\n",
                lean_present_count, h, hp_flags_last, v, hp_flags_f, wr);
    }
    hp_flags_last = v; hp_flags_f = lean_present_count;
}

/* Region-flag writers (sub_00048EC0, sub_000495D0, sub_0004CBC0 message bodies; sub_000CF300
 * one-shot clear): reg +0x12 is about to become new16. ctx = message object (esi) or 0. */
void lean_hook_probe_write(uint32_t func, uint32_t occ, uint32_t reg, uint32_t new16, uint32_t ctx)
{
    char tag[96];
    if (!hp_enabled() || reg != hp_hook()) return;
    hp_wr_func = func; hp_wr_occ = occ; hp_wr_f = lean_present_count;
    fprintf(stderr, "[HOOK] f=%ld write sub_%08X#%u hook region %08X +12 %04X -> %04X ctx %08X vtbl %08X\n",
            lean_present_count, func, occ, reg, (unsigned)MEM16(reg + 0x12), new16 & 0xFFFFu, ctx,
            hp_ptr_ok(ctx) ? MEM32(ctx) : 0u);
    if (hp_chains++ < 8u) { snprintf(tag, sizeof tag, "hook region write sub_%08X#%u f=%ld", func, occ, lean_present_count); lean_print_host_chain(tag); }
}
