#ifndef NIGHTFIRE_GAME_AUDIO_H
#define NIGHTFIRE_GAME_AUDIO_H
#include "nightfire_audio_spatial.h"
/* Experimental PAL buffer transport. Guest allocation/refcounts still execute.
 * No guest callbacks run on audio threads. Windows owns playback progress.
 * Spatialization/DSP effects remain incomplete. PAL streaming is opt-in. */
typedef struct {
    uint32_t object, encoded_bytes, frames, cursor, start, loop_start, loop_frames;
    uint32_t rate, frequency;
    uint16_t codec, channels, align, bits;
    int32_t volume;
    unsigned playing, looping, deferred;
    int16_t *pcm;
    IXAudio2SourceVoice *voice;
    uint32_t source, source_bytes, chunk_frames, submitted, ring;
    uint8_t *ready;
    NightfireSpatial spatial, pending_spatial;
    unsigned spatial_enabled, spatial_dirty;
    uint32_t headroom;
    float applied_gain;
    unsigned gain_valid;
} GameSound;
static GameSound game_sounds[256];
static IXAudio2 *game_engine;
static IXAudio2MasteringVoice *game_master;
static SRWLOCK game_audio_lock=SRWLOCK_INIT;
static unsigned game_play_count;
static uint64_t game_gain_updates,game_gain_repeats;
static float game_listener[3],game_pending_listener[3];
static unsigned game_listener_dirty;
static int game_spatial_enabled(void){static int e=-1;if(e<0){const char *v=getenv("NIGHTFIRE_NATIVE_GAME_SPATIAL");e=v && !strcmp(v,"1");}return e;}
static int game_stream_enabled(void){return getenv("NIGHTFIRE_NATIVE_GAME_STREAM") && !strcmp(getenv("NIGHTFIRE_NATIVE_GAME_STREAM"),"1");}
#ifdef NIGHTFIRE_NATIVE_SOUND_AVAILABLE
int nds_on(void);
#endif
static int game_enabled(void){
#ifdef NIGHTFIRE_NATIVE_SOUND_AVAILABLE
    if(nds_on())return 0; /* NIGHTFIRE_NATIVE_SOUND: native DirectSound replaces this layer */
#endif
    static LONG enabled;LONG current=InterlockedCompareExchange(&enabled,0,0);
    if(!current){const char *e=getenv("NIGHTFIRE_NATIVE_GAME_AUDIO");current=e && !strcmp(e,"1")?2:1;InterlockedExchange(&enabled,current);}
    return current==2;
}
static void game_check(HRESULT hr,const char *operation){
    if(FAILED(hr)){fprintf(stderr,"[GAME-PLAYBACK] %s failed=%08lX\n",operation,(unsigned long)hr);fail(operation);}
}
static float game_gain(GameSound *s){
    /* Listening diagnostic only: isolate game-produced streaming from effects.
     * Does not change guest music selection, gain, events or source content. */
    const char *isolate=getenv("NIGHTFIRE_AUDIO_ISOLATE_STREAM");
    if(!s->ring && isolate && !strcmp(isolate,"1"))return 0.0f;
    float gain=powf(10.0f,(s->volume-(game_spatial_enabled()?(int)s->headroom:0))/2000.0f);
    if(game_spatial_enabled() && s->spatial_enabled){
        float d[3];for(unsigned i=0;i<3;i++)d[i]=s->spatial.position[i]-game_listener[i];
        gain*=nightfire_distance_gain(sqrtf(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]),&s->spatial);
    }
    return gain;
}
static void game_apply_gain(GameSound *s){
    if(!s->voice)return;
    float gain=game_gain(s);
    /* XAudio2 retains voice volume. Repeated deferred game settings often
     * resolve to the same gain; avoid resubmitting them to the mixer. */
    if(s->gain_valid && gain==s->applied_gain){game_gain_repeats++;return;}
    game_check(IXAudio2SourceVoice_SetVolume(s->voice,gain,0),"game gain update");
    s->applied_gain=gain;s->gain_valid=1;game_gain_updates++;
}
static GameSound *game_find(uint32_t object){
    for(unsigned i=0;i<256;i++)if(game_sounds[i].object==object && object)return &game_sounds[i];
    return NULL;
}
static uint32_t game_frame(GameSound *s,uint32_t bytes){
    return s->codec==0x69 ? bytes/s->align*64 : bytes/s->align;
}
static uint32_t game_byte(GameSound *s,uint32_t frame){
    return s->codec==0x69 ? frame/64*s->align : frame*s->align;
}
static void game_destroy_voice(GameSound *s){
    if(s->voice){s->voice->lpVtbl->DestroyVoice(s->voice);s->voice=NULL;}
    s->gain_valid=0;
    s->playing=0;
}
static void game_clear(GameSound *s){
    /* DestroyVoice waits for the audio thread before its sample memory is freed. */
    game_destroy_voice(s);free(s->pcm);s->pcm=NULL;
    free(s->ready);s->ready=NULL;s->ring=s->source=s->source_bytes=s->submitted=s->chunk_frames=0;
    s->deferred=0;
    s->frames=s->encoded_bytes=s->cursor=s->start=s->loop_start=s->loop_frames=0;
}
static void game_shutdown(void){
    AcquireSRWLockExclusive(&game_audio_lock);
    for(unsigned i=0;i<256;i++)game_clear(&game_sounds[i]);
    if(game_master){game_master->lpVtbl->DestroyVoice(game_master);game_master=NULL;}
    if(game_engine){IXAudio2_Release(game_engine);game_engine=NULL;}
    fprintf(stderr,"[GAME-GAIN] submitted=%llu unchanged_skipped=%llu\n",(unsigned long long)game_gain_updates,(unsigned long long)game_gain_repeats);
    ReleaseSRWLockExclusive(&game_audio_lock);
}
static void game_create_voice(GameSound *s){
    if(s->voice)return;
    if(!game_engine){
        HRESULT hr=CoInitializeEx(NULL,COINIT_MULTITHREADED);
        if(hr!=RPC_E_CHANGED_MODE)game_check(hr,"game COM init");
        game_check(XAudio2Create(&game_engine,0,XAUDIO2_DEFAULT_PROCESSOR),"game engine");
        game_check(IXAudio2_CreateMasteringVoice(game_engine,&game_master,2,48000,0,NULL,NULL,0),"game master");
        atexit(game_shutdown);
    }
    WAVEFORMATEX wf={0};wf.wFormatTag=1;wf.nChannels=s->channels;wf.nSamplesPerSec=s->rate;
    wf.wBitsPerSample=16;wf.nBlockAlign=2*s->channels;wf.nAvgBytesPerSec=s->rate*wf.nBlockAlign;
    game_check(IXAudio2_CreateSourceVoice(game_engine,&s->voice,&wf,0,16.0f,NULL,NULL,NULL),"game source");
    game_apply_gain(s);
    game_check(IXAudio2SourceVoice_SetFrequencyRatio(s->voice,(float)s->frequency/s->rate,0),"game rate");
}
/* The PAL producer writes through E0DD0/E18A0. Each encoded block becomes ready
 * only after that copy returns. Submitted PCM is private and immutable, so the
 * guest can reuse its ring without racing the Windows audio thread. */
static void game_ring_pump(GameSound *s){
    if(!s->ring || !s->voice || !s->playing)return;
    XAUDIO2_VOICE_STATE state={0};IXAudio2SourceVoice_GetState(s->voice,&state,0);
    unsigned queued=state.BuffersQueued;
    while(queued<2){
        uint32_t frame=(s->start+s->submitted)%s->frames;
        uint32_t first=frame/64,blocks=s->chunk_frames/64;
        unsigned complete=1;for(uint32_t b=0;b<blocks;b++)if(!s->ready[first+b]){complete=0;break;}
        if(!complete)break;
        int16_t *out=s->pcm+frame*s->channels,block[130];
        const uint8_t *data=(const uint8_t *)(g_xbox_mem_offset+(uintptr_t)s->source);
        for(uint32_t b=0;b<blocks;b++){
            if(adpcm_decode_block(block,data+(first+b)*s->align,s->align,s->channels)!=65)
                fail("invalid filled music ring block");
            memcpy(out+b*64*s->channels,block,64*s->channels*2);
        }
        XAUDIO2_BUFFER buffer={0};buffer.AudioBytes=s->chunk_frames*s->channels*2;
        buffer.pAudioData=(const BYTE *)out;
        game_check(IXAudio2SourceVoice_SubmitSourceBuffer(s->voice,&buffer,NULL),"music ring submit");
        memset(s->ready+first,0,blocks);s->submitted+=s->chunk_frames;queued++;
        unsigned packet=s->submitted/s->chunk_frames;
        if(packet<=2 || !(packet%128)){
            int peak=0;for(uint32_t j=0;j<s->chunk_frames*s->channels;j++){int v=out[j];if(v<0)v=-v;if(v>peak)peak=v;}
            fprintf(stderr,"[GAME-MUSIC] packet=%u played=%llu cursor=%u volume=%d peak=%d queued=%u\n",packet,(unsigned long long)state.SamplesPlayed,s->cursor,s->volume,peak,queued);
        }
    }
}
static void game_ring_write(uint32_t slot,uint32_t offset,uint32_t bytes){
    if(!game_stream_enabled() || slot>=64 || !bytes)return;
    uint32_t entry=0x2ae8c0+slot*64;
    GameSound *s=game_find(MEM32(entry));
    if(!s || (!s->deferred && !s->ring) || s->codec!=0x69 ||
       s->source!=MEM32(entry+40) || s->source_bytes!=MEM32(entry+44))return;
    if(offset%s->align || bytes%s->align || offset>s->source_bytes || bytes>s->source_bytes-offset)
        fail("unaligned music producer copy");
    if(!s->ring){
        uint32_t blocks=s->source_bytes/s->align;
        if(blocks<4 || blocks%4)return; /* only equal, whole-ADPCM-block quarters */
        s->encoded_bytes=s->source_bytes;s->frames=blocks*64;s->chunk_frames=s->frames/4;
        s->pcm=(int16_t *)calloc(s->frames*s->channels,2);s->ready=(uint8_t *)calloc(blocks,1);
        if(!s->pcm || !s->ready)fail("music ring allocation");
        s->ring=1;s->deferred=0;
        fprintf(stderr,"[GAME-MUSIC] ring object=%08X source=%08X bytes=%u chunk=%u\n",s->object,s->source,s->source_bytes,s->chunk_frames);
    }
    memset(s->ready+offset/s->align,1,bytes/s->align);
    game_ring_pump(s);
}
static void game_progress(GameSound *s){
    if(!s->voice || !s->playing)return;
    XAUDIO2_VOICE_STATE state={0};IXAudio2SourceVoice_GetState(s->voice,&state,0);
    if(s->ring){s->cursor=(uint32_t)((s->start+state.SamplesPlayed)%s->frames);game_ring_pump(s);return;}
    if(!state.BuffersQueued){s->playing=0;s->cursor=0;return;}
    uint64_t frame=(uint64_t)s->start+state.SamplesPlayed;
    uint32_t length=s->loop_frames?s->loop_frames:s->frames-s->loop_start;
    if(s->looping && length && frame>=s->loop_start+length)
        frame=s->loop_start+(frame-s->loop_start)%length;
    s->cursor=(uint32_t)(frame<s->frames?frame:0);
}
static void game_play(GameSound *s,unsigned looping){
    if(!s->pcm || !s->frames)return;
    game_progress(s);
    if(s->playing && s->looping==looping)return;
    if(s->ring && s->voice && !s->playing){
        if(!looping)fail("unsupported non-looping music ring");
        s->playing=1;game_ring_pump(s);game_check(IXAudio2SourceVoice_Start(s->voice,0,0),"music resume");return;
    }
    game_destroy_voice(s);game_create_voice(s); /* resets Windows SamplesPlayed */
    if(s->cursor>=s->frames)s->cursor=0;
    if(s->ring){
        if(!looping || s->cursor%s->chunk_frames)fail("unsupported music ring play/seek");
        s->start=s->cursor;s->submitted=0;s->looping=1;s->playing=1;
        game_ring_pump(s);game_check(IXAudio2SourceVoice_Start(s->voice,0,0),"music ring start");
        fprintf(stderr,"[GAME-MUSIC] start object=%08X frequency=%u volume=%d queued_frames=%u\n",s->object,s->frequency,s->volume,s->submitted);return;
    }
    XAUDIO2_BUFFER buffer={0};buffer.Flags=XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes=s->frames*s->channels*2;buffer.pAudioData=(const BYTE *)s->pcm;
    buffer.PlayBegin=s->cursor;buffer.PlayLength=s->frames-s->cursor;
    if(looping){
        uint32_t length=s->loop_frames?s->loop_frames:s->frames-s->loop_start;
        if(!length || s->loop_start+length>s->frames || s->cursor>=s->loop_start+length)
            fail("unsupported gameplay loop region");
        buffer.LoopBegin=s->loop_start;buffer.LoopLength=length;buffer.LoopCount=XAUDIO2_LOOP_INFINITE;
    }
    game_check(IXAudio2SourceVoice_SubmitSourceBuffer(s->voice,&buffer,NULL),"game submit");
    game_check(IXAudio2SourceVoice_Start(s->voice,0,0),"game start");
    s->start=s->cursor;s->looping=looping;s->playing=1;
    if(++game_play_count<=64 || !(game_play_count%256)){
        int peak=0;for(uint32_t i=0;i<s->frames*s->channels;i++){int v=s->pcm[i];if(v<0)v=-v;if(v>peak)peak=v;}
        fprintf(stderr,"[GAME-PLAYBACK] play=%u object=%08X codec=%04X frames=%u ch=%u rate=%u frequency=%u volume=%d loop=%u peak=%d gain=%.6f spatial=%u headroom=%u\n",
            game_play_count,s->object,s->codec,s->frames,s->channels,s->rate,s->frequency,s->volume,looping,peak,game_gain(s),s->spatial_enabled,s->headroom);
    }
}
static void game_register(uint32_t desc,uint32_t object){
    if(!readable(desc,24) || !object || game_find(object))return;
    uint32_t f=MEM32(desc+12);if(!readable(f,20))return;
    uint16_t codec=MEM16(f),channels=MEM16(f+2),align=MEM16(f+12),bits=MEM16(f+14);
    uint32_t rate=MEM32(f+4);
    if(channels<1 || channels>2 || rate<8000 || rate>96000 ||
       !((codec==0x69 && align==36*channels && bits==4 && MEM16(f+18)==64) ||
         (codec==1 && (bits==8 || bits==16) && align==channels*bits/8))){
        fprintf(stderr,"[GAME-PLAYBACK] unsupported format object=%08X codec=%04X\n",object,codec);return;
    }
    GameSound *s=NULL;for(unsigned i=0;i<256;i++)if(!game_sounds[i].object){s=&game_sounds[i];break;}
    if(!s)fail("gameplay buffer registry full");
    memset(s,0,sizeof *s);s->object=object;s->codec=codec;s->channels=channels;s->align=align;
    s->bits=bits;s->rate=s->frequency=rate;
    uint32_t caps=MEM32(desc+4);s->spatial_enabled=(caps&0x10)!=0;
    s->headroom=(caps&(0x182000|0x10))?0:600; /* PAL114037 constructor */
    s->spatial.minimum=1.0f;s->spatial.maximum=1000000000.0f;s->pending_spatial=s->spatial;
}
static void game_data(GameSound *s,uint32_t address,uint32_t size){
    game_clear(s);if(!address && !size)return;
    if(size>16*1024*1024 || !readable(address,size) || size%s->align)fail("invalid gameplay sample buffer");
    s->encoded_bytes=size;s->frames=game_frame(s,size);
    if(!s->frames)return;
    s->pcm=(int16_t *)malloc(s->frames*s->channels*2);if(!s->pcm)fail("game sample allocation");
    const uint8_t *data=(const uint8_t *)(g_xbox_mem_offset+(uintptr_t)address);
    if(s->codec==0x69){
        int16_t block[130];
        for(uint32_t at=0,out=0;at<size;at+=s->align,out+=64*s->channels){
            if(adpcm_decode_block(block,data+at,s->align,s->channels)!=65){
                fprintf(stderr,"[GAME-PLAYBACK] deferred incomplete sample object=%08X address=%08X size=%u offset=%u channels=%u prefix=",s->object,address,size,at,s->channels);
                for(unsigned k=0;k<8;k++)fprintf(stderr,"%02X",data[at+k]);fputc('\n',stderr);
                game_clear(s);s->source=address;s->source_bytes=size;s->deferred=1;return;
            }
            memcpy(s->pcm+out,block,64*s->channels*2);
        }
    }else if(s->bits==16)memcpy(s->pcm,data,size);
    else for(uint32_t i=0;i<size;i++)s->pcm[i]=(int16_t)(((int)data[i]-128)*256);
}
int nightfire_game_audio_call(uint32_t va){
    if(va!=0x11343a || !game_enabled() || !readable(g_esp,20))return 0;
    AcquireSRWLockExclusive(&game_audio_lock);
    GameSound *s=game_find(MEM32(g_esp+4));int handled=s && !s->deferred && s->pcm && s->frames;
    if(handled){
        uint32_t flags=MEM32(g_esp+16);if(flags&~1u)fail("unsupported gameplay Play flags");
        game_play(s,flags&1u);g_eax=0;g_esp+=20; /* PAL stdcall: return + four arguments */
    }
    ReleaseSRWLockExclusive(&game_audio_lock);return handled;
}
static void game_audio_boundary(uint32_t va,unsigned after){
    if(va==0xe0dd0 && game_enabled()){
        static RECOMP_TLS uint32_t copy[4];static RECOMP_TLS unsigned pending;
        if(!after){if(readable(g_esp,20)){for(unsigned i=0;i<4;i++)copy[i]=MEM32(g_esp+4+i*4);pending=1;}}
        else if(pending){pending=0;AcquireSRWLockExclusive(&game_audio_lock);game_ring_write(copy[0],copy[1],copy[3]);ReleaseSRWLockExclusive(&game_audio_lock);}
        return;
    }
    if(va<0x112700 || va>0x114700)return;
    /* Registry and native voice operations serialized, never exposed to a
     * Windows callback. Disabled unless explicitly requested for this preview. */
    if(!game_enabled())return;
    static const uint32_t targets[]={0x114647,0x1143e8,0x11343a,0x11345e,0x113496,0x113476,0x1134b2,0x1134d2,0x1133ca,0x113c2b,0x112749,0x1133e6,0x113c47,0x113c6b,0x113c8f,0x113cf9,0x11437e,0x113c13};
    static RECOMP_TLS struct {uint32_t args[5];unsigned pending;} call[sizeof targets/sizeof targets[0]];
    unsigned k=0;while(k<sizeof targets/sizeof targets[0] && targets[k]!=va)k++;
    if(k==sizeof targets/sizeof targets[0])return;
    if(k>=11 && !game_spatial_enabled())return;
    if(!after){if(!readable(g_esp,24))return;for(unsigned j=0;j<5;j++)call[k].args[j]=MEM32(g_esp+4+4*j);call[k].pending=1;return;}
    if(!call[k].pending)return;call[k].pending=0;
    if((int32_t)g_eax<0)return;uint32_t *a=call[k].args;
    AcquireSRWLockExclusive(&game_audio_lock);
    if(va==0x11437e){
        memcpy(game_pending_listener,a+1,12);game_listener_dirty=1;
        if(!a[4]){memcpy(game_listener,game_pending_listener,12);game_listener_dirty=0;for(unsigned j=0;j<256;j++)game_apply_gain(&game_sounds[j]);}
    }else if(va==0x113c13){
        if(game_listener_dirty){memcpy(game_listener,game_pending_listener,12);game_listener_dirty=0;}
        for(unsigned j=0;j<256;j++){
            GameSound *s=&game_sounds[j];if(s->spatial_dirty){s->spatial=s->pending_spatial;s->spatial_dirty=0;}
            if(s->spatial_enabled)game_apply_gain(s);
        }
    }else if(va==0x114647){if(readable(a[2],4))game_register(a[1],MEM32(a[2]));}
    else {
        GameSound *s=game_find(a[0]);
        /* The observed music buffers are registered while still filled
         * with 0x98, then continuously rewritten by the guest streaming code.
         * A static snapshot transport cannot service those rings. Leave their
         * complete status/cursor path guest-owned until streaming is supported. */
        if(s && (va==0x113c47 || va==0x113c6b || va==0x113c8f || va==0x113cf9)){
            unsigned deferred=va==0x113c8f?a[4]:va==0x113cf9?a[3]:a[2];
            if(va==0x113c47)memcpy(&s->pending_spatial.maximum,a+1,4);
            if(va==0x113c6b)memcpy(&s->pending_spatial.minimum,a+1,4);
            if(va==0x113c8f)memcpy(s->pending_spatial.position,a+1,12);
            if(va==0x113cf9){
                if(a[2]>64 || (a[2] && !readable(a[1],a[2]*4)))fail("unsupported sound rolloff curve");
                s->pending_spatial.count=a[2];if(a[2])memcpy(s->pending_spatial.curve,(void *)(g_xbox_mem_offset+(uintptr_t)a[1]),a[2]*4);
            }
            s->spatial_dirty=1;
            if(!deferred){s->spatial=s->pending_spatial;s->spatial_dirty=0;game_apply_gain(s);}
        }
        if(s && (!s->deferred || va==0x1143e8 || va==0x112749 || va==0x1133ca || va==0x113c2b || va==0x1133e6))switch(va){
        case 0x1143e8:game_data(s,a[1],a[2]);break;
        case 0x11343a:if(a[3]&~1u)fail("unsupported gameplay Play flags");game_play(s,a[3]&1);break;
        case 0x11345e:game_progress(s);if(s->ring && s->voice){game_check(IXAudio2SourceVoice_Stop(s->voice,0,0),"music stop");s->playing=0;}else game_destroy_voice(s);break;
        case 0x113496:game_progress(s);if(readable(a[1],4))MEM32(a[1])=s->playing?(1u|(s->looping?4u:0u)):0u;break;
        case 0x113476:s->loop_start=game_frame(s,a[1]);s->loop_frames=game_frame(s,a[2]);break;
        case 0x1134b2:game_progress(s);if(readable(a[1],4))MEM32(a[1])=game_byte(s,s->cursor);if(readable(a[2],4))MEM32(a[2])=game_byte(s,s->cursor);break;
        case 0x1134d2:{unsigned resume=s->playing;game_destroy_voice(s);s->cursor=game_frame(s,a[1]);if(resume)game_play(s,s->looping);break;}
        case 0x1133ca:s->volume=(int32_t)a[1];if(s->volume>0)s->volume=0;if(s->volume< -10000)s->volume=-10000;game_apply_gain(s);break;
        case 0x1133e6:s->headroom=a[1];game_apply_gain(s);break;
        case 0x113c2b:s->frequency=a[1]?a[1]:s->rate;if(s->voice)game_check(IXAudio2SourceVoice_SetFrequencyRatio(s->voice,(float)s->frequency/s->rate,0),"game frequency update");break;
        case 0x112749:if(!g_eax){game_clear(s);memset(s,0,sizeof *s);}break;
        }
    }
    ReleaseSRWLockExclusive(&game_audio_lock);
}
#endif
