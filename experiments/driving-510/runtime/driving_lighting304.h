#ifndef DRIVING_LIGHTING304_H
#define DRIVING_LIGHTING304_H
#include <windows.h>
#include <stdlib.h>
#include <string.h>
static int driving_lighting304_setting=-1;
static int driving_lighting304_enabled(void){
 if(driving_lighting304_setting<0){DWORD saved=GetLastError();const char*v=getenv("DRIVING_LIGHTING304");driving_lighting304_setting=v&&!strcmp(v,"1");SetLastError(saved);}return driving_lighting304_setting;
}
static unsigned driving_group304(const uint32_t*s,const unsigned char*k,unsigned family,unsigned primitive){
 if(primitive!=6||!driving_lighting304_enabled())return 0;
 if(family==18){
  /* Only the already-qualified301 material, with these recorded inactive
   * stencil words. Test disabled means no stencil test or operation occurs. */
  if(!driving_group301(s,k,family,primitive))return 0;
  static const unsigned m[]={0x32c,0x360,0x364,0x368,0x36c,0x378};
  static const uint32_t v[]={0,1,0x205,1,1,0x1e01};
  for(unsigned i=0;i<sizeof m/sizeof*m;i++)if(!k[m[i]/4]||s[m[i]/4]!=v[i])return 0;
  return 9;
 }
 if(family==12){
  static const unsigned m[]={0x29c,0x2a0,0x2a4,0x2a8,0x9c0,0x9c4,0x9c8,0x300,0x304,0x308,0x30c,0x314,0x32c,0x330,0x334,0x338,0x340,0x344,0x348,0x354,0x35c,0x1768,0x1bc4,0x1bc8,0x1bcc,0x1bd4};
  static const uint32_t v[]={0x2601,2,0,0xff140f16,0x40000b4e,0xba34e335,0,0,1,0,1,0,0,0,0,0,16,0x302,0x303,0x203,0,0x822,0x06610e29,0x10101,0x4003ffc0,0x02063f01};
  for(unsigned i=0;i<sizeof m/sizeof*m;i++)if(!k[m[i]/4]||s[m[i]/4]!=v[i])return 0;
  return 10;
 }
 return 0;
}
static int driving_stencil304(unsigned m,uint32_t v){
 switch(m){case 0x360:case 0x368:case 0x36c:return v==1;case 0x364:return v==0x205;case 0x378:return v==0x1e01;default:return 0;}
}
#endif

