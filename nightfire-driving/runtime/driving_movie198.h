/* Optional read-only movie command observer. Runs on the existing synchronous
 * consumer after Kelvin binding validation. No GPU draw is replaced or skipped.
 * Snapshot files own command state; DMA results are addresses, not host pointers.
 * They do not establish source lifetime, page access or GPU alias completion. */
#ifndef DRIVING_MOVIE198_H
#define DRIVING_MOVIE198_H
#include "immediate195.h"
#include "driving_dma183.h"
static DrivingImmediate195 movie198;
static int movie_enabled198=-1;
static unsigned movie_counts198[2],movie_saved198,movie_errors198;
static int movie_file198(const char *folder,unsigned serial,const char *suffix,const void *data,size_t bytes){
 char path[2048];int n=snprintf(path,sizeof path,"%s/movie198-%02u-%s",folder,serial,suffix);
 if(n<0||(size_t)n>=sizeof path)return 0;
 FILE *f=fopen(path,"wb");if(!f)return 0;
 int ok=fwrite(data,1,bytes,f)==bytes;if(fclose(f))ok=0;return ok;
}
static void movie_observe198(unsigned method,uint32_t value){
 if(movie_enabled198<0){const char *v=getenv("DRIVING_MOVIE_CAPTURE198");movie_enabled198=v&&v[0]=='1';}
 if(!movie_enabled198||movie_saved198>=8)return;
 int result=driving_immediate195(&movie198,method,value);
 if(result<0){if(movie_errors198++<4)fprintf(stderr,"[MOVIE198] collector rejected method=%04X value=%08X\n",method,value);return;}
 if(!result)return;
 const DrivingDraw195 *d=&movie198.draw;unsigned family=d->state[0x1b04/4]==0x11229;
 unsigned ordinal=++movie_counts198[family];
 /* First pair plus later pairs. Bound I/O without relying on unrelated frame
  * counters or guessing whether the game's movie source is already nonblack. */
 if(ordinal!=1&&ordinal!=120&&ordinal!=240&&ordinal!=480)return;
 const char *folder=getenv("DRIVING_CAPTURE_DIR");if(!folder)return;
 unsigned serial=++movie_saved198;DrivingSurface145 surface={0};
 DrivingSpan183 color={0},depth={0},texture={0};
 const uint32_t *s=d->state;uint32_t ramht=0,allocated=xbox_ContiguousAllocatedBytes();
 unsigned storage_known=d->known[0x200/4]&&d->known[0x204/4]&&d->known[0x208/4]&&d->known[0x20c/4];
 int shape=storage_known&&driving_surface145(&surface,s[0x208/4],s[0x200/4],s[0x204/4],s[0x20c/4]&0xffff);
 int dma=read143(NULL,0xfd002210,&ramht);
 int color_ok=shape&&dma&&surface.span<=UINT32_MAX&&driving_span183(NULL,read143,ramht,s,d->known,DRIVING_COLOR183,0,(uint32_t)surface.span,allocated,&color);
 size_t depth_span=shape?(size_t)(surface.height-1)*(s[0x20c/4]>>16)+surface.storage_width*4u:0;
 int depth_ok=shape&&dma&&(s[0x20c/4]>>16)>=surface.storage_width*4u&&depth_span<=UINT32_MAX&&driving_span183(NULL,read143,ramht,s,d->known,DRIVING_DEPTH183,0,(uint32_t)depth_span,allocated,&depth);
 unsigned tw=s[0x1b1c/4]>>16,th=s[0x1b1c/4]&0xffff,tp=s[0x1b10/4]>>16,bpp=family?4:2;
 size_t ts=th?(size_t)(th-1)*tp+(size_t)tw*bpp:0;
 int tex_ok=dma&&d->known[0x1b10/4]&&d->known[0x1b1c/4]&&tw&&th&&tp>=tw*bpp&&ts<=UINT32_MAX&&driving_span183(NULL,read143,ramht,s,d->known,DRIVING_TEXTURE183,0,(uint32_t)ts,allocated,&texture);
 int ok=movie_file198(folder,serial,"state.bin",s,sizeof d->state);
 ok&=movie_file198(folder,serial,"known.bin",d->known,sizeof d->known);
 ok&=movie_file198(folder,serial,"program.bin",&d->program,sizeof d->program);
 ok&=movie_file198(folder,serial,"vertices.bin",d->vertices,d->count*sizeof d->vertices[0]);
 ok&=movie_file198(folder,serial,"masks.bin",d->masks,d->count*sizeof d->masks[0]);
 char text[1536];int n=snprintf(text,sizeof text,
 "kind=original-command-state-not-rendered-frame\nfamily=%s ordinal=%u primitive=%u count=%u\n"
 "state_bytes=%zu program_bytes=%zu vertex_bytes=%zu\n"
 "surface_known=%u shape=%d width=%u height=%u lanes=%u pitch=%u\n"
 "color_ok=%d color=%08X color_available=%u depth_ok=%d depth=%08X depth_available=%u\n"
 "texture_ok=%d texture=%08X texture_available=%u width=%u height=%u pitch=%u\n"
 "address=%08X filter=%08X files_ok=%d collector_errors=%u\n"
 "limits=no-source-read-no-page-validation-no-alias-completion-no-rendering\n",
 family?"resolve":"movie",ordinal,d->primitive,d->count,sizeof d->state,sizeof d->program,sizeof d->vertices[0],
 storage_known,shape,surface.width,surface.height,surface.lanes,surface.pitch,
 color_ok,color.address,color.available,depth_ok,depth.address,depth.available,tex_ok,texture.address,texture.available,tw,th,tp,
 s[0x1b08/4],s[0x1b14/4],ok,movie_errors198);
 if(n>0&&(size_t)n<sizeof text)ok&=movie_file198(folder,serial,"metadata.txt",text,n);else ok=0;
 fprintf(stderr,"[MOVIE198] saved=%u family=%s ordinal=%u vertices=%u surface=%d color=%d depth=%d texture=%d files_ok=%d\n",serial,family?"resolve":"movie",ordinal,d->count,shape,color_ok,depth_ok,tex_ok,ok);
}
#endif
