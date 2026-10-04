#ifndef DRIVING_PC508_H
#define DRIVING_PC508_H
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
/* Checkpoint508 only. Runtime opt-in, initialized before guest/worker startup.
 * 0 permission264, 1 contig237, 2 publication324, 3 APU MMIO trap. */
extern volatile LONG driving_pc508_active;
extern __declspec(thread) unsigned driving_pc508_depth[4];
void driving_pc508_startup(void);
int driving_pc508_enabled(void);
void driving_pc508_guard_native(const void *address,size_t bytes,const char *site);
void driving_pc508_guard_guest(uint32_t address,size_t bytes,const char *site);
#endif
