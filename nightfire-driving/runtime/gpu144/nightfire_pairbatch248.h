/* Entire owned flush only; no guest/drain ownership is implied by this API. */
#ifndef NIGHTFIRE_PAIRBATCH248_H
#define NIGHTFIRE_PAIRBATCH248_H
#include "nightfire_batch_limit260.h"
#if defined(NIGHTFIRE_PAIR_BATCH248) && defined(NIGHTFIRE_BATCH234) && defined(NIGHTFIRE_PAIR234) && defined(NIGHTFIRE_MATERIAL221) && !defined(NIGHTFIRE_GPU_FALLBACK96) && !defined(NIGHTFIRE_RESIDENT_MAIN130) && !defined(NIGHTFIRE_DEFER_MAIN128) && !defined(NIGHTFIRE_GPU_TIMING_DIAGNOSTIC) && !defined(NIGHTFIRE_DEPTH_OBSERVE_DIAGNOSTIC) && !defined(NIGHTFIRE_SURFACE_PROBE_DIAGNOSTIC) && !defined(NIGHTFIRE_COLOR_REUSE110) && !defined(NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC)
#include "nightfire_lane_flush275.h"
#include "nightfire_readback294.h"
#include "nightfire_native_pair302.h"
static uint64_t batch_calls248,batch_draws248,batch_refused248,batch_failed248;
/*329: schedule ordinary staging copies together, still before all original
 * blocking Maps. Only private, separately-created lane textures are retained,
 * on this stack and inside this call. No guest target residency is implied. */
static int late_copies329_enabled(void){
 static int on=-1;if(on<0){DWORD error=GetLastError();int crt=errno;
  const char*v=getenv("DRIVING_LATE_COPIES329");on=v&&!strcmp(v,"1");
  errno=crt;SetLastError(error);}return on;
}
static int batch_enabled248(void){static int setting=-1;if(setting<0){const char*v=getenv("DRIVING_PAIR_BATCH248");setting=v&&!strcmp(v,"1");}return setting;}
static int batch_preflight248(const NFHardwareBatchDraw248 *list,unsigned count){
 const size_t bytes=640u*480*4;
 if(!count||count>nf_batch_limit260()||!pair_span234(list,count*sizeof*list,0))return 0;
 const void *outputs[4]={list[0].states[0].color,list[0].states[0].depth,list[0].states[1].color,list[0].states[1].depth};
 for(unsigned j=0;j<4;j++)if(range179(outputs[j],bytes,list,count*sizeof*list))return 0;
 for(unsigned i=0;i<count;i++){
  for(unsigned lane=0;lane<2;lane++)if(list[i].states[lane].color!=outputs[2*lane]||list[i].states[lane].depth!=outputs[2*lane+1])return 0;
  if(!pair_preflight234(list[i].states,list[i].vertices,list[i].count))return 0;
 }
 return 1;
}
/* Existing full list preflight precedes this additional experimental gate. */
static int fragment291_list(const NFHardwareBatchDraw248 *list,unsigned count){
 for(unsigned i=0;i<count;i++)for(unsigned lane=0;lane<2;lane++){
  const NFHardwareState *s=&list[i].states[lane];
  if(s->material221->integer_depth291!=291||s->depth_enable!=1||s->depth_write>1||
     (s->depth_func!=0x203&&s->depth_func!=0x207))return 0;
  /* Ordinary complete vertex/clip preflight already ran. Original VS,
   * viewport0..1 and DepthClipEnable=TRUE handle mixed W and clipped Z. */
 }return 1;
}
static int batch_refusal248(unsigned w,unsigned h){prepared_width234=w;prepared_height234=h;batch_refused248++;return 0;}
static int nf_hw_material_batch_impl261(const NFHardwareBatchDraw248 *list,unsigned count,uint64_t token276,uint64_t token278){if(resident313_blocked()){return 0;}
 NFDepthSeedScope278 seed278={0};memcpy(seed278.previous,depth278_prepared,sizeof seed278.previous);
 seed278.lease=nf_color_take276(&depth278_policy,nf_hw_depth_seed278_enabled()?token278:0);seed278.epoch=depth278_policy.epoch;
 NFColorRecord276 lease276=nf_color_take276(&color276_policy,nf_hw_color_seed276_enabled()?token276:0);
 if(!batch_enabled248())return 0;
 const int reuse267=validation267_enabled();const unsigned control267=reuse267?validation267_control():UINT32_MAX;
 BT244_START(preflight_time248);int prepared248=batch_preflight248(list,count);BT244_END(preflight_time248,BT244_BATCH_PREFLIGHT248);
 if(!prepared248){batch_refused248++;return 0;}
 const unsigned status267=reuse267?validation267_status():0;
 VALIDATION267_HOOK(0);
 unsigned saved_prepared_w248=prepared_width234,saved_prepared_h248=prepared_height234;
 if(!nf_hw_batch234_prepare(640,480))return batch_refusal248(saved_prepared_w248,saved_prepared_h248);
 if(quant256_enabled()&&!quant256_prepare())return batch_refusal248(saved_prepared_w248,saved_prepared_h248);
 if(depth268_enabled()&&!depth268_prepare())return batch_refusal248(saved_prepared_w248,saved_prepared_h248);
 if(pair_failure234(64)||!pair_create234(&pair_surfaces234[0])||pair_failure234(65)||!pair_create234(&pair_surfaces234[1]))
  return batch_refusal248(saved_prepared_w248,saved_prepared_h248);
 const int reuse272=depth272_enabled();
 if(reuse272&&!depth272_prepare())return batch_refusal248(saved_prepared_w248,saved_prepared_h248);
 if((quant256_enabled()||reuse272)&&(!pair_surfaces234[0].depth_srv256||!pair_surfaces234[1].depth_srv256))return batch_refusal248(saved_prepared_w248,saved_prepared_h248);
 int query_failure248=pair_failure234(66);
 if(!pair_event234||query_failure248){D3D11_QUERY_DESC q={D3D11_QUERY_EVENT,0};if(query_failure248||FAILED(ID3D11Device_CreateQuery(dev,&q,&pair_event234)))return batch_refusal248(saved_prepared_w248,saved_prepared_h248);}
 const int use302=native302_prepare(list,count);
 const int use294=read_prepare294(reuse272);unsigned control294=_mm_getcsr()&~0x3fu;
 if(readback294_enabled()&&!use294)read_fallback294++;
 int use291=0;
 if(fragment291_enabled()){
  fragment291_counts.considered++;
  if(fragment291_current||!fragment291_options()||!fragment291_mode()||depth268_enabled()||depth269_enabled()||depth272_enabled()||nf_hw_depth_seed278_enabled())fragment291_counts.config++;
  else if(!fragment291_list(list,count))fragment291_counts.ineligible++;
  else{
   use291=1;
   for(unsigned i=0;i<count&&use291;i++)for(unsigned lane=0;lane<2;lane++)if(!material_prepare291(list[i].states[lane].material221)){use291=0;break;}
   if(!fragment291_mode())use291=0;
   if(use291)fragment291_counts.eligible++;else fragment291_counts.shader++;
  }
 }
 NFNativeScope302 scope302={0},*saved302=native302_current;scope302.control=native302_control();native302_current=use302?&scope302:NULL;
 NFFragmentScope291 scope291={0},*saved291=fragment291_current;scope291.control=validation267_control();fragment291_current=use291?&scope291:NULL;
 NFColorUpload276 upload276={{pair_surfaces234[0].color,pair_surfaces234[1].color},{list[0].states[0].color,list[0].states[1].color},0,0,color276_policy.epoch};
 upload276.allow=nf_color_allow276(&color276_policy,&lease276,upload276.resource[0],upload276.resource[1],list[0].states[0].color,list[0].states[1].color);
 if(lease276.epoch!=color276_policy.epoch)lease276.valid=0;
 NFColorUpload276 *saved_color276=color276_current;color276_current=nf_hw_color_seed276_enabled()?&upload276:NULL;
 seed278.output[0]=list[0].states[0].depth;seed278.output[1]=list[0].states[1].depth;
 if(seed278.lease.epoch!=depth278_policy.epoch||seed278.lease.storage!=seed278.output[0]||
  !seed278.lease.storage||seed278.lease.storage+NF_COLOR_BYTES276!=seed278.output[1])seed278.lease.valid=0;
 NFDepthSeedScope278 *saved278=depth278_current;depth278_current=nf_hw_depth_seed278_enabled()?&seed278:NULL;
 NFPairSurface234 saved={color,depth,color_read,depth_transfer,rtv,dsv};unsigned sw=width,sh=height;NFHardwareState sa=active;
 D3D11_MAPPED_SUBRESOURCE maps[4]={{0}};ID3D11Resource *resources[4]={0};unsigned mapped=0;int ok=0;
 const int late329=late_copies329_enabled()&&!use294&&!use302&&!use291&&!color276_current&&!depth278_current;
 ID3D11Resource *copy_sources329[4]={0};
 NFDepthClean269 proof269={0},*saved_proof269=depth269_current;
 const int reuse269=depth269_enabled()&&!reuse272;depth269_current=reuse269?&proof269:NULL;
 NFDepthScope272 scope272={0},*saved272=depth272_current;depth272_current=reuse272?&scope272:NULL;
 ID3D11Texture2D *saved_target268=depth268_target;
 ID3D11Texture2D *saved_target256=quant256_target;ID3D11ShaderResourceView *saved_srv256=quant256_selected_srv;
 gt254_begin(dev,ctx,batch_calls248,count);
 gt297_begin(dev,ctx,batch_calls248,count,(reuse272?1u:0u)|(depth278_current?2u:0u)|(use291?4u:0u)|(use294?8u:0u)|(use302?16u:0u));
 tx341_enter(list,count,reuse272&&!use302&&!use291&&!use294&&!late329&&!color276_current&&!depth278_current&&!reuse269&&!depth268_enabled()&&!quant256_enabled());
 if(!backing341_active)tx339_enter(list,count,reuse272&&!use302&&!use291&&!use294&&!late329&&!color276_current&&!depth278_current&&!reuse269&&!depth268_enabled()&&!quant256_enabled());
 for(unsigned lane=0;lane<2;lane++){
  NFPairSurface234 selected302=pair_surfaces234[lane];
  if(use302){selected302.depth=native302_sets[lane].texture;selected302.depth_transfer=native302_sets[lane].transfer;selected302.dsv=native302_sets[lane].view;
#ifdef NF_DEPTH_SRV256_AVAILABLE
   selected302.depth_srv256=NULL;
#endif
  }
  backing341_select(&selected302,lane);
  pair_select234(&selected302,640,480);
  if(use302){scope302.texture=depth;scope302.transfer=depth_transfer;scope302.view=dsv;scope302.imported=scope302.drawn=0;}
  if(use291){scope291.texture=depth;scope291.view=dsv;scope291.imported=scope291.drawn=0;}
  if(reuse269)depth269_lane(&proof269,depth);
  if(reuse272){memset(&scope272,0,sizeof scope272);scope272.private331=1;scope272.slots[0]=(NFDepthSlot272){depth,dsv,selected302.depth_srv256};scope272.slots[1]=depth272_alternate[lane];scope272.at=0;
   if(depth278_current)depth278_select(&seed278,lane,&scope272);
  }
  depth268_target=depth268_enabled()?depth:NULL;
  quant256_target=quant256_enabled()?depth:NULL;quant256_selected_srv=quant256_enabled()?(reuse272?scope272.slots[scope272.at].srv:pair_surfaces234[lane].depth_srv256):NULL;
  for(unsigned i=0;i<count;i++){
   const NFHardwareState *s=&list[i].states[lane];
   if(use302){scope302.expected=s;scope302.vertices=list[i].vertices[lane];scope302.count=list[i].count;}
   if(use291){scope291.expected=s;scope291.vertices=list[i].vertices[lane];scope291.count=list[i].count;}
   gt297_select(lane,i,list[i].count,s->material221->pixel.program,s->depth_enable|(s->depth_write<<1)|(s->alpha_enable<<2)|(s->blend_enable<<3),&s->material221->pixel,sizeof s->material221->pixel);
   gt254_select(lane,i,list[i].count,s->material221->pixel.program,s->depth_enable|(s->depth_write<<1)|(s->alpha_enable<<2)|(s->blend_enable<<3));
   if(i){gt254_boundary(ctx,0);gt297_op(ctx,GT297_BOUNDARY,0,0);if(!nf_hw_batch234_boundary(s))goto done;gt297_op(ctx,GT297_BOUNDARY,1,0);gt254_boundary(ctx,3);depth268_target=depth268_enabled()?depth:NULL;}
   if(!i){gt254_init(ctx,lane,0);gt297_op(ctx,GT297_INIT,0,0);}
   NFValidation267 proof267={s,s->material221,list[i].vertices[lane],list[i].count,control267,status267,0};
   if(reuse267){if(!nf_hw_begin_checked267(s,&proof267))goto done;proof267.begun=1;}
   else if(!nf_hw_begin(s))goto done;
   VALIDATION267_HOOK(1);
   if(!i){gt297_op(ctx,GT297_INIT,1,0);gt254_init(ctx,lane,1);}
   depth269_before_draw();
   depth331_before_draw(list[i].vertices[lane],list[i].count);
   if(!(reuse267?nf_hw_draw_material_checked267(list[i].vertices[lane],list[i].count,&proof267):nf_hw_draw_material221(list[i].vertices[lane],list[i].count))||pair_failure234(nf_batch_failure260(lane,i)))goto done;
   depth269_after_draw();
   depth331_after_draw();
  }
  if(pair_failure234(lane+1))goto done;
  if(depth278_current){
   if(!reuse272||scope272.at>1||depth!=scope272.slots[scope272.at].texture||dsv!=scope272.slots[scope272.at].dsv)goto done;
   seed278.final[lane]=scope272.slots[scope272.at];
  }
  ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);bound.targets=0;
  if(late329){
   /* depth may be the final lane-specific272 alternate, not the initial DSV. */
   copy_sources329[2*lane]=(ID3D11Resource*)color;
   copy_sources329[2*lane+1]=(ID3D11Resource*)depth;
  }else{
  gt254_copies(ctx,lane,0);gt297_op(ctx,GT297_FINAL,0,use294?294:0);
  if(use294){if((_mm_getcsr()&~0x3fu)!=control294||!read_dispatch294(lane,color,depth))goto done;}
  else{
  backing341_copy((ID3D11Resource*)color_read,(ID3D11Resource*)color);
  backing341_copy((ID3D11Resource*)depth_transfer,(ID3D11Resource*)depth);
  }
  gt297_op(ctx,GT297_FINAL,1,0);gt254_copies(ctx,lane,1);
  }
#ifdef NF_PAIR234_FLUSH_FIRST
  if(!lane&&!lane_flush275_omitted())ID3D11DeviceContext_Flush(ctx);
#elif defined(NF_PAIR234_TEST)
  if(!lane&&pair_flush_first234&&!lane_flush275_omitted())ID3D11DeviceContext_Flush(ctx);
#endif
  resources[2*lane]=(ID3D11Resource*)color_read;resources[2*lane+1]=(ID3D11Resource*)depth_transfer;
  pending=0;nf_hw_clear_pending=0;
 }
 if(late329)for(unsigned lane=0;lane<2;lane++){
  const NFHardwareState*s=&list[count-1].states[lane];
  gt297_select(lane,count-1,list[count-1].count,s->material221->pixel.program,s->depth_enable|(s->depth_write<<1)|(s->alpha_enable<<2)|(s->blend_enable<<3),&s->material221->pixel,sizeof s->material221->pixel);
  gt254_copies(ctx,lane,0);gt297_op(ctx,GT297_FINAL,0,329);
  ID3D11DeviceContext_CopyResource(ctx,resources[2*lane],copy_sources329[2*lane]);
  ID3D11DeviceContext_CopyResource(ctx,resources[2*lane+1],copy_sources329[2*lane+1]);
  gt297_op(ctx,GT297_FINAL,1,0);gt254_copies(ctx,lane,1);
 }
 if(use291&&!fragment291_identity())goto done;
 if(use302&&!native302_identity())goto done;
 gt254_end(ctx);gt297_end(ctx);
 /* The whole-result Map (or all four staging Maps) precedes caller output. */
 if(use294)resources[0]=(ID3D11Resource*)read_buffer294;
 for(unsigned i=0;i<(use294?1u:4u);i++){
  BT244_START(map_time248);gt297_cpu(2+2*i);
  if(pair_failure234(i+3)||FAILED(ID3D11DeviceContext_Map(ctx,resources[i],0,D3D11_MAP_READ,0,&maps[i])))goto done;
  gt297_cpu(3+2*i);BT244_END(map_time248,(i&1)?BT244_DEPTH_MAP:BT244_COLOR_MAP);mapped|=1u<<i;
 }
 if(use291&&!fragment291_identity())goto done;
 if(use302){if(!native302_identity())goto done;for(unsigned i=0;i<4;i++)if(!maps[i].pData||maps[i].RowPitch<2560)goto done;}
 if(use294&&(!read_mode294()||(_mm_getcsr()&~0x3fu)!=control294||pair_failure234(203)))goto done;
 for(unsigned lane=0;lane<2;lane++){
  BT244_START(copy_time248);
  if(use294)memcpy(list[0].states[lane].color,(const char*)maps[0].pData+lane*2*RB294_BYTES,RB294_BYTES);
  else for(unsigned y=0;y<480;y++)memcpy(list[0].states[lane].color+(size_t)y*2560,(const char*)maps[2*lane].pData+(size_t)y*maps[2*lane].RowPitch,2560);
  BT244_END(copy_time248,BT244_COLOR_COPY);BT244_START(pack_time248);
  if(use294){const uint32_t*src=(const uint32_t*)maps[0].pData+(lane*2+1)*RB294_PIXELS;uint32_t*dst=(uint32_t*)list[0].states[lane].depth;for(unsigned i=0;i<RB294_PIXELS;i++)dst[i]=src[i]|(dst[i]&255);}
  else if(use302)for(unsigned y=0;y<480;y++){uint32_t*dst=(uint32_t*)(list[0].states[lane].depth+(size_t)y*2560);const uint32_t*src=(const uint32_t*)((const char*)maps[2*lane+1].pData+(size_t)y*maps[2*lane+1].RowPitch);for(unsigned x=0;x<640;x++)dst[x]=(native302_from_dxgi(src[x])&0xffffff00u)|(dst[x]&255u);}
  else for(unsigned y=0;y<480;y++)nf_depth_pack((uint32_t*)(list[0].states[lane].depth+(size_t)y*2560),(const float*)((const char*)maps[2*lane+1].pData+(size_t)y*maps[2*lane+1].RowPitch),640);
  BT244_END(pack_time248,BT244_DEPTH_PACK);
 }
 gt297_cpu(10);
 if(use294){read_calls294++;read_report294();}
 transfers+=2;ok=1;batch_calls248++;batch_draws248+=count;
 if(batch_calls248==1||!(batch_calls248%120))fprintf(stderr,"[PAIR-BATCH248] batches=%llu original_draws=%llu refused=%llu failed=%llu\n",(unsigned long long)batch_calls248,(unsigned long long)batch_draws248,(unsigned long long)batch_refused248,(unsigned long long)batch_failed248);
done:
 tx341_leave(ok);
 tx339_leave(ok);
 gt254_finish(ctx,ok);gt297_finish(ctx,ok);
 for(unsigned i=0;i<4;i++)if(mapped&(1u<<i))ID3D11DeviceContext_Unmap(ctx,resources[i],0);
 if(!ok){
  if(use294)read_failed294++;
  batch_failed248++;ID3D11DeviceContext_End(ctx,(ID3D11Asynchronous*)pair_event234);ID3D11DeviceContext_Flush(ctx);
  ULONGLONG limit=GetTickCount64()+5000;HRESULT hr;
  while((hr=ID3D11DeviceContext_GetData(ctx,(ID3D11Asynchronous*)pair_event234,NULL,0,0))==S_FALSE&&GetTickCount64()<limit)Sleep(1);
  if(hr!=S_OK)pair_poisoned234=1;
 }
 if(use302&&!ok)native302_counts.failed++;native302_current=saved302;if(use302)native302_report();
 if(use291&&!ok)fragment291_counts.failed++;fragment291_current=saved291;if(fragment291_enabled())fragment291_report();
 nf_color_complete276(&depth278_policy,&seed278.lease,ok,seed278.final[0].texture,seed278.final[1].texture,seed278.output[0],seed278.output[1]);
 if(depth278_policy.completed.valid)memcpy(depth278_completed,seed278.final,sizeof depth278_completed);
 depth278_current=saved278;if(!ok)depth278_counts.failed++;depth278_report();
 depth272_current=saved272;if(reuse272){depth272_report();depth332_report();depth333_report();}
 depth269_current=saved_proof269;if(reuse269)depth269_report();
 depth268_target=saved_target268;
 quant256_target=saved_target256;quant256_selected_srv=saved_srv256;
 nf_color_complete276(&color276_policy,&lease276,ok,pair_surfaces234[0].color,pair_surfaces234[1].color,list[0].states[0].color,list[0].states[1].color);
 color276_current=saved_color276;if(!ok)color276_counts.failed++;color276_report();
 pair_select234(&saved,sw,sh);active=sa;gt254_report(ctx);gt297_report(ctx);if(reuse267)validation267_report();return ok?1:-1;
}
int nf_hw_material_batch_seeds278(const NFHardwareBatchDraw248 *list,unsigned count,uint64_t color_token,uint64_t depth_token){if(resident313_blocked())return 0;
 if(!nf_host_enabled261())return nf_hw_material_batch_impl261(list,count,color_token,depth_token);
 NFHostScope261 scope;nf_host_enter261(&scope);
 int result=nf_hw_material_batch_impl261(list,count,color_token,depth_token);
 nf_host_leave261(&scope,result);return result;
}
int nf_hw_material_batch_seed276(const NFHardwareBatchDraw248 *list,unsigned count,uint64_t token){return nf_hw_material_batch_seeds278(list,count,token,0);}
int nf_hw_material_batch248(const NFHardwareBatchDraw248 *list,unsigned count){return nf_hw_material_batch_seed276(list,count,0);}
#else
int nf_hw_material_batch_seeds278(const NFHardwareBatchDraw248 *list,unsigned count,uint64_t color_token,uint64_t depth_token){if(resident313_blocked())return 0;(void)list;(void)count;(void)color_token;(void)depth_token;nf_hw_color_seed276_revoke();nf_hw_depth_seed278_revoke();return 0;}
int nf_hw_material_batch_seed276(const NFHardwareBatchDraw248 *list,unsigned count,uint64_t token){return nf_hw_material_batch_seeds278(list,count,token,0);}
int nf_hw_material_batch248(const NFHardwareBatchDraw248 *list,unsigned count){return nf_hw_material_batch_seed276(list,count,0);}
#endif
#endif
