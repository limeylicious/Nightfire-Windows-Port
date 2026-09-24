/* Private CPU-span observer. No ownership or synchronization authority.
 * Exact registered guest apertures only. Bulk/atomic/kernel/escaped-pointer
 * accesses and other aliases are unobserved unless explicitly instrumented. */
#ifndef DRIVING_CPU320_H
#define DRIVING_CPU320_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#define DRIVING_CPU320_FIRST 16u
#define DRIVING_CPU320_RECENT 32u
typedef struct DrivingCpu320Event {
 uint64_t sequence,epoch;
 uintptr_t site;
 uint32_t thread,address,bytes,span_index,span_address,span_bytes;
} DrivingCpu320Event;
typedef struct DrivingCpu320Snapshot {
 uint64_t epoch,publications,clears,invalid_publications,invalid_notes,notes,hits,overwritten,init_refused;
 uint32_t address[2],bytes[2],valid,first_count,recent_count,recent_next;
 DrivingCpu320Event first[DRIVING_CPU320_FIRST],recent[DRIVING_CPU320_RECENT];
#ifdef DRIVING_ACCESS321
 uint64_t category_notes[32],category_hits[32];
#endif
} DrivingCpu320Snapshot;
/* Init is called only before workers start; it never changes enabled once on. */
extern int driving_cpu320_enabled;
extern volatile LONG driving_cpu320_apertures;
int driving_cpu320_init(int enabled);
void driving_cpu320_publish(const uint32_t addresses[2],const uint32_t bytes[2]);
void driving_cpu320_clear(void);
void driving_cpu320_note(uint32_t address,unsigned bytes,uintptr_t site);
int driving_cpu320_snapshot(DrivingCpu320Snapshot *out);
void driving_cpu320_report(void);
#if defined(_MSC_VER)
#define CPU320_INLINE static __forceinline
#else
#define CPU320_INLINE static inline
#endif
/* Monotonic coarse hint: publication can add apertures, never remove them.
 * Clearing exact watches may leave false positives, never false negatives.
 * Widths >8 use the full path; sized generated macros use1/2/4/8. */
CPU320_INLINE void driving_cpu320_note_if_possible(uint32_t address,unsigned bytes,uintptr_t site){
 if(!driving_cpu320_enabled)return;
 if(bytes && bytes<=8 && (uint64_t)address+bytes<=UINT64_C(0x100000000)){
  unsigned first=address>>28,last=(uint32_t)((uint64_t)address+bytes-1)>>28;
  LONG hint=InterlockedCompareExchange(&driving_cpu320_apertures,0,0);
  if(!((unsigned)hint&((1u<<first)|(1u<<last))))return;
 }
 driving_cpu320_note(address,bytes,site);
}
#endif
