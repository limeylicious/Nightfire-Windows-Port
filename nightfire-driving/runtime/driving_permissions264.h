/* Permission generation only; no source-data or GPU ownership lock. */
#ifndef DRIVING_PERMISSIONS264_H
#define DRIVING_PERMISSIONS264_H
#include <stdint.h>
int driving_permissions264_enabled(void);
/* Caller must pair each read_begin with read_end, even if retired returns0. */
uint64_t driving_permissions264_read_begin(void);
void driving_permissions264_read_end(void);
/* Token0 means disabled/no lock; preserve token through the original mutation. */
unsigned driving_permissions264_write_begin(void);
void driving_permissions264_write_end(unsigned token);
void driving_permissions264_retire(void);
#endif
