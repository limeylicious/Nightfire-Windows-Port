/* Optional diagnostic controller file: hex buttons, LX LY RX RY LT RT.
 * Legacy one-field button files remain valid. Never edits game state directly.
 */
#ifndef NIGHTFIRE_TEST_INPUT_H
#define NIGHTFIRE_TEST_INPUT_H
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static unsigned nf_test_input_parse(const char *text,unsigned char state[18]) {
 unsigned buttons=0;int lx=0,ly=0,rx=0,ry=0,lt=0,rt=0;
 memset(state,0,18);
 int n=sscanf(text,"%x %d %d %d %d %d %d",&buttons,&lx,&ly,&rx,&ry,&lt,&rt);
 if(n!=1 && n!=7)return 0;
 if(lx < -32768 || lx>32767 || ly < -32768 || ly>32767 ||
    rx < -32768 || rx>32767 || ry < -32768 || ry>32767 || lt<0 || lt>255 || rt<0 || rt>255)return 0;
 int16_t axes[4]={(int16_t)lx,(int16_t)ly,(int16_t)rx,(int16_t)ry};
 state[8]=(unsigned char)lt;state[9]=(unsigned char)rt;memcpy(state+10,axes,8);
 return buttons&0xf00ffu;
}
#endif
