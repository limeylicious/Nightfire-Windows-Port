/* Native input at the verified PAL XAPI boundaries. Port zero is a keyboard
 * controller, merged with Windows XInput port zero. No game state is changed. */
#include <windows.h>
#include <Xinput.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "nightfire_test_input.h"
#include "nightfire_flight_host116.h"
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#include "nightfire_diagnostics.h"
extern unsigned nightfire_window_buttons(void);
extern void nightfire_window_input_begin(unsigned layout,int direct);
int nightfire_direct_mouse122_enabled(void) {
    static int enabled=-1;
    if(enabled<0) {
        const char *v=getenv("NIGHTFIRE_DIRECT_MOUSE122");
        enabled=v && !strcmp(v,"1") && !getenv("NIGHTFIRE_INPUT_TEST");
        if(enabled)fprintf(stderr,"[DIRECT-MOUSE122] relative mouse displacement enabled; controller processing retained\n");
    }
    return enabled;
}
extern void nightfire_window_pc_input(unsigned char state[18],unsigned *buttons);
void nightfire_input_begin_frame(void) {
    if(!getenv("NIGHTFIRE_INPUT_TEST"))nightfire_window_input_begin(MEM16(0x1fe6de),nightfire_direct_mouse122_enabled());
}
#define PAD_TYPE 0x0015722cu
#define PAD_HANDLE 0x4e460001u
static int opened;
static uint32_t serial;
static unsigned char previous[18];
static ULONGLONG start;
static int valid(uint32_t p,unsigned n) {return p>=0x1000 && (uint64_t)p+n<=0x4000000;}
int nightfire_input_call(uint32_t va)
{
    const char *enabled=getenv("NIGHTFIRE_NATIVE_INPUT");
    if(enabled && !strcmp(enabled,"0")) return 0;
    uint32_t a=MEM32(g_esp+4),b=MEM32(g_esp+8),c=MEM32(g_esp+12);
    if(va==0x158137 && a==PAD_TYPE) {
        if(!start) {
            start=GetTickCount64();
            const char *preview=getenv("NIGHTFIRE_MENU_PREVIEW");
            if(preview && !strcmp(preview,"1")) {
                if(MEM8(0x17C558)!=1) nightfire_diagnostic_stop("menu preview","unexpected PAL mini-mission switch");
                MEM8(0x17C558)=0;
                fprintf(stderr,"[MENU-PREVIEW] Opening mini-mission disabled using PAL switch 0017C558. Driving is unsupported; this is a startup bypass, not completed gameplay.\n");
            }
            const char *modern=getenv("NIGHTFIRE_MODERN_CONTROLS140");
            if(modern && !strcmp(modern,"1"))
                fprintf(stderr,"[INPUT140] Modern keys: WASD move, mouse look, LMB fire, RMB/T aim, E/R contextual use/reload, Space jump, Ctrl/C crouch, Q next weapon, G next gadget, F alternate fire, Tab Back/objectives. Menus: arrows, Z accept, X back, Enter Start/pause. Original pad layout preserved.\n");
            else fprintf(stderr,"[INPUT] PC/controller port 0: Enter Start, arrows D-pad, Z A, X B; WASD movement, captured mouse look, LMB/RT fire, RMB/T/LT aim, E/R A, C/Ctrl X, Space Y. Game layout determines actions.\n");
        }
        g_eax=1;g_esp+=8;return 1;
    }
    if(va==0x158159 && a==PAD_TYPE) {
        if(!valid(b,4)||!valid(c,4)) nightfire_diagnostic_stop("input","invalid changes pointers");
        MEM32(b)=MEM32(c)=0;g_eax=0;g_esp+=16;return 1;
    }
    if(va==0x15800f && a==PAD_TYPE) {
        opened=(b==0 && c==0);g_eax=opened?PAD_HANDLE:0;g_esp+=20;return 1;
    }
    if(a!=PAD_HANDLE) return 0;
    if(va==0x158065) {opened=0;g_eax=0;g_esp+=8;return 1;}
    if(va==0x1580dd) {
        /* Rumble transport remains unimplemented; do not leave feedback pending. */
        if(valid(b,4)) MEM32(b)=ERROR_NOT_SUPPORTED;
        g_eax=ERROR_NOT_SUPPORTED;g_esp+=12;return 1;
    }
    if(va!=0x158071) return 0;
    if(!opened || !valid(b,22)) {g_eax=ERROR_DEVICE_NOT_CONNECTED;g_esp+=12;return 1;}
    int test=getenv("NIGHTFIRE_INPUT_TEST")!=NULL;
    unsigned char state[18]={0}; unsigned buttons=test?0:nightfire_window_buttons();
    XINPUT_STATE host={0};
    if(!test && XInputGetState(0,&host)==ERROR_SUCCESS) {
        buttons|=host.Gamepad.wButtons&255;
        for(unsigned i=0;i<4;i++) state[2+i]=(host.Gamepad.wButtons&(0x1000u<<i))?255:0;
        state[6]=(host.Gamepad.wButtons&0x100)?255:0;state[7]=(host.Gamepad.wButtons&0x200)?255:0;
        state[8]=host.Gamepad.bLeftTrigger;state[9]=host.Gamepad.bRightTrigger;
        memcpy(state+10,&host.Gamepad.sThumbLX,8);
    }
    /* Explicit diagnostic input, off by default, sent through the same API. */
    if(!test)nightfire_window_pc_input(state,&buttons);
    if(getenv("NIGHTFIRE_INPUT_TEST")) {
        const char *delay_text=getenv("NIGHTFIRE_INPUT_TEST_DELAY_MS");
        ULONGLONG delay=delay_text?strtoul(delay_text,NULL,10):0;
        ULONGLONG elapsed=GetTickCount64()-start;
        ULONGLONG ms=elapsed>=delay?elapsed-delay:0;
        if(getenv("NIGHTFIRE_INPUT_TEST_READY_START")) {
            static ULONGLONG ready;
            uint32_t page=MEM32(0x25F38C);
            if(!ready && page>=0x80000000u && page<0x83ffff00u &&
               MEM32(page+0x18)==0x40000009u && MEM32(page+0xc8)>=MEM32(page+0xcc)+2) {
                ready=GetTickCount64();
                fprintf(stderr,"[INPUT-READY] title page=%08X update=%u minimum=%u; pressing Start through XInput\n",page,MEM32(page+0xc8),MEM32(page+0xcc));
            }
            /* Wait for the game's own input gate; never modify menu state. */
            ms=ready ? 35000+GetTickCount64()-ready : 0;
        }
        /* A file-driven route owns its menu inputs. Legacy timed pulses would
         * otherwise race that route when startup/rendering gets faster. */
        static int file_only=-1;
        if(file_only<0)file_only=getenv("NIGHTFIRE_INPUT_TEST_FILE_ONLY")!=NULL;
        if(!file_only){
            if(ms>=35000 && ms<35500) buttons|=0x10;
            if(ms>=45000 && ms<45500) buttons|=2;
            if(ms>=50000 && ms<50500) buttons|=1;
        }
        const char *input_file=getenv("NIGHTFIRE_INPUT_TEST_FILE");
        if(input_file) {
            static ULONGLONG next_read; static unsigned injected;static unsigned char injected_state[18];
            ULONGLONG now=GetTickCount64();
            if(now>=next_read) {
                next_read=now+50; injected=0;memset(injected_state,0,18);
                FILE *f=fopen(input_file,"r");
                if(f) {char line[256];if(fgets(line,sizeof line,f))injected=nf_test_input_parse(line,injected_state);fclose(f);}
            }
            buttons|=injected&0xf00ffu;
            memcpy(state+8,injected_state+8,10);
        }
    }
    for(unsigned i=0;i<6;i++)if(buttons&(0x10000u<<i))state[2+i]=255;
    state[0]=buttons&255;state[1]=0;
    nightfire_flight116_pad(state,MEM32(0x1f65b0),MEM32(0x1f65b4));
    if(memcmp(previous,state,18)) {
        memcpy(previous,state,18);serial++;
        fprintf(stderr,"[INPUT] change=%u digital=%02X A=%u B=%u\n",serial,state[0],state[2],state[3]);
        if(test){int16_t axes[4];memcpy(axes,state+10,8);fprintf(stderr,"[INPUT-AXES] LX=%d LY=%d RX=%d RY=%d LT=%u RT=%u\n",axes[0],axes[1],axes[2],axes[3],state[8],state[9]);}
    }
    MEM32(b)=serial;memcpy((void *)(g_xbox_mem_offset+b+4),state,18);
    if(test){
        static uint64_t polls;static ULONGLONG next_report;polls++;
        ULONGLONG now=GetTickCount64();
        if(now>=next_report){next_report=now+5000;int16_t axes[4];memcpy(axes,state+10,8);
            fprintf(stderr,"[INPUT-POLL] calls=%llu packet=%u LX=%d LY=%d RX=%d RY=%d A=%u X=%u RT=%u\n",
                (unsigned long long)polls,serial,axes[0],axes[1],axes[2],axes[3],state[2],state[4],state[9]);}
    }
    g_eax=0;g_esp+=12;return 1;
}
