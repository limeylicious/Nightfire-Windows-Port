/* Included after pair235/private248, where span and resource ownership exist. */
int nf_hw_color_seed276_split(const NFHardwareColorKey276 *key,uint64_t allocation,
 const uint8_t *source,uint8_t *storage,uint64_t *token){if(resident313_blocked()){return 0;}
#ifdef NF_PAIR276_AVAILABLE
 if(!nf_hw_color_seed276_enabled()||!allocation||!pair_span234(token,sizeof *token,1)||
  !pair_span234(key,sizeof *key,0)||!pair_span234(source,2u*NF_COLOR_BYTES276,0)||
  !pair_span234(storage,2u*NF_COLOR_BYTES276,1)||
  range179(storage,2u*NF_COLOR_BYTES276,source,2u*NF_COLOR_BYTES276)||
  range179(storage,2u*NF_COLOR_BYTES276,key,sizeof *key)||
  range179(storage,2u*NF_COLOR_BYTES276,token,sizeof *token)||
  range179(source,2u*NF_COLOR_BYTES276,token,sizeof *token)||range179(key,sizeof *key,token,sizeof *token)){
  nf_hw_color_seed276_revoke();return 0;
 }
 NFColorRecord276 *old=&color276_policy.certificate;
 unsigned eligible=!color276_policy.dead&&old->valid&&old->epoch==color276_policy.epoch&&old->storage==storage&&
  old->allocation==allocation&&!memcmp(&old->key,key,sizeof *key);
 color276_counts.split++;color276_counts.eligible_lanes+=eligible?2u:0u;
 *token=nf_color_split276(&color276_policy,key,allocation,source,storage);
 if(*token&&color276_policy.prepared.valid)color276_counts.equal_lanes+=color276_bits(color276_policy.prepared.equal);
 return 1;
#else
 (void)key;(void)allocation;(void)source;(void)storage;(void)token;nf_hw_color_seed276_revoke();return 0;
#endif
}
void nf_hw_color_seed276_publish(uint64_t token){if(resident313_blocked())return;
 if(nf_hw_color_seed276_enabled()&&nf_color_publish276(&color276_policy,token))color276_counts.published++;
}
