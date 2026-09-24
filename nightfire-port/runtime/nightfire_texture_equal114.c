#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "nightfire_texture_equal114.h"
#include "nightfire_bytes_equal.h"
#if defined(_M_X64) || defined(_M_IX86)
#include <intrin.h>
#endif
/* Private kernel: callers must use the checked dispatcher, not this symbol. */
extern int nf_texture_equal114_avx2_kernel(const void *,const void *,size_t);

int nf_texture_equal114_features(unsigned max_leaf,unsigned ecx,uint64_t xcr0,unsigned ebx){
    const unsigned required=(1u<<26)|(1u<<27)|(1u<<28); /* XSAVE/OSXSAVE/AVX */
    return max_leaf>=7 && (ecx&required)==required && (xcr0&6)==6 && (ebx&(1u<<5))!=0;
}
static int detect114(void){
#if defined(_M_X64) || defined(_M_IX86)
    int regs[4];unsigned maximum,ecx;uint64_t xcr0;
    __cpuid(regs,0);maximum=(unsigned)regs[0];
    if(maximum<7)return 0;
    __cpuidex(regs,1,0);ecx=(unsigned)regs[2];
    /* Do not execute XGETBV unless the OS and CPU advertise its prerequisites. */
    if((ecx&((1u<<26)|(1u<<27)|(1u<<28)))!=((1u<<26)|(1u<<27)|(1u<<28)))return 0;
    xcr0=_xgetbv(0);
    if((xcr0&6)!=6)return 0;
    __cpuidex(regs,7,0);
    return nf_texture_equal114_features(maximum,ecx,xcr0,(unsigned)regs[1]);
#else
    return 0;
#endif
}
int nf_texture_equal114_avx2_available(void){
    /* No initialization wait or mutable function pointer. Concurrent first
     * calls may repeat read-only detection; exactly one result is published. */
    static volatile LONG selected;
    /* Windows aligned LONG loads are atomic. This scalar is the entire
     * published state; there is no associated data requiring publication. */
    LONG value=selected;
    if(!value){
        LONG candidate=detect114()?2:1;
        value=InterlockedCompareExchange(&selected,candidate,0);
        if(!value)value=candidate;
    }
    return value==2;
}
int nf_texture_equal114_baseline(const void *left,const void *right,size_t length){
    if(!length)return 1; /* Including inaccessible or null pointers. */
    return nf_bytes_equal(left,right,length);
}
int nf_texture_equal114(const void *left,const void *right,size_t length){
    if(!length)return 1;
    if(nf_texture_equal114_avx2_available())return nf_texture_equal114_avx2_kernel(left,right,length);
    return nf_texture_equal114_baseline(left,right,length);
}
