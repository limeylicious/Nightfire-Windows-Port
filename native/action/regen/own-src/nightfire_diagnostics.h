#ifndef NIGHTFIRE_DIAGNOSTICS_H
#define NIGHTFIRE_DIAGNOSTICS_H
#include <stdint.h>
void nightfire_memory_barrier(void);
void nightfire_dsp_startup_command(uint32_t address);
void nightfire_ac97_write8(uint32_t address, uint8_t value);
void nightfire_thread_check_arm(void);
void nightfire_fence_trace(const char *stage, uint32_t device, uint32_t target, uint32_t completed);
uint8_t nightfire_port_read8(uint16_t port);
void nightfire_port_write8(uint16_t port, uint8_t value);
void nightfire_diagnostic_stop(const char *function, const char *reason);
void nightfire_memory_trace_dump(void);
void nightfire_call_history_dump(void);
#endif
