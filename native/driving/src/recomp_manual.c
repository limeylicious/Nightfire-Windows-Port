/**
 * Manual function overrides and ICALL diagnostics
 *
 * This file provides:
 *   - recomp_lookup_manual()  : intercept specific Xbox VAs with hand-written code
 *   - recomp_icall_fail_log() : log when an indirect call target can't be resolved
 *   - ICALL trace ring buffer  : globals used by the RECOMP_ICALL macro
 *
 * The recomp pipeline generates an auto-dispatch table (recomp_lookup) that
 * resolves most function addresses. recomp_lookup_manual() is called FIRST,
 * giving you a chance to override any function with a custom implementation.
 *
 * Common reasons to add manual overrides:
 *   - Trace a function to understand call flow (wrap the generated version)
 *   - Fix a function the lifter translated incorrectly
 *   - Stub out a function that crashes (return early, set eax to a safe value)
 *   - Redirect a function to a native implementation (e.g., skip CRT init)
 *   - Intercept D3D/audio calls for custom rendering or sound
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* ── ICALL trace ring buffer ───────────────────────────────── */

/*
 * These globals are written by the RECOMP_ICALL macro (defined in
 * recomp_types.h) every time an indirect call is dispatched. When a
 * crash occurs, the VEH handler or recomp_icall_fail_log() can dump
 * the last 16 call targets to help you trace what happened.
 *
 * The runtime owns them: xbox_kernel defines all three in
 * src/kernel/xbox_memory_layout.c, and recomp_types.h declares them extern.
 * Declare, do not define -- a definition here as well is a duplicate symbol,
 * and a project copied from this template failed to link on all three:
 *
 *   xbox_memory_layout.obj : error LNK2005: g_icall_count already defined
 *                            in recomp_manual.obj
 */
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);
extern void sub_0002BD40(void);
extern void sub_0016EC40(void);
extern void sub_00158EF0(void);
extern void sub_00184502(void);
extern void sub_00183BF4(void);
extern void sub_0010C110(void);
extern void sub_000EE490(void);
extern void sub_00122580(void);
extern void sub_00051B40(void);
extern void sub_00051770(void);
extern void sub_000EE190(void);

/* ── Register state (defined in xbox_memory_layout.c) ──────── */

extern __declspec(thread) uint32_t g_eax;
extern __declspec(thread) uint32_t g_esp, g_ebp, g_ecx, g_edx, g_ebx, g_esi, g_edi;
extern ptrdiff_t g_xbox_mem_offset;

/* Once USB starts a worker, the toolkit's shared ring can hide the main
 * thread's failure under thousands of polling calls. Keep a local history. */
static __declspec(thread) uint32_t driving_calls[16], driving_returns[16];
static __declspec(thread) uint32_t driving_call_index;
void driving_trace_icall(uint32_t va)
{
    unsigned slot = driving_call_index++ & 15u;
    driving_calls[slot] = va;
    driving_returns[slot] = (g_xbox_mem_offset && g_esp >= 0x10000 && g_esp <= 0x04000000 - 4)
        ? *(const uint32_t *)(g_xbox_mem_offset + g_esp) : 0;
}

void driving_trace_entry(unsigned int va)
{
    fprintf(stderr, "[DRIVING ENTRY] %08X\n", va);
}

void driving_translation_stop(const char *function, const char *instruction)
{
    fprintf(stderr, "[DRIVING STOP] %s: unsupported %s\n", function, instruction);
    fflush(stderr);
    ExitProcess(142);
}

/* ── Manual function overrides ─────────────────────────────── */

/*
 * Return a function pointer to override the given Xbox VA, or NULL
 * to fall through to the auto-generated dispatch table.
 *
 * This is called on every indirect call (RECOMP_ICALL) and every
 * direct call through the dispatch table, so keep it fast. A chain
 * of if-statements on uint32_t compiles to a simple comparison
 * sequence; for large override tables, consider a sorted array
 * with binary search.
 *
 * Examples of common override patterns:
 *
 *   // Trace wrapper: log entry/exit around the generated function
 *   extern void sub_00012345(void);
 *   static void traced_sub_00012345(void) {
 *       fprintf(stderr, "[TRACE] sub_00012345 entered, eax=0x%08X\n", g_eax);
 *       sub_00012345();
 *       fprintf(stderr, "[TRACE] sub_00012345 returned, eax=0x%08X\n", g_eax);
 *   }
 *
 *   // Stub: skip a function entirely (return 0 in eax)
 *   static void stub_00067890(void) {
 *       g_eax = 0;
 *   }
 *
 *   // Fix: replace a broken lifted function with correct C
 *   static void fixed_sub_000ABCDE(void) {
 *       // Read arguments from stack/registers per calling convention
 *       uint32_t arg1 = g_ecx;
 *       uint32_t arg2 = MEM32(g_esp + 4);
 *       // ... correct implementation ...
 *       g_eax = result;
 *   }
 */
extern void sub_0003C310(void);
extern void sub_0003C790(void);
extern void sub_0003C8B0(void);
extern void sub_0003B630(void);
extern void sub_0003BD50(void);
extern void sub_0003BE90(void);
extern void sub_0003BFB0(void);
extern void sub_0003C0D0(void);
extern void sub_0003C1F0(void);
extern void sub_0003C9D0(void);
extern void sub_0003CDD0(void);
extern void sub_0013BF10(void);
extern void sub_0011AA80(void);
extern void sub_00118220(void);
extern void sub_001184B0(void);
extern void sub_00118750(void);
extern void sub_00091DB0(void);
extern void sub_00092170(void);
extern void sub_000E9410(void);
extern void sub_000EA1D0(void);
extern void sub_000ECBE0(void);
extern void sub_000ED070(void);
extern void sub_000ED2B0(void);
extern void sub_000EF4C0(void);
extern void sub_000EF780(void);
extern void sub_00128D10(void);
#include "driving_aliases176.h"
extern void sub_00144710(void);
extern void sub_00145AB0(void);
extern void sub_00145BF0(void);
extern void sub_0013BB20(void);
extern void sub_0013D5C0(void);
extern void sub_001413D0(void);
extern void sub_000964B0(void);
extern void sub_0008CFA0(void);
extern void sub_0008CE50(void);
extern void sub_00037620(void);
extern void sub_00036D10(void);
extern void sub_00123C50(void);
#include "../runtime/driving_producer235.h"
extern void sub_00020A60(void);
extern void sub_0001A880(void);
extern void sub_0001BE60(void);
extern void sub_0015CD30(void);
extern void sub_0015CD40(void);
extern void sub_00055F60(void);
extern void sub_000561F0(void);
extern void sub_00058570(void);
extern void sub_00119EC0(void);
extern void sub_0009D5F0(void);
extern void sub_000BD9A0(void);
extern void sub_00037AF0(void);
extern void sub_0010B920(void);
extern void sub_0010BAE0(void);
extern void sub_0009AF70(void);
extern void sub_000182B0(void);
extern void sub_00037660(void);
extern void sub_00059C30(void);
extern void sub_000903A0(void);
extern void sub_00094FF0(void);
extern void sub_0015CD20(void);
extern void sub_0015CDC0(void);
extern void sub_0015CE10(void);
extern void sub_0015CEB0(void);
extern void sub_0015CED0(void);
extern void sub_0015CEF0(void);
extern void sub_0011D580(void);
extern void sub_001235A0(void);
extern void sub_00125B90(void);
extern void sub_001267F0(void);
extern void sub_0012F820(void);
extern void sub_00117350(void);
extern void sub_0015D200(void);
extern void sub_0015D210(void);
extern void sub_0015D220(void);
extern void sub_0015D250(void);
extern void sub_0015D260(void);
extern void sub_0015D2D0(void);
extern void sub_0015D2F0(void);
extern void sub_0015D300(void);
extern void sub_000F3AB0(void);
extern void sub_000F3BF0(void);
extern void sub_00119E80(void);
extern void sub_00124750(void);
extern void sub_0011A810(void);
extern void sub_0015D150(void);
extern void sub_0015D140(void);
extern void sub_0015D230(void);
extern void sub_0015D2C0(void);
extern void sub_0015CE40(void);
extern void sub_000B5310(void);
extern void sub_0015D010(void);
extern void sub_00144BC0(void);
extern void sub_00144CA0(void);
extern void sub_00144D00(void);
extern void sub_001450B0(void);
recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    if(xbox_va==0x001450B0u)return sub_001450B0;

    if(xbox_va==0x00144D00u)return sub_00144D00;

    if(xbox_va==0x00144BC0u)return sub_00144BC0;
    if(xbox_va==0x00144CA0u)return sub_00144CA0;

    if(xbox_va==0x000B5310u)return sub_000B5310;
    if(xbox_va==0x0015D010u)return sub_0015D010;

    if(xbox_va==0x000F3AB0u)return sub_000F3AB0;
    if(xbox_va==0x000F3BF0u)return sub_000F3BF0;
    if(xbox_va==0x00119E80u)return sub_00119E80;
    if(xbox_va==0x00124750u)return sub_00124750;
    if(xbox_va==0x0011A810u)return sub_0011A810;
    if(xbox_va==0x0015D150u)return sub_0015D150;
    if(xbox_va==0x0015D140u)return sub_0015D140;
    if(xbox_va==0x0015D230u)return sub_0015D230;
    if(xbox_va==0x0015D2C0u)return sub_0015D2C0;
    if(xbox_va==0x0015CE40u)return sub_0015CE40;

    if(xbox_va==0x0011D580u)return sub_0011D580;
    if(xbox_va==0x001235A0u)return sub_001235A0;
    if(xbox_va==0x00125B90u)return sub_00125B90;
    if(xbox_va==0x001267F0u)return sub_001267F0;
    if(xbox_va==0x0012F820u)return sub_0012F820;
    if(xbox_va==0x00117350u)return sub_00117350;
    if(xbox_va==0x0015D200u)return sub_0015D200;
    if(xbox_va==0x0015D210u)return sub_0015D210;
    if(xbox_va==0x0015D220u)return sub_0015D220;
    if(xbox_va==0x0015D250u)return sub_0015D250;
    if(xbox_va==0x0015D260u)return sub_0015D260;
    if(xbox_va==0x0015D2D0u)return sub_0015D2D0;
    if(xbox_va==0x0015D2F0u)return sub_0015D2F0;
    if(xbox_va==0x0015D300u)return sub_0015D300;

    if(xbox_va==0x000182B0u)return sub_000182B0;
    if(xbox_va==0x00037660u)return sub_00037660;
    if(xbox_va==0x00059C30u)return sub_00059C30;
    if(xbox_va==0x000903A0u)return sub_000903A0;
    if(xbox_va==0x00094FF0u)return sub_00094FF0;
    if(xbox_va==0x0015CD20u)return sub_0015CD20;
    if(xbox_va==0x0015CDC0u)return sub_0015CDC0;
    if(xbox_va==0x0015CE10u)return sub_0015CE10;
    if(xbox_va==0x0015CEB0u)return sub_0015CEB0;
    if(xbox_va==0x0015CED0u)return sub_0015CED0;
    if(xbox_va==0x0015CEF0u)return sub_0015CEF0;

    if(xbox_va==0x0009AF70u)return sub_0009AF70;
    if(xbox_va==0x0010BAE0u)return sub_0010BAE0;
    if(xbox_va==0x00037AF0u)return sub_00037AF0;
    if(xbox_va==0x0010B920u)return sub_0010B920;

    if(xbox_va==0x00119EC0u)return sub_00119EC0;
    if(xbox_va==0x0009D5F0u)return sub_0009D5F0;
    if(xbox_va==0x000BD9A0u)return sub_000BD9A0;

    if(xbox_va==0x00055F60u)return sub_00055F60;
    if(xbox_va==0x000561F0u)return sub_000561F0;
    if(xbox_va==0x00058570u)return sub_00058570;

    if(xbox_va==0x0001A880u)return sub_0001A880;
    if(xbox_va==0x0001BE60u)return sub_0001BE60;
    if(xbox_va==0x0015CD30u)return sub_0015CD30;
    if(xbox_va==0x0015CD40u)return sub_0015CD40;

    if(xbox_va==0x00020A60u)return sub_00020A60;
    if(xbox_va==0x000F0FC0u && producer_active235())return traced_sub_000F0FC0;
    if(xbox_va==0x000F6030u && producer_active235())return traced_sub_000F6030;
    if(xbox_va==0x000F6270u && producer_active235())return traced_sub_000F6270;
    if(xbox_va==0x000F6320u && producer_active235())return traced_sub_000F6320;
    if(xbox_va==0x000F6600u && producer_active235())return traced_sub_000F6600;
    if (xbox_va == 0x00123C50u) return sub_00123C50;
    if (xbox_va == 0x00036D10u) return sub_00036D10;
    if (xbox_va == 0x00037620u) return sub_00037620;
    if (xbox_va == 0x0008CE50u) return sub_0008CE50;
    if (xbox_va == 0x0008CFA0u) return sub_0008CFA0;
    if (xbox_va == 0x000964B0u) return sub_000964B0;
    if (xbox_va == 0x001413D0u) return sub_001413D0;
    if (xbox_va == 0x0013D5C0u) return sub_0013D5C0;
    if (xbox_va == 0x0013BB20u) return sub_0013BB20;
    if (xbox_va == 0x00145BF0u) return sub_00145BF0;
    if (xbox_va == 0x00145AB0u) return sub_00145AB0;
    if (xbox_va == 0x00144710u) return sub_00144710;
    recomp_func_t alias176=driving_alias176(xbox_va); if(alias176)return alias176;
    if (xbox_va == 0x00128D10u) return sub_00128D10;
    if (xbox_va == 0x00122580u) return sub_00122580;
    if (xbox_va == 0x000EF780u) return sub_000EF780;
    if (xbox_va == 0x000EF4C0u) return sub_000EF4C0;
    if (xbox_va == 0x000ED2B0u) return sub_000ED2B0;
    if (xbox_va == 0x000ED070u) return sub_000ED070;
    if (xbox_va == 0x000ECBE0u) return sub_000ECBE0;
    if (xbox_va == 0x000EA1D0u) return sub_000EA1D0;
    if (xbox_va == 0x000E9410u) return sub_000E9410;
    if (xbox_va == 0x00092170u) return sub_00092170;
    if (xbox_va == 0x00091DB0u) return sub_00091DB0;
    if (xbox_va == 0x00118750u) return sub_00118750;
    if (xbox_va == 0x001184B0u) return sub_001184B0;
    if (xbox_va == 0x00118220u) return sub_00118220;
    if (xbox_va == 0x0011AA80u) return sub_0011AA80;
    if (xbox_va == 0x0013BF10u) return sub_0013BF10;
    if (xbox_va == 0x0003CDD0u) return sub_0003CDD0;
    if (xbox_va == 0x0003C9D0u) return sub_0003C9D0;
    if (xbox_va == 0x0003C1F0u) return sub_0003C1F0;
    if (xbox_va == 0x0003C0D0u) return sub_0003C0D0;
    if (xbox_va == 0x0003BFB0u) return sub_0003BFB0;
    if (xbox_va == 0x0003BE90u) return sub_0003BE90;
    if (xbox_va == 0x0003BD50u) return sub_0003BD50;
    if (xbox_va == 0x0003B630u) return sub_0003B630;
    if (xbox_va == 0x0003C8B0u) return sub_0003C8B0;
    if (xbox_va == 0x0003C790u) return sub_0003C790;
    if (xbox_va == 0x0003C310u) return sub_0003C310;
    if (xbox_va == 0x00051B40u) return sub_00051B40;
    if (xbox_va == 0x00051770u) return sub_00051770;
    if (xbox_va == 0x000EE490u) return sub_000EE490;
    if (xbox_va == 0x000EE190u) return sub_000EE190;
    if (xbox_va == 0x0010C110u) return sub_0010C110;
    if (xbox_va == 0x0016EC40u) return sub_0016EC40;
    if (xbox_va == 0x00158EF0u) return sub_00158EF0;
    if (xbox_va == 0x00184502u) return sub_00184502;
    if (xbox_va == 0x00183BF4u) return sub_00183BF4;
    if (xbox_va == 0x0002BD40u) return sub_0002BD40;
    /*
     * TODO: Add your overrides here. Examples:
     *
     * if (xbox_va == 0x00012345) return traced_sub_00012345;
     * if (xbox_va == 0x00067890) return stub_00067890;
     * if (xbox_va == 0x000ABCDE) return fixed_sub_000ABCDE;
     */

    (void)xbox_va;
    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

/*
 * Called when RECOMP_ICALL cannot resolve a target address.
 * This usually means one of:
 *   - A vtable dispatch to an address not in the dispatch table
 *   - A function pointer loaded from uninitialized or corrupt memory
 *   - A kernel thunk address that the bridge doesn't handle
 *
 * This driving diagnostic exits immediately on every unresolved call. Continuing
 * would silently omit original behavior and hide the first actionable failure.
 */
extern void driving_scheduler_dump173(void);
void recomp_icall_fail_log(uint32_t va)
{
    driving_scheduler_dump173();
    fprintf(stderr, "[ICALL] Failed to resolve VA 0x%08X (total calls: %llu)\n",
            va, (unsigned long long)g_icall_count);
    fprintf(stderr, "  eax=%08X ecx=%08X edx=%08X ebx=%08X esi=%08X edi=%08X ebp=%08X esp=%08X\n",
            g_eax, g_ecx, g_edx, g_ebx, g_esi, g_edi, g_ebp, g_esp);
    if (g_xbox_mem_offset && g_esp >= 0x10000 && g_esp <= 0x04000000 - 128) {
        const uint32_t *stack = (const uint32_t *)(g_xbox_mem_offset + g_esp);
        for (int i = 0; i < 32; ++i)
            fprintf(stderr, "  stack +%02X: %08X\n", i * 4, stack[i]);
    }

    /* Dump last 16 call targets from the ring buffer */
    fprintf(stderr, "  This thread (%lu) recent ICALL targets and return addresses:\n", GetCurrentThreadId());
    for (unsigned i = 0; i < 16; ++i) {
        unsigned slot = (driving_call_index - 16u + i) & 15u;
        if (driving_calls[slot])
            fprintf(stderr, "    [%2u] %08X return=%08X\n", i, driving_calls[slot], driving_returns[slot]);
    }
    fprintf(stderr, "  Recent ICALL targets:\n");
    for (int i = 0; i < 16; i++) {
        int idx = (g_icall_trace_idx - 16 + i) & 15;
        if (g_icall_trace[idx])
            fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
    }
    fflush(stderr);
    ExitProcess(142);
}

/* An indirect call whose target is not code: a null or wild function pointer.
 *
 * Skipping these is right -- calling a data address is worse -- but skipping
 * them *silently* is not. They almost always arrive inside a loop, so the
 * symptom is a hang with no output rather than a diagnosable null vtable call.
 *
 * Rate-limited per address: a spin can produce millions of these, and the
 * useful information is which addresses occur, not how often.
 */
void recomp_icall_not_code_log(uint32_t va)
{
    enum { SLOTS = 16 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    /* Report at 1, 10, 100, 1000 ... rather than once. A single line says a
     * wild pointer was skipped; the progression says it is being skipped in a
     * loop, which is the difference between a curiosity and the reason the
     * title is hung. */
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }
    fprintf(stderr, "[ICALL] target 0x%08X is not code -- skipped %llu time(s) "
                    "(null or wild function pointer, at call #%llu)\n",
            va, (unsigned long long)hits[i],
            (unsigned long long)g_icall_count);
    fflush(stderr);
    /* Diagnostic354: preserve the existing fatal142 result; reuse the original
     * missing-target report to identify the caller of a null/wild target.
     * No register/guest-memory writes and no attempted dispatch of the target. */
    { const char *trace = getenv("DRIVING_FAILURE_TRACE354");
      if (trace && !strcmp(trace,"1")) recomp_icall_fail_log(va); }
    ExitProcess(142);
}
