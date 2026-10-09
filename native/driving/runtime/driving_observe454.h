/*454 diagnostic only. Owned draw state and remembered target metadata; no guest
 * reads, DMA resolution, GPU calls, admission changes or extra publications.
 * Recorded incoming DMA handles/offsets are NOT proof of backing identity. */
#ifndef DRIVING_OBSERVE454_H
#define DRIVING_OBSERVE454_H
#include <stddef.h>
enum { OBS454_ROWS=64 };
typedef struct {
 uint32_t primitive,surface,pixel,mode,start,length,code_hash,mask;
 BeginReason280 rejection[DRIVING_PROFILES221];
} Key454;
static struct {
 int setting;unsigned initialized,used;uint64_t total,overflow,overhead;
 struct {Key454 key;uint64_t hits,published,zero_queue,ticks;} row[OBS454_ROWS];
} observe454;
typedef struct {DWORD error;int crt;fenv_t fp;unsigned mxcsr;} Saved454;
static Saved454 save454(void){Saved454 a; a.error=GetLastError();a.crt=errno;
 a.mxcsr=_mm_getcsr();fegetenv(&a.fp);return a;}
static void restore454(const Saved454 *a){fesetenv(&a->fp);_mm_setcsr(a->mxcsr);errno=a->crt;SetLastError(a->error);}
static uint64_t ticks454(void){LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}
typedef struct {uint64_t start,published;unsigned row,active;} Token454;
static Token454 observe_before454(const uint32_t*s,const unsigned char*k,
 const NFVertexProgram*p,unsigned primitive,unsigned queued,int held,int enabled){
 Token454 token={0};if(observe454.initialized&&!observe454.setting)return token;
 Saved454 saved=save454();
 if(!observe454.initialized){const char*v=getenv("DRIVING_OBSERVE454");
  observe454.setting=v&&!strcmp(v,"1");observe454.initialized=1;}
 if(!observe454.setting||!pc450.dirty||!s||!k||!p){restore454(&saved);return token;}
 uint64_t begin=ticks454();Key454 key={0};key.primitive=primitive;key.surface=s[0x208/4];
 key.pixel=s[0x1e70/4];key.mode=p->mode;key.start=p->start;key.code_hash=2166136261u;
 for(unsigned pc=p->start;pc<136;pc++){
  if(p->valid[pc]!=15)break;
  for(unsigned c=0;c<4;c++)for(unsigned b=0;b<4;b++)key.code_hash=(key.code_hash^((p->code[pc][c]>>(b*8))&255))*16777619u;
  key.length++;if(p->code[pc][3]&1)break;
 }
 for(unsigned q=0;q<DRIVING_PROFILES221;q++){
  const DrivingProfile221*c=&profiles221[q];unsigned pc;
  for(pc=0;pc<c->length;pc++)if(p->valid[pc]!=15||memcmp(p->code[pc],c->code[pc],16))break;
  if(pc==c->length){key.mask|=1u<<q;key.rejection[q]=begin280_reject(s,k,p,primitive,q);}
 }
 unsigned row;for(row=0;row<observe454.used;row++)if(!memcmp(&observe454.row[row].key,&key,sizeof key))break;
 observe454.total++;
 if(row==observe454.used&&row<OBS454_ROWS){
  observe454.row[row].key=key;observe454.used++;
  const char*dir=getenv("DRIVING_CAPTURE_DIR");char path[1536];FILE*f=NULL;
  int path_length=dir?snprintf(path,sizeof path,"%s/observe454-%02u.bin",dir,row):-1;
  if(path_length>0&&(size_t)path_length<sizeof path)f=fopen(path,"wbx");
  if(!f)fprintf(stderr,"[OBS454-IO] row=%u snapshot_open_failed=1 diagnostic_only=1\n",row);
  else {
   uint32_t state[2048];unsigned char known[2048];
   memcpy(state,s,sizeof state);memcpy(known,k,sizeof known);
   state[0x17fc/4]=primitive;known[0x17fc/4]=1; /*hook precedes normal state update*/
   uint32_t header[]={0x3435344fu,1,sizeof(NFVertexProgram),sizeof(DrivingSpan183),
    offsetof(NFVertexProgram,code),offsetof(NFVertexProgram,valid),offsetof(NFVertexProgram,constant_words),
    offsetof(NFVertexProgram,constant_valid),offsetof(NFVertexProgram,load),offsetof(NFVertexProgram,start),
    offsetof(NFVertexProgram,mode),queued,(unsigned)held,(unsigned)enabled};
   int ok=fwrite(header,1,sizeof header,f)==sizeof header&&
    fwrite(state,1,sizeof state,f)==sizeof state&&fwrite(known,1,sizeof known,f)==sizeof known&&
    fwrite(p,1,sizeof *p,f)==sizeof *p&&fwrite(pc450.target_fields,1,sizeof pc450.target_fields,f)==sizeof pc450.target_fields&&
    fwrite(pc450.spans,1,sizeof pc450.spans,f)==sizeof pc450.spans;
   if(fclose(f))ok=0;
   if(!ok)fprintf(stderr,"[OBS454-IO] row=%u snapshot_write_failed=1 diagnostic_only=1\n",row);
  }
  fprintf(stderr,"[OBS454-KEY] row=%u primitive=%u surface=%08X pixel=%08X mode=%u start=%u instructions=%u code_fnv=%08X profiles=%08X queued=%u held=%d enabled=%d session=%llu publication=%llu old_color=%08X old_depth=%08X mapped_color=%p mapped_depth=%p incoming_color_handle=%08X incoming_depth_handle=%08X incoming_color_offset=%08X incoming_depth_offset=%08X unresolved_incoming_DMA=1 metadata_only=1\n",
   row,primitive,key.surface,key.pixel,key.mode,key.start,key.length,key.code_hash,key.mask,queued,held,enabled,
   (unsigned long long)pc450.sessions,(unsigned long long)pc450.published,pc450.spans[0].address,pc450.spans[1].address,
   (void*)pc450.mapped[0],(void*)pc450.mapped[1],s[0x194/4],s[0x198/4],s[0x210/4],s[0x214/4]);
  for(unsigned q=0;q<DRIVING_PROFILES221;q++)if(key.mask&(1u<<q)){
   BeginReason280 a=key.rejection[q];fprintf(stderr,"[OBS454-REJECT] row=%u profile=%u reason=%s at=%04X actual=%08X expected=%08X\n",row,q,begin280_names[a.why],a.at,a.actual,a.expected);
  }
 }
 if(row<OBS454_ROWS){observe454.row[row].hits++;observe454.row[row].zero_queue+=!queued;}
 else observe454.overflow++;
 token.active=1;token.row=row;token.published=pc450.published;
 observe454.overhead+=ticks454()-begin;
 token.start=ticks454();restore454(&saved);return token;
}
static void observe_after454(Token454 token){
 if(!token.active)return;
 Saved454 saved=save454();uint64_t end=ticks454(),begin=end;
 if(token.row<OBS454_ROWS){observe454.row[token.row].ticks+=end-token.start;
  observe454.row[token.row].published+=pc450.published-token.published;}
 if(observe454.total==1||!(observe454.total%128)){
  LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
  fprintf(stderr,"[OBS454] events=%llu keys=%u overflow=%llu qpc_frequency=%llu observer_ticks=%llu rows=id:events:zero_queue:actual_publications:flush_ticks",
   (unsigned long long)observe454.total,observe454.used,(unsigned long long)observe454.overflow,
   (unsigned long long)frequency.QuadPart,(unsigned long long)observe454.overhead);
  for(unsigned i=0;i<observe454.used;i++)fprintf(stderr," %u:%llu:%llu:%llu:%llu",i,
   (unsigned long long)observe454.row[i].hits,(unsigned long long)observe454.row[i].zero_queue,
   (unsigned long long)observe454.row[i].published,(unsigned long long)observe454.row[i].ticks);
  fputc('\n',stderr);
 }
 observe454.overhead+=ticks454()-begin;restore454(&saved);
}
#endif
