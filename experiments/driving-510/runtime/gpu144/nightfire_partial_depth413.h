/*413 Isolated private staging encoding. Requires411's ordinary, bounded
 * sidecar import. Outside the aligned raster envelope staging stores original
 * q24/stencil words, never GPU-consumed float data. No Map or guest write omitted. */
#ifndef NIGHTFIRE_PARTIAL_DEPTH413_H
#define NIGHTFIRE_PARTIAL_DEPTH413_H
#ifdef NIGHTFIRE_MATERIAL221
static int setting413=-1;
static uint64_t imports413,pixels413,late_groups413;
static int depthrect_plain407(void);
static int enabled413(void){if(setting413<0){NBGuard334 g;nb334_save(&g);const char*v=getenv("DRIVING_PARTIAL_DEPTH413");setting413=v&&!strcmp(v,"1");nb334_restore(&g);}return setting413;}
static int unpack413(const NFHardwareState*s,const D3D11_MAPPED_SUBRESOURCE*m){
 NFImport411*p=current411;
 if(!p||!enabled413()||!depthrect_control407()||!depthrect_plain407()||!transaction_plain411()||
    p->state!=s||p->original_depth!=depth||p->transfer!=depth_transfer||width!=640||height!=480||
    !p->rect||!p->side||!p->side->texture||!p->side->view||!p->shader||!m->pData||m->RowPitch<2560||s->depth_pitch!=2560)return 0;
 const NFColorRect406*r=p->rect;
 if(r->left>=r->right||r->top>=r->bottom||r->right>640||r->bottom>480)return 0;
 p->csr413=_mm_getcsr();p->left413=r->left&~3u;p->right413=(r->right+3u)&~3u;
 for(unsigned y=0;y<480;y++){
  uint32_t*dst=(uint32_t*)((char*)m->pData+(size_t)y*m->RowPitch);
  const uint32_t*src=(const uint32_t*)(s->depth+(size_t)y*2560);
  /* Retain the immutable owned input outside the only GPU copy rectangle. */
  if(y>=r->top&&y<r->bottom){
   memcpy(dst,src,p->left413*4);memcpy(dst+p->right413,src+p->right413,(640-p->right413)*4);
   nf_depth_unpack((float*)(dst+p->left413),src+p->left413,p->right413-p->left413);pixels413+=p->right413-p->left413;
  }else memcpy(dst,src,2560);
 }
 p->encoded413=1;imports413++;return 1;
}
static void pack_row413(const NFImport411*p,uint32_t*dst,const float*src,unsigned y){
 int row=y>=p->rect->top&&y<p->rect->bottom;
 if(depthrect_control407()){
  if(row)nf_depth_pack(dst+p->left413,src+p->left413,p->right413-p->left413);
  return;
 }
 /* Changed publication controls: recreate original imported float groups under
  * saved import controls, then restore current controls/flags before each pack.
  * Preserve original left-to-right four-wide packing, including sticky flags. */
 for(unsigned x=0;x<640;x+=4){
  if(row&&x>=p->left413&&x<p->right413)nf_depth_pack(dst+x,src+x,4);
  else{float decoded[4];unsigned publication=_mm_getcsr();_mm_setcsr(p->csr413);
   nf_depth_unpack(decoded,(const uint32_t*)(src+x),4);_mm_setcsr(publication);
   nf_depth_pack(dst+x,decoded,4);late_groups413++;
  }
 }
}
static void report413(void){
 static uint64_t reported;if(!imports413||imports413/2048==reported)return;reported=imports413/2048;
 NBGuard334 g;nb334_save(&g);fprintf(stderr,"[PARTIAL413] imports=%llu converted_pixels=%llu late_groups=%llu full_guest_publication=1\n",(unsigned long long)imports413,(unsigned long long)pixels413,(unsigned long long)late_groups413);nb334_restore(&g);
}
#else
static int unpack413(const NFHardwareState*s,const D3D11_MAPPED_SUBRESOURCE*m){(void)s;(void)m;return 0;}
#endif
#endif
