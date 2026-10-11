/* Host diagnostics only: preserve the initialized main thread context. */
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#include "../../nightfire_diagnostics.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef NIGHTFIRE_OBSERVER_OUTLINE104
#if defined(_MSC_VER)
#define NF104_COLD __declspec(noinline)
#else
#define NF104_COLD __attribute__((noinline))
#endif
/* Outline host diagnostics only. Universal polls and guest history stay ordered. */
#endif
#ifdef NIGHTFIRE_GAME_PACING93
#include "../../../runtime/nightfire_game_pacing93.h"
#endif
#ifdef NIGHTFIRE_GAME_RATE94
#include "../../../runtime/nightfire_game_rate94.h"
#endif
#ifdef NIGHTFIRE_FRAME_PROBE_DIAGNOSTIC
#include "../../../runtime/nightfire_frame_probe82.h"
#endif
#ifdef NIGHTFIRE_CAMERA_RECORDER_DIAGNOSTIC
#include "../../../runtime/nightfire_camera_recorder.h"
#endif
#ifdef NIGHTFIRE_POSE_CAPTURE_DIAGNOSTIC
#include "../../../runtime/nightfire_pose_capture.h"
#endif
static RECOMP_TLS uint32_t expected_fs, expected_prcb;
static RECOMP_TLS unsigned armed, cursor;
#include "../../../runtime/nightfire_direct_mouse122.h"
#include "../../../runtime/nightfire_bot_trace231.h"
#include "../../../runtime/nightfire_direct_mouse_dispatch127.h"
#ifdef NIGHTFIRE_DEV_DAMAGE134
#include "../../../runtime/nightfire_dev_damage134.h"
#endif
static RECOMP_TLS struct { uint32_t va, esp, words[8]; unsigned after, readable; } history[32];
void nightfire_thread_check_arm(void)
{
    {extern void nf_lockstep_arm(void);nf_lockstep_arm();}   /* NIGHTFIRE_LOCKSTEP: this is the game thread */
    expected_fs = g_fs_base;
    if (expected_fs < 0x1000 || expected_fs > 0x03ffffd0)
        nightfire_diagnostic_stop("thread initialization", "invalid thread-information base");
    expected_prcb = MEM32(expected_fs + 0x20);
    if (expected_prcb < 0x1000 || expected_prcb > 0x03fffdac)
        nightfire_diagnostic_stop("thread initialization", "invalid processor-context pointer");
    armed = 1;
    fprintf(stderr, "[THREAD-CHECK] armed fs=%08X prcb=%08X\n",
            (unsigned)expected_fs, (unsigned)expected_prcb);
}
static int main_readable(uint32_t address, uint32_t length)
{
    return address >= 0x1000 && (uint64_t)address + length <= 0x04000000u;
}
/* Optional, read-only investigation of the newly reached render call path.
 * These three PAL routines preserve EBX/ESI/EDI and use a plain RET. */
#ifdef NIGHTFIRE_OBSERVER_OUTLINE104
static NF104_COLD void render_return_watch_cold104(uint32_t va,unsigned after)
{
    if((va==0x5a020 || va==0x5a160) && after) {
        static RECOMP_TLS unsigned step_aim_returns[2];
        unsigned count=++step_aim_returns[va==0x5a160];
        if(count<=4 || count%256==0)
            fprintf(stderr,"[STEP-AIM-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
    }
    if(after) {
        static const uint32_t death_entries[]={0x567f0,0x56a30,0x56bd0};
        static RECOMP_TLS unsigned death_returns[3];
        for(unsigned n=0;n<3;n++) if(va==death_entries[n]) {
            unsigned count=++death_returns[n];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[DEATH-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
            break;
        }
        static const uint32_t alert_entries[]={0x58270,0x58650,0x58a10};
        static RECOMP_TLS unsigned alert_returns[3];
        for(unsigned n=0;n<3;n++) if(va==alert_entries[n]) {
            unsigned count=++alert_returns[n];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[ALERT-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
            break;
        }
        static const uint32_t registry_entries[]={0x4f440,0x4fa40,0x52660,0x528c0,0x556f0,0x4bbf0,0x58190,0x59040,0x5bf10,0x5dc10,0x4e040};
        static RECOMP_TLS unsigned registry_returns[11];
        for(unsigned n=0;n<11;n++) if(va==registry_entries[n]) {
            unsigned count=++registry_returns[n];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[GUARD-REGISTRY-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
            break;
        }
        if(va==0x5d390) {
            static RECOMP_TLS unsigned noise_returns;
            unsigned count=++noise_returns;
            if(count<=4 || count%256==0)
                fprintf(stderr,"[NOISE-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
        }
        if(va==0x68510) {
            static RECOMP_TLS unsigned emitter_returns;
            unsigned count=++emitter_returns;
            if(count<=4 || count%256==0)
                fprintf(stderr,"[EMITTER-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
        }
        static RECOMP_TLS unsigned castle_returns[3];
        unsigned castle=va==0x52f20?0:va==0x536c0?1:va==0x54130?2:3;
        if(castle<3) {
            unsigned count=++castle_returns[castle];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[CASTLE-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
        }
        static RECOMP_TLS unsigned cover_returns[3];
        unsigned cover=va==0x4cf20?0:va==0x5ba50?1:va==0x5c420?2:3;
        if(cover<3) {
            unsigned count=++cover_returns[cover];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[COVER-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
        }
        static RECOMP_TLS unsigned combat_returns[3];
        unsigned c=va==0x573f0?0:va==0x576b0?1:va==0x57c80?2:3;
        if(c<3) {
            unsigned count=++combat_returns[c];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[COMBAT-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
        }
        static RECOMP_TLS unsigned aim_returns[4];
        unsigned i=va==0x591d0?0:va==0x59390?1:va==0x59c30?2:va==0x59d50?3:4;
        if(i<4) {
            unsigned count=++aim_returns[i];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[AIM-FIRE-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
        }
    }
    /* Pair nested boundaries so a long GPU upload cannot erase the first
     * broken direct helper return. PAL E04E0 helpers use RET, except _chkstk. */
    static RECOMP_TLS struct {uint32_t va,sp,b,s,d,expected;unsigned check;} frames[256];
    static RECOMP_TLS unsigned depth;
    if(!after && (depth || va==0xe04e0)) {
        if(depth==256)nightfire_diagnostic_stop("render watch","observer nesting limit");
        unsigned check=depth && frames[depth-1].va==0xe04e0;
        frames[depth].va=va;frames[depth].sp=g_esp;
        frames[depth].b=g_ebx;frames[depth].s=g_esi;frames[depth].d=g_edi;
        frames[depth].expected=g_esp+(va==0x111fc0?16:4)-(va==0xee460?g_eax:0);
        frames[depth].check=check || va==0x111fc0;
        depth++;
    } else if(after && depth) {
        unsigned n=--depth;
        if(frames[n].va!=va)nightfire_diagnostic_stop("render watch","unpaired observer boundary");
        if(frames[n].check && (g_esp!=frames[n].expected || g_ebx!=frames[n].b || g_esi!=frames[n].s || g_edi!=frames[n].d)) {
            fprintf(stderr,"[RENDER-HELPER] %08X before SP/B/S/D=%08X/%08X/%08X/%08X expected-SP=%08X after=%08X/%08X/%08X/%08X\n",va,frames[n].sp,frames[n].b,frames[n].s,frames[n].d,frames[n].expected,g_esp,g_ebx,g_esi,g_edi);
            nightfire_diagnostic_stop("render helper return","first E04E0 helper stack/register mismatch");
        }
        if(va==0x111fc0) {
            static RECOMP_TLS unsigned inversions;
            inversions++;
            if(inversions<=4 || inversions%256==0)
                fprintf(stderr,"[INVERSE-RETURN] count=%u before-SP=%08X after-SP=%08X PAL-expected=%08X\n",inversions,frames[n].sp,g_esp,frames[n].expected);
        }
    }
    static RECOMP_TLS struct {uint32_t sp,b,s,d;unsigned active;} saved[3];
    static RECOMP_TLS struct {uint32_t va,sp,b,s,d;unsigned after;} trail[128];
    static RECOMP_TLS unsigned pos;
    unsigned i=va==0x16c20?0:va==0xe0960?1:va==0xe04e0?2:3;
    if(i<3 && !after){saved[i].sp=g_esp;saved[i].b=g_ebx;saved[i].s=g_esi;saved[i].d=g_edi;saved[i].active=1;}
    if(saved[0].active){unsigned n=pos++%128;trail[n].va=va;trail[n].after=after;trail[n].sp=g_esp;trail[n].b=g_ebx;trail[n].s=g_esi;trail[n].d=g_edi;}
    if(i<3 && after && saved[i].active){
        saved[i].active=0;
        if(g_esp!=saved[i].sp+4 || g_ebx!=saved[i].b || g_esi!=saved[i].s || g_edi!=saved[i].d){
            fprintf(stderr,"[RENDER-RETURN] %08X before SP/B/S/D=%08X/%08X/%08X/%08X after=%08X/%08X/%08X/%08X\n",va,saved[i].sp,saved[i].b,saved[i].s,saved[i].d,g_esp,g_ebx,g_esi,g_edi);
            for(unsigned k=pos>128?pos-128:0;k<pos;k++){unsigned n=k%128;fprintf(stderr,"[RENDER-TRAIL] %s %08X SP/B/S/D=%08X/%08X/%08X/%08X\n",trail[n].after?"after":"before",trail[n].va,trail[n].sp,trail[n].b,trail[n].s,trail[n].d);}
            nightfire_diagnostic_stop("render return","PAL callee-saved register or stack invariant failed");
        }
    }
}
static void render_return_watch(uint32_t va,unsigned after)
{
    static int enabled=-1;
    if(enabled<0)enabled=getenv("NIGHTFIRE_RENDER_RETURN_WATCH")!=NULL;
    if(!enabled || !armed || after>1)return;
    render_return_watch_cold104(va,after);
}
#else
static void render_return_watch(uint32_t va,unsigned after)
{
    static int enabled=-1;
    if(enabled<0)enabled=getenv("NIGHTFIRE_RENDER_RETURN_WATCH")!=NULL;
    if(!enabled || !armed || after>1)return;
    if((va==0x5a020 || va==0x5a160) && after) {
        static RECOMP_TLS unsigned step_aim_returns[2];
        unsigned count=++step_aim_returns[va==0x5a160];
        if(count<=4 || count%256==0)
            fprintf(stderr,"[STEP-AIM-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
    }
    if(after) {
        static const uint32_t death_entries[]={0x567f0,0x56a30,0x56bd0};
        static RECOMP_TLS unsigned death_returns[3];
        for(unsigned n=0;n<3;n++) if(va==death_entries[n]) {
            unsigned count=++death_returns[n];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[DEATH-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
            break;
        }
        static const uint32_t alert_entries[]={0x58270,0x58650,0x58a10};
        static RECOMP_TLS unsigned alert_returns[3];
        for(unsigned n=0;n<3;n++) if(va==alert_entries[n]) {
            unsigned count=++alert_returns[n];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[ALERT-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
            break;
        }
        static const uint32_t registry_entries[]={0x4f440,0x4fa40,0x52660,0x528c0,0x556f0,0x4bbf0,0x58190,0x59040,0x5bf10,0x5dc10,0x4e040};
        static RECOMP_TLS unsigned registry_returns[11];
        for(unsigned n=0;n<11;n++) if(va==registry_entries[n]) {
            unsigned count=++registry_returns[n];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[GUARD-REGISTRY-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
            break;
        }
        if(va==0x5d390) {
            static RECOMP_TLS unsigned noise_returns;
            unsigned count=++noise_returns;
            if(count<=4 || count%256==0)
                fprintf(stderr,"[NOISE-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
        }
        if(va==0x68510) {
            static RECOMP_TLS unsigned emitter_returns;
            unsigned count=++emitter_returns;
            if(count<=4 || count%256==0)
                fprintf(stderr,"[EMITTER-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
        }
        static RECOMP_TLS unsigned castle_returns[3];
        unsigned castle=va==0x52f20?0:va==0x536c0?1:va==0x54130?2:3;
        if(castle<3) {
            unsigned count=++castle_returns[castle];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[CASTLE-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
        }
        static RECOMP_TLS unsigned cover_returns[3];
        unsigned cover=va==0x4cf20?0:va==0x5ba50?1:va==0x5c420?2:3;
        if(cover<3) {
            unsigned count=++cover_returns[cover];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[COVER-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
        }
        static RECOMP_TLS unsigned combat_returns[3];
        unsigned c=va==0x573f0?0:va==0x576b0?1:va==0x57c80?2:3;
        if(c<3) {
            unsigned count=++combat_returns[c];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[COMBAT-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
        }
        static RECOMP_TLS unsigned aim_returns[4];
        unsigned i=va==0x591d0?0:va==0x59390?1:va==0x59c30?2:va==0x59d50?3:4;
        if(i<4) {
            unsigned count=++aim_returns[i];
            if(count<=4 || count%256==0)
                fprintf(stderr,"[AIM-FIRE-RETURN] routine=%08X count=%u result=%08X\n",va,count,g_eax);
        }
    }
    /* Pair nested boundaries so a long GPU upload cannot erase the first
     * broken direct helper return. PAL E04E0 helpers use RET, except _chkstk. */
    static RECOMP_TLS struct {uint32_t va,sp,b,s,d,expected;unsigned check;} frames[256];
    static RECOMP_TLS unsigned depth;
    if(!after && (depth || va==0xe04e0)) {
        if(depth==256)nightfire_diagnostic_stop("render watch","observer nesting limit");
        unsigned check=depth && frames[depth-1].va==0xe04e0;
        frames[depth].va=va;frames[depth].sp=g_esp;
        frames[depth].b=g_ebx;frames[depth].s=g_esi;frames[depth].d=g_edi;
        frames[depth].expected=g_esp+(va==0x111fc0?16:4)-(va==0xee460?g_eax:0);
        frames[depth].check=check || va==0x111fc0;
        depth++;
    } else if(after && depth) {
        unsigned n=--depth;
        if(frames[n].va!=va)nightfire_diagnostic_stop("render watch","unpaired observer boundary");
        if(frames[n].check && (g_esp!=frames[n].expected || g_ebx!=frames[n].b || g_esi!=frames[n].s || g_edi!=frames[n].d)) {
            fprintf(stderr,"[RENDER-HELPER] %08X before SP/B/S/D=%08X/%08X/%08X/%08X expected-SP=%08X after=%08X/%08X/%08X/%08X\n",va,frames[n].sp,frames[n].b,frames[n].s,frames[n].d,frames[n].expected,g_esp,g_ebx,g_esi,g_edi);
            nightfire_diagnostic_stop("render helper return","first E04E0 helper stack/register mismatch");
        }
        if(va==0x111fc0) {
            static RECOMP_TLS unsigned inversions;
            inversions++;
            if(inversions<=4 || inversions%256==0)
                fprintf(stderr,"[INVERSE-RETURN] count=%u before-SP=%08X after-SP=%08X PAL-expected=%08X\n",inversions,frames[n].sp,g_esp,frames[n].expected);
        }
    }
    static RECOMP_TLS struct {uint32_t sp,b,s,d;unsigned active;} saved[3];
    static RECOMP_TLS struct {uint32_t va,sp,b,s,d;unsigned after;} trail[128];
    static RECOMP_TLS unsigned pos;
    unsigned i=va==0x16c20?0:va==0xe0960?1:va==0xe04e0?2:3;
    if(i<3 && !after){saved[i].sp=g_esp;saved[i].b=g_ebx;saved[i].s=g_esi;saved[i].d=g_edi;saved[i].active=1;}
    if(saved[0].active){unsigned n=pos++%128;trail[n].va=va;trail[n].after=after;trail[n].sp=g_esp;trail[n].b=g_ebx;trail[n].s=g_esi;trail[n].d=g_edi;}
    if(i<3 && after && saved[i].active){
        saved[i].active=0;
        if(g_esp!=saved[i].sp+4 || g_ebx!=saved[i].b || g_esi!=saved[i].s || g_edi!=saved[i].d){
            fprintf(stderr,"[RENDER-RETURN] %08X before SP/B/S/D=%08X/%08X/%08X/%08X after=%08X/%08X/%08X/%08X\n",va,saved[i].sp,saved[i].b,saved[i].s,saved[i].d,g_esp,g_ebx,g_esi,g_edi);
            for(unsigned k=pos>128?pos-128:0;k<pos;k++){unsigned n=k%128;fprintf(stderr,"[RENDER-TRAIL] %s %08X SP/B/S/D=%08X/%08X/%08X/%08X\n",trail[n].after?"after":"before",trail[n].va,trail[n].sp,trail[n].b,trail[n].s,trail[n].d);}
            nightfire_diagnostic_stop("render return","PAL callee-saved register or stack invariant failed");
        }
    }
}
#endif
#ifdef NIGHTFIRE_OBSERVER_OUTLINE104
static NF104_COLD void video_memory_watch_cold104(uint32_t va, unsigned after)
{
    static RECOMP_TLS uint32_t watched_video, watched_width, previous_va, previous_after;
    if (!after && va==0x130624 && main_readable(g_esp,8)) {
        uint32_t p=MEM32(g_esp+4);
        if(main_readable(p,0x160) && p!=watched_video) {
            watched_video=p; watched_width=MEM32(p+0xe0);
            fprintf(stderr,"[VIDEO-WATCH] object=%08X width=%08X\n",p,watched_width);
        }
    }
    if(watched_video && MEM32(watched_video+0xe0)!=watched_width) {
        uint32_t w=MEM32(watched_video+0xe0);
        fprintf(stderr,"[VIDEO-WATCH] object=%08X width %08X -> %08X between %08X/%u and %08X/%u\n",watched_video,watched_width,w,previous_va,previous_after,va,after);
        watched_width=w;
    }
    previous_va=va; previous_after=after;
}
static void video_memory_watch(uint32_t va, unsigned after)
{
    static int enabled=-1;
    if(enabled<0) enabled=getenv("NIGHTFIRE_VIDEO_WATCH")!=NULL;
    if(!enabled) return;
    video_memory_watch_cold104(va,after);
}
#else
static void video_memory_watch(uint32_t va, unsigned after)
{
    static int enabled=-1;
    if(enabled<0) enabled=getenv("NIGHTFIRE_VIDEO_WATCH")!=NULL;
    if(!enabled) return;
    static RECOMP_TLS uint32_t watched_video, watched_width, previous_va, previous_after;
    if (!after && va==0x130624 && main_readable(g_esp,8)) {
        uint32_t p=MEM32(g_esp+4);
        if(main_readable(p,0x160) && p!=watched_video) {
            watched_video=p; watched_width=MEM32(p+0xe0);
            fprintf(stderr,"[VIDEO-WATCH] object=%08X width=%08X\n",p,watched_width);
        }
    }
    if(watched_video && MEM32(watched_video+0xe0)!=watched_width) {
        uint32_t w=MEM32(watched_video+0xe0);
        fprintf(stderr,"[VIDEO-WATCH] object=%08X width %08X -> %08X between %08X/%u and %08X/%u\n",watched_video,watched_width,w,previous_va,previous_after,va,after);
        watched_width=w;
    }
    previous_va=va; previous_after=after;
}
#endif
#ifdef NIGHTFIRE_OBSERVER_OUTLINE104
static NF104_COLD void video_progress_cold104(uint32_t va, unsigned after)
{
    static RECOMP_TLS uint32_t object, status_address, calls, decodes, outputs;
    static RECOMP_TLS uint32_t stream_desc, stream_out;
    if (va == 0x001148ffu) {
        if (!after && main_readable(g_esp,12)) { stream_desc=MEM32(g_esp+4); stream_out=MEM32(g_esp+8); }
        if (after && main_readable(stream_desc,24) && main_readable(stream_out,4)) {
            uint32_t fmt=MEM32(stream_desc+8), stream=MEM32(stream_out);
#ifdef NIGHTFIRE_NATIVE_VIDEO_AUDIO_HOOKS
            extern void nightfire_audio_attach(uint32_t,uint32_t);
            if((int32_t)g_eax>=0) nightfire_audio_attach(stream_desc,stream);
#endif
            fprintf(stderr,"[STREAM-CREATE] result=%08X stream=%08X flags=%08X max=%u callback=%08X context=%08X fmt=%08X",
                    g_eax,stream,MEM32(stream_desc),MEM32(stream_desc+4),MEM32(stream_desc+12),MEM32(stream_desc+16),fmt);
            if(main_readable(fmt,20)) for(unsigned i=0;i<20;i++) fprintf(stderr," %02X",MEM8(fmt+i));
            fputc('\n',stderr);
            if(main_readable(stream,4) && main_readable(MEM32(stream),28)) {
                uint32_t v=MEM32(stream);
                fprintf(stderr,"[STREAM-VTABLE] %08X: %08X %08X %08X %08X %08X %08X %08X\n",v,
                        MEM32(v),MEM32(v+4),MEM32(v+8),MEM32(v+12),MEM32(v+16),MEM32(v+20),MEM32(v+24));
            }
        }
    }
    if (!after && va == 0x001328e1u) decodes++;
    if (!after && va == 0x00130d2fu) outputs++;
    if (va != 0x00130624u) return;
    if (!after && main_readable(g_esp,20)) {
        object=MEM32(g_esp+4); status_address=MEM32(g_esp+12); calls++;
    }
    if (!after || !main_readable(object,0x160)) return;
    if (calls > 12 && calls % 256) return;
    fprintf(stderr,"[VIDEO-PROGRESS] call=%u decoded=%u converted=%u obj=%08X result=%08X status=%u",
            calls,decodes,outputs,object,g_eax,
            main_readable(status_address,4) ? MEM32(status_address) : 0xffffffffu);
    const unsigned fields[]={0x18,0x1c,0x20,0x24,0x38,0x3c,0x44,0x4c,0x50,0x54,0x58,0x5c,0x60,0x64,0x6c,0xa8,0xac,0xb0,0xb4,0xb8,0xbc,0xc4,0xc8,0xcc,0xd0,0x128,0x12c,0x130,0x134,0x138,0x13c,0x140,0x144,0x148,0x14c,0x150,0x154};
    for (unsigned i=0;i<sizeof fields/sizeof fields[0];i++)
        fprintf(stderr," %03X=%08X",fields[i],MEM32(object+fields[i]));
    fputc('\n',stderr);
    for(unsigned offset=0x130;offset<=0x154;offset+=4) {
        uint32_t pointer=MEM32(object+offset);
        if(main_readable(pointer,16))
            fprintf(stderr,"[VIDEO-ARRAY] %03X @%08X: %08X %08X %08X %08X\n",offset,pointer,
                    MEM32(pointer),MEM32(pointer+4),MEM32(pointer+8),MEM32(pointer+12));
    }
}
static void video_progress(uint32_t va,unsigned after)
{
    video_memory_watch(va,after);
    switch(va){
    case 0x1148ff:case 0x1328e1:case 0x130d2f:case 0x130624:
        video_progress_cold104(va,after);break;
    }
}
#else
static void video_progress(uint32_t va, unsigned after)
{
    video_memory_watch(va,after);
    static RECOMP_TLS uint32_t object, status_address, calls, decodes, outputs;
    static RECOMP_TLS uint32_t stream_desc, stream_out;
    if (va == 0x001148ffu) {
        if (!after && main_readable(g_esp,12)) { stream_desc=MEM32(g_esp+4); stream_out=MEM32(g_esp+8); }
        if (after && main_readable(stream_desc,24) && main_readable(stream_out,4)) {
            uint32_t fmt=MEM32(stream_desc+8), stream=MEM32(stream_out);
#ifdef NIGHTFIRE_NATIVE_VIDEO_AUDIO_HOOKS
            extern void nightfire_audio_attach(uint32_t,uint32_t);
            if((int32_t)g_eax>=0) nightfire_audio_attach(stream_desc,stream);
#endif
            fprintf(stderr,"[STREAM-CREATE] result=%08X stream=%08X flags=%08X max=%u callback=%08X context=%08X fmt=%08X",
                    g_eax,stream,MEM32(stream_desc),MEM32(stream_desc+4),MEM32(stream_desc+12),MEM32(stream_desc+16),fmt);
            if(main_readable(fmt,20)) for(unsigned i=0;i<20;i++) fprintf(stderr," %02X",MEM8(fmt+i));
            fputc('\n',stderr);
            if(main_readable(stream,4) && main_readable(MEM32(stream),28)) {
                uint32_t v=MEM32(stream);
                fprintf(stderr,"[STREAM-VTABLE] %08X: %08X %08X %08X %08X %08X %08X %08X\n",v,
                        MEM32(v),MEM32(v+4),MEM32(v+8),MEM32(v+12),MEM32(v+16),MEM32(v+20),MEM32(v+24));
            }
        }
    }
    if (!after && va == 0x001328e1u) decodes++;
    if (!after && va == 0x00130d2fu) outputs++;
    if (va != 0x00130624u) return;
    if (!after && main_readable(g_esp,20)) {
        object=MEM32(g_esp+4); status_address=MEM32(g_esp+12); calls++;
    }
    if (!after || !main_readable(object,0x160)) return;
    if (calls > 12 && calls % 256) return;
    fprintf(stderr,"[VIDEO-PROGRESS] call=%u decoded=%u converted=%u obj=%08X result=%08X status=%u",
            calls,decodes,outputs,object,g_eax,
            main_readable(status_address,4) ? MEM32(status_address) : 0xffffffffu);
    const unsigned fields[]={0x18,0x1c,0x20,0x24,0x38,0x3c,0x44,0x4c,0x50,0x54,0x58,0x5c,0x60,0x64,0x6c,0xa8,0xac,0xb0,0xb4,0xb8,0xbc,0xc4,0xc8,0xcc,0xd0,0x128,0x12c,0x130,0x134,0x138,0x13c,0x140,0x144,0x148,0x14c,0x150,0x154};
    for (unsigned i=0;i<sizeof fields/sizeof fields[0];i++)
        fprintf(stderr," %03X=%08X",fields[i],MEM32(object+fields[i]));
    fputc('\n',stderr);
    for(unsigned offset=0x130;offset<=0x154;offset+=4) {
        uint32_t pointer=MEM32(object+offset);
        if(main_readable(pointer,16))
            fprintf(stderr,"[VIDEO-ARRAY] %03X @%08X: %08X %08X %08X %08X\n",offset,pointer,
                    MEM32(pointer),MEM32(pointer+4),MEM32(pointer+8),MEM32(pointer+12));
    }
}
#endif
#ifdef NIGHTFIRE_OBSERVER_OUTLINE104
static NF104_COLD void audio_boundary_cold104(uint32_t va, unsigned after)
{
    static RECOMP_TLS unsigned lines;
    if (lines++ < 128) {
        fprintf(stderr, "[AUDIO-SETUP] %s %08X eax=%08X ecx=%08X esp=%08X",
                after ? "after" : "before", (unsigned)va, (unsigned)g_eax,
                (unsigned)g_ecx, (unsigned)g_esp);
        if (!after && main_readable(g_esp,20))
            fprintf(stderr," stack=%08X %08X %08X %08X %08X",
                    (unsigned)MEM32(g_esp),(unsigned)MEM32(g_esp+4),
                    (unsigned)MEM32(g_esp+8),(unsigned)MEM32(g_esp+12),
                    (unsigned)MEM32(g_esp+16));
        fputc('\n',stderr);
    }
    if (after && va == 0x11BBF6 && (int32_t)g_eax < 0)
        nightfire_diagnostic_stop("audio initialization", "device initialization failed; see AUDIO-SETUP return code");
    if (after && va == 0x11B8FA) {
        for (unsigned i=0;i<16;i++) {
            uint32_t a=0x12FC48+i*16;
            fprintf(stderr,"[AUDIO-TABLE] %08X base=%08X physical=%08X size=%08X owned=%08X\n",
                    (unsigned)a,(unsigned)MEM32(a),(unsigned)MEM32(a+4),
                    (unsigned)MEM32(a+8),(unsigned)MEM32(a+12));
        }
    }
    if (!after && va == 0x11A393 && g_ecx == 0)
        nightfire_diagnostic_stop("audio copy", "null audio object; stopped before dereferencing");
    if (!after && va == 0x11A393 && main_readable(g_ecx,0x14) && main_readable(g_esp,16)) {
        uint32_t descriptor=MEM32(g_ecx+0x10);
        if (main_readable(descriptor,16)) {
            uint32_t base=MEM32(descriptor), offset=MEM32(g_esp+4);
            uint32_t source=MEM32(g_esp+8), length=MEM32(g_esp+12);
            fprintf(stderr,"[AUDIO-COPY] object=%08X descriptor=%08X base=%08X offset=%08X source=%08X length=%08X\n",
                    (unsigned)g_ecx,(unsigned)descriptor,(unsigned)base,
                    (unsigned)offset,(unsigned)source,(unsigned)length);
            fflush(stderr);
            if (base==0 && length)
                nightfire_diagnostic_stop("audio copy", "uninitialized destination buffer; stopped before copying");
        }
    }
}
static void audio_boundary(uint32_t va, unsigned after)
{
    if (va != 0x11B8FA && va != 0x11B692 && va != 0x1149B5 &&
        va != 0x11A28E && va != 0x11A393 && va != 0x11B78A &&
        va != 0x11BBF6) return;
    audio_boundary_cold104(va,after);
}
#else
static void audio_boundary(uint32_t va, unsigned after)
{
    static RECOMP_TLS unsigned lines;
    if (va != 0x11B8FA && va != 0x11B692 && va != 0x1149B5 &&
        va != 0x11A28E && va != 0x11A393 && va != 0x11B78A &&
        va != 0x11BBF6) return;
    if (lines++ < 128) {
        fprintf(stderr, "[AUDIO-SETUP] %s %08X eax=%08X ecx=%08X esp=%08X",
                after ? "after" : "before", (unsigned)va, (unsigned)g_eax,
                (unsigned)g_ecx, (unsigned)g_esp);
        if (!after && main_readable(g_esp,20))
            fprintf(stderr," stack=%08X %08X %08X %08X %08X",
                    (unsigned)MEM32(g_esp),(unsigned)MEM32(g_esp+4),
                    (unsigned)MEM32(g_esp+8),(unsigned)MEM32(g_esp+12),
                    (unsigned)MEM32(g_esp+16));
        fputc('\n',stderr);
    }
    if (after && va == 0x11BBF6 && (int32_t)g_eax < 0)
        nightfire_diagnostic_stop("audio initialization", "device initialization failed; see AUDIO-SETUP return code");
    if (after && va == 0x11B8FA) {
        for (unsigned i=0;i<16;i++) {
            uint32_t a=0x12FC48+i*16;
            fprintf(stderr,"[AUDIO-TABLE] %08X base=%08X physical=%08X size=%08X owned=%08X\n",
                    (unsigned)a,(unsigned)MEM32(a),(unsigned)MEM32(a+4),
                    (unsigned)MEM32(a+8),(unsigned)MEM32(a+12));
        }
    }
    if (!after && va == 0x11A393 && g_ecx == 0)
        nightfire_diagnostic_stop("audio copy", "null audio object; stopped before dereferencing");
    if (!after && va == 0x11A393 && main_readable(g_ecx,0x14) && main_readable(g_esp,16)) {
        uint32_t descriptor=MEM32(g_ecx+0x10);
        if (main_readable(descriptor,16)) {
            uint32_t base=MEM32(descriptor), offset=MEM32(g_esp+4);
            uint32_t source=MEM32(g_esp+8), length=MEM32(g_esp+12);
            fprintf(stderr,"[AUDIO-COPY] object=%08X descriptor=%08X base=%08X offset=%08X source=%08X length=%08X\n",
                    (unsigned)g_ecx,(unsigned)descriptor,(unsigned)base,
                    (unsigned)offset,(unsigned)source,(unsigned)length);
            fflush(stderr);
            if (base==0 && length)
                nightfire_diagnostic_stop("audio copy", "uninitialized destination buffer; stopped before copying");
        }
    }
}
#endif
#ifdef NIGHTFIRE_OBSERVER_OUTLINE104
static NF104_COLD void input_diagnostic104(uint32_t va,unsigned after)
{
    if(after==1 && (va==0xe76a0 || va==0xde5c0) && getenv("NIGHTFIRE_INPUT_TEST")){
        static uint32_t last_packet=~0u;uint32_t packet=MEM32(0x2ff4a0);
        if(packet!=last_packet){last_packet=packet;fprintf(stderr,"[INPUT-PROCESSED] packet=%u axes=%.6g,%.6g,%.6g,%.6g\n",packet,MEMF(0x2ff4b8),MEMF(0x2ff4bc),MEMF(0x2ff4c0),MEMF(0x2ff4c4));}
    }
}
static NF104_COLD void menu_entry104(uint32_t va,unsigned after,uint32_t menu_pass[4])
{
    if(!after && va==0x95a80 && main_readable(g_esp,16)) {
        for(unsigned i=0;i<4;i++) menu_pass[i]=MEM32(g_esp+4*i);
        static uint32_t seen_pages[64];
        uint32_t p=menu_pass[2];
        if(p && menu_pass[3]) {
            uint32_t id=MEM32(p+0x18); unsigned i;
            for(i=0;i<64 && seen_pages[i] && seen_pages[i]!=id;i++);
            if(i<64 && !seen_pages[i]) {seen_pages[i]=id;fprintf(stderr,"[MENU-PAGE] id=%08X ptr=%08X\n",id,p);}
        }
    }
}
static NF104_COLD void boundary_diagnostics104(uint32_t va,unsigned after,uint32_t menu_pass[4])
{
    if(after==1 && va==0x72e20 && (g_eax&0xffff) && main_readable(g_esp,12)) {
        static unsigned lines;
        if(lines++<128) fprintf(stderr,"[MENU-ACTION] caller=%08X controller=%08X action=%08X mode=%08X result=%04X\n",MEM32(g_esp-4),MEM32(g_esp),MEM32(g_esp+4),MEM32(g_esp+8),g_eax&0xffff);
        if(lines<128) fprintf(stderr,"[MENU-PASS] caller=%08X context=%08X page=%08X input=%08X c8=%08X cc=%08X\n",menu_pass[0],menu_pass[1],menu_pass[2],menu_pass[3],menu_pass[2]?MEM32(menu_pass[2]+0xc8):0,menu_pass[2]?MEM32(menu_pass[2]+0xcc):0);
        if(lines<128 && MEM32(g_esp+4)==0x19 && menu_pass[2]) {
            uint32_t p=menu_pass[2], f=MEM32(p+0xac), n=MEM32(p+0x98)-4*MEM32(p+0xa0);
            fprintf(stderr,"[MENU-FOCUS] page=%08X id=%08X focus=%08X id=%08X tag=%08X\n",p,MEM32(p+0x18),f,f?MEM32(f+0x18):0,f?MEM32(f+0x20):0);
            uint32_t cur=MEM32(menu_pass[1]+0x94);
            if(cur) fprintf(stderr,"[MENU-CURSOR] ptr=%08X x=%d y=%d\n",cur,(int16_t)MEM16(cur+0x24),(int16_t)MEM16(cur+0x26));
            for(unsigned i=0;n && i<16;i++,n=MEM32(n+4))
                fprintf(stderr,"[MENU-CONTROL] ptr=%08X id=%08X tag=%08X flags=%02X xywh=%d,%d,%d,%d\n",n,MEM32(n+0x18),MEM32(n+0x20),MEM8(n+0x7a),(int16_t)MEM16(n+0x70),(int16_t)MEM16(n+0x72),(int16_t)MEM16(n+0x74),(int16_t)MEM16(n+0x76));
        }
    }
    if(!after && va>=0x85000 && va<0x86000 && main_readable(g_esp,20)) {
        static uint32_t seen[128];unsigned i;
        uint32_t event=MEM32(g_esp+16), key=va ^ (event<<20);
        for(i=0;i<128 && seen[i] && seen[i]!=key;i++);
        if(i<128 && !seen[i]) {seen[i]=key;fprintf(stderr,"[FRONTEND] handler=%08X page=%08X event=%08X intro_step=%u\n",va,MEM32(g_esp+4),event,MEM32(0x25DDD8));}
    }
    if(!after && va==0x92c40 && main_readable(g_esp,20)) {
        static unsigned lines; uint32_t event=MEM32(g_esp+8);
        if(lines<96 && (event==0x44 || event==0x5f || event==0x68)) {
            lines++;fprintf(stderr,"[MENU-EVENT] object=%08X event=%08X arg=%08X value=%08X\n",MEM32(g_esp+4),event,MEM32(g_esp+12),MEM32(g_esp+16));
        }
    }
    if(after==1 && (va==0x0006ABF0u || va==0x0006ABC0u)) {
        unsigned depth=MEM16(0x17BFE8);
        uint32_t object=depth<256?MEM32(0x17BFEC+depth*4):0;
        fprintf(stderr,"[GAMEFLOW] call=%08X depth=%u state=%u\n",va,depth,object);
    }
    if(!after && va>=0x0015800Fu && va<=0x001581C6u && main_readable(g_esp,20)) {
        static unsigned input_calls;
        if(input_calls++<32 || input_calls%2048==0)
            fprintf(stderr,"[INPUT-CALL] %08X args=%08X %08X %08X %08X count=%u\n",
                va,MEM32(g_esp+4),MEM32(g_esp+8),MEM32(g_esp+12),MEM32(g_esp+16),input_calls);
    }
#ifdef NIGHTFIRE_VIDEO_MAPPING
    if (!after && (va == 0x00130D2Fu || va == 0x00130EAFu) && main_readable(g_esp,32)) {
        static unsigned video_calls;
        if (video_calls++ < 8)
            fprintf(stderr,"[VIDEO-CALL] %08X args=%08X %08X %08X %08X %08X %08X %08X\n",
                    va,MEM32(g_esp+4),MEM32(g_esp+8),MEM32(g_esp+12),MEM32(g_esp+16),MEM32(g_esp+20),MEM32(g_esp+24),MEM32(g_esp+28));
    }
    /* PAL sub_00130D2F args: blocks-x, blocks-y, Y, U, V, output, pitch.
     * Observe the real conversion destination; leave guest state untouched. */
    if (!after && va == 0x00130D2Fu && main_readable(g_esp, 32)) {
        extern void nightfire_gpu_note_video_output(uint32_t, uint32_t, uint32_t);
        nightfire_gpu_note_video_output(MEM32(g_esp+24), MEM32(g_esp+28), MEM32(g_esp+8));
    }
#endif
}
static NF104_COLD void context_failure104(uint32_t va,unsigned after,uint32_t actual)
{
    fprintf(stderr, "[THREAD-CHECK] changed %s call %08X; fs=%08X expected=%08X; "
            "prcb=%08X expected=%08X; esp=%08X\n",
            after == 2 ? "at context read in" : (after ? "after" : "before"), (unsigned)va, (unsigned)g_fs_base,
            (unsigned)expected_fs, (unsigned)actual, (unsigned)expected_prcb,
            (unsigned)g_esp);
    for (unsigned n=cursor > 32 ? cursor-32 : 0; n<cursor; n++)
        fprintf(stderr, "  call-boundary %s %08X\n",
                history[n%32].after == 2 ? "read  " : (history[n%32].after ? "after " : "before"), (unsigned)history[n%32].va);
    for (unsigned i=0;i<12;i++)
        fprintf(stderr,"  TIB+%02X %08X\n", i*4, (unsigned)MEM32(expected_fs+i*4));
    fflush(stderr);
    nightfire_diagnostic_stop("thread context", "main-thread context changed; see call-boundary history");
}
#endif
void nightfire_thread_check(uint32_t va, unsigned after)
{
    if(armed && (va==0x4e180 || va==0x1b6b0 || va==0x3d670 || va==0xa12b0 || va==0x9d700 || va==0x1b120 || va==0x1cb20))
        nightfire_bot231(va,after);
#ifdef NIGHTFIRE_FRAME_PROBE_DIAGNOSTIC
    if(armed)nightfire_frame_probe82(va,after);
#endif
#ifdef NIGHTFIRE_POSE_CAPTURE_DIAGNOSTIC
    nightfire_pose_capture(va,after);
#endif
    extern void nightfire_input_begin_frame(void);
    extern void nf_lockstep_input_frame(void);
    if(va==0x6cf50 && !after){nightfire_input_begin_frame();nf_lockstep_input_frame();}
    if(armed && va==0xb7a50 && !after) {
        nightfire_mouse122_probe230();
        nightfire_mouse122_boundary();
    }
#ifdef NIGHTFIRE_CAMERA_RECORDER_DIAGNOSTIC
    if(armed)nightfire_camera_recorder(va,after);
#endif
    render_return_watch(va,after);
    uint32_t actual;
#ifdef NIGHTFIRE_OBSERVER_OUTLINE104
    if(after==1 && (va==0xe76a0 || va==0xde5c0))input_diagnostic104(va,after);
#else
    if(after==1 && (va==0xe76a0 || va==0xde5c0) && getenv("NIGHTFIRE_INPUT_TEST")){
        static uint32_t last_packet=~0u;uint32_t packet=MEM32(0x2ff4a0);
        if(packet!=last_packet){last_packet=packet;fprintf(stderr,"[INPUT-PROCESSED] packet=%u axes=%.6g,%.6g,%.6g,%.6g\n",packet,MEMF(0x2ff4b8),MEMF(0x2ff4bc),MEMF(0x2ff4c0),MEMF(0x2ff4c4));}
    }
#endif
    static RECOMP_TLS uint32_t menu_pass[4];
#ifdef NIGHTFIRE_OBSERVER_OUTLINE104
    if(!after && va==0x95a80)menu_entry104(va,after,menu_pass);
#else
    if(!after && va==0x95a80 && main_readable(g_esp,16)) {
        for(unsigned i=0;i<4;i++) menu_pass[i]=MEM32(g_esp+4*i);
        static uint32_t seen_pages[64];
        uint32_t p=menu_pass[2];
        if(p && menu_pass[3]) {
            uint32_t id=MEM32(p+0x18); unsigned i;
            for(i=0;i<64 && seen_pages[i] && seen_pages[i]!=id;i++);
            if(i<64 && !seen_pages[i]) {seen_pages[i]=id;fprintf(stderr,"[MENU-PAGE] id=%08X ptr=%08X\n",id,p);}
        }
    }
#endif
#ifdef NIGHTFIRE_SYNC_GPU
    {extern void nightfire_profile(uint32_t,unsigned);nightfire_profile(va,after);}
    if(armed) {
        extern void nightfire_gpu_drain(void);extern void nightfire_window_exit_if_closed(void);
        nightfire_window_exit_if_closed();
        nightfire_gpu_drain();
    }
    if(armed && va==0x00103730u && after==1) {
        extern void nightfire_gpu_present(void);nightfire_gpu_present();
    }
#endif
#ifdef NIGHTFIRE_NATIVE_VIDEO_AUDIO_HOOKS
    extern void nightfire_audio_boundary(uint32_t,unsigned);
    nightfire_audio_boundary(va,after);
#endif
    video_progress(va, after);
#ifdef NIGHTFIRE_OBSERVER_OUTLINE104
    if((after==1 && (va==0x72e20 || va==0x6abf0 || va==0x6abc0)) ||
       (!after && ((va>=0x85000 && va<0x86000) || va==0x92c40 ||
        (va>=0x15800f && va<=0x1581c6) || va==0x130d2f || va==0x130eaf)))boundary_diagnostics104(va,after,menu_pass);
#else
    if(after==1 && va==0x72e20 && (g_eax&0xffff) && main_readable(g_esp,12)) {
        static unsigned lines;
        if(lines++<128) fprintf(stderr,"[MENU-ACTION] caller=%08X controller=%08X action=%08X mode=%08X result=%04X\n",MEM32(g_esp-4),MEM32(g_esp),MEM32(g_esp+4),MEM32(g_esp+8),g_eax&0xffff);
        if(lines<128) fprintf(stderr,"[MENU-PASS] caller=%08X context=%08X page=%08X input=%08X c8=%08X cc=%08X\n",menu_pass[0],menu_pass[1],menu_pass[2],menu_pass[3],menu_pass[2]?MEM32(menu_pass[2]+0xc8):0,menu_pass[2]?MEM32(menu_pass[2]+0xcc):0);
        if(lines<128 && MEM32(g_esp+4)==0x19 && menu_pass[2]) {
            uint32_t p=menu_pass[2], f=MEM32(p+0xac), n=MEM32(p+0x98)-4*MEM32(p+0xa0);
            fprintf(stderr,"[MENU-FOCUS] page=%08X id=%08X focus=%08X id=%08X tag=%08X\n",p,MEM32(p+0x18),f,f?MEM32(f+0x18):0,f?MEM32(f+0x20):0);
            uint32_t cur=MEM32(menu_pass[1]+0x94);
            if(cur) fprintf(stderr,"[MENU-CURSOR] ptr=%08X x=%d y=%d\n",cur,(int16_t)MEM16(cur+0x24),(int16_t)MEM16(cur+0x26));
            for(unsigned i=0;n && i<16;i++,n=MEM32(n+4))
                fprintf(stderr,"[MENU-CONTROL] ptr=%08X id=%08X tag=%08X flags=%02X xywh=%d,%d,%d,%d\n",n,MEM32(n+0x18),MEM32(n+0x20),MEM8(n+0x7a),(int16_t)MEM16(n+0x70),(int16_t)MEM16(n+0x72),(int16_t)MEM16(n+0x74),(int16_t)MEM16(n+0x76));
        }
    }
    if(!after && va>=0x85000 && va<0x86000 && main_readable(g_esp,20)) {
        static uint32_t seen[128];unsigned i;
        uint32_t event=MEM32(g_esp+16), key=va ^ (event<<20);
        for(i=0;i<128 && seen[i] && seen[i]!=key;i++);
        if(i<128 && !seen[i]) {seen[i]=key;fprintf(stderr,"[FRONTEND] handler=%08X page=%08X event=%08X intro_step=%u\n",va,MEM32(g_esp+4),event,MEM32(0x25DDD8));}
    }
    if(!after && va==0x92c40 && main_readable(g_esp,20)) {
        static unsigned lines; uint32_t event=MEM32(g_esp+8);
        if(lines<96 && (event==0x44 || event==0x5f || event==0x68)) {
            lines++;fprintf(stderr,"[MENU-EVENT] object=%08X event=%08X arg=%08X value=%08X\n",MEM32(g_esp+4),event,MEM32(g_esp+12),MEM32(g_esp+16));
        }
    }
    if(after==1 && (va==0x0006ABF0u || va==0x0006ABC0u)) {
        unsigned depth=MEM16(0x17BFE8);
        uint32_t object=depth<256?MEM32(0x17BFEC+depth*4):0;
        fprintf(stderr,"[GAMEFLOW] call=%08X depth=%u state=%u\n",va,depth,object);
    }
    if(!after && va>=0x0015800Fu && va<=0x001581C6u && main_readable(g_esp,20)) {
        static unsigned input_calls;
        if(input_calls++<32 || input_calls%2048==0)
            fprintf(stderr,"[INPUT-CALL] %08X args=%08X %08X %08X %08X count=%u\n",
                va,MEM32(g_esp+4),MEM32(g_esp+8),MEM32(g_esp+12),MEM32(g_esp+16),input_calls);
    }
#ifdef NIGHTFIRE_VIDEO_MAPPING
    if (!after && (va == 0x00130D2Fu || va == 0x00130EAFu) && main_readable(g_esp,32)) {
        static unsigned video_calls;
        if (video_calls++ < 8)
            fprintf(stderr,"[VIDEO-CALL] %08X args=%08X %08X %08X %08X %08X %08X %08X\n",
                    va,MEM32(g_esp+4),MEM32(g_esp+8),MEM32(g_esp+12),MEM32(g_esp+16),MEM32(g_esp+20),MEM32(g_esp+24),MEM32(g_esp+28));
    }
    /* PAL sub_00130D2F args: blocks-x, blocks-y, Y, U, V, output, pitch.
     * Observe the real conversion destination; leave guest state untouched. */
    if (!after && va == 0x00130D2Fu && main_readable(g_esp, 32)) {
        extern void nightfire_gpu_note_video_output(uint32_t, uint32_t, uint32_t);
        nightfire_gpu_note_video_output(MEM32(g_esp+24), MEM32(g_esp+28), MEM32(g_esp+8));
    }
#endif
#endif
    if (!armed) return; /* Main thread only; workers have their own context. */
    audio_boundary(va, after);
    unsigned slot=cursor % 32;
    history[slot].va=va; history[slot].after=after; history[slot].esp=g_esp;
    history[slot].readable=main_readable(g_esp,32);
    if (history[slot].readable)
        for (unsigned i=0;i<8;i++) history[slot].words[i]=MEM32(g_esp+4*i);
    cursor++;
    actual=MEM32(expected_fs + 0x20);
    if (g_fs_base == expected_fs && actual == expected_prcb) {
#ifdef NIGHTFIRE_DEV_DAMAGE134
        if(va==0xac0a0)nightfire_dev_damage134(va,after);
#endif
        if(va==0xd5030 && !after)nightfire_mouse127_boundary();
        if(va==0xdd1d0){extern void nf_lockstep_frame(unsigned);nf_lockstep_frame(after);}   /* NIGHTFIRE_LOCKSTEP frame clock */
#ifdef NIGHTFIRE_GAME_PACING93
        if(va==0xdd1d0)nightfire_game_pacing93(after);
#endif
#ifdef NIGHTFIRE_GAME_RATE94
        if(va==0xdd1d0 || va==0x6b040)nightfire_game_pacing94(va,after);
#endif
        return;
    }
#ifdef NIGHTFIRE_OBSERVER_OUTLINE104
    context_failure104(va,after,actual);
#else
    fprintf(stderr, "[THREAD-CHECK] changed %s call %08X; fs=%08X expected=%08X; "
            "prcb=%08X expected=%08X; esp=%08X\n",
            after == 2 ? "at context read in" : (after ? "after" : "before"), (unsigned)va, (unsigned)g_fs_base,
            (unsigned)expected_fs, (unsigned)actual, (unsigned)expected_prcb,
            (unsigned)g_esp);
    for (unsigned n=cursor > 32 ? cursor-32 : 0; n<cursor; n++)
        fprintf(stderr, "  call-boundary %s %08X\n",
                history[n%32].after == 2 ? "read  " : (history[n%32].after ? "after " : "before"), (unsigned)history[n%32].va);
    for (unsigned i=0;i<12;i++)
        fprintf(stderr,"  TIB+%02X %08X\n", i*4, (unsigned)MEM32(expected_fs+i*4));
    fflush(stderr);
    nightfire_diagnostic_stop("thread context", "main-thread context changed; see call-boundary history");
#endif
}

/* Captured before a fault; unlike the live stack these survive a runaway copy. */
void nightfire_call_history_dump(void)
{
    fprintf(stderr,"[CALL-HISTORY] last resolved boundaries, oldest first\n");
    for (unsigned n=cursor > 32 ? cursor-32 : 0; n<cursor; n++) {
        unsigned i=n%32;
        fprintf(stderr,"  call-boundary %s %08X esp=%08X",
            history[i].after==2 ? "read  " : (history[i].after ? "after " : "before"),
            (unsigned)history[i].va,(unsigned)history[i].esp);
        if (history[i].readable)
            for (unsigned j=0;j<8;j++) fprintf(stderr," %08X",(unsigned)history[i].words[j]);
        fputc('\n',stderr);
    }
}

