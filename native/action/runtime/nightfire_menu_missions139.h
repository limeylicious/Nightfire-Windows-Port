#ifndef NIGHTFIRE_MENU_MISSIONS139_H
#define NIGHTFIRE_MENU_MISSIONS139_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/* Read-only preview policy for the original mission menu. Never changes the
 * twelve progression bytes consumed by the original save-mask getter76240.
 *   NIGHTFIRE_MENU_MISSIONS139=1    tested preview: missions 1,5,6,7,11 only
 *   NIGHTFIRE_MENU_MISSIONS139=all  every mission open (alpha "All Missions", 2026-10-10)
 *   anything else                   the original progression from the player's saves */
static int nf_menu139_enabled(void) {
 static int enabled=-1;
 if(enabled<0){const char *e=getenv("NIGHTFIRE_MENU_MISSIONS139");
  enabled=!e?0:!strcmp(e,"1")?1:!strcmp(e,"all")?2:0;
  if(enabled==1)fprintf(stderr,"[MENU139] tested mission preview enabled; saved unlock bytes unchanged; allowed indices=1,5,6,7,11\n");
  else if(enabled==2)fprintf(stderr,"[MENU139] all missions open; saved unlock bytes unchanged\n");}
 return enabled;
}
static uint8_t nf_menu139_available(uint32_t row,uint8_t original) {
 static const uint32_t levels[12]={0x09000001,0x07000005,0x09000005,
  0x09000006,0x07000001,0x07000009,0x0700000c,0x07000011,
  0x09000002,0x09000003,0x07000014,0x0700001b};
 int mode=nf_menu139_enabled();
 if(!mode || row<0x17c580 || row>=0x17c6a0 || (row-0x17c580)%24)return original;
 uint32_t index=(row-0x17c580)/24;
 if(MEM32(row+12)!=levels[index])return mode==2?original:0;
 if(mode==2)return 1;
 return (uint8_t)((0x8e2u>>index)&1u);
}
/* Only called at the mission-specific acceptance site, before the original
 * global cheat branch; all other pages retain their original behavior. */
static int nf_menu139_accept(uint32_t index) {
 if(index>=12)return 0;
 return nf_menu139_available(0x17c580+24*index,0)!=0;
}
#endif
