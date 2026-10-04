#ifndef DRIVING_DISPATCHER143_H
#define DRIVING_DISPATCHER143_H
#include <stdint.h>
#if defined(_MSC_VER)
#define DRIVING_WAIT143_NORETURN __declspec(noreturn)
#else
#define DRIVING_WAIT143_NORETURN _Noreturn
#endif
DRIVING_WAIT143_NORETURN void driving_wait143_stop(const char *operation,uint32_t va);
uint32_t driving_wait143_single(uint32_t object,unsigned alertable,uint32_t timeout);
uint32_t driving_wait143_multiple(unsigned count,uint32_t objects,unsigned wait_type,
                                 unsigned alertable,uint32_t timeout);
uint32_t driving_wait143_set_event(uint32_t event,unsigned wait_next);
void driving_wait143_initialize_timer(uint32_t timer,unsigned type);
void driving_wait143_validate_timer(uint32_t timer);
/* Caller holds the scheduler lock for reset/signal, ordered with cancel/rearm. */
void driving_wait143_reset_timer(uint32_t timer);
void driving_wait143_signal_timer(uint32_t timer);
#endif
