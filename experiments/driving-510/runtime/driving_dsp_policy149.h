#ifndef DRIVING_DSP_POLICY149_H
#define DRIVING_DSP_POLICY149_H
#include <stdint.h>
#include <stddef.h>
/* Explicit diagnostic handshake policy, NOT a DSP implementation.
 * Invoke only after original PAL17C8ED writes command3. Live context is the
 * original17C788 'this', retained at guest[EBP-0x14]. Callbacks must safely
 * read/write guest DWORDs or fail; they must not clobber guest registers.
 */
typedef int (*driving_dsp_read149)(void*,uint32_t,uint32_t*);
typedef int (*driving_dsp_write149)(void*,uint32_t,uint32_t);
enum{DSP149_INVALID=-1,DSP149_UNKNOWN=-2,DSP149_WAIT=0,DSP149_ACK=1,DSP149_IDLE=2};
static int driving_dsp_span149(uint32_t p,uint32_t n)
{
 uint32_t physical;
 if(!p||(p&3)||!n)return 0;
 if(p<0x04000000u)physical=p;
 else if(p>=0x80000000u&&p<0x84000000u)physical=p-0x80000000u;
 else return 0;
 return n<=0x04000000u-physical;
}
static int driving_dsp_startup149(void *opaque,driving_dsp_read149 read32,
                                  driving_dsp_write149 write32,int enabled,
                                  uint32_t site,uint32_t context,
                                  uint32_t address,uint32_t *observed_command)
{
 uint32_t owner,pages,page0,command;
 if(!read32||!write32||site!=0x0017C8EDu||
    !driving_dsp_span149(context,12)||
    !read32(opaque,context+8,&owner)||
    !driving_dsp_span149(owner,20)||
    !read32(opaque,owner+0x10,&pages)||
    !driving_dsp_span149(pages,4)||!read32(opaque,pages,&page0)||
    !driving_dsp_span149(page0,0x814)||address!=page0+0x810||
    !read32(opaque,address,&command))return DSP149_INVALID;
 if(observed_command)*observed_command=command;
 if(command==0)return DSP149_IDLE;
 if(command!=3)return DSP149_UNKNOWN;
 if(!enabled)return DSP149_WAIT;
 return write32(opaque,address,0)?DSP149_ACK:DSP149_INVALID;
}
#endif
