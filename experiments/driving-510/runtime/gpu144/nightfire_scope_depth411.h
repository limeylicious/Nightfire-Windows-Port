/*411 ISOLATED candidate. One original ALWAYS-depth scope draw per AA lane.
 * Keep the original full staging WRITE/unpack and READ/pack. Replace only the
 * intervening DSV copies/draw with a rectangle-seeded R32 MRT depth sidecar.
 * No sidecar content survives as authority across a pair/publication boundary. */
#ifndef NIGHTFIRE_SCOPE_DEPTH411_H
#define NIGHTFIRE_SCOPE_DEPTH411_H
#ifdef NIGHTFIRE_MATERIAL221
typedef struct {ID3D11Texture2D*texture;ID3D11RenderTargetView*view;} NFSide411;
static NFSide411 side411[2];
static ID3D11BlendState*blend411;
static NFMaterialShader221 shaders411[64];static unsigned next_shader411;
static int setting411=-1;
static uint64_t attempted411,imported411,drawn411,pixels411,refused411;
typedef struct {const NFHardwareState*state;ID3D11Texture2D*original_depth,*transfer;const NFColorRect406*rect;NFSide411*side;ID3D11PixelShader*shader;int imported;int encoded413;unsigned csr413,left413,right413;} NFImport411;
static NFImport411*current411;
static int transaction_plain411(void);
#ifdef NF_PAIR234_TEST
static unsigned setup_fail411,setup_seen411;
static int fail_setup411(void){return setup_fail411&&++setup_seen411==setup_fail411;}
#else
static int fail_setup411(void){return 0;}
#endif
static int enabled411(void){if(setting411<0){const char*v=getenv("DRIVING_SCOPE_DEPTH411");setting411=v&&!strcmp(v,"1");}return setting411;}
static void release_side411(NFSide411*s){RELEASE(s->view);RELEASE(s->texture);}
static int create_side411(NFSide411*s){
 if(s->texture&&s->view)return 1;
 NFSide411 fresh={0};D3D11_TEXTURE2D_DESC d={0};d.Width=640;d.Height=480;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
 d.Format=DXGI_FORMAT_R32_TYPELESS;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_RENDER_TARGET;
 D3D11_RENDER_TARGET_VIEW_DESC rd={0};rd.Format=DXGI_FORMAT_R32_FLOAT;rd.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
 if(fail_setup411()||FAILED(ID3D11Device_CreateTexture2D(dev,&d,NULL,&fresh.texture))||fail_setup411()||FAILED(ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)fresh.texture,&rd,&fresh.view))){release_side411(&fresh);return 0;}
 release_side411(s);*s=fresh;return 1;
}
static ID3D11PixelShader*shader411(const NFHardwareMaterial221*m){
 for(unsigned i=0;i<64;i++)if(shaders411[i].shader&&!memcmp(&m->pixel,&shaders411[i].key,sizeof m->pixel))return shaders411[i].shader;
 char source[32768];size_t n=sizeof material_vertex221-1;memcpy(source,material_vertex221,n);
 const char rename[]="\n#define pixel original_color411\n";memcpy(source+n,rename,sizeof rename-1);n+=sizeof rename-1;
 if(!nf_pixel221_emit(&m->pixel,source+n,sizeof source-n))return NULL;
 const char suffix[]="\n#undef pixel\nstruct Result411{float4 color:SV_Target0;float depth:SV_Target1;};Result411 pixel(P p){Result411 o;o.color=original_color411(p);o.depth=clamp(p.p.z,0.0,1.0);return o;}";
 if(strlen(source)+sizeof suffix>sizeof source)return NULL;strcat(source,suffix);
 ID3DBlob*b=NULL,*errors=NULL;ID3D11PixelShader*result=NULL;
 HRESULT hr=fail_setup411()?E_FAIL:D3DCompile(source,strlen(source),"scope-depth411",NULL,NULL,"pixel","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&b,&errors);
 if(SUCCEEDED(hr))hr=fail_setup411()?E_FAIL:ID3D11Device_CreatePixelShader(dev,b->lpVtbl->GetBufferPointer(b),b->lpVtbl->GetBufferSize(b),NULL,&result);
 if(FAILED(hr)){fprintf(stderr,"[SCOPE411] optional shader refused %08lX %s\n",(unsigned long)hr,errors?(char*)errors->lpVtbl->GetBufferPointer(errors):"");RELEASE(result);}
 RELEASE(b);RELEASE(errors);if(!result)return NULL;
 NFMaterialShader221*entry=&shaders411[next_shader411++%64];RELEASE(entry->shader);entry->shader=result;entry->key=m->pixel;return result;
}
static int prepare_impl411(const NFHardwareState*s,const NFHardwareMaterialVertex221*const v[2],unsigned n,int rect,int plain,ID3D11PixelShader*ps[2]){
 if(!enabled411()||!rect||!plain||!transaction_plain411()||current411||n!=6||depthrect_enabled407())return 0;
 for(unsigned lane=0;lane<2;lane++){
  const NFHardwareState*a=&s[lane];if(a->color_only||a->color_layout||a->depth_enable!=1||a->depth_write!=1||a->depth_func!=0x207||a->color_write_mask212!=0x17||a->blend_enable!=1||a->blend_src!=0x302||a->blend_dst!=0x303)return 0;
  /* Constant depth avoids any interpolation precision ambiguity. Unit W and
   * XY bounds were established by the unchanged conservative406 plan. */
  const float z=v[lane][0].position[2];uint32_t bits;memcpy(&bits,&z,4);if(bits==0x80000000u||(z!=0&&(z<1||z>16777215.f)))return 0;
  for(unsigned i=0;i<n;i++)if(memcmp(&v[lane][i].position[2],&z,4))return 0;
 }
 attempted411++;
 if(!create_side411(&side411[0])||!create_side411(&side411[1]))goto refuse;
 if(!blend411){D3D11_BLEND_DESC b={0};b.IndependentBlendEnable=TRUE;
  for(unsigned i=0;i<8;i++){b.RenderTarget[i].SrcBlend=b.RenderTarget[i].SrcBlendAlpha=D3D11_BLEND_ONE;b.RenderTarget[i].DestBlend=b.RenderTarget[i].DestBlendAlpha=D3D11_BLEND_ZERO;b.RenderTarget[i].BlendOp=b.RenderTarget[i].BlendOpAlpha=D3D11_BLEND_OP_ADD;}
  D3D11_RENDER_TARGET_BLEND_DESC*t=&b.RenderTarget[0];t->BlendEnable=TRUE;t->SrcBlend=t->SrcBlendAlpha=D3D11_BLEND_SRC_ALPHA;t->DestBlend=t->DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;t->RenderTargetWriteMask=7;
  b.RenderTarget[1].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_RED;
  if(fail_setup411()||FAILED(ID3D11Device_CreateBlendState(dev,&b,&blend411)))goto refuse;
 }
 for(unsigned lane=0;lane<2;lane++){ps[lane]=shader411(s[lane].material221);if(!ps[lane])goto refuse;}
 return 1;
refuse:refused411++;return 0;
}
static int prepare411(const NFHardwareState*s,const NFHardwareMaterialVertex221*const v[2],unsigned n,int rect,int plain,ID3D11PixelShader*ps[2]){
 NBGuard334 guard;nb334_save(&guard);int ok=prepare_impl411(s,v,n,rect,plain,ps);nb334_restore(&guard);return ok;
}
static int import411(const NFHardwareState*s){
 NFImport411*p=current411;if(!p||p->state!=s||p->original_depth!=depth||p->transfer!=depth_transfer||width!=640||height!=480)return 0;
 /* Called ONLY after ordinary full WRITE/unpack/Unmap. A full-max DSV clear
  * never reaches this hook and must keep the original draw/copy path. */
 const NFColorRect406*r=p->rect;D3D11_BOX b={r->left,r->top,0,r->right,r->bottom,1};
 ID3D11DeviceContext_CopySubresourceRegion(ctx,(ID3D11Resource*)p->side->texture,0,r->left,r->top,0,(ID3D11Resource*)depth_transfer,0,&b);
 p->imported=1;imported411++;pixels411+=(uint64_t)(r->right-r->left)*(r->bottom-r->top);return 1;
}
static void bind411(const NFImport411*p){
 ID3D11RenderTargetView*views[2]={rtv,p->side->view};ID3D11DeviceContext_OMSetRenderTargets(ctx,2,views,NULL);
 ID3D11DeviceContext_OMSetBlendState(ctx,blend411,NULL,~0u);ID3D11DeviceContext_PSSetShader(ctx,p->shader,NULL,0);
 /* Normal cache cannot describe a two-target binding. No later begin occurs
  * before the draw; pair_select234 invalidates the entire cache afterward. */
 memset(&bound,0,sizeof bound);drawn411++;
#ifdef NF_SCOPE_BIND_TEST411
 NF_SCOPE_BIND_TEST411(p);
#endif
}
static void copyback411(const NFImport411*p){
 const NFColorRect406*r=p->rect;D3D11_BOX b={r->left,r->top,0,r->right,r->bottom,1};
 ID3D11DeviceContext_CopySubresourceRegion(ctx,(ID3D11Resource*)p->transfer,0,r->left,r->top,0,(ID3D11Resource*)p->side->texture,0,&b);
}
static void report411(void){
 if(drawn411!=2&&drawn411%2048)return;
 NBGuard334 guard;nb334_save(&guard);fprintf(stderr,"[SCOPE411] attempts=%llu imported_lanes=%llu drawn_lanes=%llu rect_pixels=%llu optional_refusals=%llu original_maps_and_publication=1\n",(unsigned long long)attempted411,(unsigned long long)imported411,(unsigned long long)drawn411,(unsigned long long)pixels411,(unsigned long long)refused411);nb334_restore(&guard);
}
#else
static int import411(const NFHardwareState*s){(void)s;return 0;}
#endif
#endif
