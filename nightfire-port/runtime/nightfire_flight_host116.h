#ifndef NIGHTFIRE_FLIGHT_HOST116_H
#define NIGHTFIRE_FLIGHT_HOST116_H
#include <stdint.h>
void nightfire_flight116_start(void);
void nightfire_flight116_stop(const char *function,const char *reason);
void nightfire_flight116_pad(const unsigned char state[18],uint32_t frame,uint32_t tick);
void nightfire_flight116_load(uint32_t guest_sp);
#endif
