/* Opt-in read-only PAL bot/collision diagnostics. No guest writes or overrides. */
#ifndef NIGHTFIRE_BOT_TRACE231_H
#define NIGHTFIRE_BOT_TRACE231_H
static int nf_bot231_span(uint32_t p,uint32_t n) {
    return (p>=0x1000u && (uint64_t)p+n<=0x04000000u) ||
           (p>=0x80000000u && (uint64_t)p+n<=0x84000000u);
}
static uint32_t nf_bot231_word(uint32_t p) {return nf_bot231_span(p,4)?MEM32(p):0;}
static float nf_bot231_float(uint32_t p) {return nf_bot231_span(p,4)?MEMF(p):0;}
static void nightfire_bot231(uint32_t va,unsigned after) {
    static int enabled=-1;
    static unsigned events[250][72],damage_count,other_count;
    static struct {uint32_t sp,va,obj,state;float health;} frames[32];
    static unsigned depth;
    if(enabled<0){const char *v=getenv("NIGHTFIRE_BOT_TRACE231");enabled=v && !strcmp(v,"1");}
    if(!enabled || MEM32(0x260018)!=1 || !nf_bot231_span(g_esp,28))return;
    if(va==0x4e180 && !after) {
        uint32_t obj=MEM32(g_esp+4),id=MEM32(g_esp+8),evp=MEM32(g_esp+12);
        if(id<0xc4 || id>=250 || !nf_bot231_span(obj,16) || !nf_bot231_span(evp,4))return;
        uint32_t ev=MEM32(evp),slot=ev<71?ev:71;
        unsigned n=++events[id][slot];
        if(n>2 && n%600)return;
        uint32_t phys=MEM32(obj),state=MEM32(obj+4),ext=nf_bot231_word(state+0x974);
        fprintf(stderr,"[BOT231] id=%02X event=%X count=%u obj=%08X phys=%08X state=%08X ext=%08X health=%.8g flags=%08X mode=%08X mstate=%08X player=%08X\n",
            id,ev,n,obj,phys,state,ext,nf_bot231_float(state+0x90),nf_bot231_word(state+0x3f4),
            nf_bot231_word(ext+0x758),nf_bot231_word(nf_bot231_word(obj+12)+4),MEM32(0x1f6654));
        return;
    }
    if(va==0x1b6b0 || va==0x3d670 || va==0xa12b0) {
        if(!after) {
            if(depth==32){fprintf(stderr,"[BOT-DAMAGE231] trace nesting exhausted\n");return;}
            uint32_t obj=MEM32(g_esp+4);
            uint32_t state=nf_bot231_word(obj+(va==0xa12b0?0xbc:4));
            frames[depth].sp=g_esp;frames[depth].va=va;frames[depth].obj=obj;
            frames[depth].state=state;frames[depth].health=nf_bot231_float(state+0x90);++depth;
            if(++damage_count>200)return;
            uint32_t phys=nf_bot231_word(obj),ext=nf_bot231_word(state+0x974);
            fprintf(stderr,"[BOT-DAMAGE231] enter=%05X ret=%08X obj=%08X state=%08X phys=%08X damage=%.8g args=%08X/%08X/%08X health=%.8g flags=%08X phys_da_db=%04X armour_word=%08X gameflow=%u\n",
                va,MEM32(g_esp),obj,state,phys,MEMF(g_esp+8),MEM32(g_esp+12),MEM32(g_esp+16),MEM32(g_esp+20),
                nf_bot231_float(state+0x90),nf_bot231_word(state+0x3f4),
                nf_bot231_span(phys+0xda,2)?MEM16(phys+0xda):0,nf_bot231_word(ext+0x75c),MEM32(0x17bfec+4*MEM16(0x17bfe8)));
        } else {
            if(!depth || frames[depth-1].va!=va || g_esp!=frames[depth-1].sp+4) {
                fprintf(stderr,"[BOT-DAMAGE231] unpaired trace return=%05X depth=%u sp=%08X\n",va,depth,g_esp);return;
            }
            --depth;
            if(damage_count<=200)fprintf(stderr,"[BOT-DAMAGE231] leave=%05X obj=%08X health_before=%.8g health_after=%.8g eax=%08X\n",
                va,frames[depth].obj,frames[depth].health,nf_bot231_float(frames[depth].state+0x90),g_eax);
        }
    } else if(!after && ++other_count<=160) {
        fprintf(stderr,"[BOT-PATH231] va=%05X ret=%08X args=%08X/%08X/%08X/%08X\n",
            va,MEM32(g_esp),MEM32(g_esp+4),MEM32(g_esp+8),MEM32(g_esp+12),MEM32(g_esp+16));
    }
}
#endif
