#ifndef DRIVING_LIGHTING301_H
#define DRIVING_LIGHTING301_H
#include <windows.h>
#include <stdlib.h>
#include <string.h>
static int driving_lighting301_setting=-1;
static int driving_lighting301_enabled(void){
 if(driving_lighting301_setting<0){DWORD saved=GetLastError();const char*v=getenv("DRIVING_LIGHTING301");driving_lighting301_setting=v&&!strcmp(v,"1");SetLastError(saved);}return driving_lighting301_setting;
}
/* Only the complete original300 profile18 fog/anisotropy tuple. This helper
 * does not replace full code/constants/knownness/arrays/material validation. */
static int driving_group301(const uint32_t*s,const unsigned char*k,unsigned family,unsigned primitive){
 if(family!=18||primitive!=6||!driving_lighting301_enabled())return 0;
 static const unsigned m[]={0x29c,0x2a0,0x2a4,0x2a8,0x9c0,0x9c4,0x9c8,0x300,0x304,0x308,0x30c,0x314,0x340,0x344,0x348,0x354,0x35c,0x1b0c};
 static const uint32_t v[]={0x2601,2,1,0xff140f16,0x40000b4e,0xba34e335,0,1,1,0,1,0,1,0x302,0x303,0x203,1,0x4003ffc0};
 for(unsigned i=0;i<sizeof m/sizeof*m;i++)if(!k[m[i]/4]||s[m[i]/4]!=v[i])return 0;
 return 1;
}
#endif
