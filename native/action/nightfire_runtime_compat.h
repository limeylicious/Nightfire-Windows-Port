#ifndef NIGHTFIRE_RUNTIME_COMPAT_H
#define NIGHTFIRE_RUNTIME_COMPAT_H
/* Missing upstream declarations exposed by the first Windows build. */
#include <stdint.h>
#include <stdlib.h>
#include <process.h>
/* Defined in kernel_bridge.c, called before its definition. */
uint32_t xbox_GetConnectedInterrupt(uint32_t vector);
#endif
