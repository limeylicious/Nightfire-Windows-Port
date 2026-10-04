/* Optional owned observation of the original completed immediate world resolve,
 * before its raster work. This is not a rendered frame or savegame. */
static int file222(const char *dir,const char *name,const void *data,size_t bytes)
{
 char path[2048];int n=snprintf(path,sizeof path,"%s/resolve222-%s",dir,name);if(n<0||(size_t)n>=sizeof path)return 0;
 FILE *f=fopen(path,"wb");if(!f)return 0;int okay=fwrite(data,1,bytes,f)==bytes;if(fclose(f))okay=0;return okay;
}
static void capture_resolve222(const DrivingDraw195 *d)
{
 static int enabled=-1,saved,late240;if(enabled<0){const char *v=getenv("DRIVING_RESOLVE_CAPTURE222");enabled=v&&!strcmp(v,"1");const char *late=getenv("DRIVING_LATE_CAPTURE240");late240=late&&!strcmp(late,"1");if(late240)enabled=1;}
 if(!enabled||saved||(late240&&draws201[1]<60)||d->state[0x208/4]!=0x128||d->state[0x1b04/4]!=0x11229||d->state[0x1b14/4]!=0x04073f01||d->primitive!=5)return;
 saved=1;const char *dir=getenv("DRIVING_CAPTURE_DIR");if(!dir)return;
 unsigned bytes=xbox_ContiguousAllocatedBytes();SIZE_T copied=0;uint32_t ramht=0;
 if(!bytes||bytes>0x08000000||!nf_hw_sync())return;
 void *source=mapped201(0x80000000u,bytes,0),*ram=source?malloc(bytes):NULL,*pram=malloc(0x100000);
 int okay=ram&&pram&&read143(NULL,0xfd002210,&ramht)&&
  ReadProcessMemory(GetCurrentProcess(),source,ram,bytes,&copied)&&copied==bytes&&
  ReadProcessMemory(GetCurrentProcess(),(void*)((uintptr_t)xbox_GetMemoryOffset()+0xfd700000u),pram,0x100000,&copied)&&copied==0x100000;
 if(okay){
  okay=file222(dir,"ram.bin",ram,bytes)&&file222(dir,"pramin.bin",pram,0x100000)&&file222(dir,"draw.bin",d,sizeof *d);
  okay=okay&&file222(dir,"state.bin",d->state,sizeof d->state)&&file222(dir,"known.bin",d->known,sizeof d->known)&&
   file222(dir,"program.bin",&d->program,sizeof d->program)&&file222(dir,"vertices.bin",d->vertices,sizeof d->vertices)&&file222(dir,"masks.bin",d->masks,sizeof d->masks);
  char meta[256];int n=snprintf(meta,sizeof meta,"bytes=%u ramht=%08X primitive=%u count=%u invalid=%u draw_bytes=%zu\nlimits=GPU-resource-copy-not-atomic-machine-snapshot;before-original-resolve;not-presented-frame\n",bytes,ramht,d->primitive,d->count,d->invalid,sizeof *d);
  if(n>0&&(size_t)n<sizeof meta)okay=okay&&file222(dir,"metadata.txt",meta,n);
 }
 fprintf(stderr,"[RESOLVE222] owned-capture count=%u bytes=%u okay=%d\n",d->count,bytes,okay);free(ram);free(pram);
}
