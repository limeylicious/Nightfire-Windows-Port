#ifndef NIGHTFIRE_GAME_AUDIO_TRACE_H
#define NIGHTFIRE_GAME_AUDIO_TRACE_H
/* Read-only, bounded API survey. Names are research leads; stack words are
 * recorded verbatim until PAL argument layouts have been verified. */
static void game_audio_trace(uint32_t va,unsigned after){
    if(va<0x112700 || va>0x114900)return;
    static int enabled=-1;if(enabled<0)enabled=getenv("NIGHTFIRE_GAME_AUDIO_TRACE")!=NULL;if(!enabled)return;
    static const uint32_t addresses[]={0x112749,0x112c7a,0x112ccb,0x112d1a,0x112d6b,0x112df0,0x112e45,
        0x1131af,0x11343a,0x11345e,0x113476,0x113496,0x1134b2,0x1134d2,0x1139a3,
        0x113c2b,0x113c8f,0x11417e,0x1143e8,0x114647,0x1146fe,0x1148ff,
        0x1133e6,0x113c47,0x113c6b,0x113cf9,0x11437e,0x113c13};
    static RECOMP_TLS struct {uint32_t words[5];unsigned calls,active,nonempty;} records[sizeof addresses/sizeof addresses[0]];
    unsigned slot=0;while(slot<sizeof addresses/sizeof addresses[0] && addresses[slot]!=va)slot++;
    if(slot==sizeof addresses/sizeof addresses[0])return;
    if(!after){
        if(!readable(g_esp,20))return;
        records[slot].calls++;records[slot].active=1;
        for(unsigned k=0;k<5;k++)records[slot].words[k]=MEM32(g_esp+4*k);
    } else if(records[slot].active){
        records[slot].active=0;unsigned n=records[slot].calls;
        const uint32_t *w=records[slot].words;
        unsigned creation=va==0x114647 || va==0x1146fe;
        unsigned nonempty=(va==0x11417e || va==0x1143e8) && w[2] && w[3];
        if(nonempty)records[slot].nonempty++;
        if(n>32 && (n&4095) && !(creation && n<=256)
            && !(nonempty && records[slot].nonempty<=128))return;
        fprintf(stderr,"[GAME-AUDIO] va=%08X calls=%u caller=%08X args=%08X/%08X/%08X/%08X result=%08X\n",va,n,w[0],w[1],w[2],w[3],w[4],g_eax);
        if(va==0x113cf9 && w[3] && w[3]<=64 && readable(w[2],w[3]*4)){
            fprintf(stderr,"[GAME-ROLLOFF] object=%08X count=%u",w[1],w[3]);for(unsigned j=0;j<w[3];j++)fprintf(stderr," %.7g",MEMF(w[2]+j*4));fputc('\n',stderr);
        }
        if(creation && n<=256){
            if(readable(w[2],24)){
                fprintf(stderr,"[GAME-AUDIO-DESC] va=%08X",w[2]);for(unsigned k=0;k<6;k++)fprintf(stderr," %08X",MEM32(w[2]+4*k));fputc('\n',stderr);
                uint32_t format=MEM32(w[2]+12);
                if(readable(format,20)){
                    fprintf(stderr,"[GAME-AUDIO-FORMAT] ptr=%08X tag=%04X channels=%u rate=%u align=%u bits=%u extra=%u samples=%u\n",
                        format,MEM16(format),MEM16(format+2),MEM32(format+4),MEM16(format+12),MEM16(format+14),MEM16(format+16),MEM16(format+18));
                }
            }
            if(readable(w[3],4)){uint32_t object=MEM32(w[3]);fprintf(stderr,"[GAME-AUDIO-BUFFER] object=%08X\n",object);}
        }
    }
}
#endif
