/*406 Isolated conservative COLOR traffic rectangle, one synchronous pair.
 * Full-size resources/viewport/scissor and both original triangles unchanged.
 * Depth import/readback/packing untouched. No new authoritative interval.
 * Caller output outside this envelope remains its just-snapshotted input.
 * No private color content reuse may be inferred outside the envelope. */
#ifndef NIGHTFIRE_COLOR_RECT406_H
#define NIGHTFIRE_COLOR_RECT406_H
typedef struct NFColorRect406 {UINT left,top,right,bottom;} NFColorRect406;
static const NFColorRect406 *rect_current406;
static const NFHardwareState *rect_owner406;
static int rect_setting406=-1;
static uint64_t rect_calls406,rect_pixels406,rect_uploads406;
static int rect_enabled406(void){
 if(rect_setting406<0){DWORD e=GetLastError();int c=errno;const char*v=getenv("DRIVING_COLOR_RECT406");rect_setting406=v&&!strcmp(v,"1");errno=c;SetLastError(e);}return rect_setting406;
}
static int rect_plan_impl406(const NFHardwareState*s,const NFHardwareMaterialVertex221 *const v[2],unsigned n,NFColorRect406*r){
 if(!rect_enabled406()||color401_enabled()||n!=6||(pair_profile400!=25&&pair_profile400!=26))return 0;
 for(unsigned lane=0;lane<2;lane++){
  /* Entry occurs AFTER unchanged complete pair preflight. Positive unit W and
   * bounded screen coordinates exclude projective poles/ill-conditioned clip.
   * VS screen->clip->viewport error is far below one pixel for this domain.
   * Two-pixel expansion also encloses 8-bit subpixel snap and boundary pixels.
   * Homogeneous clipping with W==1 cannot expand the XY convex hull. */
  if(s[lane].width!=640||s[lane].height!=480||s[lane].left||s[lane].top||s[lane].right!=640||s[lane].bottom!=480)return 0;
  float x0=4096,y0=4096,x1=-4096,y1=-4096;
  for(unsigned i=0;i<n;i++){
   const float*p=v[lane][i].position;
   if(p[3]!=1||p[0]<-4096||p[0]>4096||p[1]<-4096||p[1]>4096)return 0;
   if(p[0]<x0)x0=p[0];if(p[0]>x1)x1=p[0];if(p[1]<y0)y0=p[1];if(p[1]>y1)y1=p[1];
  }
  int left=(int)floorf(x0)-2,top=(int)floorf(y0)-2,right=(int)ceilf(x1)+2,bottom=(int)ceilf(y1)+2;
  if(left<0)left=0;if(top<0)top=0;if(right>640)right=640;if(bottom>480)bottom=480;
  if(left>=right||top>=bottom)return 0; /* keep original empty/offscreen draw */
  r[lane].left=(UINT)left;r[lane].top=(UINT)top;r[lane].right=(UINT)right;r[lane].bottom=(UINT)bottom;
 }
 return 1;
}
static int rect_plan406(const NFHardwareState*s,const NFHardwareMaterialVertex221 *const v[2],unsigned n,NFColorRect406*r){
 fenv_t f;fegetenv(&f);unsigned m=_mm_getcsr();DWORD e=GetLastError();int c=errno;
 int okay=rect_plan_impl406(s,v,n,r);fesetenv(&f);_mm_setcsr(m);errno=c;SetLastError(e);return okay;
}
static int rect_upload406(const NFHardwareState*s){
 if(!rect_current406||s!=rect_owner406)return 0;
 const NFColorRect406*r=rect_current406;D3D11_BOX box={r->left,r->top,0,r->right,r->bottom,1};
 ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)color,0,&box,s->color+(size_t)r->top*s->pitch+r->left*4,s->pitch,0);
 rect_uploads406++;return 1;
}
/*407 use only the existing full imported depth; never skip a Map/copy.
 * Original235 proved q24->D32->q24 identity for nearest rounding. */
typedef struct {const NFHardwareState*state;ID3D11Texture2D*depth,*transfer;int imported;} NFDepthRectImport407;
static NFDepthRectImport407 *depthrect_import407;
static void depthrect_imported407(const NFHardwareState*s){
 NFDepthRectImport407*p=depthrect_import407;
 if(p&&p->state==s&&p->depth==depth&&p->transfer==depth_transfer)p->imported=1;
}
static int depthrect_setting407=-1;
static uint64_t depthrect_pixels407,depthrect_rows407,depthrect_fallback407;
static int depthrect_enabled407(void){if(depthrect_setting407<0){NBGuard334 g;nb334_save(&g);const char*v=getenv("DRIVING_DEPTH_RECT407");depthrect_setting407=v&&!strcmp(v,"1");nb334_restore(&g);}return depthrect_setting407;}
static int depthrect_control407(void){unsigned csr=_mm_getcsr();return (csr&_MM_ROUND_MASK)==_MM_ROUND_NEAREST&&(csr&_MM_MASK_MASK)==_MM_MASK_MASK&&(csr&_MM_EXCEPT_INEXACT)!=0;}
#endif
