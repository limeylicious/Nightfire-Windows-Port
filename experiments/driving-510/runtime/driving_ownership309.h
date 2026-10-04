/* Source-entry diagnostic API only; no guest execution is replaced. */
#ifndef DRIVING_OWNERSHIP309_H
#define DRIVING_OWNERSHIP309_H
#include <stdint.h>
enum { OWN309_CREATE,OWN309_DESTROY,OWN309_BIND,OWN309_LOCK,
       OWN309_RESOURCE_WAIT,OWN309_VERTEX_WAIT,OWN309_IDLE,OWN309_FENCE,OWN309_KINDS };
typedef struct {
 uint64_t call;
 uint32_t function,kind,tid,caller,stack,args[6],device,qualified,entered;
 uint32_t nested_before,stack_valid,device_valid;
} OwnScope309;
OwnScope309 own_enter309(uint32_t function,unsigned kind);
void own_leave309(OwnScope309 *scope);
void driving_ownership309_report(void);
#endif
