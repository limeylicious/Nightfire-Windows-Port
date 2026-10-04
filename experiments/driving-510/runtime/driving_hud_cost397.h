/*397 Default-OFF observer. Existing completed-draw clocks only: no extra RAM
 * reads, rendering, allocations, queue admission or publication changes.
 * A GET interval is a measurement bin, NOT a compatible batching interval.
 * In particular, every current geometry BEGIN still flushes prior work.
 * Called only by the serialized command consumer / joined owned-tail worker. */
#ifndef DRIVING_HUD_COST397_H
#define DRIVING_HUD_COST397_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <windows.h>
#include <errno.h>
#include <fenv.h>
#include <xmmintrin.h>
typedef struct HudCost397 {
 uint64_t count[2],ticks[2][5],interval,reports,report_ticks;
 uint64_t total_count[2],total_ticks[2][5];
} HudCost397;
static HudCost397 hud_cost397;
static int hud_mode397=-1;
static int hud_enabled397(void)
{
 if(hud_mode397<0){
  DWORD error=GetLastError();int crt=errno;unsigned csr=_mm_getcsr();fenv_t env;fegetenv(&env);
  const char*v=getenv("DRIVING_HUD_COST397");hud_mode397=v&&!strcmp(v,"1");
  fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(error);
 }
 return hud_mode397;
}
static void hud_add397(unsigned profile,const uint64_t t[6])
{
 if((profile!=25&&profile!=26)||!hud_enabled397())return;
 unsigned k=profile-25;hud_cost397.count[k]++;hud_cost397.total_count[k]++;
 for(unsigned i=0;i<5;i++){
  uint64_t d=t[i+1]-t[i];hud_cost397.ticks[k][i]+=d;hud_cost397.total_ticks[k][i]+=d;
 }
}
static void hud_get397(uint32_t put,uint32_t get)
{
 if(!hud_enabled397())return;
 hud_cost397.interval++;
 if(!hud_cost397.count[0]&&!hud_cost397.count[1])return;
 DWORD error=GetLastError();int crt=errno;unsigned csr=_mm_getcsr();fenv_t env;fegetenv(&env);
 LARGE_INTEGER start={0},end={0},hz={0};
 int clock_ok=QueryPerformanceCounter(&start)&&QueryPerformanceFrequency(&hz)&&hz.QuadPart>0;
 if(!clock_ok){
  fprintf(stderr,"[HUDCOST397] timing-unavailable; interval discarded\n");
  memset(hud_cost397.count,0,sizeof hud_cost397.count);memset(hud_cost397.ticks,0,sizeof hud_cost397.ticks);
  fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(error);return;
 }
 for(unsigned k=0;k<2;k++)if(hud_cost397.count[k]){
  fprintf(stderr,"[HUDCOST397] get_interval=%llu profile=%u draws=%llu resources_ticks=%llu vertices_ticks=%llu pack_ticks=%llu backend_ticks=%llu publish_ticks=%llu total_draws=%llu qpc_hz=%lld previous_report_ticks=%llu PUT=%08X GET=%08X accepted_only=1 not_batch_eligibility=1\n",
   (unsigned long long)hud_cost397.interval,k+25,(unsigned long long)hud_cost397.count[k],
   (unsigned long long)hud_cost397.ticks[k][0],(unsigned long long)hud_cost397.ticks[k][1],
   (unsigned long long)hud_cost397.ticks[k][2],(unsigned long long)hud_cost397.ticks[k][3],
   (unsigned long long)hud_cost397.ticks[k][4],(unsigned long long)hud_cost397.total_count[k],
   (long long)hz.QuadPart,(unsigned long long)hud_cost397.report_ticks,put,get);
 }
 memset(hud_cost397.count,0,sizeof hud_cost397.count);memset(hud_cost397.ticks,0,sizeof hud_cost397.ticks);
 hud_cost397.reports++;
 if(QueryPerformanceCounter(&end)&&end.QuadPart>=start.QuadPart)hud_cost397.report_ticks+=end.QuadPart-start.QuadPart;
 fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(error);
}
#endif
