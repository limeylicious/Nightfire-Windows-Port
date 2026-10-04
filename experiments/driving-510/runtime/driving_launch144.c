#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "kernel.h"
#include "xbox_memory_layout.h"
#include "driving_launch144_core.h"
extern ptrdiff_t xbox_GetMemoryOffset(void);
uint32_t driving_launch_page144(void)
{
    const char *path=getenv("DRIVING_LAUNCH_PAYLOAD144");
    if(!path || !*path)return 0;
    uint8_t payload[0xa50],page_bytes[0x1000];
    FILE *f=fopen(path,"rb");
    if(!f){fprintf(stderr,"[LAUNCH144] Cannot open original launch payload: %s\n",path);abort();}
    size_t n=fread(payload,1,sizeof(payload),f);
    int extra=fgetc(f),failed=ferror(f);fclose(f);
    if(n!=sizeof(payload) || extra!=EOF || failed ||
       !driving_launch144_page(page_bytes,sizeof(page_bytes),payload,n)){
        fprintf(stderr,"[LAUNCH144] Invalid full PAL launch payload: %s\n",path);abort();
    }
    /* The local ordinal171 bridge frees through xbox_HeapFree. Allocate an
     * aligned tracked guest-heap page so the original consumer can release it.
     * It is CPU launch data; this makes no physical-contiguity/GPU claim. */
    uint32_t page=xbox_HeapAlloc(0x1000,4096);
    if(!page){fprintf(stderr,"[LAUNCH144] Launch page allocation failed\n");abort();}
    memcpy((void *)((uintptr_t)xbox_GetMemoryOffset()+page),page_bytes,sizeof(page_bytes));
    fprintf(stderr,"[LAUNCH144] Complete original payload, type0 title45410026 map%u, page%08X; original consumer owns copy/free\n",
        driving_launch144_u32(payload+0x974),page);
    return page;
}
