#ifndef DRIVING_SCENE240_H
#define DRIVING_SCENE240_H
#include <stdlib.h>
#include <string.h>
/* Consumer-owned readiness; no flag or movie counter is fabricated. A queued
 * scene packet is flushed by submit201 before any resolved image is published. */
static unsigned scene_accepted240;
static int scene_capture_enabled240=-1;
static void scene_accept240(unsigned family,unsigned format){
 if(family>=2&&family<=4&&format==0x1128u)scene_accepted240=1;
}
static int scene_resolve240(unsigned family,unsigned movie_draws,unsigned filter){
 return family&&(movie_draws||(scene_accepted240&&filter==0x04073f01u));
}
static int scene_capture240(unsigned movie_draws,int gpu_mode){
 if(movie_draws)return 1;
 if(scene_capture_enabled240<0){const char *v=getenv("DRIVING_WORLD_GPU214");scene_capture_enabled240=v&&!strcmp(v,"1");}
 return gpu_mode>0&&scene_capture_enabled240;
}
#endif
