/*412 isolated readback wait strategy, default OFF. Completion/publication stay
 * synchronous. Only documented WAS_STILL_DRAWING is retried; every real error
 * propagates. Bounded polling always ends in the original blocking Map. */
#ifndef NIGHTFIRE_MAP_WAIT412_H
#define NIGHTFIRE_MAP_WAIT412_H
static int mode412=-1;static uint64_t hz412;
static uint64_t reads412,ready412,busy412,blocking412,errors412,polls412;
#ifndef NF_WAIT412_MAP
#define NF_WAIT412_MAP(c,r,flags,m) ID3D11DeviceContext_Map(c,r,0,D3D11_MAP_READ,flags,m)
#endif
#ifndef NF_WAIT412_FLUSH
#define NF_WAIT412_FLUSH(c) ID3D11DeviceContext_Flush(c)
#endif
#ifndef NF_WAIT412_CLOCK
static uint64_t clock412(void){NBGuard334 g;nb334_save(&g);LARGE_INTEGER t;uint64_t n=QueryPerformanceCounter(&t)&&t.QuadPart>0?(uint64_t)t.QuadPart:0;nb334_restore(&g);return n;}
#define NF_WAIT412_CLOCK() clock412()
#endif
static int enabled412(void){
 if(mode412<0){NBGuard334 g;nb334_save(&g);const char*v=getenv("DRIVING_MAP_WAIT412");mode412=v&&!strcmp(v,"1")?1:v&&!strcmp(v,"2")?2:v&&!strcmp(v,"3")?3:v&&!strcmp(v,"4")?4:0;
  LARGE_INTEGER f;if(mode412&&QueryPerformanceFrequency(&f)&&f.QuadPart>0)hz412=f.QuadPart;else mode412=0;nb334_restore(&g);
 }return mode412;
}
static HRESULT read_map412(ID3D11DeviceContext*c,ID3D11Resource*r,D3D11_MAPPED_SUBRESOURCE*m){
 if(!enabled412())return NF_WAIT412_MAP(c,r,0,m);
 NBGuard334 entry;nb334_save(&entry);uint64_t start=NF_WAIT412_CLOCK();
 if(!start||!hz412){nb334_restore(&entry);return NF_WAIT412_MAP(c,r,0,m);}
 /*1:100us,2:2ms,3:one attempt,4:one attempt+Flush. Cap API attempts. */
 uint64_t budget=mode412==2?hz412/500:hz412/10000;reads412++;
 for(unsigned attempt=0;attempt<128;attempt++){
  nb334_restore(&entry);D3D11_MAPPED_SUBRESOURCE result={0};polls412++;
  HRESULT hr=NF_WAIT412_MAP(c,r,D3D11_MAP_FLAG_DO_NOT_WAIT,&result);
  if(hr!=DXGI_ERROR_WAS_STILL_DRAWING){if(SUCCEEDED(hr)){*m=result;ready412++;}else errors412++;return hr;}
  busy412++;nb334_restore(&entry);
  if(!attempt&&mode412!=3){NF_WAIT412_FLUSH(c);nb334_restore(&entry);}
  uint64_t now=NF_WAIT412_CLOCK();
  if(mode412==3||mode412==4||!now||now<start||now-start>=budget)break;
  YieldProcessor();
 }
 nb334_restore(&entry);blocking412++;return NF_WAIT412_MAP(c,r,0,m);
}
#endif
