#ifndef DRIVING_PRESENT458_H
#define DRIVING_PRESENT458_H
/* Observation of already-owned immediate packets only. No guest reads, graphics
   calls, changed admission verdicts, presentation, or completion signals. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fenv.h>
#include <xmmintrin.h>
static int present458_mode=-1,present458_last_plan;
static unsigned present458_pass,present458_fail,present458_ends,present458_saved;
static unsigned present458_hashes[4];
static int present458_enabled(void){
 if(present458_mode<0){int e=errno;DWORD w=GetLastError();fenv_t fp;unsigned csr=_mm_getcsr();fegetenv(&fp);
  const char *v=getenv("DRIVING_PRESENT458");present458_mode=v&&!strcmp(v,"1");
  fesetenv(&fp);_mm_setcsr(csr);errno=e;SetLastError(w);
 }return present458_mode;
}
static int driving_plan201(const DrivingDraw195 *d,unsigned *family){
 int result=original_plan201_458(d,family);
 if(present458_enabled()){present458_last_plan=result;if(result)++present458_pass;else ++present458_fail;}
 return result;
}
static void present458_end(const DrivingDraw195 *d,int result,unsigned resolved){
 if(!present458_enabled()||d->primitive!=5||d->state[0x1b04/4]!=0x11229)return;
 int e=errno;DWORD w=GetLastError();fenv_t fp;unsigned csr=_mm_getcsr();fegetenv(&fp);
 ++present458_ends;
 if(!(present458_ends%120))fprintf(stderr,"[PRESENT458] end=%u collector=%d plan_pass=%u plan_fail=%u last_plan=%d resolved_before=%u packet_count=%u invalid=%u\n",present458_ends,result,present458_pass,present458_fail,present458_last_plan,resolved,d->count,d->invalid);
 if(result==1&&!present458_last_plan&&resolved>=100&&present458_saved<4){
  unsigned hash=2166136261u;const unsigned char *bytes=(const unsigned char *)d;
  for(size_t i=0;i<sizeof(*d);i++)hash=(hash^bytes[i])*16777619u;
  unsigned i=0;for(;i<present458_saved;i++)if(hash==present458_hashes[i])break;
  if(i==present458_saved){
   const char *dir=getenv("DRIVING_CAPTURE_DIR");char path[2048];int ok=0;unsigned slot=present458_saved++;
   present458_hashes[slot]=hash;
   if(dir){int n=snprintf(path,sizeof path,"%s/present458-declined-%u.bin",dir,slot);
    if(n>0&&(size_t)n<sizeof path){FILE *f=fopen(path,"wb");if(f){ok=fwrite(d,1,sizeof *d,f)==sizeof *d;if(fclose(f))ok=0;}}}
   fprintf(stderr,"[PRESENT458] owned-packet=%u saved=%d bytes=%zu hash=%08X end=%u resolved_before=%u format=%08X filter=%08X primitive=%u count=%u invalid=%u program_mode=%u start=%u NOT-frame-NOT-RAM-snapshot=1\n",slot,ok,sizeof *d,hash,present458_ends,resolved,d->state[0x208/4],d->state[0x1b14/4],d->primitive,d->count,d->invalid,d->program.mode,d->program.start);
  }
 }
 fesetenv(&fp);_mm_setcsr(csr);errno=e;SetLastError(w);
}
#endif
