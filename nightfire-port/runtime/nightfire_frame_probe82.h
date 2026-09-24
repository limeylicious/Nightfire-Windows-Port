/* Compile-only bounded phase observer. No guest writes or scheduling changes.
 * Measurements include observer overhead; never treat this build as an FPS test. */
#ifndef NIGHTFIRE_FRAME_PROBE82_H
#define NIGHTFIRE_FRAME_PROBE82_H
#include <windows.h>
typedef struct {
    uint64_t begin,end,update_start,update_ticks,render_start,render_ticks;
    unsigned sequence,update_calls,render_calls,input_calls,errors;
    unsigned state_begin,state_end,inhibit_begin,inhibit_end,pause_begin,pause_end;
    unsigned updates_begin,updates_end,renders_begin,renders_end;
    float step;
} NFFrameProbe82Row;
static NFFrameProbe82Row nf_frame82_rows[200];
static unsigned nf_frame82_calls,nf_frame82_count,nf_frame82_active,nf_frame82_done;
static DWORD nf_frame82_thread;
static uint64_t nf_frame82_clock(void){LARGE_INTEGER t;QueryPerformanceCounter(&t);return t.QuadPart;}
static unsigned nf_frame82_state(void){
    unsigned depth=MEM16(0x17bfe8);
    return depth && depth<32?MEM32(0x17bfec+depth*4):0;
}
static void nf_frame82_write(void){
    LARGE_INTEGER f;QueryPerformanceFrequency(&f);
    const char *path=getenv("NIGHTFIRE_FRAME_PROBE_FILE");
    if(!path)path="analysis/checkpoint-82/frame-probe.tsv";
    FILE *out=fopen(path,"w");
    if(!out){fprintf(stderr,"[FRAME82] cannot write %s\n",path);return;}
    fprintf(out,"sequence\tbegin_tick\tend_tick\tfrequency\tupdate_ticks\trender_ticks\tupdate_calls\trender_calls\tinput_calls\terrors\tstate_begin\tstate_end\tinhibit_begin\tinhibit_end\tpause_begin\tpause_end\tupdates_begin\tupdates_end\trenders_begin\trenders_end\tstep\n");
    for(unsigned i=0;i<nf_frame82_count;i++){
        NFFrameProbe82Row *r=&nf_frame82_rows[i];
        fprintf(out,"%u\t%llu\t%llu\t%llu\t%llu\t%llu\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%.9g\n",r->sequence,r->begin,r->end,(uint64_t)f.QuadPart,r->update_ticks,r->render_ticks,r->update_calls,r->render_calls,r->input_calls,r->errors,r->state_begin,r->state_end,r->inhibit_begin,r->inhibit_end,r->pause_begin,r->pause_end,r->updates_begin,r->updates_end,r->renders_begin,r->renders_end,r->step);
    }
    fclose(out);fprintf(stderr,"[FRAME82] saved %u outer iterations to %s; diagnostic overhead included\n",nf_frame82_count,path);
}
static void nightfire_frame_probe82(uint32_t va,unsigned after){
    if(nf_frame82_done || after>1 || (va!=0xdd1d0 && va!=0x6aa90 && va!=0xdac20 && va!=0x6cf50))return;
    DWORD thread=GetCurrentThreadId();
    if(!nf_frame82_thread){if(va!=0xdd1d0 || after)return;nf_frame82_thread=thread;}
    if(thread!=nf_frame82_thread)return;
    if(va==0xdd1d0 && !after){
        if(nf_frame82_active){nf_frame82_rows[nf_frame82_count].errors++;return;}
        unsigned n=++nf_frame82_calls;
        if(n<100)return;
        if(nf_frame82_count==200){nf_frame82_done=1;nf_frame82_write();return;}
        NFFrameProbe82Row *r=&nf_frame82_rows[nf_frame82_count];
        r->sequence=n;r->state_begin=nf_frame82_state();r->inhibit_begin=MEM8(0x1f65c0);
        r->pause_begin=MEM16(0x1fec64);r->updates_begin=MEM32(0x1f65b4);r->renders_begin=MEM32(0x1f65b0);
        nf_frame82_active=1;r->begin=nf_frame82_clock();return;
    }
    if(!nf_frame82_active)return;
    NFFrameProbe82Row *r=&nf_frame82_rows[nf_frame82_count];uint64_t now=nf_frame82_clock();
    if(va==0xdd1d0 && after){
        r->end=now;r->state_end=nf_frame82_state();r->inhibit_end=MEM8(0x1f65c0);
        r->pause_end=MEM16(0x1fec64);r->updates_end=MEM32(0x1f65b4);r->renders_end=MEM32(0x1f65b0);r->step=MEMF(0x17c104);
        if(r->update_start || r->render_start)r->errors++;
        nf_frame82_count++;nf_frame82_active=0;return;
    }
    if(va==0x6cf50){if(!after)r->input_calls++;return;}
    uint64_t *start=va==0x6aa90?&r->update_start:&r->render_start;
    if(!after){
        if(*start)r->errors++;
        *start=now;if(va==0x6aa90)r->update_calls++;else r->render_calls++;
    }else{
        if(!*start){r->errors++;return;}
        if(va==0x6aa90)r->update_ticks+=now-*start;else r->render_ticks+=now-*start;
        *start=0;
    }
}
#endif
