#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "nightfire_diagnostics.h"
#include "../runtime/nightfire_flight_host116.h"

typedef void (*recomp_func_t)(void);
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;

extern void sub_000CE280(void);
extern void sub_000D0F90(void);
extern void sub_0006ED30(void);
extern void sub_000C0DA0(void);
extern void sub_000A3CF0(void);
extern void sub_0006F550(void);
extern void sub_000B1F90(void);
extern void sub_000B2870(void);
extern void sub_000B6330(void);
extern void sub_000B67D0(void);
extern void sub_000B5190(void);
extern void sub_000B69E0(void);
extern void sub_000B54D0(void);
extern void sub_000B2B50(void);
extern void sub_000B4330(void);
extern void sub_000B4760(void);
extern void sub_000B4090(void);
extern void sub_0004F100(void);
extern void sub_0004F790(void);
extern void sub_000CE5E0(void);
extern void sub_00031530(void);
extern void sub_00033320(void);
extern void sub_0003EFF0(void);
extern void sub_000AE1C0(void);
extern void sub_000A8A60(void);
extern void sub_00033590(void);
extern void sub_00071530(void);
extern void sub_00022BD0(void);
extern void sub_00068510(void);
extern void sub_0005D390(void);
extern void sub_0004F440(void);
extern void sub_0004FA40(void);
extern void sub_00052660(void);
extern void sub_000528C0(void);
extern void sub_000556F0(void);
extern void sub_0004BBF0(void);
extern void sub_00058190(void);
extern void sub_00059040(void);
extern void sub_0005BF10(void);
extern void sub_0005DC10(void);
extern void sub_0004E040(void);
extern void sub_000324D0(void);
extern void sub_00157334(void);
extern void sub_00157C64(void);
extern void sub_001588A0(void);
extern void sub_00158E8E(void);
extern void sub_0015B5D5(void);
extern recomp_func_t nightfire_lookup_render_state(uint32_t va);
extern recomp_func_t nightfire_audio_lookup(uint32_t va);

recomp_func_t recomp_lookup_manual(uint32_t address)
{
    recomp_func_t audio = nightfire_audio_lookup(address);
    if (audio) return audio;
    switch (address) {
    case 0x000CE280u: return sub_000CE280;
    case 0x000D0F90u: return sub_000D0F90;
    case 0x000C0DA0u: return sub_000C0DA0;
    case 0x000A3CF0u: return sub_000A3CF0;
    case 0x0004E040u: return sub_0004E040;
    case 0x0005DC10u: return sub_0005DC10;
    case 0x0005BF10u: return sub_0005BF10;
    case 0x00059040u: return sub_00059040;
    case 0x00058190u: return sub_00058190;
    case 0x0004BBF0u: return sub_0004BBF0;
    case 0x000556F0u: return sub_000556F0;
    case 0x000528C0u: return sub_000528C0;
    case 0x00052660u: return sub_00052660;
    case 0x0004FA40u: return sub_0004FA40;
    case 0x0004F440u: return sub_0004F440;
    case 0x0005D390u: return sub_0005D390;
    case 0x00068510u: return sub_00068510;
    case 0x00022BD0u: return sub_00022BD0;
    case 0x00071530u: return sub_00071530;
    case 0x00033590u: return sub_00033590;
    case 0x000A8A60u: return sub_000A8A60;
    case 0x000AE1C0u: return sub_000AE1C0;
    case 0x0003EFF0u: return sub_0003EFF0;
    case 0x00033320u: return sub_00033320;
    case 0x00031530u: return sub_00031530;
    case 0x000CE5E0u: return sub_000CE5E0;
    case 0x0004F790u: return sub_0004F790;
    case 0x0004F100u: return sub_0004F100;
    case 0x000B4090u: return sub_000B4090;
    case 0x000B4760u: return sub_000B4760;
    case 0x000B4330u: return sub_000B4330;
    case 0x000B2B50u: return sub_000B2B50;
    case 0x000B1F90u: return sub_000B1F90;
    case 0x000B2870u: return sub_000B2870;
    case 0x000B6330u: return sub_000B6330;
    case 0x000B67D0u: return sub_000B67D0;
    case 0x000B5190u: return sub_000B5190;
    case 0x000B69E0u: return sub_000B69E0;
    case 0x000B54D0u: return sub_000B54D0;
    case 0x0006F550u: return sub_0006F550;
    case 0x0006ED30u: return sub_0006ED30;
    case 0x000324D0u: return sub_000324D0;
    case 0x00157334u: return sub_00157334;
    case 0x00157C64u: return sub_00157C64;
    case 0x001588A0u: return sub_001588A0;
    case 0x00158E8Eu: return sub_00158E8E;
    case 0x0015B5D5u: return sub_0015B5D5;
    default: return nightfire_lookup_render_state(address);
    }
}

void nightfire_diagnostic_stop(const char *function, const char *reason)
{
    fprintf(stderr, "\n[NIGHTFIRE STOP] %s: %s\n", function, reason);
    nightfire_call_history_dump();
    for (unsigned i = 0; i < 16; ++i) {
        unsigned slot = (g_icall_trace_idx + i) & 15;
        fprintf(stderr, "  recent indirect call: 0x%08X\n", g_icall_trace[slot]);
    }
    fflush(stderr);
    nightfire_flight116_stop(function, reason);
    exit(2);
}

void recomp_icall_fail_log(uint32_t address)
{
    fprintf(stderr, "Unresolved indirect target 0x%08X\n", address);
    nightfire_diagnostic_stop("indirect call", "target missing from dispatch");
}

void recomp_icall_not_code_log(uint32_t address)
{
    fprintf(stderr, "Invalid indirect target 0x%08X\n", address);
    nightfire_diagnostic_stop("indirect call", "target is not code");
}
