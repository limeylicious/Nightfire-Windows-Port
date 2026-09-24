#ifndef NIGHTFIRE_AUDIO_SPATIAL_H
#define NIGHTFIRE_AUDIO_SPATIAL_H
#include <math.h>
typedef struct {
    float position[3], minimum, maximum, curve[64];
    unsigned count;
} NightfireSpatial;
/* PAL114E6B: count equal intervals between min/max, with implicit gain1
 * before curve[0]. The supplied five-point curve is NOT four intervals. */
static float nightfire_distance_gain(float distance,const NightfireSpatial *s){
    if(distance<=s->minimum)return 1.0f;
    if(s->maximum<=s->minimum)return 0.0f;
    if(distance>s->maximum)distance=s->maximum;
    if(s->count){
        double step=((double)s->maximum-s->minimum)/s->count;
        double delta=(double)distance-s->minimum;
        unsigned index=(unsigned)(delta/step);
        if(index>=s->count)index=s->count-1;
        float left=index?s->curve[index-1]:1.0f,right=s->curve[index];
        return fmaxf(0.0f,(float)(left+(right-left)*(delta-index*step)/step));
    }
    return s->minimum/distance; /* default unit rolloff; custom path observed */
}
#endif
