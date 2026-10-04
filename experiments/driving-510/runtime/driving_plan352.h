/*352 common front end for proven-state families; SOURCE ONLY.
 * Generic shader execution does not establish generic raster-state support.
 * Keep exact shader/material/viewport/constant contracts at live admission.
 * Unknown programs may use the backend API in independent future fixtures;
 * they are not silently admitted to the scene by this new route. */
#ifndef DRIVING_PLAN352_H
#define DRIVING_PLAN352_H
#include "driving_plan350.h"
static int renderer_enabled352(void)
{static int on=-1;return geometry_setting350("DRIVING_RENDERER352",&on);}
static int renderer_compute352(void)
{static int on=-1;return geometry_setting350("DRIVING_COMPUTE_COMPARE352",&on);}
static int renderer_plan352(const uint32_t *state,const unsigned char *known,
 const NFVertexProgram *program,unsigned primitive,unsigned *profile)
{
 if(!profile||primitive<5||primitive>9)return 0;
 if(primitive==5||primitive==6)return driving_plan221(state,known,program,primitive,profile);
 return geometry_plan350(state,known,program,primitive,profile);
}
#endif
