/* Declarations missing from the pinned runtime's Windows source files.
 * Forced include in this project only; no runtime behavior or toolkit edits.
 * In particular getenv must return a 64-bit pointer on this host.
 */
#include <stdint.h>
#include <stdlib.h>
#include <process.h>
uint32_t xbox_GetConnectedInterrupt(uint32_t vector);
