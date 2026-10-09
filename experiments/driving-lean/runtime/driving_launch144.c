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
#ifdef DRIVING_LEAN_RENDERER
    {   /* LEAN_VIDEO_MODE=pal60|pal|ntsc (default: payload as captured). Payload +0xA1C is the
         * video mode the action game hands over (copied to 0x2445AC; Driving's own parser uses
         * ntsc=0, pal=2, pal60=3). It picks the game's step timer: table 0x1A204C gives 50 Hz
         * for pal and 60 Hz otherwise (timer 0x10AE50 -> callback list -> 0x5B9F0 -> 0x1E5204,
         * one 1/60 s step per tick). The capture came from a PAL50 run (2); a PAL console with
         * PAL60 enabled hands over 3, which is what pal60 reproduces. */
        const char *m=getenv("LEAN_VIDEO_MODE");
        if(m&&n==sizeof(payload)){
            int v=!strcmp(m,"pal60")?3:!strcmp(m,"pal")?2:!strcmp(m,"ntsc")?0:-1;
            if(v>=0){uint32_t old=driving_launch144_u32(payload+0xA1C);
                payload[0xA1C]=(uint8_t)v;payload[0xA1D]=payload[0xA1E]=payload[0xA1F]=0;
                fprintf(stderr,"[LEAN] launch video mode %s (payload +0xA1C %u -> %d)\n",m,old,v);}
            else fprintf(stderr,"[LEAN] LEAN_VIDEO_MODE=%s not recognised (pal60|pal|ntsc); payload unchanged\n",m);
        }
    }
#endif
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
