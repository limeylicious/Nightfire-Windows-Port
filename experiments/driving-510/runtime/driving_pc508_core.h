/* Included after the original450 publisher and453 helpers. No toolkit changes.
 * Publication order remains unprotect -> split -> finish -> join -> clear.
 * The RW publication window and untracked in-flight IO are accepted508 risks. */
#ifndef DRIVING_PC508_CORE_H
#define DRIVING_PC508_CORE_H
static void pc508_stop(const char *reason,uint32_t a,uint32_t b){
 fprintf(stderr,"[PC508-STOP] %s tid=%lu owner=%lu phase=%ld detail=%08X/%08X depth=%u/%u/%u/%u\n",
  reason,(unsigned long)GetCurrentThreadId(),(unsigned long)pc451.thread,
  (long)pc508_phase(),a,b,driving_pc508_depth[0],driving_pc508_depth[1],driving_pc508_depth[2],driving_pc508_depth[3]);
 fflush(stderr);fail143(reason,a,b);
}
static void pc508_check_depth(const char *reason){
 for(unsigned i=0;i<4;i++)if(driving_pc508_depth[i])pc508_stop(reason,i,driving_pc508_depth[i]);
}
static BOOL CALLBACK pc508_init(PINIT_ONCE once,PVOID argument,PVOID *context){
 (void)once;(void)argument;(void)context;
 return InitializeCriticalSectionEx(&pc508.owner_lock,0,0);
}
int driving_pc508_enabled(void){return driving_pc508_active!=0;}
void driving_pc508_startup(void){
 DWORD error=GetLastError();int crt=errno;fenv_t fp;fegetenv(&fp);unsigned csr=_mm_getcsr();
 const char *v=getenv("DRIVING_PC508");
 if(v&&!strcmp(v,"1")){
  const char *required[]={"DRIVING_PC450","DRIVING_PC451","DRIVING_PC452","DRIVING_PC453"};
  for(unsigned i=0;i<4;i++){const char *setting=getenv(required[i]);
   if(!setting||strcmp(setting,"1")){fprintf(stderr,"[PC508] missing required flag %s=1\n",required[i]);pc508_stop("pc508 required configuration",i,0);}}
  if(!pc450_config())pc508_stop("pc508 incompatible renderer configuration",0,0);
  const char *forbidden[]={"RECOMP_FB_WINDOW","RECOMP_FMV_HOST","RECOMP_USB",
   "DRIVING_TEXTURE_CAPTURE167","DRIVING_POSTMOVIE_TRACE207","DRIVING_WORLD_CAPTURE223",
   "DRIVING_LATE_CAPTURE240","DRIVING_OFFSCREEN_CAPTURE243","DRIVING_PALETTE_CAPTURE288",
   "DRIVING_REMAINING_CAPTURE290","DRIVING_LIGHTING_CAPTURE298","DRIVING_RESOLVE_CAPTURE222",
   "DRIVING_WORLD_CAPTURE210","DRIVING_WORLD_CAPTURE_BATCH217","DRIVING_WORLD_CAPTURE_BATCH218",
   "DRIVING_WORLD_CAPTURE_BATCH219","DRIVING_WORLD_CAPTURE_BATCH221","DRIVING_WORLD_CAPTURE_VISIBLE218",
   "DRIVING_WORLD_CAPTURE_SCENERY218","DRIVING_WORLD_CAPTURE_SURFACE214",
   "DRIVING_PRODUCER235","DRIVING_OWNERSHIP309","DRIVING_CADENCE204"};
  for(unsigned i=0;i<sizeof forbidden/sizeof forbidden[0];i++){
   const char *setting=getenv(forbidden[i]);
   if(setting&&(i<3||!strcmp(setting,"1"))){
    fprintf(stderr,"[PC508] refused environment %s\n",forbidden[i]);pc508_stop("pc508 unsupported accessor configuration",i,0);}
  }
  if(!InitOnceExecuteOnce(&pc508.once,pc508_init,NULL,NULL))pc508_stop("pc508 owner lock initialization",GetLastError(),0);
  InterlockedExchange(&driving_pc508_active,1);
  fprintf(stderr,"[PC508] enabled=1 session-protected pair owner-publish foreign-STOP original-publication-order accepted-RW-window=1 no-thread-suspension=1\n");
 }
 fesetenv(&fp);_mm_setcsr(csr);errno=crt;SetLastError(error);
}
static void pc508_owner_enter(const char *reason){
 DWORD error=GetLastError();int crt=errno;DWORD thread=GetCurrentThreadId();
 /* Refuse foreign pending work before taking the CS; in particular never
  * wait behind the owner while holding a foreign scanout or kernel lock. */
 if(pc508_phase()!=PC508_CLEAN&&thread!=pc451.thread)pc508_stop(reason,thread,pc451.thread);
 pc508_check_depth("pc508 publication or drain under lock or trap");
 EnterCriticalSection(&pc508.owner_lock);pc508_lock_depth++;
 if(pc508_phase()!=PC508_CLEAN&&thread!=pc451.thread)pc508_stop(reason,thread,pc451.thread);
 errno=crt;SetLastError(error);
}
static void pc508_owner_leave(void){
 DWORD error=GetLastError();int crt=errno;
 if(!pc508_lock_depth)pc508_stop("pc508 owner lock underflow",0,0);
 --pc508_lock_depth;LeaveCriticalSection(&pc508.owner_lock);errno=crt;SetLastError(error);
}
static void pc508_open(void){
 if(!pc508_lock_depth||!pc508_drain_depth||GetCurrentThreadId()!=pc451.thread)
  pc508_stop("pc508 session begin outside owner drain",pc508_lock_depth,pc508_drain_depth);
 pc508_check_depth("pc508 session begin under lock or trap");
 if(pc508_phase()!=PC508_CLEAN)pc508_stop("pc508 overlapping session",pc508_phase(),0);
 /* Pair metadata was copied from the successful original GPU begin. */
 pc451.base[0]=pc450.mapped[0];pc451.base[1]=pc450.mapped[1];
 InterlockedExchange(&pc508.phase,PC508_ARMING);
 pc451_protect_pair();
 if(pc508_phase()!=PC508_RESIDENT)pc508_stop("pc508 session protection incomplete",pc508_phase(),0);
 /* Initial-session mapping admission also needs this drain's real lifetime
  * check; a successful VirtualProtect is not an allocation identity check. */
 for(unsigned i=0;i<2;i++){MEMORY_BASIC_INFORMATION m={0};
  if(pc452_real_query(pc451.base[i],&m)!=sizeof m||m.State!=MEM_COMMIT||m.Type!=MEM_PRIVATE||
     m.Protect!=PAGE_NOACCESS||(uint8_t*)m.BaseAddress!=pc451.base[i]||m.RegionSize<DRIVING_PAIR234)
   pc508_stop("pc508 begin pair lifetime",m.Protect,i);
 }
 pc508.lifetime_pass=1;
}
static void pc508_publication_enter(void){
 pc508_owner_enter("pc508 foreign publication");
 if(pc508_phase()!=PC508_RESIDENT)pc508_stop("pc508 recursive publication or invalid phase",pc508_phase(),0);
 InterlockedExchange(&pc508.phase,PC508_PUBLISHING);
}
static void pc508_publication_leave(void){
 if(pc508_phase()!=PC508_CLEAN)pc508_stop("pc508 publication did not clear session",pc508_phase(),0);
 pc508.lifetime_pass=0;pc508_owner_leave();
}
static void pc508_drain_return(void){
 if(pc508_drain_depth!=1)pc508_stop("pc508 drain return depth",pc508_drain_depth,0);
 pc508_drain_depth=0;pc508.lifetime_pass=0;pc508_owner_leave();
}
static int pc508_overlap(uintptr_t a,size_t bytes,uintptr_t b,size_t span){
 /* Subtraction comparison avoids both end-address overflows. */
 if(!bytes||!span)return 0;
 return a<=b?(b-a<bytes):(a-b<span);
}
static int pc508_pair_overlap(const void *address,size_t bytes){
 if(!address||!bytes)return 0;
 uintptr_t a=(uintptr_t)address;
 for(unsigned i=0;i<2;i++)if(pc451.base[i]&&pc508_overlap(a,bytes,(uintptr_t)pc451.base[i],DRIVING_PAIR234))return 1;
 return 0;
}
void driving_pc508_guard_native(const void *address,size_t bytes,const char *site){
 if(!driving_pc508_active||pc508_phase()==PC508_CLEAN||!pc508_pair_overlap(address,bytes))return;
 DWORD error=GetLastError();int crt=errno;fenv_t fp;fegetenv(&fp);unsigned csr=_mm_getcsr();
 if(GetCurrentThreadId()!=pc451.thread){fprintf(stderr,"[PC508] API=%s address=%p bytes=%zu\n",site,address,bytes);
  pc508_stop("pc508 foreign guarded API access",(uint32_t)(uintptr_t)address,(uint32_t)bytes);}
 pc508_check_depth("pc508 guarded access under lock or trap");
 pc508_owner_enter("pc508 guarded API owner changed");
 if(pc508_phase()!=PC508_CLEAN&&pc508_pair_overlap(address,bytes)){
  if(pc508_phase()!=PC508_RESIDENT)pc508_stop("pc508 recursive guarded publication",pc508_phase(),0);
  if(pc451.env_valid)fesetenv(&pc451.env);_mm_setcsr(pc451.control);
  pc508.guarded++;pc450_publish(site);
 }
 pc508_owner_leave();fesetenv(&fp);_mm_setcsr(csr);errno=crt;SetLastError(error);
}
void driving_pc508_guard_guest(uint32_t address,size_t bytes,const char *site){
 if(!driving_pc508_active||pc508_phase()==PC508_CLEAN||!bytes)return;
 uintptr_t offset=(uintptr_t)xbox_GetMemoryOffset();
 if(offset>UINTPTR_MAX-address)pc508_stop("pc508 guest pointer overflow",address,(uint32_t)bytes);
 /* Same effective virtual address as XBOX_TO_NATIVE; do not reinterpret low
  * image/heap addresses as contiguous physical addresses. */
 driving_pc508_guard_native((const void *)(offset+address),bytes,site);
}
static void *pc508_target_map(uint32_t va,size_t bytes,int write){
 if(!driving_pc508_active||pc508_phase()!=PC508_RESIDENT||!write||bytes!=DRIVING_PAIR234||
    GetCurrentThreadId()!=pc451.thread||!pc508_drain_depth||!pc508.lifetime_pass)return NULL;
 uintptr_t offset=(uintptr_t)xbox_GetMemoryOffset();
 if(offset>UINTPTR_MAX-va)return NULL;
 for(unsigned i=0;i<2;i++)if(va==pc450.spans[i].address&&pc450.spans[i].available>=bytes&&
    pc450.mapped[i]==pc451.base[i]&&(uintptr_t)pc451.base[i]==offset+va&&
    (pc451.old[i]&0xff)==PAGE_READWRITE&&!(pc451.old[i]&~(DWORD)(0xff|PAGE_NOCACHE|PAGE_WRITECOMBINE))){
  pc508.admitted++;return pc451.base[i];
 }
 return NULL;
}
#endif
