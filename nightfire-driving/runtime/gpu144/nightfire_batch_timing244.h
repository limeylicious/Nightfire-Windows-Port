/* Aggregate CPU elapsed/wait timing only. No GPU timestamps or extra waits.
 * Included after hw_clock; only the serialized Driving consumer enters scope. */
#ifndef NIGHTFIRE_BATCH_TIMING244_H
#define NIGHTFIRE_BATCH_TIMING244_H
#ifdef NIGHTFIRE_BATCH_TIMING244
enum {BT244_PREFLIGHT,BT244_BEGIN,BT244_DRAW,BT244_COLOR_MAP,BT244_COLOR_COPY,
 BT244_DEPTH_MAP,BT244_DEPTH_PACK,BT244_BOUNDARY,BT244_PREPARE,BT244_DRAW_VALIDATION,BT244_VERTEX_MAP,BT244_VERTEX_COPY_SUBMIT,BT244_BATCH_PREFLIGHT248,BT244_COUNT};
static struct {uint64_t ticks[BT244_COUNT],calls[BT244_COUNT],clock_reads,
 batches,draws,total,publish,start,publish_start;unsigned active;} bt244;
static int bt244_setting=-1;
static int bt244_enabled(void){
 if(bt244_setting<0){const char*v=getenv("DRIVING_BATCH_TIMING244");bt244_setting=v&&!strcmp(v,"1");}
 return bt244_setting;
}
static uint64_t bt244_clock(void){bt244.clock_reads++;return hw_clock();}
static uint64_t bt244_start(int scope){return (scope?bt244.active:bt244_enabled())?bt244_clock():0;}
static void bt244_end(unsigned phase,uint64_t start){if(start){bt244.ticks[phase]+=bt244_clock()-start;bt244.calls[phase]++;}}
#define BT244_START(name) uint64_t name=bt244_start(1)
#define BT244_END(name,phase) bt244_end(phase,name)
void nf_hw_batch244_mark(unsigned phase,unsigned count){
 if(!bt244_enabled())return;
 if(phase==0){if(bt244.active)return;bt244.active=1;bt244.start=bt244_clock();bt244.publish_start=0;bt244.draws+=count;return;}
 if(!bt244.active)return;
 if(phase==1){bt244.publish_start=bt244_clock();return;}
 if(phase!=2)return;
 uint64_t end=bt244_clock();if(bt244.publish_start)bt244.publish+=end-bt244.publish_start;
 bt244.total+=end-bt244.start;bt244.active=0;bt244.batches++;
 if(bt244.batches!=1&&bt244.batches%120)return;
 LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);double ms=1000.0/frequency.QuadPart;
 fprintf(stderr,"[BATCH244-WALL] batches=%llu draws=%llu total_ms=%.3f prepare_ms=%.3f material_preflight_ms=%.3f batch_preflight_inclusive_ms=%.3f begin_inclusive_ms=%.3f draw_ms=%.3f draw_validation_subset_ms=%.3f vertex_map_wait_subset_ms=%.3f vertex_copy_submit_subset_ms=%.3f boundary_ms=%.3f color_map_wait_ms=%.3f depth_map_wait_ms=%.3f color_rows_ms=%.3f depth_pack_ms=%.3f guest_publish_ms=%.3f begin_calls=%llu draw_calls=%llu color_maps=%llu depth_maps=%llu boundary_calls=%llu CPU-elapsed-including-driver-wait-not-GPU-duration=1 material-preflight-overlaps-begin-and-batch-preflight=1 draw-includes-validation-map-copy-submit=1\n",
 (unsigned long long)bt244.batches,(unsigned long long)bt244.draws,bt244.total*ms,
 bt244.ticks[BT244_PREPARE]*ms,bt244.ticks[BT244_PREFLIGHT]*ms,bt244.ticks[BT244_BATCH_PREFLIGHT248]*ms,bt244.ticks[BT244_BEGIN]*ms,
 bt244.ticks[BT244_DRAW]*ms,bt244.ticks[BT244_DRAW_VALIDATION]*ms,bt244.ticks[BT244_VERTEX_MAP]*ms,bt244.ticks[BT244_VERTEX_COPY_SUBMIT]*ms,bt244.ticks[BT244_BOUNDARY]*ms,
 bt244.ticks[BT244_COLOR_MAP]*ms,bt244.ticks[BT244_DEPTH_MAP]*ms,
 bt244.ticks[BT244_COLOR_COPY]*ms,bt244.ticks[BT244_DEPTH_PACK]*ms,bt244.publish*ms,
 (unsigned long long)bt244.calls[BT244_BEGIN],(unsigned long long)bt244.calls[BT244_DRAW],
 (unsigned long long)bt244.calls[BT244_COLOR_MAP],(unsigned long long)bt244.calls[BT244_DEPTH_MAP],
 (unsigned long long)bt244.calls[BT244_BOUNDARY]);
}
#else
#define BT244_START(name)
#define BT244_END(name,phase) ((void)0)
#endif
#endif
