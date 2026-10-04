#ifndef DRIVING_FOG217_H
#define DRIVING_FOG217_H
#include <math.h>
/* Captured NV097 V_LINEAR, uploaded bias/slope convention. Keep the
 * unsaturated vertex factor: final combiner clamps after interpolation. */
static int driving_fog217(unsigned mode,float bias,float slope,float distance,float *out)
{
 if(mode!=0x2601 || !out || !isfinite(bias)||!isfinite(slope)||!isfinite(distance))return 0;
 float factor=(distance*slope+bias)-1.0f;if(!isfinite(factor))return 0;
 *out=factor;return 1;
}
#endif
