/* Permission observations for one draw's preflight only.
 * This is not an allocation lock. Mapping/protection must remain stable through
 * the original draw lifetime, exactly as required by the existing raw pointers.
 * Never reuse the context after GPU work, a callback, replay or guest return. */
#ifndef DRIVING_MAPPING230_H
#define DRIVING_MAPPING230_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "driving_mapping259.h"
#ifndef DRIVING_MAP230_QUERY
#ifdef DRIVING_REGION_CACHE445
/*465: this internal caller always supplies its own valid stack MBI.*/
SIZE_T driving_region465_map_query(LPCVOID,PMEMORY_BASIC_INFORMATION,SIZE_T);
#define DRIVING_MAP230_QUERY(address, info, bytes) driving_region465_map_query(address, info, bytes)
#else
#define DRIVING_MAP230_QUERY(address, info, bytes) VirtualQuery(address, info, bytes)
#endif
#endif
typedef struct DrivingMapRegion230 {
    uintptr_t begin, end;
    DWORD state, protect;
} DrivingMapRegion230;
typedef struct DrivingMap230 {
    DrivingMapRegion230 regions[16];
    unsigned count, active;
} DrivingMap230;
static void driving_map230_begin(DrivingMap230 *ctx)
{
    memset(ctx, 0, sizeof *ctx);
    ctx->active = 1;
    DM259_CONTEXT(1);
}
static void driving_map230_end(DrivingMap230 *ctx)
{
    DM259_CONTEXT(0);
    /* Discard observations; returned pointers retain the caller's own lifetime. */
    memset(ctx, 0, sizeof *ctx);
}
static void *driving_map230(DrivingMap230 *ctx, uint32_t va, size_t bytes,
                           int write, uintptr_t offset, uint32_t allocated)
{
    DM259_DECLARE(ctx,bytes,write);
    if (!ctx || !ctx->active || ctx->count > 16 || va < 0x80000000u || !bytes)
        DM259_RETURN(NULL,DM259_INPUT);
    DM259_CACHE(ctx->count);
    uint32_t physical = va - 0x80000000u;
    if (physical >= allocated || bytes > (size_t)(allocated - physical) ||
        offset > UINTPTR_MAX - va) DM259_RETURN(NULL,DM259_INPUT);
    uintptr_t begin = offset + va;
    if (bytes > UINTPTR_MAX - begin) DM259_RETURN(NULL,DM259_INPUT);
    uintptr_t at = begin, end = begin + bytes;
    while (at < end) {
        DM259_INC(steps);
        DrivingMapRegion230 observed, *region = NULL;
        for (unsigned i = 0; i < ctx->count; ++i) {
            DM259_INC(probes);
            if (ctx->regions[i].begin <= at && at < ctx->regions[i].end) {
                DM259_INC(hits);
                region = &ctx->regions[i];
                break;
            }
        }
        if (!region) {
            MEMORY_BASIC_INFORMATION m;
            DM259_INC(queries);
            if (DRIVING_MAP230_QUERY((const void *)at, &m, sizeof m) != sizeof m)
                DM259_RETURN(NULL,DM259_QUERY_FAILED);
            observed.begin = (uintptr_t)m.BaseAddress;
            if (observed.begin > at || m.RegionSize > UINTPTR_MAX - observed.begin)
                DM259_RETURN(NULL,DM259_METADATA);
            observed.end = observed.begin + m.RegionSize;
            observed.state = m.State;
            observed.protect = m.Protect;
            if (observed.end <= at) DM259_RETURN(NULL,DM259_METADATA);
            DM259_REGION(m.RegionSize);
            region = &observed;
            /* Saturation loses an optimization, never support for a valid span. */
            if (ctx->count < 16) {
                ctx->regions[ctx->count] = observed;
                region = &ctx->regions[ctx->count++];
                DM259_INC(inserts);DM259_CACHE(ctx->count);
            }else{DM259_INC(saturated);}
        }
        DWORD allowed = write ? (PAGE_READWRITE | PAGE_EXECUTE_READWRITE) :
            (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE);
        if (region->state != MEM_COMMIT ||
            (region->protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            !(region->protect & allowed)) DM259_RETURN(NULL,DM259_PERMISSION);
        at = region->end;
    }
    DM259_RETURN((void *)begin,DM259_OK);
}
#endif

