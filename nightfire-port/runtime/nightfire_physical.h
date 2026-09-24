#ifndef NIGHTFIRE_PHYSICAL_H
#define NIGHTFIRE_PHYSICAL_H
#include <stdint.h>
/* Translate the runtime's separately mapped 64 MiB contiguous arena.
 * Other guest mappings retain the pinned runtime's existing behavior. */
static inline uint32_t nightfire_contiguous_physical(uint32_t va)
{
    return va >= 0x80000000u && va < 0x84000000u ? va - 0x80000000u : va;
}
#endif
