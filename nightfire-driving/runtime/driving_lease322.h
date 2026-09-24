#ifndef DRIVING_LEASE322_H
#define DRIVING_LEASE322_H
#include <windows.h>
#include <stdint.h>
/* A lifetime pin for one EXISTING synchronous publication, not exclusive
 * content ownership. Native pointers may still access the backing directly.
 * The only admitted purpose forbids extending an unpublished GPU interval. */
enum { DL322_PUBLISH_EXISTING=1, DL322_RETAIN_GUEST=2, DL322_UNKNOWN_ESCAPE=3 };
typedef struct {
 uint32_t va,bytes,base,size;
 uint64_t generation;
 uintptr_t native,backing;
} DL322Key;
typedef struct {
 SRWLOCK *lock;
 const void *allocator;
 uintptr_t offset;
 DWORD thread;
 unsigned active;
} DL322Lease;
int xbox_ContiguousKey322(uint32_t va,uint32_t bytes,const void *native,DL322Key *out);
int xbox_ContiguousLease322Begin(const DL322Key keys[2],DL322Lease *lease,unsigned purpose);
int xbox_ContiguousLease322End(DL322Lease *lease);
#endif
