#ifndef DRIVING_CONTIG_REUSE237_H
#define DRIVING_CONTIG_REUSE237_H
#include <stdint.h>
#include <string.h>
#define DC237_BASE 0x80000000u
#define DC237_PAGE 4096u
#define DC237_BYTES (64u*1024u*1024u-128u*1024u)
#define DC237_PAGES (DC237_BYTES/DC237_PAGE)
typedef struct DC237 {
 uint32_t highwater, size[DC237_PAGES];
 unsigned char occupied[DC237_PAGES], ever[DC237_PAGES];
 uint64_t allocs,frees,reused,failed,live,peak,freed,interior,unknown,duplicate;
 /*311: lifetime identities only; never permission/content/ownership leases.
  * Token exhaustion disables observations without changing allocation behavior. */
 uint64_t generation311[DC237_PAGES],serial311;
 unsigned exhausted311;
} DC237;
/* Caller serializes state and zeros successful host backing before exposing it.
 * Unchanged bump-first address choice; only exhausted fresh space reuses pages. */
static int dc237_span(uint32_t address,uint32_t bytes,unsigned *first,unsigned *pages)
{
 if(!bytes || address<DC237_BASE || (address&(DC237_PAGE-1)))return 0;
 uint64_t offset=(uint64_t)address-DC237_BASE,rounded=((uint64_t)bytes+4095)&~4095ull;
 if(offset+rounded>DC237_BYTES)return 0;
 *first=(unsigned)(offset/4096);*pages=(unsigned)(rounded/4096);return 1;
}
static int dc237_available(const DC237 *s,unsigned first,unsigned pages)
{for(unsigned i=first;i<first+pages;++i)if(s->occupied[i])return 0;return 1;}
static void dc237_mark(DC237 *s,unsigned first,unsigned pages,uint32_t bytes)
{
 memset(s->occupied+first,1,pages);s->size[first]=bytes;s->ever[first]=1;
 if(!s->exhausted311){
  if(s->serial311==UINT64_MAX)s->exhausted311=1;
  else s->generation311[first]=++s->serial311;
 }
 if(s->exhausted311)s->generation311[first]=0;
 ++s->allocs;s->live+=bytes;if(s->live>s->peak)s->peak=s->live;
}
/* Legacy forced-address requests are reserved in the same ledger. They do not
 * advance the highwater, matching the old bridge, but block future reuse. */
static int dc237_reserve(DC237 *s,uint32_t address,uint32_t bytes)
{
 unsigned first,pages;if(!dc237_span(address,bytes,&first,&pages)||!dc237_available(s,first,pages)){++s->failed;return 0;}
 dc237_mark(s,first,pages,bytes);return 1;
}
static uint32_t dc237_allocate(DC237 *s,uint32_t bytes,uint32_t alignment)
{
 if(alignment<4096)alignment=4096;
 if(!bytes || (alignment&(alignment-1)) || bytes>DC237_BYTES){++s->failed;return 0;}
 uint64_t aligned=((uint64_t)DC237_BASE+s->highwater+alignment-1)&~((uint64_t)alignment-1);
 unsigned first,pages;
 if(aligned<=UINT32_MAX && dc237_span((uint32_t)aligned,bytes,&first,&pages) && dc237_available(s,first,pages)){
  dc237_mark(s,first,pages,bytes);s->highwater=(uint32_t)(aligned-DC237_BASE)+bytes;return (uint32_t)aligned;
 }
 /* Scan fresh tail as well as freed gaps, preserving forced reservations. */
 uint64_t at=((uint64_t)DC237_BASE+alignment-1)&~((uint64_t)alignment-1);
 for(;at<(uint64_t)DC237_BASE+DC237_BYTES;at+=alignment){
  if(!dc237_span((uint32_t)at,bytes,&first,&pages))break;
  if(!dc237_available(s,first,pages))continue;
  dc237_mark(s,first,pages,bytes);++s->reused;
  uint32_t end=(uint32_t)(at-DC237_BASE)+bytes;if(end>s->highwater)s->highwater=end;
  return (uint32_t)at;
 }
 ++s->failed;return 0;
}
/* Return1 exact free;0 unrecognized/null/interior/duplicate. Never changes data. */
static int dc237_free(DC237 *s,uint32_t address)
{
 ++s->frees;if(!address)return 0;
 if(address<DC237_BASE || (uint64_t)address>=DC237_BASE+(uint64_t)DC237_BYTES){++s->unknown;return 0;}
 unsigned page=(address-DC237_BASE)/4096;
 if((address&4095) || !s->size[page]){
  if(s->occupied[page])++s->interior;
  else if(!(address&4095)&&s->ever[page])++s->duplicate;
  else ++s->unknown;
  return 0;
 }
 uint32_t size=s->size[page];unsigned pages=(unsigned)(((uint64_t)size+4095)/4096);
 s->generation311[page]=0;
 memset(s->occupied+page,0,pages);s->size[page]=0;s->live-=size;s->freed+=size;return 1;
}
/* Caller holds the allocator lock for this snapshot. After return, free/reuse
 * may invalidate it immediately. No aliases, padding, multi-allocation spans,
 * committed-memory assumptions, page protections or CPU/GPU ownership implied. */
static int dc237_snapshot311(const DC237 *s,uint32_t address,uint32_t bytes,
                            uint32_t *base,uint32_t *size,uint64_t *generation)
{
 if(!base||!size||!generation)return 0;
 *base=*size=0;*generation=0;
 if(!bytes||s->exhausted311||address<DC237_BASE||
    (uint64_t)address+bytes>(uint64_t)DC237_BASE+DC237_BYTES)return 0;
 unsigned page=(address-DC237_BASE)/DC237_PAGE;
 if(!s->occupied[page])return 0;
 unsigned first=page;
 while(!s->size[first]){if(!first||!s->occupied[first-1])return 0;--first;}
 uint32_t start=DC237_BASE+first*DC237_PAGE;
 if(!s->generation311[first]||(uint64_t)address+bytes>(uint64_t)start+s->size[first])return 0;
 *base=start;*size=s->size[first];*generation=s->generation311[first];return 1;
}
#endif
