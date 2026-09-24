/* Original PAL XLaunchNewImage type-0 contract; see driving-launch142.md.
 * This wraps a COMPLETE original A50-byte payload; it does not invent settings. */
#ifndef DRIVING_LAUNCH144_CORE_H
#define DRIVING_LAUNCH144_CORE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
static uint32_t driving_launch144_u32(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int driving_launch144_page(uint8_t *page,size_t capacity,const uint8_t *payload,size_t bytes)
{
    if(!page || !payload || capacity<0x1000 || bytes!=0xa50)return 0;
    /* Only the original valid-state marker and observed map range are checked.
     * Profile, PAL language and controller bytes are preserved verbatim. */
    uint32_t map=driving_launch144_u32(payload+0x974);
    if(driving_launch144_u32(payload+0x960)!=1 || map<1 || map>8)return 0;
    memset(page,0,0x1000);
    page[4]=0x26;page[5]=0x00;page[6]=0x41;page[7]=0x45;
    memcpy(page+0x400,payload,0xa50);
    return 1;
}
#endif
