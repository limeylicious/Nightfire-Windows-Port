#ifndef NIGHTFIRE_NATIVE_PAIR302_H
#define NIGHTFIRE_NATIVE_PAIR302_H
/* Separate native resources; pair235's ordinary D32 sets are never mutated. */
typedef struct {ID3D11Texture2D *texture,*transfer;ID3D11DepthStencilView *view;} NFNativeSet302;
static NFNativeSet302 native302_sets[2];
static void native302_release(NFNativeSet302*s){RELEASE(s->view);RELEASE(s->texture);RELEASE(s->transfer);}
static int native302_create(NFNativeSet302*s){
 if(s->texture&&s->transfer&&s->view)return 1;
 UINT support=0;
 if(ID3D11Device_GetFeatureLevel(dev)<D3D_FEATURE_LEVEL_11_0||
  FAILED(ID3D11Device_CheckFormatSupport(dev,DXGI_FORMAT_D24_UNORM_S8_UINT,&support))||
  !(support&D3D11_FORMAT_SUPPORT_DEPTH_STENCIL))return 0;
 D3D11_TEXTURE2D_DESC d={0};d.Width=640;d.Height=480;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
 d.Format=DXGI_FORMAT_R24G8_TYPELESS;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;
 if(FAILED(ID3D11Device_CreateTexture2D(dev,&d,NULL,&s->texture)))goto fail;
 D3D11_DEPTH_STENCIL_VIEW_DESC v={0};v.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;v.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
 if(FAILED(ID3D11Device_CreateDepthStencilView(dev,(ID3D11Resource*)s->texture,&v,&s->view)))goto fail;
 d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ|D3D11_CPU_ACCESS_WRITE;
 if(FAILED(ID3D11Device_CreateTexture2D(dev,&d,NULL,&s->transfer)))goto fail;
 return 1;
fail:native302_release(s);return 0;
}
static int native302_list(const NFHardwareBatchDraw248 *list,unsigned count){
 for(unsigned i=0;i<count;i++)for(unsigned lane=0;lane<2;lane++)if(!native302_state(&list[i].states[lane]))return 0;
 return 1;
}
static int native302_prepare(const NFHardwareBatchDraw248 *list,unsigned count){
 if(!native302_enabled())return 0;
 native302_counts.considered++;
 unsigned conflicts=(native302_current?1u:0u)|(quant256_enabled()?2u:0u)|(depth268_enabled()?4u:0u)|
  (depth269_enabled()?8u:0u)|(depth272_enabled()?16u:0u)|(nf_hw_depth_seed278_enabled()?32u:0u)|
  (fragment291_enabled()?64u:0u)|(readback294_enabled()?128u:0u)|(!native302_mode()?256u:0u);
 if(conflicts){native302_counts.config++;native302_counts.config_mask|=conflicts;native302_report();return 0;}
 if(!native302_list(list,count)){native302_counts.contract++;native302_report();return 0;}
 unsigned csr=_mm_getcsr();DWORD error=GetLastError();int created=native302_create(&native302_sets[0])&&native302_create(&native302_sets[1]);_mm_setcsr(csr);SetLastError(error);
 if(!created){native302_counts.resource++;native302_report();return 0;}
 native302_counts.eligible++;return 1;
}
#endif
