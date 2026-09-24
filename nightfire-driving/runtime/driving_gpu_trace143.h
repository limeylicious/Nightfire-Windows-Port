/* Development-only ordered method journal. No guest memory is changed.
 * Records command values, not texture/vertex memory; not a standalone replay. */
#ifndef DRIVING_GPU_TRACE143_H
#define DRIVING_GPU_TRACE143_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static FILE *driving_trace143_file;
static unsigned driving_trace143_records;
static int driving_trace143_initialized;
static char driving_trace143_buffer[65536];
static int driving_trace143_movie_phase180;
/* Called only under the serialized consumer at a complete command boundary. */
static void driving_trace143_start_movie180(void)
{
    if(driving_trace143_file)fclose(driving_trace143_file);
    driving_trace143_file=NULL;driving_trace143_records=0;
    driving_trace143_initialized=0;driving_trace143_movie_phase180=1;
}
static void driving_trace143_start_postmovie207(void)
{
    if(driving_trace143_file)fclose(driving_trace143_file);
    driving_trace143_file=NULL;driving_trace143_records=0;
    driving_trace143_initialized=0;driving_trace143_movie_phase180=2;
}
static void driving_trace143(uint32_t tag,uint32_t a,uint32_t b,uint32_t c)
{
    if(!driving_trace143_initialized){
        driving_trace143_initialized=1;
        const char *root=getenv("DRIVING_CAPTURE_DIR");
        if(root && *root){
            char path[1024];
            const char *name=driving_trace143_movie_phase180==2?"gpu207-postmovie-methods.bin":
                driving_trace143_movie_phase180?"gpu180-movie-methods.bin":"gpu143-methods.bin";
            int n=snprintf(path,sizeof(path),"%s/%s",root,name);
            if(n>0 && (size_t)n<sizeof(path))driving_trace143_file=fopen(path,"wb");
            if(driving_trace143_file){
                setvbuf(driving_trace143_file,driving_trace143_buffer,_IOFBF,sizeof(driving_trace143_buffer));
                if(fwrite("NFGPU143",1,8,driving_trace143_file)!=8){
                    fclose(driving_trace143_file);driving_trace143_file=NULL;
                }
            }
        }
    }
    if(!driving_trace143_file)return;
    uint32_t record[4]={tag,a,b,c};
    if(driving_trace143_records==262144 || fwrite(record,sizeof(record),1,driving_trace143_file)!=1){
        fclose(driving_trace143_file);driving_trace143_file=NULL;
        fprintf(stderr,"[GPU143] method journal closed at %u records (limit or I/O failure)\n",driving_trace143_records);
        return;
    }
    driving_trace143_records++;
    if(tag==3 || tag==4)fflush(driving_trace143_file);
}
#endif
