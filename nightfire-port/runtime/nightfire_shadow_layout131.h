/* Private opt-in observation only. No GPU calls, completion, or guest writes.
 * Twenty presentation frames per changed positive trigger token. Output files
 * are created inside the explicitly supplied existing diagnostic directory.
 */
#ifndef NIGHTFIRE_SHADOW_LAYOUT131_H
#define NIGHTFIRE_SHADOW_LAYOUT131_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    unsigned frame,clear,target,format,control,width,height,clip_x,clip_y,pitch;
    unsigned indices,triangles,hardware,program_start,program_mode;
    unsigned texture,texture_format,texture_filter,texture_control,texture_address;
    const void *host;
    uint64_t program_hash;
} NFShadow131State;
void nf_shadow131_tick(unsigned frame);
int nf_shadow131_wants(unsigned frame);
void nf_shadow131_event(const NFShadow131State *state);
void nf_shadow131_publish(const void *target,const void *rows,unsigned pitch,unsigned width,unsigned height);
void nf_shadow131_texture(const void *texture,unsigned format,unsigned filter,unsigned address,size_t bytes);
void nf_material132_texture(const void *texture,unsigned format,size_t bytes);
typedef struct {
    unsigned frame,target,surface,width,height,hardware,triangles;
    unsigned texture,format,filter,address,control,depth_enable,depth_func,depth_write;
    unsigned blend_enable,blend_src,blend_dst,offset_enable,offset_scale,offset_bias;
} NFMaterial132;
void nf_material132_observe(const NFMaterial132 *state);
#endif

#if defined(NF_SHADOW131_IMPLEMENTATION) && !defined(NF_SHADOW131_IMPLEMENTED)
#define NF_SHADOW131_IMPLEMENTED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct {
    int enabled;
    const char *trigger,*directory;
    unsigned initialized,seen_frame,frame,first,token,active,rows,images,cycle;
    FILE *log;
    struct {const void *host;unsigned cycle,target;NFShadow131State draw;} target[4];
} shadow131;
static void shadow131_close(void){
    if(shadow131.log){fprintf(shadow131.log,"{\"event\":\"end\",\"frame\":%u,\"rows\":%u,\"images\":%u}\n",shadow131.frame,shadow131.rows,shadow131.images);fclose(shadow131.log);shadow131.log=NULL;}
    shadow131.active=0;
}
void nf_shadow131_tick(unsigned frame){
    if(!shadow131.initialized){
        shadow131.initialized=1;shadow131.seen_frame=~0u;
        const char *v=getenv("NIGHTFIRE_SHADOW_LAYOUT131");shadow131.enabled=v && !strcmp(v,"1");
        shadow131.trigger=getenv("NIGHTFIRE_SHADOW_LAYOUT131_TRIGGER");shadow131.directory=getenv("NIGHTFIRE_SHADOW_LAYOUT131_DIR");
    }
    if(!shadow131.enabled || !shadow131.trigger || !shadow131.directory || shadow131.seen_frame==frame)return;
    shadow131.seen_frame=shadow131.frame=frame;
    if(shadow131.active && frame-shadow131.first>=20)shadow131_close();
    if(shadow131.log)fflush(shadow131.log);
    unsigned token=0;FILE *f=fopen(shadow131.trigger,"r");if(f){fscanf(f,"%u",&token);fclose(f);}
    if(!token || token==shadow131.token)return;
    shadow131_close();shadow131.token=token;shadow131.first=frame;shadow131.rows=shadow131.images=shadow131.cycle=0;
    memset(shadow131.target,0,sizeof shadow131.target);
    char path[1024];int n=snprintf(path,sizeof path,"%s/shadow131-token%u.jsonl",shadow131.directory,token);
    if(n<0 || n>=sizeof path)return;
    shadow131.log=fopen(path,"w");shadow131.active=shadow131.log!=NULL;
    if(shadow131.log)fprintf(shadow131.log,"{\"event\":\"begin\",\"token\":%u,\"frame\":%u,\"frames\":20}\n",token,frame);
}
static int shadow131_ready(void){
    if(!shadow131.active || !shadow131.log)return 0;
    if(shadow131.rows>=4096){shadow131_close();return 0;}
    shadow131.rows++;return 1;
}
void nf_material132_observe(const NFMaterial132 *s){
    nf_shadow131_tick(s->frame);
    /* Only two frames of metadata; no source reads or GPU completions. */
    if(!shadow131.active || s->frame-shadow131.first>=2 || !shadow131_ready())return;
    fprintf(shadow131.log,"{\"event\":\"material132\",\"frame\":%u,\"target\":\"%08X\",\"surface\":\"%08X\",\"width\":%u,\"height\":%u,\"hardware\":%u,\"triangles\":%u,\"texture\":\"%08X\",\"format\":\"%08X\",\"filter\":\"%08X\",\"address\":\"%08X\",\"control\":\"%08X\",\"depth_enable\":%u,\"depth_func\":%u,\"depth_write\":%u,\"blend_enable\":%u,\"blend_src\":%u,\"blend_dst\":%u,\"offset_enable\":%u,\"offset_scale\":\"%08X\",\"offset_bias\":\"%08X\"}\n",
        s->frame,s->target,s->surface,s->width,s->height,s->hardware,s->triangles,
        s->texture,s->format,s->filter,s->address,s->control,s->depth_enable,s->depth_func,s->depth_write,
        s->blend_enable,s->blend_src,s->blend_dst,s->offset_enable,s->offset_scale,s->offset_bias);
}
int nf_shadow131_wants(unsigned frame){nf_shadow131_tick(frame);return shadow131.active && shadow131.rows<4096;}
static unsigned shadow131_find(const void *host,int create){
    for(unsigned i=0;i<4;i++)if(shadow131.target[i].host==host)return i;
    if(create)for(unsigned i=0;i<4;i++)if(!shadow131.target[i].host){shadow131.target[i].host=host;return i;}
    return 4;
}
void nf_shadow131_event(const NFShadow131State *s){
    nf_shadow131_tick(s->frame);
    if((s->width!=128 && s->width!=256) || s->height!=s->width || s->pitch!=s->width*4 || !s->host || !shadow131_ready())return;
    unsigned i=shadow131_find(s->host,1);if(i==4)return;
    if(s->clear || !shadow131.target[i].cycle)shadow131.target[i].cycle=++shadow131.cycle;
    shadow131.target[i].target=s->target;
    if(!s->clear)shadow131.target[i].draw=*s;
    fprintf(shadow131.log,"{\"event\":\"%s\",\"frame\":%u,\"cycle\":%u,\"target\":\"%08X\",\"host\":\"%p\",\"format\":\"%08X\",\"control\":\"%08X\",\"width\":%u,\"height\":%u,\"clip_x\":%u,\"clip_y\":%u,\"pitch\":%u,\"indices\":%u,\"triangles\":%u,\"hardware\":%u,\"program_start\":%u,\"program_mode\":%u,\"program_hash\":\"%016llX\",\"texture\":\"%08X\",\"texture_format\":\"%08X\",\"texture_filter\":\"%08X\",\"texture_control\":\"%08X\",\"texture_address\":\"%08X\"}\n",
        s->clear?"clear":"draw",s->frame,shadow131.target[i].cycle,s->target,s->host,s->format,s->control,s->width,s->height,s->clip_x,s->clip_y,s->pitch,s->indices,s->triangles,s->hardware,s->program_start,s->program_mode,(unsigned long long)s->program_hash,s->texture,s->texture_format,s->texture_filter,s->texture_control,s->texture_address);
}
static uint64_t shadow131_hash(const uint8_t *rows,unsigned pitch,unsigned size,unsigned *rgb,unsigned *alpha){
    uint64_t hash=14695981039346656037ull;*rgb=*alpha=0;
    for(unsigned y=0;y<size;y++)for(unsigned x=0;x<size;x++){
        const uint8_t *p=rows+(size_t)y*pitch+x*4;
        *rgb+=(p[0]|p[1]|p[2])!=0;*alpha+=p[3]!=0;
        for(unsigned k=0;k<4;k++){hash^=p[k];hash*=1099511628211ull;}
    }
    return hash;
}
static void shadow131_image(const char *kind,unsigned cycle,const uint8_t *rows,unsigned pitch,unsigned size){
    if(shadow131.images>=6)return;
    char path[1024];int n=snprintf(path,sizeof path,"%s/shadow131-token%u-frame%u-cycle%u-image%u-%s.bgra",shadow131.directory,shadow131.token,shadow131.frame,cycle,shadow131.images,kind);
    if(n<0 || n>=sizeof path)return;
    FILE *f=fopen(path,"wb");if(!f)return;
    for(unsigned y=0;y<size;y++)if(fwrite(rows+(size_t)y*pitch,1,size*4,f)!=size*4)break;
    fclose(f);shadow131.images++;
}
void nf_shadow131_publish(const void *target,const void *rows,unsigned pitch,unsigned width,unsigned height){
    if((width!=128 && width!=256) || height!=width || pitch<width*4 || !rows || !shadow131.active)return;
    unsigned i=shadow131_find(target,0);if(i==4 || !shadow131_ready())return;
    unsigned rgb,alpha;uint64_t hash=shadow131_hash(rows,pitch,width,&rgb,&alpha);
    fprintf(shadow131.log,"{\"event\":\"publication\",\"frame\":%u,\"cycle\":%u,\"target\":\"%08X\",\"draw_format\":\"%08X\",\"width\":%u,\"height\":%u,\"linear_hash\":\"%016llX\",\"rgb_nonzero\":%u,\"alpha_nonzero\":%u}\n",shadow131.frame,shadow131.target[i].cycle,shadow131.target[i].target,shadow131.target[i].draw.format,width,height,(unsigned long long)hash,rgb,alpha);
    shadow131_image("gpu-linear",shadow131.target[i].cycle,rows,pitch,width);
}
void nf_shadow131_texture(const void *texture,unsigned format,unsigned filter,unsigned address,size_t bytes){
    unsigned size=format==0x08810629?256:format==0x07710629?128:0;
    if(!size || bytes!=(size_t)size*size*4 || !shadow131.active)return;
    unsigned i=shadow131_find(texture,0);if(i==4 || !shadow131_ready())return;
    unsigned rgb,alpha;uint64_t hash=shadow131_hash(texture,size*4,size,&rgb,&alpha);
    fprintf(shadow131.log,"{\"event\":\"texture\",\"frame\":%u,\"cycle\":%u,\"target\":\"%08X\",\"format\":\"%08X\",\"filter\":\"%08X\",\"address\":\"%08X\",\"width\":%u,\"height\":%u,\"guest_hash\":\"%016llX\",\"rgb_nonzero\":%u,\"alpha_nonzero\":%u}\n",shadow131.frame,shadow131.target[i].cycle,shadow131.target[i].target,format,filter,address,size,size,(unsigned long long)hash,rgb,alpha);
    shadow131_image("guest-swizzle-input",shadow131.target[i].cycle,texture,size*4,size);
}
#include "nightfire_material_textures132.h"
#endif
