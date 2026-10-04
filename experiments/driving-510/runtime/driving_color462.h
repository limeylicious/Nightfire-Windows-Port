#ifndef DRIVING_COLOR462_H
#define DRIVING_COLOR462_H
/* Default OFF. Original packed color masks feed the already bounded integer
 * row helper. No format conversion, GPU state, publication or ownership change. */
#include "driving_clear461.h"
static int color462_setting=-1;
static int color462_enabled(void){
 if(color462_setting<0){
  DWORD error=GetLastError();int crt=errno;fenv_t env;unsigned csr=_mm_getcsr();fegetenv(&env);
  const char*v=getenv("DRIVING_COLOR462");color462_setting=v&&!strcmp(v,"1");
  fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(error);
 }
 return color462_setting;
}
#endif
