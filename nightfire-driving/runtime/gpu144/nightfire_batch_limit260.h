/* One immutable launch option shared by consumer and private248. No residency. */
#ifndef NIGHTFIRE_BATCH_LIMIT260_H
#define NIGHTFIRE_BATCH_LIMIT260_H
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
enum {NF_BATCH_MAX260=32};
static unsigned nf_batch_limit260(void){
 static unsigned limit;if(!limit){DWORD saved=GetLastError();const char*v=getenv("DRIVING_BATCH32_260");limit=v&&!strcmp(v,"1")?32:16;SetLastError(saved);}return limit;
}
/* Physical arrays reserve32 entries, but default16 keeps original accounting. */
static size_t nf_batch_metadata260(size_t maximum,size_t packet){return maximum-(NF_BATCH_MAX260-nf_batch_limit260())*packet;}
static unsigned nf_batch_capacity260(unsigned count,size_t bytes,size_t incoming,size_t maximum){
 return (count==nf_batch_limit260()?1u:0u)|(bytes+incoming>maximum?2u:0u);
}
/* Preserve original16 per-lane IDs16..47 and creation IDs64..66. */
static unsigned nf_batch_failure260(unsigned lane,unsigned draw){return draw<16?16+lane*16+draw:128+lane*16+(draw-16);}
#endif
