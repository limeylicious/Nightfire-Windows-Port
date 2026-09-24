/* Single guest-thread consumer. Drain published PUT before guest code resumes,
 * retain partial packets, and follow ring jumps instead of dropping wraps. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "nightfire_surface_probe83.h"
#ifdef NIGHTFIRE_SURFACE_PROBE_DIAGNOSTIC
extern unsigned nightfire_surface_probe_frame(void);
extern void nf_hw_surface_probe_arm(unsigned);
extern void nf_hw_surface_probe_begin_drain(void);
#endif
extern ptrdiff_t xbox_GetMemoryOffset(void);
extern void nv2a_pb_exec_method(uint32_t,uint32_t,uint32_t);
extern unsigned nv2a_pb_exec_repeat(uint32_t,uint32_t,const uint32_t *,unsigned);
extern void nv2a_pb_exec_report(void);
extern void nightfire_profile(uint32_t,unsigned);
extern void nightfire_gpu_complete(void);
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
extern void nightfire_gpu_complete_reason109(const char *reason);
#endif
extern int nf_hw_clear_pending;
extern void nightfire_gpu_read_guard(uint32_t,size_t);
static uint32_t cursor,remaining,method,subchannel,increment,return_address;
static unsigned segments,words,jumps;
static unsigned bulk_packets,bulk_words;
static int enabled=-1;
/* The guest mapping is established before this consumer starts and remains
 * fixed for this process. Cache addresses, never the published PUT value. */
static uintptr_t command_memory;
static volatile uint32_t *put_register;
/* The toolkit's background observer must never execute the same work again. */
void nv2a_pb_scan(uint32_t a,uint32_t b) {(void)a;(void)b;}
void nv2a_pb_scan_report(void) { /* Called by the background diagnostic thread. */ }
static void broken(uint32_t at,uint32_t value)
{
    fprintf(stderr,"[PB-SYNC] invalid command/path at %08X value=%08X\n",at,value);fflush(stderr);abort();
}
static uint32_t word_at(uint32_t at)
{
    if(at>=0x04000000u || (at&3)) broken(at,0);
    if(nf_hw_clear_pending)nightfire_gpu_read_guard(0x80000000u+at,4);
    return *(const uint32_t *)(command_memory+at);
}
static void consume_to(uint32_t put)
{
    if(!put || put==cursor) return;
    nf_surface_probe_end(1001); /* Before scalar/bulk reads in a new drain. */
    if(put>=0x04000000u || (put&3)) broken(put,0);
    if(!command_memory)command_memory=(uintptr_t)xbox_GetMemoryOffset()+0x80000000u;
    if(!cursor) {
        cursor=put;
        fprintf(stderr,"[PB-SYNC] initial PUT=%08X; guest-thread consumption\n",put);
        return;
    }
    unsigned budget=0;
#ifdef NIGHTFIRE_SURFACE_PROBE_DIAGNOSTIC
    nf_hw_surface_probe_begin_drain();
#endif
    nightfire_profile(0xffff0001u,0);
    while(cursor!=put) {
        if(++budget>0x100000) broken(cursor,put);
        /* Only a contiguous, published part of this non-incrementing payload.
         * Ring headers, jumps, calls and partial tails keep the original path. */
        if(remaining>1 && !increment && put>cursor && (method==0x1800 || method==0x1818)){
            unsigned count=(put-cursor)/4;if(count>remaining)count=remaining;
            if(count>0x100000-budget+1)count=0x100000-budget+1;
            if(count>1){
                if(nf_hw_clear_pending)nightfire_gpu_read_guard(0x80000000u+cursor,(size_t)count*4);
                unsigned used=nv2a_pb_exec_repeat(subchannel,method,(const uint32_t *)(command_memory+cursor),count);
                if(used){if(used>count)broken(cursor,used);cursor+=used*4;words+=used;remaining-=used;budget+=used-1;bulk_packets++;bulk_words+=used;continue;}
            }
        }
        uint32_t at=cursor,w=word_at(cursor);cursor+=4;words++;
        if(remaining) {
            nv2a_pb_exec_method(subchannel,method,w);method+=increment;remaining--;continue;
        }
        if((w&3)==1 || (w&0xe0000003u)==0x20000000u) {
            cursor=(w&3)==1 ? w&0xfffffffcu : w&0x1ffffffcu;jumps++;continue;
        }
        if((w&3)==2) {
            if(return_address) broken(at,w);
            return_address=cursor;cursor=w&0xfffffffcu;continue;
        }
        if(w==0x00020000u) {
            if(!return_address) broken(at,w);
            cursor=return_address;return_address=0;continue;
        }
        if((w&0xe0030003u)==0 || (w&0xe0030003u)==0x40000000u) {
            remaining=(w>>18)&0x7ff;method=w&0x1ffc;subchannel=(w>>13)&7;
            increment=(w&0x40000000u)?0:4;continue;
        }
        broken(at,w);
    }
    /* Guest CPU may read/write surfaces after this drain. */
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
    nightfire_gpu_complete_reason109("end-drain");
#else
    nightfire_gpu_complete();
#endif
#ifdef NIGHTFIRE_SURFACE_PROBE_DIAGNOSTIC
    nf_hw_surface_probe_arm(nightfire_surface_probe_frame());
#endif
    nightfire_profile(0xffff0001u,1);
    if(++segments<8 || segments%512==0)
        fprintf(stderr,"[PB-SYNC] segments=%u words=%u jumps=%u pending=%u PUT=%08X bulk_packets=%u bulk_words=%u\n",segments,words,jumps,remaining,put,bulk_packets,bulk_words);
}
void nightfire_gpu_drain(void)
{
    if(enabled<0){
        enabled=getenv("RECOMP_PB_EXEC")!=NULL;
        if(enabled)put_register=(volatile uint32_t *)((uintptr_t)xbox_GetMemoryOffset()+0xfd800040u);
    }
    if(enabled) consume_to(*put_register);
}
