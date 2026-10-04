/* Owned preparation for the exact captured stage3 indexed texture. Original
 * state/indices/palette remain untouched. No guest reads or persistent cache. */
#ifndef DRIVING_PALETTE287_H
#define DRIVING_PALETTE287_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
enum {DP287_INDEX_BYTES=4096,DP287_PALETTE_BYTES=1024,DP287_BGRA_BYTES=16384,
      DP287_EXTRA_BYTES=DP287_PALETTE_BYTES+DP287_BGRA_BYTES};
static int driving_palette287_state(const uint32_t *s,const unsigned char *k,unsigned slot){
 if(!s||!k||slot!=3)return 0;
 const unsigned at[]={0x184,0x1bc0,0x1bc4,0x1bc8,0x1bcc,0x1bd4,0x1be0};
 for(unsigned i=0;i<sizeof at/sizeof at[0];i++)if(!k[at[i]/4])return 0;
 return s[0x1bc4/4]==0x06610b29u&&s[0x1bc8/4]==0x30303u&&
        s[0x1bcc/4]==0x4003ffc0u&&s[0x1bd4/4]==0x02063f01u&&!(s[0x1be0/4]&63u);
}
static int driving_palette287_overlap(const void *a,size_t an,const void *b,size_t bn){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !x||!y||an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y||(x+an>y&&y+bn>x);
}
/* Inputs must already be complete owned readable snapshots. All layout/capacity
 * checks precede writes. Swizzle is a texel ordinal permutation, independent of
 * bytes-per-texel: expanding each index ordinal preserves format6's layout. */
static int driving_palette287_expand(void *out,size_t available,const void *indices,size_t index_bytes,
                                     const void *palette,size_t palette_bytes){
 if(!out||!indices||!palette||available<DP287_BGRA_BYTES||index_bytes<DP287_INDEX_BYTES||palette_bytes<DP287_PALETTE_BYTES||
    driving_palette287_overlap(out,DP287_BGRA_BYTES,indices,DP287_INDEX_BYTES)||
    driving_palette287_overlap(out,DP287_BGRA_BYTES,palette,DP287_PALETTE_BYTES))return 0;
 const unsigned char *ix=(const unsigned char*)indices,*pal=(const unsigned char*)palette;
 unsigned char *dst=(unsigned char*)out;
 for(unsigned i=0;i<DP287_INDEX_BYTES;i++)memcpy(dst+4*i,pal+4*(unsigned)ix[i],4);
 return 1;
}
#endif
