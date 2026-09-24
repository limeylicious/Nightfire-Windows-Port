/* Aggregate accepted material costs; wall times, not GPU execution times. */
#ifndef DRIVING_TIMING227_H
#define DRIVING_TIMING227_H
#include <windows.h>
#include <stdio.h>
static uint64_t driving_clock227(void){LARGE_INTEGER v;QueryPerformanceCounter(&v);return v.QuadPart;}
static void driving_timing227(const uint64_t times[6])
{
 static uint64_t sum[5];static unsigned calls;
 for(unsigned i=0;i<5;i++)sum[i]+=times[i+1]-times[i];
 if(++calls==1||!(calls%256)){
  LARGE_INTEGER hz;QueryPerformanceFrequency(&hz);double ms=1000.0/hz.QuadPart;
  fprintf(stderr,"[MATERIAL227] draws=%u resources_ms=%.1f vertices_ms=%.1f pack_ms=%.1f backend_ms=%.1f publish_ms=%.1f accepted-only wall-clock\n",calls,sum[0]*ms,sum[1]*ms,sum[2]*ms,sum[3]*ms,sum[4]*ms);
 }
}
#endif
