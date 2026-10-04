#ifndef DRIVING_TEXT510_MATERIAL_H
#define DRIVING_TEXT510_MATERIAL_H
#include "driving_diag510.h"
#include "gpu144/nightfire_hardware.h"
#include <fenv.h>
#include <errno.h>
#include <xmmintrin.h>
#include <math.h>
/* Only already validated, caller-owned prepared vertices/material descriptors.
 * No target/texture bytes are read; font atlas is recorded separately after its original validation. */
static void text_material510(unsigned path,unsigned lane,uint32_t target,unsigned profile,unsigned rawfmt,unsigned rawtexture,
 const NFHardwareState*s,const NFHardwareMaterial221*m,const NFHardwareMaterialVertex221*v,unsigned n,uint64_t atlas,int glyphs){
 if(!driving_text510_on)return;
 DWORD error=GetLastError();int crt=errno;fenv_t env;fegetenv(&env);unsigned csr=_mm_getcsr();
 D510Draw d={0};d.path=path;d.lane=lane;d.target=target;d.count=n;d.profile=profile;d.raw_format=rawfmt;d.raw_texture=rawtexture;d.atlas_hash=atlas;
 d.bounds[0]=d.bounds[1]=INFINITY;d.bounds[2]=d.bounds[3]=-INFINITY;
 for(unsigned k=0;k<4;k++){d.minimum[k]=INFINITY;d.maximum[k]=-INFINITY;}
 for(unsigned i=0;i<n;i++){
  for(unsigned k=0;k<2;k++){float f=v[i].position[k];if(f<d.bounds[k])d.bounds[k]=f;if(f>d.bounds[k+2])d.bounds[k+2]=f;}
  for(unsigned k=0;k<4;k++){float c=v[i].color[k];if(c<d.minimum[k])d.minimum[k]=c;if(c>d.maximum[k])d.maximum[k]=c;d.mean[k]+=c;}
 }if(n)for(unsigned k=0;k<4;k++)d.mean[k]/=n;
 const unsigned st[20]={s->blend_enable,s->blend_src,s->blend_dst,s->alpha_enable,s->alpha_func,s->alpha_ref,s->depth_enable,s->depth_func,s->depth_write,s->color_write_mask212,s->cull_enable,s->cull_face,s->front_face,s->left,s->top,s->right,s->bottom,s->fog_color,s->filtered,s->combiner};memcpy(d.state,st,sizeof st);
 for(unsigned k=0;k<4;k++){d.texture_identity[k]=(uintptr_t)m->textures[k].identity228;d.texture_format[k]=m->textures[k].format;d.filter[k]=m->textures[k].filter;d.address[k]=m->textures[k].address;}
 int basic510=path==P510_GEOMETRY&&(profile==214||profile==216||profile==220);d.flags=basic510?1:0;
 driving_diag510_emit(D510_DRAW,&d,sizeof d);if(!basic510)driving_diag510_emit(D510_PIXEL,&m->pixel,sizeof m->pixel);
 if(glyphs)for(unsigned q=0;q+5<n;q+=6){D510Glyph g={0};g.lane=lane;g.index=q/6;g.target=target;g.atlas_hash=atlas;
  unsigned stage510=path==P510_SPRITE?3:0;g.reserved=stage510;
  static const unsigned corner[4]={0,1,2,5};for(unsigned k=0;k<4;k++){unsigned ci=k==3&&path==P510_SPRITE?4:corner[k];const NFHardwareMaterialVertex221*p=&v[q+ci];memcpy(g.position[k],p->position,16);memcpy(g.uv[k],p->uv[stage510],16);memcpy(g.color[k],p->color,16);}driving_diag510_emit(D510_GLYPH,&g,sizeof g);}
 fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(error);
}
static void text_basic510(unsigned lane,uint32_t target,unsigned profile,const NFHardwareState*s,const void*data,unsigned n,size_t stride,size_t color_offset,int packed){
 if(!driving_text510_on)return;if(n>16384){driving_diag510_emit(D510_UNKNOWN,&profile,4);return;}
 DWORD error=GetLastError();int crt=errno;fenv_t env;fegetenv(&env);unsigned csr=_mm_getcsr();
 static NFHardwareMaterialVertex221 converted[16384];NFHardwareMaterial221 m={0};
 m.textures[0].identity228=s->texture;m.textures[0].format=s->texture_format;m.textures[0].filter=s->texture_filter;m.textures[0].address=s->texture_address;
 for(unsigned i=0;i<n;i++){const unsigned char*p=(const unsigned char*)data+i*stride;memcpy(converted[i].position,p,16);
  if(packed){uint32_t c;memcpy(&c,p+color_offset,4);converted[i].color[0]=(float)((c>>16)&255)/255;converted[i].color[1]=(float)((c>>8)&255)/255;converted[i].color[2]=(float)(c&255)/255;converted[i].color[3]=(float)(c>>24)/255;}
  else memcpy(converted[i].color,p+color_offset,16);}
 text_material510(P510_GEOMETRY,lane,target,profile,s->texture_format,0,s,&m,converted,n,0,0);
 fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(error);
}
#endif
