#ifndef DRIVING_ALIASES176_H
#define DRIVING_ALIASES176_H
#include <stdint.h>
typedef void (*DrivingAlias176)(void);
extern void sub_0013F780(void);
extern void sub_0013F880(void);
/* Original PAL single-JMP allocation/free callbacks. Returning the existing
 * destination directly retains the original guest stack and caller frame. */
static DrivingAlias176 driving_alias176(uint32_t address)
{
    if(address==0x00141860u)return sub_0013F780;
    if(address==0x00141870u)return sub_0013F880;
    return (DrivingAlias176)0;
}
#endif
