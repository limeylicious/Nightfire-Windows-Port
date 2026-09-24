/* Backend-owned exact split, independent of color276 enablement. */
int nf_hw_depth_seed278_split(const NFHardwareColorKey276 *key,uint64_t allocation,
 const uint8_t *source,uint8_t *storage,uint64_t *token){if(resident313_blocked()){return 0;}
#if defined(NF_PAIR276_AVAILABLE) && defined(NF_DEPTH_PING272_AVAILABLE)
 if(!nf_hw_depth_seed278_enabled()||!allocation||!pair_span234(token,sizeof *token,1)||
  !pair_span234(key,sizeof *key,0)||!pair_span234(source,2u*NF_COLOR_BYTES276,0)||
  !pair_span234(storage,2u*NF_COLOR_BYTES276,1)||
  range179(storage,2u*NF_COLOR_BYTES276,source,2u*NF_COLOR_BYTES276)||range179(storage,2u*NF_COLOR_BYTES276,key,sizeof *key)||
  range179(storage,2u*NF_COLOR_BYTES276,token,sizeof *token)||range179(source,2u*NF_COLOR_BYTES276,token,sizeof *token)||range179(key,sizeof *key,token,sizeof *token)){
  nf_hw_depth_seed278_revoke();return 0;
 }
 NFColorRecord276 *old=&depth278_policy.certificate;
 unsigned eligible=!depth278_policy.dead&&old->valid&&old->epoch==depth278_policy.epoch&&old->storage==storage&&old->allocation==allocation&&!memcmp(&old->key,key,sizeof *key);
 depth278_counts.splits++;depth278_counts.eligible+=eligible?2u:0u;
 memcpy(depth278_prepared,depth278_published,sizeof depth278_prepared);
 *token=nf_color_split276(&depth278_policy,key,allocation,source,storage);
 if(*token&&depth278_policy.prepared.valid)depth278_counts.equal+=color276_bits(depth278_policy.prepared.equal);
 return 1;
#else
 (void)key;(void)allocation;(void)source;(void)storage;(void)token;nf_hw_depth_seed278_revoke();return 0;
#endif
}
void nf_hw_depth_seed278_publish(uint64_t token){if(resident313_blocked())return;
#ifdef NF_DEPTH_PING272_AVAILABLE
 if(nf_hw_depth_seed278_enabled()&&nf_color_publish276(&depth278_policy,token)){
  memcpy(depth278_published,depth278_completed,sizeof depth278_published);depth278_counts.published++;
 }
#else
 (void)token;
#endif
}
