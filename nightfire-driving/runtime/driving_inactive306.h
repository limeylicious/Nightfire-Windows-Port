#ifndef DRIVING_INACTIVE306_H
#define DRIVING_INACTIVE306_H
#include <stdint.h>
#include <windows.h>
#include <stdlib.h>
#include <string.h>
static int driving_inactive306_setting=-1;
static int driving_inactive306_enabled(void){
 if(driving_inactive306_setting<0){DWORD saved=GetLastError();const char*v=getenv("DRIVING_INACTIVE_STENCIL306");driving_inactive306_setting=v&&!strcmp(v,"1");SetLastError(saved);}return driving_inactive306_setting;
}
/* These seven selectors neither test nor modify stencil when the original
 * stencil-test enable is explicitly known zero. The caller still requires
 * each field's known bit and its full ordinary profile/resource contract.
 * Never waive enable/control/depth/clear state, mutate guest state, or exempt
 * the methods from command barriers. Arbitrary retained selector bits are
 * immaterial while disabled; re-enabling stencil remains a strict refusal. */
static int driving_stencil306(const uint32_t*s,const unsigned char*k,unsigned m){
 switch(m){
 case 0x360:case 0x364:case 0x368:case 0x36c:case 0x370:case 0x374:case 0x378:
  return k[0x32c/4]&&s[0x32c/4]==0&&driving_inactive306_enabled();
 default:return 0;
 }
}
#endif
