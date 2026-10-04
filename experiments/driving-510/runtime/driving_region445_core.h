/* Driving445: portable core of the generation-validated VirtualQuery region
 * cache. No Windows headers here, so the exactness fixture can run anywhere.
 *
 * Contract (see driving_region445.c for the Windows glue):
 *  - An entry records one real query of address A at permission generation G:
 *    [page(A), page(A)+RegionSize) had identical attributes at G.
 *  - It may answer a later query of B only while the generation is still G,
 *    B lies inside that span, and the span is inside a whitelisted runtime-owned
 *    window whose protection changes are all bracketed by permissions264.
 *  - The answer is synthesised exactly as VirtualQuery(B) would report it:
 *    BaseAddress=page(B), RegionSize=end-page(B), all other fields copied.
 *  - Generation 0 means "do not use or fill the cache" (disabled, retired).
 * The core never decides generation or window policy; the caller supplies them.
 */
#ifndef DRIVING_REGION445_CORE_H
#define DRIVING_REGION445_CORE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define DR445_ENTRIES 8u
#define DR445_PAGE ((uintptr_t)4096u)

typedef struct DR445Info {
    uintptr_t base;            /* BaseAddress as reported */
    uintptr_t allocation_base;
    uint32_t allocation_protect;
    uint32_t partition_id;
    uintptr_t region_size;
    uint32_t state, protect, type;
} DR445Info;

#define DR445_RAW_BYTES 64u
typedef struct DR445Entry {
    uint64_t generation;       /* 0 = empty */
    uintptr_t begin, end;      /* page-aligned [begin,end) */
    DR445Info info;            /* as reported for begin */
    unsigned char raw[DR445_RAW_BYTES]; /* platform record copied verbatim */
} DR445Entry;

typedef struct DR445Cache {
    DR445Entry entries[DR445_ENTRIES];
    unsigned next;
} DR445Cache;

static uintptr_t dr445_page(uintptr_t a) { return a & ~(DR445_PAGE - 1u); }

/* Returns slot+1 and fills *out when a same-generation entry covers address. */
static int dr445_lookup(const DR445Cache *c, uint64_t generation, uintptr_t address, DR445Info *out)
{
    if (!c || !generation) return 0;
    uintptr_t page = dr445_page(address);
    for (unsigned i = 0; i < DR445_ENTRIES; ++i) {
        const DR445Entry *e = &c->entries[i];
        if (e->generation != generation || page < e->begin || page >= e->end) continue;
        *out = e->info;
        out->base = page;
        out->region_size = e->end - page;
        return (int)i + 1;
    }
    return 0;
}

/* Store a real result for `address` observed entirely at `generation`.
 * window_lo/hi bound the runtime-owned allocation that may be cached.
 * raw/raw_bytes is the platform record, copied verbatim for later answers.
 * Returns slot+1 when stored; refusals only lose the optimisation. */
static int dr445_store(DR445Cache *c, uint64_t generation, uintptr_t address,
                       const DR445Info *real, uintptr_t window_lo, uintptr_t window_hi,
                       const void *raw, size_t raw_bytes)
{
    if (!c || !generation || !real || raw_bytes > DR445_RAW_BYTES) return 0;
    uintptr_t page = dr445_page(address);
    if (real->base != page || !real->region_size || (real->region_size & (DR445_PAGE - 1u))) return 0;
    if (real->region_size > UINTPTR_MAX - real->base) return 0;
    uintptr_t end = real->base + real->region_size;
    if (window_lo >= window_hi || page < window_lo || end > window_hi) return 0;
    /* Called only after a lookup miss, so no current entry covers this page.
     * Reuse the first stale-generation slot, else round-robin. */
    unsigned slot = DR445_ENTRIES;
    for (unsigned i = 0; i < DR445_ENTRIES; ++i)
        if (c->entries[i].generation != generation) { if (slot == DR445_ENTRIES) slot = i; }
    if (slot == DR445_ENTRIES) { slot = c->next % DR445_ENTRIES; c->next++; }
    DR445Entry *e = &c->entries[slot];
    e->generation = generation;
    e->begin = page;
    e->end = end;
    e->info = *real;
    memset(e->raw, 0, sizeof e->raw);
    if (raw && raw_bytes) memcpy(e->raw, raw, raw_bytes);
    return (int)slot + 1;
}

static int dr445_equal(const DR445Info *a, const DR445Info *b)
{
    return a->base == b->base && a->allocation_base == b->allocation_base &&
           a->allocation_protect == b->allocation_protect && a->partition_id == b->partition_id &&
           a->region_size == b->region_size && a->state == b->state &&
           a->protect == b->protect && a->type == b->type;
}

static void dr445_clear(DR445Cache *c) { if (c) memset(c, 0, sizeof *c); }
#endif
