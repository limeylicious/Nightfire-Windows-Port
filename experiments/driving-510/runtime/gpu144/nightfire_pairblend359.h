/* Extend only the private batch's proven material blend285. This gate changes
 * no GPU state, shader math, blend equation, target identity or handoff. */
#ifndef NIGHTFIRE_PAIRBLEND359_H
#define NIGHTFIRE_PAIRBLEND359_H
#include <errno.h>
static int nf_pair_blend359_enabled(void){
 static int on=-1;DWORD error=GetLastError();int crt=errno;
 if(on<0){const char*v=getenv("DRIVING_PAIR_BLEND359");on=v&&!strcmp(v,"1");}
 int enabled=on&&nf_blend285_enabled();
 errno=crt;SetLastError(error);return enabled;
}
#endif
