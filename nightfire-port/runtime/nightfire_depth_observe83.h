/* Compile-only counters, no GPU/guest changes. One private immediate context. */
#ifdef NIGHTFIRE_DEPTH_OBSERVE_DIAGNOSTIC
static struct {
    unsigned dirty,maximum;uint64_t draws;
    uint64_t clean,changed,max_clean,zero,clean_bytes,changed_bytes;
    uint64_t disabled_draws,readonly_draws,writing_draws,clears;
} depth_observe;
static void depth_observe_begin(int maximum){
    depth_observe.dirty=0;depth_observe.maximum=maximum;depth_observe.draws=0;
}
static void depth_observe_draw(unsigned enabled,unsigned write,unsigned count){
    if(!count)return;
    depth_observe.draws++;
    if(!enabled)depth_observe.disabled_draws++;
    else if(!write)depth_observe.readonly_draws++;
    else{depth_observe.writing_draws++;depth_observe.dirty=1;}
}
static void depth_observe_clear(void){depth_observe.dirty=1;depth_observe.clears++;}
static void depth_observe_sync(unsigned w,unsigned h){
    uint64_t bytes=(uint64_t)w*h*4;
    if(depth_observe.dirty){depth_observe.changed++;depth_observe.changed_bytes+=bytes;}
    else{depth_observe.clean++;depth_observe.clean_bytes+=bytes;depth_observe.max_clean+=depth_observe.maximum;}
    depth_observe.zero+=!depth_observe.draws;
}
static void depth_observe_report(void){
    fprintf(stderr,"[DEPTH-OBSERVE] clean=%llu dirty=%llu maxclean=%llu zero=%llu cleanbytes=%llu dirtybytes=%llu disabled=%llu readonly=%llu writing=%llu clears=%llu\n",
        (unsigned long long)depth_observe.clean,(unsigned long long)depth_observe.changed,
        (unsigned long long)depth_observe.max_clean,(unsigned long long)depth_observe.zero,
        (unsigned long long)depth_observe.clean_bytes,(unsigned long long)depth_observe.changed_bytes,
        (unsigned long long)depth_observe.disabled_draws,(unsigned long long)depth_observe.readonly_draws,
        (unsigned long long)depth_observe.writing_draws,(unsigned long long)depth_observe.clears);
}
#else
#define depth_observe_begin(m) ((void)0)
#define depth_observe_draw(e,w,n) ((void)0)
#define depth_observe_clear() ((void)0)
#define depth_observe_sync(w,h) ((void)0)
#define depth_observe_report() ((void)0)
#endif
