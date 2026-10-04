/* Driving445 generation-validated VirtualQuery region cache (Windows glue).
 * Portable exactness rules live in driving_region445_core.h; see the header
 * driving_region445.h for the runtime modes. Default is the original real
 * VirtualQuery. No guest data, no GPU ownership and no allocation lifetime is
 * cached: only the host page-attribute record of runtime-owned windows whose
 * protection is changed exclusively inside permissions264 write brackets. */
#define DRIVING_REGION445_IMPLEMENTATION 1
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driving_region445.h"
#include "driving_region445_core.h"
#include "driving_permissions264.h"

extern int xbox_Region445Window(int index, uintptr_t *lo, uintptr_t *hi);

#include "driving_query464.h"

static volatile LONG region_mode445 = -1;
static __declspec(thread) DR445Cache region_cache445;
static volatile LONG64 region_counts445[9];
enum { R445_CALLS, R445_HITS, R445_MISSES, R445_STORES, R445_NO_GENERATION,
       R445_OUTSIDE, R445_RACED, R445_VERIFIED, R445_MISMATCHES };
static volatile LONG region_logged445;

static int region445_mode(void)
{
    LONG mode = InterlockedCompareExchange(&region_mode445, -1, -1);
    if (mode < 0) {
        DWORD error = GetLastError();
        const char *text = getenv("DRIVING_REGION_CACHE445");
        LONG desired = text && !strcmp(text, "1") ? 1 : text && !strcmp(text, "2") ? 2 : 0;
        LONG previous = InterlockedCompareExchange(&region_mode445, desired, -1);
        mode = previous < 0 ? desired : previous;
        if (previous < 0)
            fprintf(stderr, "[REGION445] mode=%ld (0=real VirtualQuery,1=cache,2=verify) windows=main+contiguous generation=permissions264\n", (long)mode);
        SetLastError(error);
    }
    return (int)mode;
}

static void region445_convert(const MEMORY_BASIC_INFORMATION *m, DR445Info *out)
{
    memset(out, 0, sizeof *out);
    out->base = (uintptr_t)m->BaseAddress;
    out->allocation_base = (uintptr_t)m->AllocationBase;
    out->allocation_protect = m->AllocationProtect;
    out->region_size = (uintptr_t)m->RegionSize;
    out->state = m->State;
    out->protect = m->Protect;
    out->type = m->Type;
}

static void region445_count(int index)
{
    LONG64 n = InterlockedIncrement64(&region_counts445[index]);
    if (index == R445_CALLS && !(n & ((1 << 16) - 1))) driving_region445_report();
}

void driving_region445_report(void)
{
    DWORD error = GetLastError();
    fprintf(stderr, "[REGION445] mode=%ld calls=%lld hits=%lld misses=%lld stores=%lld no_generation=%lld outside_window=%lld raced=%lld verified=%lld mismatches=%lld exact_record=1\n",
            (long)region_mode445,
            (long long)region_counts445[R445_CALLS], (long long)region_counts445[R445_HITS],
            (long long)region_counts445[R445_MISSES], (long long)region_counts445[R445_STORES],
            (long long)region_counts445[R445_NO_GENERATION], (long long)region_counts445[R445_OUTSIDE],
            (long long)region_counts445[R445_RACED], (long long)region_counts445[R445_VERIFIED],
            (long long)region_counts445[R445_MISMATCHES]);
    query464_report();
    fflush(stderr);
    SetLastError(error);
}

SIZE_T driving_region445_query(LPCVOID address, PMEMORY_BASIC_INFORMATION info, SIZE_T length)
{
    int mode = region445_mode();
    if (!mode || !info || length < sizeof(MEMORY_BASIC_INFORMATION))
        return VirtualQuery(address, info, length);
    region445_count(R445_CALLS);
    uintptr_t a = (uintptr_t)address;
    uint64_t generation = driving_permissions264_peek445();
    DR445Info predicted;
    int slot = generation ? dr445_lookup(&region_cache445, generation, a, &predicted) : 0;
    Query464Row *probe464=query464_begin((uintptr_t)_ReturnAddress(),slot&&mode==1,generation,a,&region_cache445);
    if (!generation) region445_count(R445_NO_GENERATION);
    if (slot && mode == 1) {
        /* Verbatim platform record, with the two address-relative fields
         * recomputed exactly as the kernel reports them for this page. */
        memcpy(info, region_cache445.entries[slot - 1].raw, sizeof(MEMORY_BASIC_INFORMATION));
        info->BaseAddress = (PVOID)predicted.base;
        info->RegionSize = (SIZE_T)predicted.region_size;
        region445_count(R445_HITS);
        return sizeof(MEMORY_BASIC_INFORMATION);
    }
    uint64_t started464=query464_tick(probe464);
    SIZE_T result = VirtualQuery(address, info, length);
    DWORD error = GetLastError();
    query464_end(probe464,started464,result,info,generation,&region_cache445,a);
    if (slot) {
        DR445Info real;
        region445_convert(info, &real);
        region445_count(R445_VERIFIED);
        if (result != sizeof(MEMORY_BASIC_INFORMATION) || !dr445_equal(&real, &predicted)) {
            region445_count(R445_MISMATCHES);
            if (InterlockedIncrement(&region_logged445) <= 16)
                fprintf(stderr, "[REGION445] MISMATCH address=%p generation=%llu cached_base=%p size=%llx state=%lx protect=%lx real_result=%llu base=%p size=%llx state=%lx protect=%lx\n",
                        address, (unsigned long long)generation, (void *)predicted.base,
                        (unsigned long long)predicted.region_size, (unsigned long)predicted.state,
                        (unsigned long)predicted.protect, (unsigned long long)result,
                        info->BaseAddress, (unsigned long long)info->RegionSize,
                        (unsigned long)info->State, (unsigned long)info->Protect);
            dr445_clear(&region_cache445);
        }
        SetLastError(error);
        return result;
    }
    region445_count(R445_MISSES);
    if (result == sizeof(MEMORY_BASIC_INFORMATION) && generation && info->State == MEM_COMMIT) {
        uintptr_t lo, hi;
        int window = -1;
        for (int i = 0; i < 2; ++i)
            if (xbox_Region445Window(i, &lo, &hi) && a >= lo && a < hi) { window = i; break; }
        if (window < 0) region445_count(R445_OUTSIDE);
        else if (driving_permissions264_peek445() != generation) region445_count(R445_RACED);
        else {
            DR445Info real;
            region445_convert(info, &real);
            if (dr445_store(&region_cache445, generation, a, &real, lo, hi, info, sizeof *info))
                region445_count(R445_STORES);
        }
    }
    SetLastError(error);
    return result;
}

#include "driving_hint465.h"
