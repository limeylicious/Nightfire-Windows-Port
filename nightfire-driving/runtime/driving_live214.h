/* Opt-in synchronous world adapter. Original offscreen targets only; presentation
 * remains owned by the game's flip path. Unknown state replays exact commands. */
#include "gpu144/nightfire_swizzled_shadow131.h"
#include "driving_collect214.h"
#include "driving_submit214.h"
#include "driving_submit216.h"
#include "driving_submit220.h"
#include "driving_submit221.h"
#include "driving_submit243.h"
#include "driving_packets236.h"
static DrivingCollector214 collector214;
static uint32_t state214[2048];
static unsigned char known214[2048];
static NFVertexProgram program214;
static unsigned family214,draws214[6],profiles_drawn221[DRIVING_PROFILES221],declines214;
static int enabled214=-1,enabled216=-1,enabled220=-1,enabled221=-1,enabled243=-1;
static void emit214(void *ctx,uint32_t method,uint32_t value)
{
    (void)ctx;residency_flush236("collector-replay");nv2a_pb_exec_method(0,method,value);
}
static void barrier214(void)
{
    if(driving_collect214_barrier(&collector214))
        driving_collect214_replay(&collector214,emit214,NULL);
}
static int intercept214(GPUObject143 *object,unsigned method,uint32_t value)
{
    if(enabled214<0){const char *v=getenv("DRIVING_WORLD_GPU214");enabled214=v&&!strcmp(v,"1");}
    if(enabled216<0){const char *v=getenv("DRIVING_WORLD_GPU216");enabled216=v&&!strcmp(v,"1");}
    if(enabled220<0){const char *v=getenv("DRIVING_WORLD_GPU220");enabled220=v&&!strcmp(v,"1");}
    if(enabled221<0){const char *v=getenv("DRIVING_WORLD_GPU221");enabled221=v&&!strcmp(v,"1");}
    if(enabled243<0){const char *v=getenv("DRIVING_OFFSCREEN_GPU243");enabled243=v&&!strcmp(v,"1");}
    if(!enabled214 || enabled201<=0)return 0; /* Exact scene classification does not require a prior movie. */
    if(collector214.phase==DC214_IDLE){
        if(method!=0x17fc || !value)return 0;
        int family=driving_plan214(object->state,object->written167,&live201.program,value);
        if(family<0 && enabled216 && driving_plan216(object->state,object->written167,&live201.program,value))family=2;
        if(family<0 && enabled220 && driving_plan220(object->state,object->written167,&live201.program,value))family=3;
        unsigned profile221;
        if(enabled221 && driving_plan221(object->state,object->written167,&live201.program,value,&profile221))family=4;
        unsigned profile243;
        if(enabled243&&driving_plan243(object->state,object->written167,&live201.program,value,&profile243))family=5;
        if(family<0)return 0;
        memcpy(state214,object->state,sizeof state214);
        memcpy(known214,object->written167,sizeof known214);
        program214=live201.program;family214=(unsigned)family;
        return driving_collect214_begin(&collector214,value);
    }
    unsigned result=driving_collect214_feed(&collector214,method,value);
    if(result==DC214_HELD)return 1;
    if(result==DC214_READY){
        unsigned count;const uint32_t *indices=driving_collect214_result(&collector214,&count);
        int accepted=family214==5?submit243(state214,known214,&program214,collector214.primitive,indices,count):
            family214==4?submit_residency236(state214,known214,&program214,collector214.primitive,indices,count):
            family214==3?submit220(state214,known214,&program214,collector214.primitive,indices,count):
            family214==2?submit216(state214,known214,&program214,collector214.primitive,indices,count):
            submit214(state214,known214,&program214,collector214.primitive,indices,count,family214);
        if(accepted){
            scene_accept240(family214,state214[0x208/4]);
            unsigned n=++draws214[family214];
            if(family214==4){unsigned p;
                if(driving_plan221(state214,known214,&program214,collector214.primitive,&p)){
                    unsigned seen=++profiles_drawn221[p];
                    if(seen<=4||!(seen%60))fprintf(stderr,"[GPU221] profile=%u accepted=%u total=%u indices=%u target=%08X original-two-lane=1 experimental-sample-model=1\n",p,seen,n,count,state214[0x210/4]);
                }
            }
            if(family214<4 && (n<=8 || !(n%120))){
                if(family214==3)fprintf(stderr,"[GPU220] dual completed=%u indices=%u color=%08X original-two-lane=1 experimental-sample-model=1\n",n,count,state214[0x210/4]);
                else if(family214==2)fprintf(stderr,"[GPU216] main completed=%u indices=%u color=%08X original-two-lane=1 experimental-sample-model=1\n",n,count,state214[0x210/4]);
                else fprintf(stderr,"[GPU214] family=%u completed=%u indices=%u color=%08X depth=%08X original-offscreen=1\n",family214,n,count,state214[0x210/4],state214[0x214/4]);
            }
            driving_collect214_commit(&collector214);return 1;
        }
        if(++declines214<=8)fprintf(stderr,"[GPU214] family=%u count=%u declined; exact command replay\n",family214,count);
        driving_collect214_replay(&collector214,emit214,NULL);return 1;
    }
    if(result==DC214_FLUSH_BEFORE_CURRENT)
        driving_collect214_replay(&collector214,emit214,NULL);
    return 0;
}
