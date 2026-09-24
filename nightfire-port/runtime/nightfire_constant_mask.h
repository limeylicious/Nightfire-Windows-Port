/* Cache derived validity bits, comparing every source byte on each use. */
#ifndef NIGHTFIRE_CONSTANT_MASK_H
#define NIGHTFIRE_CONSTANT_MASK_H
#include "nightfire_bytes_equal.h"
typedef struct {uint32_t valid[192],known[8];unsigned ready;} NFConstantMaskCache;
static const uint32_t *nf_constant_mask(NFConstantMaskCache *cache,const uint32_t valid[192]){
 if(!cache->ready || !nf_bytes_equal(cache->valid,valid,sizeof cache->valid)){
  memcpy(cache->valid,valid,sizeof cache->valid);memset(cache->known,0,sizeof cache->known);
  for(unsigned i=0;i<192;i++)if(valid[i]==15)cache->known[i/32]|=1u<<(i%32);
  cache->ready=1;
 }
 return cache->known;
}
static int nf_constants_available(const uint32_t known[8],const uint32_t required[6]){
 for(unsigned i=0;i<6;i++)if((known[i]&required[i])!=required[i])return 0;
 return 1;
}
#endif
