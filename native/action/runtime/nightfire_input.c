/* Native input at the verified PAL XAPI boundaries. Port zero is a keyboard
 * controller, merged with Windows XInput port zero. No game state is changed.
 *
 * NF_PADS=4 (off by default) adds game ports 1 to 3 from XInput controllers
 * 1 to 3, reports plug and unplug through XGetDevices/XGetDeviceChanges, and
 * gives each port its own handle and packet number. NF_KEYBOARD_PLAYER=own
 * makes the keyboard and mouse player 1 on their own and moves controllers
 * 0 to 2 to ports 1 to 3. NF_RUMBLE=1 forwards XInputSetState to Windows.
 *
 * NF_VIRTUAL_PADS=1..3 (with NF_PADS=4) fills empty ports 1-3 with virtual
 * controllers, so split-screen can be tried with only a keyboard. F6 moves the
 * keyboard and mouse to the next connected player, F5 back to player 1.
 *
 * With NF_PADS=4 the direct mouse (NIGHTFIRE_DIRECT_MOUSE122) turns whichever
 * player the keyboard and mouse drive, found by that player's controller
 * number (nightfire_direct_mouse122.h). If that player is not found while a
 * match is running, mouse look goes through the right stick instead. */
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
extern int nf_lockstep_on(void);
extern void nf_lockstep_pad(unsigned port,unsigned char state[18]);
extern void nightfire_window_input_begin(unsigned layout,int direct);
/* Port the keyboard and mouse drive (0 unless F6 moved them). */
static int focus_port;
static int direct_mouse_on(void) {
    static int enabled=-1;
    if(enabled<0) {
        const char *v=getenv("NIGHTFIRE_DIRECT_MOUSE122");
        /* NIGHTFIRE_LOCKSTEP: the mouse moves the right stick, so it is in the recorded presses. */
        enabled=v && !strcmp(v,"1") && !getenv("NIGHTFIRE_INPUT_TEST") && !nf_lockstep_on();
        if(enabled)fprintf(stderr,"[DIRECT-MOUSE122] relative mouse displacement enabled; controller processing retained\n");
    }
    return enabled;
}
/* Single-player direct mouse and the guided-rocket mouse: player 1 only. */
int nightfire_direct_mouse122_enabled(void) {
    return direct_mouse_on() && focus_port==0;
}
extern void nightfire_window_pc_input(unsigned char state[18],unsigned *buttons);
static void pad_focus_keys(void);
static int pad_ports(void);
/* NF_PADS=4: the port whose player the direct mouse turns, or -1 when the
 * single-player path applies. */
int nightfire_pads_mouse_port(void) {
    return pad_ports()>1 && direct_mouse_on() ? focus_port : -1;
}
/* Called at each match Player_Update: port -1 = a match is running, else the
 * keyboard's player was found there and can take the mouse. */
static unsigned input_frame,match_frame=-100u,found_frame=-100u;
static int found_port=-1;
void nightfire_pads_mouse_note(int port) {
    if(port<0)match_frame=input_frame;
    else {found_frame=input_frame;found_port=port;}
}
static int pads_mouse_direct(void) {
    static int last=-1;
    int running=input_frame-match_frame<=2;
    int found=found_port==focus_port && input_frame-found_frame<=30;
    int direct=direct_mouse_on() && (!running || found);
    if(running && direct!=last) {
        static unsigned lines;
        if(++lines<=40)fprintf(stderr,"[PADS] player %d mouse look: %s\n",focus_port+1,direct?"direct":"right stick");
        last=direct;
    }
    return direct;
}
void nightfire_input_begin_frame(void) {
    if(getenv("NIGHTFIRE_INPUT_TEST"))return;
    pad_focus_keys();
    input_frame++;
    /* Per-player control layout: 0x1FE6DE + player*0x158. */
    nightfire_window_input_begin(MEM16(0x1fe6de + focus_port*0x158),
        pad_ports()>1?pads_mouse_direct():nightfire_direct_mouse122_enabled());
}
#define PAD_TYPE 0x0015722cu
#define PAD_HANDLE 0x4e460001u
#define PORTS 4
static int opened[PORTS];
static uint32_t serial[PORTS];
static unsigned char previous[PORTS][18];
static ULONGLONG start;
static int valid(uint32_t p,unsigned n) {return p>=0x1000 && (uint64_t)p+n<=0x4000000;}

/* -1 until read; 1 = only port 0 (original behaviour), 4 = four ports. */
static int pad_ports(void) {
    static int ports=-1;
    if(ports<0) {
        const char *v=getenv("NF_PADS");
        ports=(v && !strcmp(v,"4") && !getenv("NIGHTFIRE_INPUT_TEST"))?PORTS:1;
        if(ports>1)fprintf(stderr,"[PADS] four controller ports: port 0 = keyboard/mouse%s, ports 1-3 = XInput %s\n",
            getenv("NF_KEYBOARD_PLAYER") && !strcmp(getenv("NF_KEYBOARD_PLAYER"),"own")?" only":" + XInput 0",
            getenv("NF_KEYBOARD_PLAYER") && !strcmp(getenv("NF_KEYBOARD_PLAYER"),"own")?"0-2":"1-3");
    }
    return ports;
}
static int keyboard_own(void) {
    static int own=-1;
    if(own<0){const char *v=getenv("NF_KEYBOARD_PLAYER");own=v && !strcmp(v,"own");}
    return own;
}
/* Windows controller index for a game port, or -1 for none. */
static int xinput_index(unsigned port) {
    if(port>=(unsigned)pad_ports()) return -1;
    if(keyboard_own()) return port==0 ? -1 : (int)port-1;
    return (int)port;
}
static int virtual_pads(void) {
    static int count=-1;
    if(count<0) {
        const char *v=getenv("NF_VIRTUAL_PADS");
        count=v?atoi(v):0;
        if(count<0)count=0;
        if(count>PORTS-1)count=PORTS-1;
        if(pad_ports()<2)count=0;
        if(count)fprintf(stderr,"[PADS] %d virtual controller(s) fill empty ports; F6 = keyboard/mouse to next player, F5 = back to player 1\n",count);
    }
    return count;
}
/* Port 0 always exists (keyboard); others while their controller is in, or
 * while a virtual controller fills them. Asking Windows about an empty
 * controller slot is slow, so the answer is refreshed at most every 250 ms. */
static uint32_t real_mask=1;   /* ports with a real controller (port 0 always) */
static uint32_t connected_mask(void) {
    static uint32_t mask=1; static ULONGLONG next;
    ULONGLONG now=GetTickCount64();
    if(now<next) return mask;
    next=now+250; mask=1; real_mask=1;
    int spare=virtual_pads();
    for(unsigned port=1;port<(unsigned)pad_ports();port++) {
        XINPUT_STATE probe;
        if(XInputGetState(xinput_index(port),&probe)==ERROR_SUCCESS) {mask|=1u<<port;real_mask|=1u<<port;}
        else if(spare>0) {spare--;mask|=1u<<port;}
    }
    return mask;
}
/* F5/F6 only while the game's window (or the one-window host) is in front. */
static int focus_key(int vk) {
    extern HWND nightfire_window_hwnd(void);
    HWND own=nightfire_window_hwnd(),front=GetForegroundWindow();
    DWORD pid=0;
    if(own && (GetWindowLongPtrA(own,GWL_STYLE)&WS_CHILD) && front==GetAncestor(own,GA_ROOT))pid=GetCurrentProcessId();
    else GetWindowThreadProcessId(front,&pid);
    return pid==GetCurrentProcessId() && (GetAsyncKeyState(vk)&0x8000);
}
static void pad_focus_keys(void) {
    if(pad_ports()<2)return;
    static int held5,held6;
    int d5=focus_key(VK_F5),d6=focus_key(VK_F6),next=focus_port;
    if(d6 && !held6) {
        uint32_t mask=connected_mask();
        for(int i=1;i<=PORTS;i++){int p=(focus_port+i)%PORTS;if(mask>>p&1){next=p;break;}}
    }
    if(d5 && !held5)next=0;
    held5=d5;held6=d6;
    if(next!=focus_port) {
        focus_port=next;
        fprintf(stderr,"[PADS] keyboard and mouse now control player %d\n",next+1);
    }
}
static uint32_t reported_mask=1;

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
        /* XGetDevices also restarts change tracking from the current set. */
        reported_mask=pad_ports()>1?connected_mask():1;
        g_eax=reported_mask;g_esp+=8;return 1;
    }
    if(va==0x158159 && a==PAD_TYPE) {
        if(!valid(b,4)||!valid(c,4)) nightfire_diagnostic_stop("input","invalid changes pointers");
        uint32_t now=pad_ports()>1?connected_mask():1;
        uint32_t in=now&~reported_mask, out=reported_mask&~now;
        if(in||out)fprintf(stderr,"[PADS] plugged=%X unplugged=%X now=%X\n",in,out,now);
        reported_mask=now;
        MEM32(b)=in;MEM32(c)=out;g_eax=(in||out)?1:0;g_esp+=16;return 1;
    }
    if(va==0x15800f && a==PAD_TYPE) {
        unsigned ok=(b<(unsigned)pad_ports() && c==0 && (b==0 || (connected_mask()>>b&1)));
        if(ok){opened[b]=1;serial[b]++;}
        g_eax=ok?PAD_HANDLE+b:0;g_esp+=20;return 1;
    }
    if(a<PAD_HANDLE || a>=PAD_HANDLE+PORTS) return 0;
    unsigned port=a-PAD_HANDLE;
    if(va==0x158065) {opened[port]=0;g_eax=0;g_esp+=8;return 1;}
    if(va==0x1580dd) {
        static int rumble=-1;
        if(rumble<0){const char *v=getenv("NF_RUMBLE");rumble=v && !strcmp(v,"1");}
        int index=xinput_index(port);
        if(!rumble || index<0 || !valid(b,70)) {
            /* Rumble off: do not leave feedback pending. */
            if(valid(b,4)) MEM32(b)=ERROR_NOT_SUPPORTED;
            g_eax=ERROR_NOT_SUPPORTED;g_esp+=12;return 1;
        }
        /* XINPUT_FEEDBACK: 66-byte header (status, event, reserved), then
         * the two motor speeds. Offset to be confirmed on the laptop from the
         * [RUMBLE] log before NF_RUMBLE goes in any player launcher. */
        uint16_t left=MEM16(b+66),right=MEM16(b+68);
        XINPUT_VIBRATION v={left,right};
        DWORD r=XInputSetState(index,&v);
        static unsigned logged;
        if(logged<8){logged++;fprintf(stderr,"[RUMBLE] port=%u left=%u right=%u result=%lu\n",port,left,right,(unsigned long)r);}
        MEM32(b)=r==ERROR_SUCCESS?ERROR_SUCCESS:ERROR_DEVICE_NOT_CONNECTED;
        g_eax=MEM32(b);g_esp+=12;return 1;
    }
    if(va!=0x158071) return 0;
    if(!opened[port] || !valid(b,22)) {g_eax=ERROR_DEVICE_NOT_CONNECTED;g_esp+=12;return 1;}
    int test=getenv("NIGHTFIRE_INPUT_TEST")!=NULL;
    unsigned char state[18]={0};
    unsigned buttons=(test||port!=(unsigned)focus_port)?0:nightfire_window_buttons();
    XINPUT_STATE host={0};
    int index=test?-1:xinput_index(port);
    /* A virtual controller has no Windows slot to ask (empty slots are slow). */
    if(port!=0 && !(connected_mask()&real_mask&(1u<<port)))index=-1;
    if(index>=0 && XInputGetState(index,&host)==ERROR_SUCCESS) {
        buttons|=host.Gamepad.wButtons&255;
        for(unsigned i=0;i<4;i++) state[2+i]=(host.Gamepad.wButtons&(0x1000u<<i))?255:0;
        state[6]=(host.Gamepad.wButtons&0x100)?255:0;state[7]=(host.Gamepad.wButtons&0x200)?255:0;
        state[8]=host.Gamepad.bLeftTrigger;state[9]=host.Gamepad.bRightTrigger;
        memcpy(state+10,&host.Gamepad.sThumbLX,8);
    } else if(port!=0 && !(connected_mask()>>port&1)) {
        /* Controller pulled out while open: the game sees the port go away.
         * A virtual controller stays connected with nothing pressed. */
        g_eax=ERROR_DEVICE_NOT_CONNECTED;g_esp+=12;return 1;
    }
    /* Keyboard and mouse go to the port they drive (F5/F6). */
    if(port==(unsigned)focus_port && !test)nightfire_window_pc_input(state,&buttons);
    /* Diagnostic input belongs to port 0 only. */
    if(port==0) {
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
    }
    for(unsigned i=0;i<6;i++)if(buttons&(0x10000u<<i))state[2+i]=255;
    state[0]=buttons&255;state[1]=0;
    nf_lockstep_pad(port,state);   /* NIGHTFIRE_LOCKSTEP: one state per input frame; record/replay */
    if(port==0)nightfire_flight116_pad(state,MEM32(0x1f65b0),MEM32(0x1f65b4));
    if(memcmp(previous[port],state,18)) {
        memcpy(previous[port],state,18);serial[port]++;
        if(pad_ports()>1){int16_t ax[4];memcpy(ax,state+10,8);
            fprintf(stderr,"[INPUT] port=%u change=%u digital=%02X A=%u B=%u LX=%d LY=%d RX=%d RY=%d focus=%d\n",port,serial[port],state[0],state[2],state[3],ax[0],ax[1],ax[2],ax[3],focus_port);}
        else fprintf(stderr,"[INPUT] port=%u change=%u digital=%02X A=%u B=%u\n",port,serial[port],state[0],state[2],state[3]);
        if(test){int16_t axes[4];memcpy(axes,state+10,8);fprintf(stderr,"[INPUT-AXES] LX=%d LY=%d RX=%d RY=%d LT=%u RT=%u\n",axes[0],axes[1],axes[2],axes[3],state[8],state[9]);}
    }
    MEM32(b)=serial[port];memcpy((void *)(g_xbox_mem_offset+b+4),state,18);
    if(test){
        static uint64_t polls;static ULONGLONG next_report;polls++;
        ULONGLONG now=GetTickCount64();
        if(now>=next_report){next_report=now+5000;int16_t axes[4];memcpy(axes,state+10,8);
            fprintf(stderr,"[INPUT-POLL] calls=%llu packet=%u LX=%d LY=%d RX=%d RY=%d A=%u X=%u RT=%u\n",
                (unsigned long long)polls,serial[port],axes[0],axes[1],axes[2],axes[3],state[2],state[4],state[9]);}
    }
    g_eax=0;g_esp+=12;return 1;
}
