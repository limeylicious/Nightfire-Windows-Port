/* Include after standard declarations, before the selected unit's code.
 * This intentionally instruments only the included translation unit. */
#include "driving_access321.h"
#ifndef DRIVING_ACCESS321_KIND
#error Select the observed caller category
#endif
#ifdef DRIVING_ACCESS321_RENDER
/* Tiny renderer copies include per-pixel clear operations. Observe clears at
 * their exact row boundaries instead. Other <=8-byte renderer memcpy calls
 * are explicitly unobserved in this mode; leave the intrinsic inline. */
static __forceinline void *driving_access321_render_copy(void*d,const void*s,size_t n){
 if(n>8){driving_access321_note(s,n,DA321_RENDER_READ);driving_access321_note(d,n,DA321_RENDER_WRITE);}
 return memcpy(d,s,n);
}
#define memcpy(d,s,n) driving_access321_render_copy((d),(s),(n))
#else
#define memcpy(d,s,n) driving_access321_memcpy((d),(s),(n),DRIVING_ACCESS321_KIND)
#endif
#define memmove(d,s,n) driving_access321_memmove((d),(s),(n),DRIVING_ACCESS321_KIND)
#define memset(d,v,n) driving_access321_memset((d),(v),(n),DRIVING_ACCESS321_KIND+1)
#define ReadFile(h,p,n,d,o) driving_access321_readfile((h),(p),(n),(d),(o))
#define WriteFile(h,p,n,d,o) driving_access321_writefile((h),(p),(n),(d),(o))
#define ReadProcessMemory(h,p,o,n,d) driving_access321_rpm((h),(p),(o),(n),(d))
