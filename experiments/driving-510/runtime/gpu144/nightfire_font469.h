/*469 Default-OFF exact-font completion. Original imported/attached DSV and draw
 * remain. Only discarded private depth export is omitted. No guest ownership,
 * publication boundary, font selector, RAM depth or generic sync change. */
#ifndef NIGHTFIRE_FONT469_H
#define NIGHTFIRE_FONT469_H
#include <fenv.h>
#include <errno.h>
static volatile LONG font469_setting=-1;
static int font469_enabled(void){
 LONG v=InterlockedCompareExchange(&font469_setting,-1,-1);
 if(v<0){DWORD e=GetLastError();int c=errno;fenv_t f;fegetenv(&f);unsigned mx=_mm_getcsr();
  const char*s=getenv("DRIVING_FONT469");LONG want=s&&!strcmp(s,"1"),old=InterlockedCompareExchange(&font469_setting,want,-1);v=old<0?want:old;
  fesetenv(&f);_mm_setcsr(mx);errno=c;SetLastError(e);
 }return v!=0;
}
static int font469_gate(const NFHardwareState *s){
 if(!s||!pending||resident313_blocked()||initialized<=0||!ctx||!dev||!color||!depth||!color_read||!depth_transfer||!rtv||!dsv)return 0;
#ifdef RESIDENT313_H
 if(r313.phase)return 0;
#endif
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
 if(deferred128.valid)return 0;
#endif
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
 return 0; /*Keep diagnostic completion reporting entirely original.*/
#endif
 /*begin_impl copies the material descriptor into material_active221 and
  * redirects ONLY that pointer. Verify this actual copy, then normalize that
  * known pointer for full descriptor comparison. Never ignore changed content.*/
#ifdef NIGHTFIRE_MATERIAL221
 if(!s->material221||active.material221!=&material_active221||memcmp(s->material221,&material_active221,sizeof material_active221))return 0;
#else
 return 0;
#endif
 NFHardwareState expected469=*s;expected469.material221=active.material221;
 if(memcmp(&expected469,&active,sizeof active)||width!=640||height!=480||s->width!=640||s->height!=480||s->pitch!=2560||s->depth_pitch!=2560||s->left||s->top||s->right!=640||s->bottom!=480)return 0;
 if(!s->color||!s->depth||!s->material221||s->color_only||s->color_layout||s->depth_enable!=1||s->depth_func!=0x207||s->depth_write||s->direct28||s->direct90||s->experimental_resolve193||s->experimental_movie200)return 0;
 const uintptr_t a=(uintptr_t)s->color,b=(uintptr_t)s->depth;const size_t bytes=640u*480*4;
 if(a>UINTPTR_MAX-bytes||b>UINTPTR_MAX-bytes||(a<b+bytes&&b<a+bytes))return 0;
 return 1;
}
static uint64_t font469_completions,font469_declines;
#ifdef NF_FONT469_TEST
void nf_hw_font469_test_mode(unsigned on,unsigned fail){font469_setting=(LONG)on;font469_fail_color_map=fail;font469_color_maps=font469_depth_maps=0;font469_completions=font469_declines=0;}
void nf_hw_font469_test_counts(uint64_t out[4]){out[0]=font469_color_maps;out[1]=font469_depth_maps;out[2]=font469_completions;out[3]=font469_declines;}
#endif
int nf_hw_font_sync469(const NFHardwareState *s){
 if(!font469_enabled())return nf_hw_sync();
 if(!font469_gate(s)){
#ifdef NF_FONT469_TEST
  if(!font469_declines&&s)fprintf(stderr,"[FONT469-TEST] decline pending=%d init=%d blocked=%d equal=%d sizes=%u/%u/%u/%u pitch=%u/%u rect=%u,%u,%u,%u depth=%u/%x/%u\n",pending,initialized,resident313_blocked(),memcmp(s,&active,sizeof active),width,height,s->width,s->height,s->pitch,s->depth_pitch,s->left,s->top,s->right,s->bottom,s->depth_enable,s->depth_func,s->depth_write);
  if(!font469_declines&&s)for(size_t i=0;i<sizeof active;i++)if(((const unsigned char*)s)[i]!=((const unsigned char*)&active)[i])fprintf(stderr,"[FONT469-TEST] descriptor byte %zu expected=%u active=%u\n",i,((const unsigned char*)s)[i],((const unsigned char*)&active)[i]);
#endif
  font469_declines++;return nf_hw_sync();}
 uint64_t t=hw_clock();int ok=sync_impl_mode469(0,1);
 if(!ok)nf_gpu_time84_abandon(ctx);sync_ticks+=hw_clock()-t;
 if(ok&&++font469_completions==1){DWORD e=GetLastError();int c=errno;fprintf(stderr,"[FONT469] first color-only completion; original depth import/draw retained; private depth export omitted\n");errno=c;SetLastError(e);}
 return ok;
}
#endif
