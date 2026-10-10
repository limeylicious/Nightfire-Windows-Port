/* Native Windows playback for PAL XMV DirectSoundStream packets.
 * Guest stream creation/refcounts remain translated. Only registered video
 * streams use this transport; packets complete when waveOut reports DONE.
 * Guest callbacks run on the submitting thread, never a Windows audio thread.
 */
#define COBJMACROS
#include <windows.h>
#include <xaudio2.h>
#include <math.h>
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")
#include <mmsystem.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
#define NIGHTFIRE_MEMORY_TRACE_IMPL
#include "recomp_types.h"
#include "recomp_funcs.h"
#include "nightfire_diagnostics.h"
#ifdef NIGHTFIRE_NATIVE_D3D
/* Native build: movie sound uses the project's own IMA ADPCM decoder
 * (runtime/native_action/nds_dsp.c); nothing xemu-derived is included. */
int nds_own_adpcm_decode_block(int16_t *out,const uint8_t *in,size_t n,int channels);
#define adpcm_decode_block(out,in,n,channels) nds_own_adpcm_decode_block(out,in,n,channels)
#else
#include "nightfire_adpcm.h"
#endif

#define MAX_STREAMS 8
#define MAX_PACKETS 8
#define PENDING 0x8000000au
#define FLUSHED 0x80004004u
typedef struct {
    WAVEHDR hdr;
    uint32_t size, completed, status, context;
    unsigned active, sequence;
} Packet;
typedef struct {
    uint32_t object, callback, context, max_packets, references;
    uint16_t format, channels, block_align, bits;
    uint32_t rate, methods[7], submitted, done;
    unsigned sync_wait;
    ULONGLONG empty_since;
    HWAVEOUT device;
    Packet packets[MAX_PACKETS];
} Stream;
static Stream streams[MAX_STREAMS];
static DWORD owner_thread;
/* Other guest threads may query the transport, but only its owner changes
 * streams. Publish occupancy after opening and after the last flush/close. */
static volatile LONG active_streams;
static RECOMP_TLS unsigned polling;
static int readable(uint32_t p, uint32_t n)
{
    return p && ((p < 0x04000000u && (uint64_t)p+n <= 0x04000000u)
        || (p >= 0x80000000u && (uint64_t)p+n <= 0x84000000u));
}
static Stream *find_stream(uint32_t object)
{
    if(!ReadAcquire(&active_streams)) return NULL;
    if (owner_thread && GetCurrentThreadId()!=owner_thread) return NULL;
    for(unsigned i=0;i<MAX_STREAMS;i++) if(streams[i].object==object && object) return &streams[i];
    return NULL;
}
static void fail(const char *why) { nightfire_diagnostic_stop("native video audio",why); }
static void checked(MMRESULT result,const char *operation)
{
    if(result) { fprintf(stderr,"[NATIVE-AUDIO] %s failed: %u\n",operation,result); fail(operation); }
}
/* A callback may clobber caller-saved registers. Preserve the suspended guest
 * context, including floating-point state, instead of invoking on a new thread. */
static void callback(Stream *s,uint32_t packet_context,uint32_t status)
{
    if(!s->callback) return;
    recomp_func_t fn=recomp_lookup(s->callback);
    if(!fn) fail("unresolved stream callback");
    uint32_t *gp[]={&g_eax,&g_ebx,&g_ecx,&g_edx,&g_esi,&g_edi,&g_esp,&g_ebp,&g_seh_ebp,&g_fs_base};
    uint32_t saved[10];
    RecompMmx *mp[]={&g_mm0,&g_mm1,&g_mm2,&g_mm3,&g_mm4,&g_mm5,&g_mm6,&g_mm7}, ms[8];
    RecompXmm *xp[]={&g_xmm0,&g_xmm1,&g_xmm2,&g_xmm3,&g_xmm4,&g_xmm5,&g_xmm6,&g_xmm7}, xs[8];
    double fs[8]; int ft=g_fp_top, fc=g_fp_cmp, df=g_df; uint16_t cw=g_fp_control_word;
    for(unsigned i=0;i<10;i++) saved[i]=*gp[i];
    for(unsigned i=0;i<8;i++) { ms[i]=*mp[i]; xs[i]=*xp[i]; fs[i]=g_fp_stack[i]; }
    PUSH32(g_esp,status); PUSH32(g_esp,packet_context); PUSH32(g_esp,s->context);
    PUSH32(g_esp,0x00113525u); fn();
    if(g_esp!=saved[6]) fail("stream callback stack imbalance");
    for(unsigned i=0;i<10;i++) *gp[i]=saved[i];
    for(unsigned i=0;i<8;i++) { *mp[i]=ms[i]; *xp[i]=xs[i]; g_fp_stack[i]=fs[i]; }
    g_fp_top=ft; g_fp_cmp=fc; g_fp_control_word=cw; g_df=df;
}
static void finish(Stream *s,Packet *p,uint32_t status)
{
    uint32_t context=p->context;
    checked(waveOutUnprepareHeader(s->device,&p->hdr,sizeof p->hdr),"unprepare");
    if(p->completed) MEM32(p->completed)=status ? 0 : p->size;
    if(p->status) MEM32(p->status)=status;
    free(p->hdr.lpData); memset(p,0,sizeof *p); s->done++;
    if(s->done<8 || s->done%64==0)
        fprintf(stderr,"[NATIVE-AUDIO] completed stream=%08X packet=%u status=%08X context=%u\n",s->object,s->done,status,context);
    unsigned active=0;for(unsigned i=0;i<MAX_PACKETS;i++) active+=s->packets[i].active!=0;
    if(!active && !status) s->empty_since=GetTickCount64();
    callback(s,context,status);
}
static Packet *oldest_packet(Stream *s)
{
    Packet *oldest=NULL;
    for(unsigned j=0;j<MAX_PACKETS;j++) {
        Packet *p=&s->packets[j];
        if(p->active && (!oldest || p->sequence<oldest->sequence)) oldest=p;
    }
    return oldest;
}
void nightfire_audio_poll(void)
{
    if(!ReadAcquire(&active_streams)) return;
    if(polling || !owner_thread || GetCurrentThreadId()!=owner_thread) return;
    polling=1;
    for(unsigned i=0;i<MAX_STREAMS;i++) if(streams[i].object)
        for(unsigned n=0;n<MAX_PACKETS;n++) {
            Packet *oldest=oldest_packet(&streams[i]);
            if(!oldest || !(oldest->hdr.dwFlags & WHDR_DONE)) break;
            finish(&streams[i],oldest,0);
        }
    polling=0;
}
static void flush(Stream *s)
{
    checked(waveOutReset(s->device),"reset");
    for(unsigned i=0;i<MAX_PACKETS;i++) if(s->packets[i].active) finish(s,&s->packets[i],FLUSHED);
    s->sync_wait=0;
}
static void process(void)
{
    Stream *s=find_stream(MEM32(g_esp+4));
    uint32_t input=MEM32(g_esp+8), output=MEM32(g_esp+12);
    if(!s || !readable(input,24) || output) fail("unsupported stream Process arguments");
    nightfire_audio_poll();
    unsigned active=0; Packet *p=NULL;
    for(unsigned i=0;i<MAX_PACKETS;i++) { if(s->packets[i].active) active++; else if(!p) p=&s->packets[i]; }
    if(!p || active>=s->max_packets) fail("stream queue full");
    if(!active && s->empty_since) {
        ULONGLONG gap=GetTickCount64()-s->empty_since;
        if(gap>2) fprintf(stderr,"[AUDIO-GAP] stream=%08X empty-to-refill=%llu ms (lower bound)\n",s->object,(unsigned long long)gap);
        s->empty_since=0;
    }
    uint32_t source=MEM32(input), size=MEM32(input+4);
    uint32_t completed=MEM32(input+8), status=MEM32(input+12);
    if(!size || size>4*1024*1024 || !readable(source,size)
        || (completed && !readable(completed,4)) || (status && !readable(status,4))) fail("invalid audio packet memory");
    if(size%s->block_align) fail("audio packet ends inside a codec block");
    uint32_t pcm_size=s->format==0x69 ? (size/s->block_align)*64*s->channels*2 : size;
    char *pcm=(char *)malloc(pcm_size); if(!pcm) fail("audio allocation failed");
    const uint8_t *data=(const uint8_t *)(g_xbox_mem_offset+(uintptr_t)source);
    if(s->format==0x69) {
        int16_t block[65*2];
        for(uint32_t at=0, out=0;at<size;at+=s->block_align,out+=64*s->channels*2) {
            if(adpcm_decode_block(block,data+at,s->block_align,s->channels)!=65) fail("invalid Xbox ADPCM block");
            /* Xbox advances 64 sample frames per block, as pinned apu_vp.c. */
            memcpy(pcm+out,block,64*s->channels*2);
        }
    } else memcpy(pcm,data,size);
    memset(p,0,sizeof *p);
    p->hdr.lpData=pcm; p->hdr.dwBufferLength=pcm_size;
    p->size=size; p->completed=completed; p->status=status; p->context=MEM32(input+16);
    checked(waveOutPrepareHeader(s->device,&p->hdr,sizeof p->hdr),"prepare");
    if(completed) MEM32(completed)=0;
    if(status) MEM32(status)=PENDING;
    checked(waveOutWrite(s->device,&p->hdr,sizeof p->hdr),"write");
    p->active=1; p->sequence=++s->submitted;
    if(s->submitted<8 || s->submitted%64==0)
        fprintf(stderr,"[NATIVE-AUDIO] submit stream=%08X packet=%u bytes=%u pcm=%u timestamp=%u paused=%u\n",
                s->object,s->submitted,size,pcm_size,p->context,s->sync_wait);
    g_eax=0; g_esp+=16;
}
static void flush_method(void)
{
    Stream *s=find_stream(MEM32(g_esp+4)); if(!s) fail("missing stream");
    flush(s); g_eax=0; g_esp+=8;
}
static void release_method(void)
{
    Stream *s=find_stream(MEM32(g_esp+4)); if(!s) fail("missing stream");
    /* Preserve the real guest refcount/destruction path, after draining native
     * work on its last release. Native packets never enter the guest APU queue. */
    if(s->references>1) s->references--;
    else { flush(s); checked(waveOutClose(s->device),"close"); s->object=0; InterlockedDecrement(&active_streams); }
    sub_00112F1E();
}
recomp_func_t nightfire_audio_lookup(uint32_t va)
{
    if(!ReadAcquire(&active_streams)) return NULL;
    if(!readable(g_esp,16)) return NULL;
    Stream *s=find_stream(MEM32(g_esp+4)); if(!s) return NULL;
    if(va==s->methods[4]) return process;
    if(va==s->methods[5]) return flush_method;
    if(va==s->methods[1]) return release_method;
    return NULL;
}
#ifdef NIGHTFIRE_NATIVE_SOUND_AVAILABLE
int nds_on(void);
#endif
void nightfire_audio_attach(uint32_t desc,uint32_t object)
{
#ifdef NIGHTFIRE_NATIVE_SOUND_AVAILABLE
    if(nds_on())return; /* NIGHTFIRE_NATIVE_SOUND: native DirectSound plays movie streams */
#endif
    if(!readable(desc,24) || !readable(object,4) || MEM32(desc+12)!=0x00130437u) return;
    const char *enabled=getenv("NIGHTFIRE_NATIVE_VIDEO_AUDIO");
    if(enabled && !strcmp(enabled,"0")) return;
    uint32_t format=MEM32(desc+8);
    if(!readable(format,20)) fail("invalid stream format pointer");
    if(find_stream(object)) return;
    Stream *s=NULL;
    for(unsigned i=0;i<MAX_STREAMS;i++) if(!streams[i].object) { s=&streams[i]; break; }
    if(!s) fail("too many video audio streams");
    if(owner_thread && GetCurrentThreadId()!=owner_thread) fail("video audio stream changed threads");
    owner_thread=GetCurrentThreadId(); memset(s,0,sizeof *s);
    s->format=MEM16(format); s->channels=MEM16(format+2); s->rate=MEM32(format+4);
    s->block_align=MEM16(format+12); s->bits=MEM16(format+14);
    if(s->channels<1 || s->channels>2 || s->rate<8000 || s->rate>96000) fail("unsupported video audio format");
    if(s->format==0x69) {
        if(s->block_align!=36*s->channels || MEM16(format+18)!=64 || s->bits!=4) fail("invalid Xbox ADPCM format");
    } else if(s->format!=1 || (s->bits!=8 && s->bits!=16) || s->block_align!=s->channels*s->bits/8)
        fail("unsupported video audio codec");
    uint32_t vtable=MEM32(object); if(!readable(vtable,28)) fail("invalid stream vtable");
    for(unsigned i=0;i<7;i++) s->methods[i]=MEM32(vtable+4*i);
    if(s->methods[1]!=0x00112f1eu || s->methods[4]!=0x001130beu || s->methods[5]!=0x00112fd3u) fail("PAL stream interface mismatch");
    WAVEFORMATEX wf={0}; wf.wFormatTag=WAVE_FORMAT_PCM; wf.nChannels=s->channels; wf.nSamplesPerSec=s->rate;
    wf.wBitsPerSample=s->format==0x69 ? 16 : s->bits;
    wf.nBlockAlign=wf.nChannels*wf.wBitsPerSample/8; wf.nAvgBytesPerSec=wf.nSamplesPerSec*wf.nBlockAlign;
    checked(waveOutOpen(&s->device,WAVE_MAPPER,&wf,0,0,CALLBACK_NULL),"open");
    s->object=object; s->callback=MEM32(desc+12); s->context=MEM32(desc+16); s->max_packets=MEM32(desc+4); s->references=1;
    if(!s->max_packets || s->max_packets>MAX_PACKETS) fail("unsupported packet queue length");
    InterlockedIncrement(&active_streams);
    fprintf(stderr,"[NATIVE-AUDIO] opened stream=%08X codec=%04X channels=%u rate=%u max=%u; real waveOut completion\n",
            object,s->format,s->channels,s->rate,s->max_packets);
}
#include "nightfire_game_audio_trace.h"
#include "nightfire_game_audio.h"
void nightfire_audio_boundary(uint32_t va,unsigned after)
{
    game_audio_trace(va,after);
    game_audio_boundary(va,after);
    if(!ReadAcquire(&active_streams)) return;
    if(after || !owner_thread || GetCurrentThreadId()!=owner_thread) return;
    if(va==0x001134fdu || va==0x00130624u) nightfire_audio_poll();
    if(va==0x001133b2u) {
        for(unsigned i=0;i<MAX_STREAMS;i++) if(streams[i].object && streams[i].sync_wait) {
            checked(waveOutRestart(streams[i].device),"synchronized restart"); streams[i].sync_wait=0;
        }
    }
    if(!readable(g_esp,12)) return;
    Stream *s=find_stream(MEM32(g_esp+4)); if(!s) return;
    if(va==s->methods[0]) s->references++;
    if(va==0x001134f8u || va==0x00113135u) {
        uint32_t mode=MEM32(g_esp+8);
        if(mode>2) fail("unsupported stream pause mode");
        checked(mode ? waveOutPause(s->device) : waveOutRestart(s->device),"pause/restart");
        s->sync_wait=mode==2;
    }
}
