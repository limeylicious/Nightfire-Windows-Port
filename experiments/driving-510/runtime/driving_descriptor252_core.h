/* Fresh descriptor tuple only. No cache, no object lookup, no persistent state. */
#ifndef DRIVING_DESCRIPTOR252_CORE_H
#define DRIVING_DESCRIPTOR252_CORE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef int (*ReadWord252)(void*,uint32_t,uint32_t*);
typedef int (*ReadTuple252)(void*,uint32_t,uint32_t[3]);
static int driving_descriptor252(void *ctx,ReadWord252 read,ReadTuple252 bulk,uint32_t instance,uint32_t result[3]){
 uint32_t owned[3];
 if(!read||!result)return 0;
 /* Object143 always produces a16-byte aligned PRAM instance. Keep the generic
  * scalar fallback for any other address; only the qualified tuple is batched. */
 if(bulk&&instance>=0xfd700000u&&instance<=0xfd7ffff0u&&!(instance&15u)&&bulk(ctx,instance,owned)){
  memcpy(result,owned,sizeof owned);return 1;
 }
 if(!read(ctx,instance,&owned[0])||!read(ctx,instance+4,&owned[1])||!read(ctx,instance+8,&owned[2]))return 0;
 memcpy(result,owned,sizeof owned);return 1;
}
#endif
