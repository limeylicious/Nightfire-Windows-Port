/* Explicit development entry, not normal progression or a gameplay fix.
 * PAL BE080 requests the menu after boot initialization at 6AD2D..6AD98.
 * BDDF0 accepts (level, flag, skip_movie); its original loader still runs.
 */
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#include "../../nightfire_diagnostics.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void nightfire_dev_load_request(void) {
 static int enabled=-1,used;
 if(enabled<0){const char *e=getenv("NIGHTFIRE_DEV_START_EXCHANGE");
  const char *m=getenv("NIGHTFIRE_DEV_MISSION137");
  enabled=(e && !strcmp(e,"1")) || (m && *m);}
 if(!enabled || used)return;
 if(g_esp<0x1000 || g_esp>0x03fffff0)nightfire_diagnostic_stop("development start","invalid loader stack");
 if(MEM32(g_esp)!=0x000be09du)return;
 if(MEM32(g_esp+4)!=0x07000048u || MEM32(g_esp+8)!=0 || MEM32(g_esp+12)!=0)
  nightfire_diagnostic_stop("development start","unexpected initial PAL load request");
 used=1;
 const char *section=getenv("NIGHTFIRE_DEV_EXCHANGE_SECTION");
 if(section && strcmp(section,"1") && strcmp(section,"2") && strcmp(section,"3") && strcmp(section,"4"))
  nightfire_diagnostic_stop("development start","expected Exchange section 1, 2, 3 or 4");
 uint32_t level=section && !strcmp(section,"4")?0x07000008u:
                section && !strcmp(section,"3")?0x07000007u:
                section && !strcmp(section,"2")?0x07000006u:0x07000005u;
 const char *mission=getenv("NIGHTFIRE_DEV_MISSION137");
 if(mission && *mission){
  /* PAL normal mission-menu rows at17C580, stride24, level+12. Only these
   * seven entries use this action XBE. Hidden table parents and 0900xxxx
   * driving-XBE requests are intentionally not development entry options. */
  static const struct { const char *text; uint32_t level; } allowed[]={
   {"07000005",0x07000005u},{"07000001",0x07000001u},
   {"07000009",0x07000009u},{"0700000C",0x0700000cu},
   {"07000011",0x07000011u},{"07000014",0x07000014u},
   {"0700001B",0x0700001bu}};
  unsigned i;int found=0,menu_found=0;
  if(section && strcmp(section,"1"))
   nightfire_diagnostic_stop("development start","mission137 cannot select an Exchange subsection");
  for(i=0;i<sizeof(allowed)/sizeof(allowed[0]);i++)
   if(!strcmp(mission,allowed[i].text)){level=allowed[i].level;found=1;break;}
  if(!found)nightfire_diagnostic_stop("development start","mission137 requires an allowlisted PAL action mission ID");
  for(i=0;i<12;i++)if(MEM32(0x17c580u+i*24u+12u)==level)menu_found=1;
  if(!menu_found)nightfire_diagnostic_stop("development start","PAL mission menu does not match selected entry");
 }
 /* PAL mission table17E180 recognizes these sections (fourth row17E1F8).
  * Keep flag0 for a
  * fresh start: flag1 restores player/mission carryover from the prior area. */
 MEM32(g_esp+4)=level;
 MEM32(g_esp+12)=1;
 fprintf(stderr,"[DEV-START] %s level=%08X flag=0 skip_movie=1 after normal boot setup; fresh development shortcut, no prior-section carryover or normal progression proof. difficulty=%u layout=%u\n",mission && *mission?"PAL action mission137":"Exchange",level,MEM32(0x1f65a8),MEM16(0x1fe6de));
}
