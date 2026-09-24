/*261: permission evidence for caller-owned spans, one synchronous private248
 * API only. No content cache and no borrowed VirtualQuery-region cache. Caller
 * already serializes the backend and keeps these allocations/protections stable
 * until return (pair235 contract). General entry points have no active scope. */
#ifndef NIGHTFIRE_HOST_SPANS261_H
#define NIGHTFIRE_HOST_SPANS261_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NF_HOST_CAPACITY261 256u
typedef struct NFHostSpan261 {uintptr_t begin,end;unsigned write;} NFHostSpan261;
typedef struct NFHostScope261 {
 struct NFHostScope261 *previous;
 NFHostSpan261 spans[NF_HOST_CAPACITY261];unsigned count;
 uint64_t requests,hits,queries,failures,uncached;
} NFHostScope261;
static NFHostScope261 *nf_host_current261;
static struct {uint64_t calls,success,refused,failed,requests,hits,queries,failures,uncached;unsigned peak;} nf_host_totals261;
static int nf_host_enabled261(void){static int setting=-1;if(setting<0){DWORD saved=GetLastError();const char*v=getenv("DRIVING_HOST_SPANS261");setting=v&&!strcmp(v,"1");SetLastError(saved);}return setting;}
#ifndef NF_HOST_QUERY261
#define NF_HOST_QUERY261(p,m,n) VirtualQuery(p,m,n)
#endif
static int nf_host_span261(const void *p,size_t n,int write){
 NFHostScope261 *s=nf_host_current261;if(!s)return 0;
 s->requests++;uintptr_t a=(uintptr_t)p;
 if(!a||!n||n>UINTPTR_MAX-a){s->failures++;return 0;}
 uintptr_t end=a+n;
 for(unsigned i=s->count;i;i--){const NFHostSpan261 *v=&s->spans[i-1];
  if(a>=v->begin&&end<=v->end&&(!write||v->write)){s->hits++;return 1;}
 }
 uintptr_t at=a;
 while(at<end){
  MEMORY_BASIC_INFORMATION m;s->queries++;
  if(!NF_HOST_QUERY261((const void*)at,&m,sizeof m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))goto refused;
  DWORD q=m.Protect&255;int rw=q==PAGE_READWRITE||q==PAGE_WRITECOPY||q==PAGE_EXECUTE_READWRITE||q==PAGE_EXECUTE_WRITECOPY;
  if(!rw&&(write||(q!=PAGE_READONLY&&q!=PAGE_EXECUTE_READ)))goto refused;
  uintptr_t b=(uintptr_t)m.BaseAddress;
  if(m.RegionSize>UINTPTR_MAX-b||b+m.RegionSize<=at)goto refused;
  at=b+m.RegionSize;
 }
 if(s->count<NF_HOST_CAPACITY261){NFHostSpan261 *v=&s->spans[s->count++];v->begin=a;v->end=end;v->write=!!write;}
 else s->uncached++;
 return 1;
refused:s->failures++;return 0;
}
static void nf_host_enter261(NFHostScope261 *s){memset(s,0,sizeof*s);s->previous=nf_host_current261;nf_host_current261=s;}
static void nf_host_leave261(NFHostScope261 *s,int result){
 /* Restore before reporting: a nested API never donates evidence to its caller. */
 nf_host_current261=s->previous;
 nf_host_totals261.calls++;nf_host_totals261.success+=result>0;nf_host_totals261.refused+=result==0;nf_host_totals261.failed+=result<0;
 nf_host_totals261.requests+=s->requests;nf_host_totals261.hits+=s->hits;nf_host_totals261.queries+=s->queries;
 nf_host_totals261.failures+=s->failures;nf_host_totals261.uncached+=s->uncached;
 if(s->count>nf_host_totals261.peak)nf_host_totals261.peak=s->count;
 if(nf_host_totals261.calls==1||!(nf_host_totals261.calls%512))fprintf(stderr,"[HOST-SPANS261] calls=%llu success=%llu refused=%llu failed=%llu requests=%llu cache_hits=%llu virtual_queries=%llu invalid_spans=%llu uncached_full=%llu peak_entries=%u call_scoped=1\n",
  (unsigned long long)nf_host_totals261.calls,(unsigned long long)nf_host_totals261.success,(unsigned long long)nf_host_totals261.refused,(unsigned long long)nf_host_totals261.failed,
  (unsigned long long)nf_host_totals261.requests,(unsigned long long)nf_host_totals261.hits,(unsigned long long)nf_host_totals261.queries,(unsigned long long)nf_host_totals261.failures,(unsigned long long)nf_host_totals261.uncached,nf_host_totals261.peak);
}
#endif
