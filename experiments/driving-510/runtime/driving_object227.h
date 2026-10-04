/* Fresh bounded PRAM table read; no mapping/protection or descriptor cache. */
#ifndef DRIVING_OBJECT227_H
#define DRIVING_OBJECT227_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
extern ptrdiff_t xbox_GetMemoryOffset(void);
static int driving_object227_read(void *ctx,uint32_t va,void *destination,size_t bytes)
{
    (void)ctx;
    if(!destination || (bytes!=4096 && bytes!=8192 && bytes!=16384 && bytes!=32768) ||
       (va&4095u) || va<0xfd700000u || (uint64_t)va+bytes>0xfd800000ull)return 0;
    uintptr_t address=(uintptr_t)xbox_GetMemoryOffset()+va;
    if(address<va || bytes>UINTPTR_MAX-address)return 0;
#ifdef DRIVING_OBJECT227_BEFORE_READ
    DRIVING_OBJECT227_BEFORE_READ(va,bytes);
#endif
    SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),(const void*)address,destination,bytes,&copied) && copied==bytes;
}
#endif
