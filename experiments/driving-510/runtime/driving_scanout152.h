#ifndef DRIVING_SCANOUT152_H
#define DRIVING_SCANOUT152_H
#include <stdint.h>
/* Diagnostic executor serialization, not a guest/GPU fence implementation. */
void driving_scanout152_enter(void);
void driving_scanout152_leave(unsigned complete);
void driving_scanout152_begin(unsigned primitive);
void driving_scanout152_report(void);
void driving_scanout152_av(uint32_t mode,uint32_t format,uint32_t pitch,uint32_t physical);
void driving_scanout155_leave(unsigned complete,unsigned submitted);
void driving_scanout152_capture(uint32_t context,uint32_t base,uint32_t physical,uint32_t argument_address);
/* Pure conservative contract: the only already-observed display mode. */
static int driving_scanout152_bounds(uint32_t physical,uint32_t original,
    uint32_t allocated,uint32_t pitch,uint32_t mode,uint32_t flags,uint32_t inhibited,
    uint32_t av_mode,uint32_t av_format,uint32_t av_pitch)
{
    return physical && physical==original && !(physical&3u)
        && allocated<=0x08000000u && (uint64_t)physical+2560u*480u<=allocated
        && pitch==2560 && mode==0x0801010du && flags==0x00600104u && !inhibited
        && av_mode==mode && av_format==0x12 && av_pitch==pitch;
}
/* Only the established original AV linear32 mode; no assumed current target. */
static int driving_avbuffer155_bounds(uint32_t physical,uint32_t allocated,
    uint32_t mode,uint32_t format,uint32_t pitch)
{
    return physical && !(physical&3u) && allocated<=0x08000000u
        && (uint64_t)physical+2560u*480u<=allocated
        && mode==0x0801010du && format==0x12u && pitch==2560u;
}
#endif
