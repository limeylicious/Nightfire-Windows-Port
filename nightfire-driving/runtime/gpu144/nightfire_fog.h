#ifndef NIGHTFIRE_FOG_H
#define NIGHTFIRE_FOG_H
#include <math.h>
/* NV097 exponential fog: the uploaded slope is -density/(16*ln(2)).
 * The Exchange uses mode800 and bias1.5. Preserve the vertex result until
 * interpolation; saturation belongs to the final pixel combiner. */
static float nf_fog_exp(float distance,float bias,float slope){
    if(!isfinite(distance))return 1.0f;
    float f=exp2f(distance*(16.0f*slope))+(bias-1.5f);
    return isnan(f)?1.0f:fminf(f,3.402823466e+38f);
}
#define NF_FOG_PARAMS_HLSL "cbuffer Params:register(b0){float2 surface;float alphaOn;float alphaFunc;float alphaRef;float mode;float rgbScale;float fogOn;float4 fogColor;float fogBias;float fogSlope;float2 packedSize;}"
#define NF_FOG_HLSL "float fogFactor(float d){if(fogOn==0 || !isfinite(d))return 1;float f=exp2(d*(16*fogSlope))+(fogBias-1.5);return isnan(f)?1:min(f,3.402823466e+38);}" 
#endif
