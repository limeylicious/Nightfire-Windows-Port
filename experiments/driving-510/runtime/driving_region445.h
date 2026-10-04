/* Driving445 optional VirtualQuery region cache (compile gate DRIVING_REGION_CACHE445).
 *
 * Include immediately after <windows.h> in a translation unit whose hot
 * permission checks should use the cache. Every later textual VirtualQuery call
 * in that unit (including included headers such as mapping230, host_spans261,
 * command_read265, semaphore281 and pair234/235) is routed through
 * driving_region445_query, which returns exactly what VirtualQuery would
 * return for the same address or falls back to the real call.
 *
 * Runtime gate DRIVING_REGION_CACHE445:
 *   unset/0  real VirtualQuery only (original behaviour, one getenv per process)
 *   1        answer from the per-thread cache when the permission generation
 *            and the runtime-owned window prove the recorded span unchanged
 *   2        verify: always call the real VirtualQuery, compare with what the
 *            cache would have returned, count/log mismatches, return real data
 * The cache also requires DRIVING_BATCH_MAP264=1 (permission tracking ON);
 * otherwise it behaves exactly like mode0. */
#ifndef DRIVING_REGION445_H
#define DRIVING_REGION445_H
#include <windows.h>
SIZE_T driving_region445_query(LPCVOID address, PMEMORY_BASIC_INFORMATION info, SIZE_T length);
void driving_region445_report(void);
#ifndef DRIVING_REGION445_IMPLEMENTATION
#define VirtualQuery driving_region445_query
#endif
#endif
