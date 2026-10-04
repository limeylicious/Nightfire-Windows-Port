/* Diagnostic only: four verified PAL boundaries, at most two records each.
 * No translated register, flag, guest-memory, call, or pacing changes. Host
 * logging has incidental overhead; do not use an enabled run as a timing test. */
#include <windows.h>
#include "../runtime/driving_pc508_guards.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern ptrdiff_t g_xbox_mem_offset;
extern void driving_gpu_movie_phase180(void);
static volatile LONG movie144_counts[4];

static int movie144_read(uint64_t va, void *out, size_t n)
{
    SIZE_T got=0; uintptr_t base=(uintptr_t)g_xbox_mem_offset, address;
    /* Known guest RAM only; never inspect MMIO or follow arbitrary host VAs. */
    if (!va || !n || n>0x100 || va+n<va ||
        !((va<0x04000000ull && va+n<=0x04000000ull) ||
          (va>=0x80000000ull && va+n<=0x84000000ull))) return 0;
    address=base+(uintptr_t)va;
    if(address<base || address+n<address) return 0;
    PC508_GUARD_GUEST((uint32_t)va,n,"movie_probe144.read");
    return ReadProcessMemory(GetCurrentProcess(),(const void*)address,out,n,&got) && got==n;
}
static int movie144_word(uint64_t va,uint32_t *out)
{ return movie144_read(va,out,sizeof *out); }
static void movie144_dump(FILE *f,const char *name,uint32_t va,unsigned bytes)
{
    unsigned char data[0x100];
    fprintf(f,"%s.va=%08X bytes=%u",name,va,bytes);
    if(!movie144_read(va,data,bytes)){fprintf(f," unreadable\n");return;}
    fprintf(f," hex=");for(unsigned i=0;i<bytes;i++)fprintf(f,"%02X",data[i]);
    fprintf(f,"\n");
}
void driving_movie_probe144(uint32_t site,uint32_t object,uint32_t value,uint32_t stack)
{
    DWORD saved_error=GetLastError();
    int slot=site==0x1307D0?0:site==0x1308F5?1:site==0x13092D?2:site==0x14D1C8?3:-1;
    const char *directory;char path[1200];FILE *f=NULL;LONG sample;
    uint32_t pointer=0,av=0;int written;
    if(slot==3)driving_gpu_movie_phase180();
    if(slot<0 || InterlockedCompareExchange(&movie144_counts[slot],0,0)>=2)goto done;
    directory=getenv("DRIVING_CAPTURE_DIR");
    /* Never fall back to cwd or another logging directory. */
    if(!directory || !directory[0] || strlen(directory)>1000 ||
       !((strlen(directory)>2 && directory[1]==':' &&
          (directory[2]=='\\'||directory[2]=='/')) ||
         (directory[0]=='\\' && directory[1]=='\\')))goto done;
    {DWORD attrs=GetFileAttributesA(directory);
     if(attrs==INVALID_FILE_ATTRIBUTES || !(attrs&FILE_ATTRIBUTE_DIRECTORY))goto done;}
    sample=InterlockedIncrement(&movie144_counts[slot]);if(sample>2)goto done;
    written=snprintf(path,sizeof path,"%s/movie144-%lu-%08X-%ld.txt",directory,
                     (unsigned long)GetCurrentProcessId(),site,sample);
    if(written<0 || (size_t)written>=sizeof path)goto done;
    f=fopen(path,"wb");if(!f)goto done;
    fprintf(f,"site=%08X thread=%lu object=%08X value=%08X stack=%08X\n",
            site,(unsigned long)GetCurrentThreadId(),object,value,stack);
    if(slot==3){
        /* PAL14D1C8: local EBP is the child, second original argument is
         * [ESP+1C] after the original calls have cleaned their arguments.
         * Follow only the proven metadata chain, never pixel data. */
        uint32_t render=0,owner=0,record=0,header=0,shape=0;
        movie144_dump(f,"converted.child",object,0x3C);
        if(object && movie144_word((uint64_t)object+4,&shape) && shape)
            movie144_dump(f,"converted.shape",shape,0x14);
        else fprintf(f,"converted.shape.pointer=unavailable\n");
        if(stack && movie144_word((uint64_t)stack+0x1C,&render) && render &&
           movie144_word((uint64_t)render+0x48,&owner) && owner &&
           movie144_word((uint64_t)owner+0x40,&record) && record){
            uint32_t fields[0x34/4];
            fprintf(f,"render=%08X owner=%08X record=%08X\n",render,owner,record);
            movie144_dump(f,"texture.record",record,0x34);
            if(movie144_read(record,fields,sizeof fields)){
                fprintf(f,"texture.width=%u height=%u flags=%08X format=%08X "
                        "shape=%08X source=%08X owned=%08X header=%08X\n",
                        fields[2],fields[3],fields[4],fields[5],fields[7],
                        fields[8],fields[9],fields[10]);
                header=fields[10];
                if(header)movie144_dump(f,"texture.native.header",header,0x14);
                else fprintf(f,"texture.native.header.pointer=null\n");
            }
        }else fprintf(f,"texture.chain=unavailable render=%08X owner=%08X record=%08X\n",render,owner,record);
    }else if(slot==0){
        /* 1307D0 before its prologue: this=ECX, filename=[ESP+4]. */
        if(!movie144_word((uint64_t)stack+4,&pointer))fprintf(f,"filename.pointer=unreadable\n");
        else{
            unsigned i;int terminated=0,readable=1;
            fprintf(f,"filename.va=%08X escaped=",pointer);
            for(i=0;i<512;i++){
                unsigned char c=0;
                if(!movie144_read((uint64_t)pointer+i,&c,1)){readable=0;break;}
                if(!c){terminated=1;break;}
                if(c>=32 && c<127 && c!='\\')fputc(c,f);else fprintf(f,"\\x%02X",c);
            }
            fprintf(f,"\nfilename.readable=%d terminated=%d bytes=%u\n",readable,terminated,i);
        }
        movie144_dump(f,"open.arguments",stack,16);
    }else{
        /* Object+14 is the original 68-byte AV object. These are raw descriptor
         * bytes, not a claim that the MAD frame is YUY2 or audio is PCM. */
        movie144_dump(f,"movie.object",object,0x20);
        if(movie144_word((uint64_t)object+0x14,&av)){
            movie144_dump(f,"av.object",av,0x68);
            /* AV+0 is checked by14C7F0 and passed to14CEB0;14CD70
             * initializes fields through+10. Snapshot only that known prefix. */
            if(movie144_word(av,&pointer))movie144_dump(f,"av.audio.prefix",pointer,0x14);
        }
        if(slot==1){
            movie144_dump(f,"first.frame.prefix",value,8);
            /* PAL13097D dereferences frame+4, then shape+4/+6 signed16. */
            if(movie144_word((uint64_t)value+4,&pointer))movie144_dump(f,"frame.shape.prefix",pointer,8);
        }else{
            /* 14D510 allocates a1C-byte drawing object; six original call
             * arguments remain on ESP until the next original ADD ESP,18. */
            movie144_dump(f,"drawing.object",value,0x1C);
            movie144_dump(f,"drawing.arguments",stack,0x18);
        }
    }
done:
    if(f)fclose(f);
    SetLastError(saved_error);
}

/* Isolated diagnostic validation; reader must make bounded, fault-safe copies. */
#ifndef MOVIE_FRAME178_CORE_H
#define MOVIE_FRAME178_CORE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef int (*Movie178Read)(void *, uint32_t, void *, size_t);
typedef struct Movie178State {
    uint32_t stack[20], child[15], parent[7], frame[2], input[4], output[4];
    uint32_t movie_pointer, movie[6], av[26];
    uint32_t pixels, frame_count;
} Movie178State;
static int movie178_range(uint64_t p, size_t n)
{
    return n && n<=614400 && p &&
        ((p<0x04000000ull && p+n<=0x04000000ull) ||
         (p>=0x80000000ull && p+n<=0x84000000ull));
}
static int movie178_collect(Movie178Read read, void *ctx, uint32_t child,
                            uint32_t status, uint32_t stack, Movie178State *s)
{
    uint32_t parent, frame, shape, dstshape, y;
    memset(s,0,sizeof *s);
    if(status || !movie178_range(stack,sizeof s->stack) ||
       !read(ctx,stack,s->stack,sizeof s->stack)) return 0;
    /* Nine cdecl converter arguments remain before the original ADD ESP,24. */
    if(s->stack[3]!=640 || s->stack[4]!=640 || s->stack[5]!=480 ||
       s->stack[7]!=1280 || s->stack[8]!=s->stack[5] ||
       s->stack[14]!=0x14D452 || s->stack[18]!=0x130A38 ||
       s->stack[15]!=s->stack[0]) return 0;
    /* PAL14D14B/154 reuses child argument for Y; parent argument remains frame. */
    parent=s->stack[17]; frame=s->stack[19];
    if(!read(ctx,child,s->child,sizeof s->child) ||
       !read(ctx,parent,s->parent,sizeof s->parent) ||
       !read(ctx,frame,s->frame,sizeof s->frame)) return 0;
    if(s->child[0]!=0x1A7710 || s->parent[0]!=0x1A7714 ||
       s->child[13]!=2 || s->child[14]!=3 || s->parent[2]>1 ||
       s->parent[4+s->parent[2]]!=child || s->parent[6]!=s->stack[16]) return 0;
    shape=s->frame[1]; dstshape=s->child[1];
    if(!read(ctx,shape,s->input,sizeof s->input) ||
       !read(ctx,dstshape,s->output,sizeof s->output)) return 0;
    /* Only the captured inline 640x480 planar source and inline RGB565 output. */
    if((s->input[0]&255)!=0x6C || (s->output[0]&255)!=0x78 ||
       s->input[1]!=0x01E00280 || s->output[1]!=0x01E00280 ||
       (s->input[3]&0x1000) || (s->output[3]&0x1000) ||
       !movie178_range((uint64_t)shape+16,460800) ||
       !movie178_range((uint64_t)dstshape+16,614400)) return 0;
    y=shape+16; s->pixels=dstshape+16;
    if(s->stack[0]!=y || s->stack[1]!=y+307200 || s->stack[2]!=y+384000 ||
       s->stack[6]!=s->pixels ||
       ((uint64_t)y<s->pixels+614400ull && (uint64_t)s->pixels<y+460800ull)) return 0;
    if(!read(ctx,0x244798,&s->movie_pointer,4) ||
       !read(ctx,s->movie_pointer,s->movie,sizeof s->movie) ||
       !read(ctx,s->movie[5],s->av,sizeof s->av)) return 0;
    /* Original 130AEA publishes AV+44 into movie+C; AV+64 owns the codec.
     * The first frame instead uses the INC at130903, while AV+44 is still zero. */
    s->frame_count=s->movie[3];
    if(!s->frame_count || (s->frame_count!=s->av[17] && !(s->frame_count==1 && s->av[17]==0)) ||
       s->av[25]!=s->parent[1]) return 0;
    return 1;
}
static int movie178_slot(uint32_t count)
{ return count==1?0:count==15?1:count==30?2:count==120?3:count==240?4:count==480?5:count==600?6:-1; }
#endif

/* CP178: after original successful MAD conversion, before texture registration.
 * Appended to the existing probe module. No new player or presentation path. */
static volatile LONG movie178_claims, movie178_busy;
static int movie178_read(void *unused,uint32_t va,void *out,size_t n)
{
    uintptr_t base=(uintptr_t)g_xbox_mem_offset,p; SIZE_T got=0;
    (void)unused;
    if(!movie178_range(va,n))return 0;
    p=base+va;if(p<base || p+n<p)return 0;
    PC508_GUARD_GUEST(va,n,"movie_frame178.read");
    return ReadProcessMemory(GetCurrentProcess(),(void*)p,out,n,&got) && got==n;
}
void driving_movie_frame178(uint32_t child,uint32_t result,uint32_t stack)
{
    DWORD saved=GetLastError(); Movie178State before,after;
    const char *dir; unsigned char *pixels=NULL; FILE *f=NULL;
    char path[1200]; int slot,owns=0; LONG prior,bit;
    /* CP185: bound the evidence needed to explain a rejected178 snapshot.
     * These are metadata only, never substitute decoded/display pixels. */
    {static volatile LONG observations185;
     const char *d=getenv("DRIVING_CAPTURE_DIR");LONG sample185;
     if(d&&strlen(d)<1000&&InterlockedCompareExchange(&observations185,0,0)<3&&
        (sample185=InterlockedIncrement(&observations185))<=3){
      snprintf(path,sizeof path,"%s/movie185-%lu-%ld.txt",d,
               (unsigned long)GetCurrentProcessId(),sample185);
      FILE *debug=fopen(path,"wb");
      if(debug){
       uint32_t words[20]={0},p=0,av=0,shape=0;
       fprintf(debug,"child=%08X status=%u stack=%08X\n",child,result,stack);
       movie144_dump(debug,"stack",stack,80);movie144_dump(debug,"child",child,60);
       if(movie144_read(stack,words,sizeof words)){
        movie144_dump(debug,"parent",words[17],28);
        movie144_dump(debug,"frame",words[19],8);
        if(movie144_word((uint64_t)words[19]+4,&shape))movie144_dump(debug,"input",shape,16);
       }
       if(movie144_word((uint64_t)child+4,&shape))movie144_dump(debug,"output",shape,16);
       if(movie144_word(0x244798,&p)){
        movie144_dump(debug,"movie",p,24);
        if(movie144_word((uint64_t)p+20,&av))movie144_dump(debug,"av",av,104);
       }
       fprintf(debug,"guard_accepts=%d\n",movie178_collect(movie178_read,NULL,child,result,stack,&before));
       fclose(debug);
      }
     }
    }
    if(result || (InterlockedCompareExchange(&movie178_claims,0,0)&127)==127)goto done;
    dir=getenv("DRIVING_CAPTURE_DIR");
    if(!dir || strlen(dir)>1000 ||
       !((strlen(dir)>2 && dir[1]==':' && (dir[2]=='/'||dir[2]=='\\')) ||
         (dir[0]=='\\'&&dir[1]=='\\')))goto done;
    {DWORD a=GetFileAttributesA(dir);
     if(a==INVALID_FILE_ATTRIBUTES || !(a&FILE_ATTRIBUTE_DIRECTORY))goto done;}
    if(InterlockedCompareExchange(&movie178_busy,1,0))goto done;
    owns=1;
    if(!movie178_collect(movie178_read,NULL,child,result,stack,&before))goto done;
    slot=movie178_slot(before.frame_count);if(slot<0)goto done;
    bit=1L<<slot;prior=InterlockedOr(&movie178_claims,bit);if(prior&bit)goto done;
    /* CP192: at most seven original frames, including later intro content. */
    pixels=(unsigned char*)malloc(614400);if(!pixels)goto done;
    if(!movie178_read(NULL,before.pixels,pixels,614400) ||
       !movie178_collect(movie178_read,NULL,child,result,stack,&after) ||
       memcmp(&before,&after,sizeof before))goto done;
    snprintf(path,sizeof path,"%s/movie178-%lu-frame%u.rgb565",dir,
             (unsigned long)GetCurrentProcessId(),before.frame_count);
    f=fopen(path,"wb");if(!f)goto done;
    {size_t n=fwrite(pixels,1,614400,f);int ok=fclose(f)==0;f=NULL;
     if(n!=614400 || !ok)goto done;}
    snprintf(path,sizeof path,"%s/movie178-%lu-frame%u.txt",dir,
             (unsigned long)GetCurrentProcessId(),before.frame_count);
    f=fopen(path,"wb");if(!f)goto done;
    fprintf(f,"kind=original-game-decoded-MAD-conversion-not-presentation\n"
        "site=0014D1B8 result=%u thread=%lu movie=%08X original_frame_count=%u\n"
        "child=%08X parent=%08X source_frame=%08X source_shape=%08X output_shape=%08X\n"
        "pixels=%08X format=linear-little-endian-RGB565 width=640 height=480 pitch=1280 bytes=614400\n"
        "ownership=verified-original-stack-parent-child-codec-shape-chain\n"
        "concurrent_allocator_lifetime_lock=none metadata_revalidated_after_copy=1\n",
        result,(unsigned long)GetCurrentThreadId(),before.movie_pointer,before.frame_count,
        child,before.stack[17],before.stack[19],before.frame[1],before.child[1],before.pixels);
    fprintf(f,"capture_tick_ms=%llu\n",(unsigned long long)GetTickCount64());
done:
    if(f)fclose(f);free(pixels);
    if(owns)InterlockedExchange(&movie178_busy,0);
    SetLastError(saved);
}
