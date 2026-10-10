/* Lean build: in-between frames (LEAN_INTERP=1, default off).
 *
 * The game still runs and renders at 50 Hz exactly as before. While it does,
 * every draw, clear and blit of a game frame is recorded (D3D objects with an
 * extra reference, vertex/index bytes, vertex-program and pixel constants).
 * A display thread then replays the newest frame N at the display rate into
 * its own copies of the render targets, with each draw's vertex-program
 * constants blended toward the matching draw of frame N-1:
 *     alpha = (now - time N was finished) / 20 ms, clamped to [0,1]
 * so the picture runs one game tick behind and moves smoothly from N-1 to N.
 * Draws are matched by shaders, textures, target, primitive and index count;
 * among equal keys, by the most identical constant rows (interp_match).
 * Unmatched draws use frame N unblended.
 *
 * The replay is recorded on a deferred context and executed with the
 * immediate context's state restored, so the game thread's rendering is not
 * disturbed; only ExecuteCommandList and Present run on the display thread.
 * The game's own surfaces are never written by the replay.
 *
 * Interface (portable to another renderer):
 *   interp_rec_draw / interp_rec_clear / interp_rec_blit  - record one op
 *   interp_end_frame(scanout texture)                       - frame finished
 *   interp_thread                                           - display loop
 * Switches: LEAN_INTERP=1, LEAN_INTERP_HZ (target rate without vsync, default
 * 120), LEAN_PRESENT_VSYNC=1 (pace to the monitor instead).
 * With NF_OVERLAY=1 the settings decide instead (lean_overlay.inc): Smooth motion
 * (when the launcher sets no LEAN_INTERP), and the frame rate, re-read every
 * picture: match the screen (vsync), a cap (timer) or uncapped. Display only:
 * the game's own update clock is never touched.
 */
#include <d3d11_4.h>
#include <math.h>

typedef struct IOp {
    uint8_t kind;                       /* 0 draw, 1 clear, 2 blit, 3 filtered copy between internal resolutions */
    ID3D11VertexShader *vs;ID3D11InputLayout *il;ID3D11PixelShader *ps;int sprite;
    ID3D11ShaderResourceView *srv[4];ID3D11SamplerState *smp[4];
    const uint32_t *crows;              /* constant rows the program reads (NULL: all) */
    const int *prows;                   /* the program's position-transform rows (4, or -1) */
    ID3D11Buffer *vbuf;                 /* vertex buffer: the renderer's ring or a cached one */
    ID3D11BlendState *bs;float bf[4];ID3D11DepthStencilState *dss;UINT sref;ID3D11RasterizerState *rs;
    UINT topo;ID3D11Texture2D *ct,*dt;UINT vw,vh;
    UINT stride,nverts,nidx;size_t voff,ioff,coff;
    uint64_t key;int match;
    /* clear */
    float col[4];float zv;UINT sv,flags,cmask;D3D11_RECT rect;int full;
    /* blit */
    ID3D11Texture2D *src;D3D11_BOX box;UINT dx,dy;
    /* filtered copy (rect = destination, uv = source rectangle 0..1) */
    ID3D11ShaderResourceView *ssrv;float uv[4];
} IOp;

#define IF_SLOTS 4
#define IV_FLOATS (192*4+12)
#define IP_FLOATS (96)
typedef struct IFrame {
    IOp *ops;unsigned nops,cap;
    uint8_t *mem;size_t used,memcap;
    ID3D11Texture2D *scanout;ID3D11ShaderResourceView *scanout_srv;uint64_t t,arrive;unsigned serial;
    volatile LONG inuse;
    unsigned matched_with;               /* serial of the frame the matches refer to */
    /* Render targets this frame sampled before (or without) drawing into them,
     * copied at that moment: the in-between frames read these copies, never the
     * game's live surface, which the game is already redrawing for the next
     * frame (an object textured from it vanished for a frame). */
    struct {ID3D11Resource *res;ID3D11Texture2D *tex;ID3D11ShaderResourceView *srv;} snap[16];unsigned nsnap;
} IFrame;
static IFrame ifr[IF_SLOTS];
static int if_rec=-1;static volatile LONG if_latest=-1;static unsigned if_serial;
static int interp_on=-1;static HANDLE interp_event;
static volatile int ihb_stage;static volatile uint64_t ihb_t;   /* display-thread heartbeat */
static LARGE_INTEGER interp_hz;
static struct {uint64_t frames,replays,draws,matched,snapped,rec_ticks,exec_ticks,rec_bytes,rec_ops,blend_ticks,match_ticks;double replay_ms;} ist;

static uint64_t iqpc(void){LARGE_INTEGER t;QueryPerformanceCounter(&t);return (uint64_t)t.QuadPart;}

/* Snapshot textures are reused: a frame hands its copies back here, and the
 * next frame that needs a surface of the same size and format takes one. */
typedef struct {ID3D11Texture2D *tex;ID3D11ShaderResourceView *srv;D3D11_TEXTURE2D_DESC td;D3D11_SHADER_RESOURCE_VIEW_DESC vd;} SnapPool;
static SnapPool snap_pool[32];static unsigned snap_pool_n;
static void snap_put(ID3D11Texture2D *t,ID3D11ShaderResourceView *v){
    if(snap_pool_n<32){SnapPool *e=&snap_pool[snap_pool_n++];e->tex=t;e->srv=v;ID3D11Texture2D_GetDesc(t,&e->td);ID3D11ShaderResourceView_GetDesc(v,&e->vd);return;}
    ID3D11ShaderResourceView_Release(v);ID3D11Texture2D_Release(t);
}
static int snap_take(const D3D11_TEXTURE2D_DESC *td,const D3D11_SHADER_RESOURCE_VIEW_DESC *vd,ID3D11Texture2D **t,ID3D11ShaderResourceView **v){
    for(unsigned i=0;i<snap_pool_n;i++){SnapPool *e=&snap_pool[i];
        if(!memcmp(&e->td,td,sizeof *td)&&!memcmp(&e->vd,vd,sizeof *vd)){*t=e->tex;*v=e->srv;snap_pool[i]=snap_pool[--snap_pool_n];return 1;}}
    return 0;
}
static void if_release(IFrame *f){
    /* ops hold raw pointers (kept alive by defer_release in lean_d3d.c) */
    if(f->scanout)ID3D11Texture2D_Release(f->scanout);if(f->scanout_srv)ID3D11ShaderResourceView_Release(f->scanout_srv);
    f->nops=0;f->used=0;f->scanout=NULL;f->scanout_srv=NULL;f->matched_with=0;
    for(unsigned i=0;i<f->nsnap;i++)if(f->snap[i].srv&&f->snap[i].tex)snap_put(f->snap[i].tex,f->snap[i].srv);
    f->nsnap=0;
}
static void *if_alloc(IFrame *f,size_t n){
    n=(n+15)&~(size_t)15;
    if(f->used+n>f->memcap){size_t nc=f->memcap?f->memcap*2:(8u<<20);while(nc<f->used+n)nc*=2;
        uint8_t *m=realloc(f->mem,nc);if(!m)return NULL;f->mem=m;f->memcap=nc;}
    void *p=f->mem+f->used;f->used+=n;return p;
}
static IOp *if_op(void){
    if(if_rec<0){
        int s=(int)((if_latest+1)%IF_SLOTS);if(s<0)s=0;
        for(int k=0;k<IF_SLOTS&&ifr[s].inuse;k++)s=(s+1)%IF_SLOTS;
        while(ifr[s].inuse)Sleep(0);
        if_rec=s;if_release(&ifr[s]);
    }
    IFrame *f=&ifr[if_rec];
    if(f->nops==f->cap){unsigned nc=f->cap?f->cap*2:2048;IOp *o=realloc(f->ops,nc*sizeof *o);if(!o)return NULL;f->ops=o;f->cap=nc;}
    IOp *o=&f->ops[f->nops++];memset(o,0,sizeof *o);return o;
}
static int interp_enabled(void){
    if(interp_on<0){const char *v=getenv("LEAN_INTERP");interp_on=v&&*v=='1';
        if(!(v&&*v)&&nfo_enabled()){nfo_load();interp_on=nfo_smooth!=0;}   /* NF_OVERLAY: the Smooth motion setting, read once */
        InterlockedExchange(&nfo_smooth_told,interp_on);
        if(interp_on)LOG("[LEAN-INTERP] in-between frames on (game logic keeps its own rate)\n");}
    return interp_on;
}

/* LEAN_INTERP_SNAPSHOT=0 turns the render-target copies off (old behaviour). */
static ID3D11ShaderResourceView *if_snapshot(IFrame *f,ID3D11ShaderResourceView *v){
    static int on=-1;if(on<0){const char *e=getenv("LEAN_INTERP_SNAPSHOT");on=!(e&&e[0]=='0');}
    if(!on||!v)return v;
    /* Most views are ordinary textures: remember those (cleared every 64 frames
     * in case a view address is reused for a render target). */
    static ID3D11ShaderResourceView *plain[512];static unsigned plain_serial;
    if(plain_serial!=if_serial/64){plain_serial=if_serial/64;memset(plain,0,sizeof plain);}
    unsigned ph=(unsigned)(((uintptr_t)v>>4)*2654435761u)&511;if(plain[ph]==v)return v;
    ID3D11Resource *res=NULL;ID3D11View_GetResource((ID3D11View*)v,&res);if(!res)return v;ID3D11Resource_Release(res);
    D3D11_RESOURCE_DIMENSION dim;ID3D11Resource_GetType(res,&dim);if(dim!=D3D11_RESOURCE_DIMENSION_TEXTURE2D)return v;
    D3D11_TEXTURE2D_DESC td;ID3D11Texture2D_GetDesc((ID3D11Texture2D*)res,&td);
    if(!(td.BindFlags&(D3D11_BIND_RENDER_TARGET|D3D11_BIND_DEPTH_STENCIL))){plain[ph]=v;return v;}   /* ordinary texture */
    for(unsigned i=0;i+1<f->nops;i++)if(f->ops[i].kind!=255&&((ID3D11Resource*)f->ops[i].ct==res||(ID3D11Resource*)f->ops[i].dt==res))return v;  /* drawn earlier this frame: the replay has it */
    for(unsigned i=0;i<f->nsnap;i++)if(f->snap[i].res==res)return f->snap[i].srv;
    if(f->nsnap==16)return v;
    ID3D11Texture2D *t=NULL;ID3D11ShaderResourceView *sv=NULL;D3D11_SHADER_RESOURCE_VIEW_DESC vd;ID3D11ShaderResourceView_GetDesc(v,&vd);
    if(!snap_take(&td,&vd,&t,&sv)){
        if(FAILED(ID3D11Device_CreateTexture2D(dev,&td,NULL,&t)))return v;
        if(FAILED(ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)t,&vd,&sv))){ID3D11Texture2D_Release(t);return v;}}
    ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)t,res);
    f->snap[f->nsnap].res=res;f->snap[f->nsnap].tex=t;f->snap[f->nsnap].srv=sv;f->nsnap++;
    return sv;
}

/* Recording, on the game's rendering thread. */
static void interp_rec_draw(ID3D11VertexShader *vs,ID3D11InputLayout *il,ID3D11PixelShader *ps,int sprite,
        ID3D11ShaderResourceView *const *srv,ID3D11SamplerState *const *smp,ID3D11BlendState *bs,const float *bf,
        ID3D11DepthStencilState *dss,UINT sref,ID3D11RasterizerState *rs,UINT topo,RT *color,RT *depth,UINT vw,UINT vh,
        UINT stride,UINT voff,UINT nverts,UINT ioff,UINT nidx,const float *vconst,const float *pconst){
    uint64_t rt0=iqpc();
    IOp *o=if_op();if(!o)return;IFrame *f=&ifr[if_rec];
    o->kind=0;o->crows=interp_cur_cmask;o->prows=interp_cur_prows;o->vs=vs;o->il=il;o->ps=ps;o->sprite=sprite;o->bs=bs;memcpy(o->bf,bf,16);o->dss=dss;o->sref=sref;o->rs=rs;o->topo=topo;
    for(unsigned k=0;k<4;k++){o->srv[k]=if_snapshot(f,srv[k]);o->smp[k]=smp[k];
        }
    o->ct=color?color->tex:NULL;o->dt=depth?depth->tex:NULL;
    o->vw=vw;o->vh=vh;o->stride=stride;o->nverts=nverts;o->nidx=nidx;
    /* geometry stays in the renderer's ring buffers (offsets recorded) */
    o->voff=voff;o->ioff=ioff;o->vbuf=interp_cur_vb;
    uint8_t *p=if_alloc(f,(IV_FLOATS+IP_FLOATS)*4);if(!p){o->kind=255;return;}o->coff=p-f->mem;memcpy(p,vconst,IV_FLOATS*4);memcpy(p+IV_FLOATS*4,pconst,IP_FLOATS*4);
    /* Key: shaders, the textures as bound (not this frame's snapshot copies, which
     * are new every frame) and the draw's size. The target is added when matching
     * (interp_key): the screen surface alternates between two buffers each frame. */
    uint64_t k=hash_words((const uint32_t*)&o->vs,sizeof(void*)/4,1);
    k=hash_bytes(&o->ps,sizeof o->ps,k);k=hash_bytes(srv,sizeof o->srv,k);
    k=hash_words((uint32_t[]){topo,nidx,(uint32_t)sprite},3,k);o->key=k;
    ist.rec_ticks+=iqpc()-rt0;
}
static void interp_rec_clear(RT *c,RT *z,UINT flags,const float *col,float zv,UINT sv,UINT cmask,int full,D3D11_RECT r,UINT vw,UINT vh){
    IOp *o=if_op();if(!o)return;
    o->kind=1;o->ct=c?c->tex:NULL;o->dt=z?z->tex:NULL;
    o->flags=flags;memcpy(o->col,col,16);o->zv=zv;o->sv=sv;o->cmask=cmask;o->full=full;o->rect=r;o->vw=vw;o->vh=vh;
}
static void interp_rec_blit(RT *s,RT *d,D3D11_BOX box,UINT dx,UINT dy){
    IOp *o=if_op();if(!o)return;
    o->kind=2;o->src=s->tex;o->ct=d->tex;o->box=box;o->dx=dx;o->dy=dy;
}
static void interp_rec_stretch(RT *s,RT *d,D3D11_RECT r,float u0,float v0,float u1,float v1){
    IOp *o=if_op();if(!o)return;
    o->kind=3;o->src=s->tex;o->ssrv=s->srv;o->ct=d->tex;o->rect=r;o->uv[0]=u0;o->uv[1]=v0;o->uv[2]=u1;o->uv[3]=v1;
}
static DWORD WINAPI interp_thread(void *unused);
/* The game finished a frame whose scanout surface is tex. */
static void interp_end_frame(ID3D11Texture2D *tex,ID3D11ShaderResourceView *srv){
    static int started;
    if(if_rec<0){if_op();ifr[if_rec].nops=0;}        /* a frame with no draws */
    IFrame *f=&ifr[if_rec];
    f->scanout=tex;if(tex)ID3D11Texture2D_AddRef(tex);f->scanout_srv=srv;if(srv)ID3D11ShaderResourceView_AddRef(srv);
    /* Stamp with the game's own clock: the 50 Hz vblank this frame belongs
     * to (kernel_bridge.c), not when rendering happened to finish. */
    {extern volatile long long lean_vblank_due;long long v=lean_vblank_due;f->arrive=iqpc();f->t=v?(uint64_t)v:f->arrive;}
    {   /* LEAN_INTERP_GAMECLOCK=1: stamp with the game's own step timer instead (counter
         * 0x1E5204, one tick per 1000/rate ms, rate at 0x242424: 62.5 Hz in pal60, 50 in pal),
         * so the span between frames is exactly the game time they cover, 2-step frames
         * included, and a 60 Hz vblank cannot beat against the 62.5 Hz steps. */
        static int gc=-1;if(gc<0){const char *e=getenv("LEAN_INTERP_GAMECLOCK");gc=e&&e[0]=='1';if(gc)LOG("[LEAN-INTERP] frames stamped by the game's step timer\n");}
        if(gc){const uint8_t *c=lean_guest(0x1E5204u,4),*r=lean_guest(0x242424u,4);
            if(!interp_hz.QuadPart)QueryPerformanceFrequency(&interp_hz);
            uint32_t ticks=c?*(const uint32_t *)c:0,rate=r?*(const uint32_t *)r:0;
            if(ticks&&rate>=25&&rate<=120)f->t=(uint64_t)ticks*(uint64_t)(interp_hz.QuadPart*(1000/rate)/1000);}
    }
    f->serial=++if_serial;
    {static uint64_t lastrep;uint64_t nq=iqpc();if(ihb_t&&interp_hz.QuadPart&&nq-ihb_t>(uint64_t)interp_hz.QuadPart*2&&nq-lastrep>(uint64_t)interp_hz.QuadPart*5){
        lastrep=nq;LOG("[LEAN-INTERP] display thread stalled %.1f s at stage %d (inuse %ld %ld %ld %ld)\n",(double)(nq-ihb_t)/interp_hz.QuadPart,ihb_stage,ifr[0].inuse,ifr[1].inuse,ifr[2].inuse,ifr[3].inuse);}}
    {static uint64_t lastt;static unsigned n;if(!interp_hz.QuadPart)QueryPerformanceFrequency(&interp_hz);if(n>=1200&&n<1212){LOG("[LEAN-INTERP] stamp serial=%u dt=%.2f ms now-stamp=%.2f ms\n",if_serial+0,lastt?(double)(f->t-lastt)*1000.0/interp_hz.QuadPart:0.0,(double)(iqpc()-f->t)*1000.0/interp_hz.QuadPart);}n++;lastt=f->t;}
    ist.rec_bytes+=f->used;ist.rec_ops+=f->nops;
    InterlockedExchange(&if_latest,if_rec);if_rec=-1;ist.frames++;
    grave_on=1;grave_tick();
    if(!started){started=1;interp_event=CreateEventA(NULL,FALSE,FALSE,NULL);
        /* the immediate context is shared under lean_ctx_lock (lean_d3d.c) */
        HANDLE h=CreateThread(NULL,0,interp_thread,NULL,0,NULL);if(h){SetThreadPriority(h,THREAD_PRIORITY_ABOVE_NORMAL);CloseHandle(h);}}
    if(interp_event)SetEvent(interp_event);
}

/* ------------------------------------------------------------- replay */
typedef struct Shadow {ID3D11Texture2D *game,*tex;ID3D11RenderTargetView *rtv;ID3D11DepthStencilView *dsv;ID3D11ShaderResourceView *srv;int written;unsigned used;} Shadow;
#define MAX_SHADOW 64
static Shadow shadows[MAX_SHADOW];static unsigned shadow_replays;
static Shadow *shadow_for(ID3D11Texture2D *game,int create){
    for(unsigned i=0;i<MAX_SHADOW;i++)if(shadows[i].game==game){shadows[i].used=shadow_replays;return &shadows[i];}
    if(!create)return NULL;
    unsigned s=MAX_SHADOW;for(unsigned i=0;i<MAX_SHADOW;i++)if(!shadows[i].game){s=i;break;}
    if(s==MAX_SHADOW){   /* recycle: drop one not used this replay */
        for(unsigned i=0;i<MAX_SHADOW;i++)if(!shadows[i].written){s=i;break;}
        if(s==MAX_SHADOW)return NULL;
        Shadow *o=&shadows[s];if(o->rtv)ID3D11RenderTargetView_Release(o->rtv);if(o->dsv)ID3D11DepthStencilView_Release(o->dsv);
        if(o->srv)ID3D11ShaderResourceView_Release(o->srv);if(o->tex)ID3D11Texture2D_Release(o->tex);if(o->game)ID3D11Texture2D_Release(o->game);
        memset(o,0,sizeof *o);
    }
    Shadow *sh=&shadows[s];D3D11_TEXTURE2D_DESC td;ID3D11Texture2D_GetDesc(game,&td);
    if(FAILED(ID3D11Device_CreateTexture2D(dev,&td,NULL,&sh->tex)))return NULL;
    if(td.BindFlags&D3D11_BIND_DEPTH_STENCIL){
        D3D11_DEPTH_STENCIL_VIEW_DESC dv={0};dv.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;dv.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
        ID3D11Device_CreateDepthStencilView(dev,(ID3D11Resource*)sh->tex,&dv,&sh->dsv);
        D3D11_SHADER_RESOURCE_VIEW_DESC sv={0};sv.Format=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sv.Texture2D.MipLevels=1;
        ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)sh->tex,&sv,&sh->srv);
    }else{
        ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)sh->tex,NULL,&sh->rtv);
        ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)sh->tex,NULL,&sh->srv);
    }
    sh->game=game;ID3D11Texture2D_AddRef(game);sh->used=shadow_replays;
    return sh;
}
/* Which matching draw of the previous frame each draw of f blends with. */
typedef struct {uint64_t key;unsigned idx;} IKey;
static int ikey_cmp(const void *a,const void *b){
    const IKey *x=a,*y=b;if(x->key!=y->key)return x->key<y->key?-1:1;return x->idx<y->idx?-1:x->idx>y->idx;
}
/* Pair each draw of f with a draw of p that has the same key. Many objects
 * share a key (every tree of one kind: same shaders, texture and index count),
 * and the set drawn changes from frame to frame as objects come into view, so
 * pairing the n-th with the n-th would blend one tree toward another and make
 * it jump in the in-between frames. Instead, within a key, each draw takes the
 * unused candidate whose vertex-program constants are nearest (sum of
 * differences over the transform rows): the same object moves a little from
 * one tick to the next, another instance of the same mesh is far away. Ties
 * keep the old order. Candidates are
 * searched within a window around the draw's position in the group.
 * LEAN_INTERP_MATCH=0 restores order-only pairing. */
/* Distance between two draws' constants over the first 48 rows (where this
 * game's per-object transforms live), stopping early past `limit`. With the
 * camera moving, a shader's combined transform changes a little for the same
 * object and a lot for another instance of the same mesh. */
static float interp_const_distance(IFrame *f,IOp *a,IFrame *p,IOp *b,float limit){
    const float *x=(const float*)(f->mem+a->coff),*y=(const float*)(p->mem+b->coff);float d=0;
    for(unsigned k=0;k<48*4;k++){float t=x[k]-y[k];d+=t<0?-t:t;if(d>limit)return d;}
    return d;
}
/* A draw's matching key: its recorded key plus its target, where the frame's
 * own screen surface counts as one target whichever flip buffer it is. */
static uint64_t interp_key(const IFrame *fr,const IOp *o){
    uint64_t t=(o->ct&&o->ct==(void*)fr->scanout)?1:(uint64_t)(uintptr_t)o->ct;
    return hash_bytes(&t,sizeof t,o->key);
}
static void interp_match(IFrame *f,IFrame *p){
    static int smart=-1;if(smart<0){const char *v=getenv("LEAN_INTERP_MATCH");smart=!(v&&v[0]=='0');}
    if(f->matched_with==p->serial)return;
    for(unsigned i=0;i<f->nops;i++)f->ops[i].match=-1;
    IKey *a=malloc(sizeof(IKey)*(f->nops+1)),*b=malloc(sizeof(IKey)*(p->nops+1));unsigned na=0,nb=0;
    uint8_t *used=malloc(p->nops+1);
    if(a&&b&&used){
        for(unsigned i=0;i<f->nops;i++)if(f->ops[i].kind==0){a[na].key=interp_key(f,&f->ops[i]);a[na++].idx=i;}
        for(unsigned i=0;i<p->nops;i++)if(p->ops[i].kind==0){b[nb].key=interp_key(p,&p->ops[i]);b[nb++].idx=i;}
        qsort(a,na,sizeof *a,ikey_cmp);qsort(b,nb,sizeof *b,ikey_cmp);
        memset(used,0,p->nops+1);
        for(unsigned i=0,j=0;i<na&&j<nb;){
            if(a[i].key<b[j].key){i++;continue;}
            if(a[i].key>b[j].key){j++;continue;}
            unsigned i1=i,j1=j;while(i1<na&&a[i1].key==a[i].key)i1++;while(j1<nb&&b[j1].key==b[j].key)j1++;
            unsigned ga=i1-i,gb=j1-j;
            if(!smart||(ga==1&&gb==1)){for(unsigned k=0;k<ga&&k<gb;k++)f->ops[a[i+k].idx].match=(int)b[j+k].idx;}
            else for(unsigned k=0;k<ga;k++){
                IOp *o=&f->ops[a[i+k].idx];
                unsigned centre=(unsigned)((uint64_t)k*gb/ga),lo=centre>32?centre-32:0,hi=centre+33<gb?centre+33:gb;
                int best=-1;float bestv=3.4e38f;unsigned bestd=~0u;
                for(unsigned m=lo;m<hi;m++){unsigned bi=b[j+m].idx;if(used[bi])continue;
                    float v=interp_const_distance(f,o,p,&p->ops[bi],bestv);unsigned d=m>centre?m-centre:centre-m;
                    if(best<0||v<bestv||(v==bestv&&d<bestd)){best=(int)bi;bestv=v;bestd=d;}}
                if(best>=0){o->match=best;used[best]=1;}
            }
            i=i1;j=j1;
        }
    }
    free(a);free(b);free(used);f->matched_with=p->serial;
}

/* Which resource a texture view shows; looked up on the display thread and
 * cached (views are kept alive by the frames that reference them). */
#define SRV_CACHE 1024
static struct {ID3D11ShaderResourceView *srv;ID3D11Resource *res;} srv_cache[SRV_CACHE];
static ID3D11Resource *srv_resource(ID3D11ShaderResourceView *v){
    unsigned h=(unsigned)(((uintptr_t)v>>4)*2654435761u)%SRV_CACHE;
    if(srv_cache[h].srv==v)return srv_cache[h].res;
    ID3D11Resource *r=NULL;ID3D11View_GetResource((ID3D11View*)v,&r);if(r)ID3D11Resource_Release(r);
    srv_cache[h].srv=v;srv_cache[h].res=r;return r;
}
static ID3D11Buffer *rvb,*rib,*rvcb,*rpcb,*rclear_cb;static UINT rvb_size=64u<<20,rib_size=16u<<20;
static ID3D11DeviceContext *dc;static ID3D11DeviceContext1 *dc1;

/* Rotating objects (2026-10-08, the "bouncing tyres"): blending the four rows of a
 * draw's position transform one number at a time is right for movement but not for
 * turning. A car wheel turns 40-90 degrees between two game frames (31 ms at 32 game
 * frames/s); the straight-line blend of two rotations shrinks the wheel toward its
 * hub, to cos(angle/2) at the half-way frame (about 0.7), and rows that change by
 * more than half their size were also snapped to frame N while the others blended,
 * shearing it. So each in-between frame pulsed the wheels smaller and back.
 * Instead: the object-space step D = A^-1 B between frame N-1's transform A and frame
 * N's B; when D is a rigid motion (rotation + translation; the camera's own motion
 * included), its rotation is interpolated by angle (quaternion) and its translation
 * linearly, plus the small remainder B - A*D' blended linearly so frame N is exact.
 * For a pure translation this equals the old blend. Camera cuts and teleports (the
 * object's screen position jumping) keep the old rule. LEAN_INTERP_ROTATE=0: off.
 * LEAN_INTERP_ROTDIAG=1: log the first rotated draws (angle per game frame). */
static int inv4(const double m[4][4],double r[4][4]){
    double a[4][8];for(int i=0;i<4;i++)for(int j=0;j<4;j++){a[i][j]=m[i][j];a[i][j+4]=i==j;}
    double scale=0;for(int i=0;i<4;i++)for(int j=0;j<4;j++)scale=fmax(scale,fabs(m[i][j]));if(scale<=0)return 0;
    for(int c=0;c<4;c++){int p=c;for(int i=c+1;i<4;i++)if(fabs(a[i][c])>fabs(a[p][c]))p=i;
        if(fabs(a[p][c])<1e-9*scale)return 0;
        if(p!=c)for(int j=0;j<8;j++){double t=a[c][j];a[c][j]=a[p][j];a[p][j]=t;}
        double iv=1.0/a[c][c];for(int j=0;j<8;j++)a[c][j]*=iv;
        for(int i=0;i<4;i++)if(i!=c&&a[i][c]!=0){double f=a[i][c];for(int j=0;j<8;j++)a[i][j]-=f*a[c][j];}}
    for(int i=0;i<4;i++)for(int j=0;j<4;j++)r[i][j]=a[i][j+4];
    return 1;
}
static void quat_mat(double w,double x,double y,double z,double R[3][3]){
    R[0][0]=1-2*(y*y+z*z);R[0][1]=2*(x*y-w*z);R[0][2]=2*(x*z+w*y);
    R[1][0]=2*(x*y+w*z);R[1][1]=1-2*(x*x+z*z);R[1][2]=2*(y*z-w*x);
    R[2][0]=2*(x*z-w*y);R[2][1]=2*(y*z+w*x);R[2][2]=1-2*(x*x+y*y);
}
static struct {uint64_t rotated,not_rigid,jump,tiny;} irs;
static int interp_rotate(const float *pv,const float *cv,const int *pr,float alpha,float *vbuf,uint64_t key,unsigned nidx){
    static int on=-1,diag=0;static unsigned nlog;
    if(on<0){const char *e=getenv("LEAN_INTERP_ROTATE");on=!(e&&e[0]=='0');e=getenv("LEAN_INTERP_ROTDIAG");diag=e?atoi(e):0;
        LOG("[LEAN-INTERP] turning objects: %s\n",on?"rotation blended by angle":"rows blended linearly (old)");}
    if(!on||!pr||pr[0]<0)return 0;
    double A[4][4],B[4][4],Ai[4][4],D[4][4];int same=1;
    for(int i=0;i<4;i++)for(int j=0;j<4;j++){A[i][j]=pv[pr[i]*4+j];B[i][j]=cv[pr[i]*4+j];if(A[i][j]!=B[i][j])same=0;}
    if(same||!inv4(A,Ai))return 0;
    for(int i=0;i<4;i++)for(int j=0;j<4;j++){double s=0;for(int k=0;k<4;k++)s+=Ai[i][k]*B[k][j];D[i][j]=s;}
    /* rigid: bottom row (0,0,0,1), upper 3x3 orthonormal within a few percent */
    if(fabs(D[3][0])+fabs(D[3][1])+fabs(D[3][2])+fabs(D[3][3]-1)>0.02){irs.not_rigid++;return 0;}
    for(int j=0;j<3;j++){double n=D[0][j]*D[0][j]+D[1][j]*D[1][j]+D[2][j]*D[2][j];if(fabs(n-1)>0.08){irs.not_rigid++;return 0;}
        for(int k=j+1;k<3;k++){double d=D[0][j]*D[0][k]+D[1][j]*D[1][k]+D[2][j]*D[2][k];if(fabs(d)>0.05){irs.not_rigid++;return 0;}}}
    /* quaternion of the rotation part (Shepperd) */
    double w,x,y,z,tr=D[0][0]+D[1][1]+D[2][2];
    if(tr>0){double S=sqrt(tr+1)*2;w=0.25*S;x=(D[2][1]-D[1][2])/S;y=(D[0][2]-D[2][0])/S;z=(D[1][0]-D[0][1])/S;}
    else if(D[0][0]>D[1][1]&&D[0][0]>D[2][2]){double S=sqrt(1+D[0][0]-D[1][1]-D[2][2])*2;w=(D[2][1]-D[1][2])/S;x=0.25*S;y=(D[0][1]+D[1][0])/S;z=(D[0][2]+D[2][0])/S;}
    else if(D[1][1]>D[2][2]){double S=sqrt(1+D[1][1]-D[0][0]-D[2][2])*2;w=(D[0][2]-D[2][0])/S;x=(D[0][1]+D[1][0])/S;y=0.25*S;z=(D[1][2]+D[2][1])/S;}
    else{double S=sqrt(1+D[2][2]-D[0][0]-D[1][1])*2;w=(D[1][0]-D[0][1])/S;x=(D[0][2]+D[2][0])/S;y=(D[1][2]+D[2][1])/S;z=0.25*S;}
    double qn=sqrt(w*w+x*x+y*y+z*z);if(qn<=0)return 0;w/=qn;x/=qn;y/=qn;z/=qn;if(w<0){w=-w;x=-x;y=-y;z=-z;}
    double theta=2*acos(w>1?1:w);
    if(theta<0.03){irs.tiny++;return 0;}   /* the linear blend is within 0.01% */
    /* a cut or teleport: the object's origin jumps across the screen */
    if(A[3][3]<=1e-6||B[3][3]<=1e-6){irs.jump++;return 0;}
    {double dx=A[0][3]/A[3][3]-B[0][3]/B[3][3],dy=A[1][3]/A[3][3]-B[1][3]/B[3][3];if(fabs(dx)+fabs(dy)>0.5){irs.jump++;return 0;}}
    double s=sin(theta/2),ax=x/s,ay=y/s,az=z/s,t=alpha,ht=theta*t/2,sn=sin(ht);
    double R1[3][3],Rt[3][3];quat_mat(w,x,y,z,R1);quat_mat(cos(ht),ax*sn,ay*sn,az*sn,Rt);
    double D1[4][4]={{R1[0][0],R1[0][1],R1[0][2],D[0][3]},{R1[1][0],R1[1][1],R1[1][2],D[1][3]},{R1[2][0],R1[2][1],R1[2][2],D[2][3]},{0,0,0,1}};
    double Dt[4][4]={{Rt[0][0],Rt[0][1],Rt[0][2],D[0][3]*t},{Rt[1][0],Rt[1][1],Rt[1][2],D[1][3]*t},{Rt[2][0],Rt[2][1],Rt[2][2],D[2][3]*t},{0,0,0,1}};
    for(int i=0;i<4;i++)for(int j=0;j<4;j++){double a1=0,at=0;for(int k=0;k<4;k++){a1+=A[i][k]*D1[k][j];at+=A[i][k]*Dt[k][j];}
        vbuf[pr[i]*4+j]=(float)(at+t*(B[i][j]-a1));}
    irs.rotated++;
    if(diag&&nlog<200&&theta>0.2){nlog++;LOG("[LEAN-INTERP-ROT] key %016llX nidx %u rows %d-%d-%d-%d: turned %.1f deg per game frame (axis %.2f %.2f %.2f), alpha %.2f; old linear blend at half-way scaled it to %.3f\n",
        (unsigned long long)key,nidx,pr[0],pr[1],pr[2],pr[3],theta*57.2958,ax,ay,az,alpha,cos(theta/2));}
    return 1;
}

/* The vertex-program constants an in-between frame draws op o with: frame N's,
 * blended toward them from its matching draw in frame N-1 (vbuf, IV_FLOATS). */
static void blend_constants(IFrame *f,IFrame *p,IOp *o,float alpha,float *vbuf){
            const float *cv=(const float*)(f->mem+o->coff);
            /* Snap instead of blending when a constant vector changes by more than
             * half its size in one tick: a camera cut or teleport, not motion.
             * Per vec4, so a tiny component changing sign does not count. Rows
             * whose size is below 1 never snap: animated values near zero (sway,
             * scrolling, flicker phases) change sign every few ticks, and snapping
             * on them drew the whole object a tick ahead of the rest -- the tree
             * flicker. LEAN_INTERP_SNAP_MIN=0.001 restores the old threshold. */
            /* Per row since 2026-10-07: only the rows that jump are taken from
             * frame N; the others still blend. A single garbage or teleporting row
             * (an unused light slot, a phase counter) no longer drags the whole
             * object a tick ahead of everything else. */
            int snap=0;static float snap_min=-1;if(snap_min<0){const char *v=getenv("LEAN_INTERP_SNAP_MIN");snap_min=v?(float)atof(v):1.0f;}
            if(o->match>=0&&alpha<1.0f){
                const float *pv=(const float*)(p->mem+p->ops[o->match].coff);
                for(unsigned r=0;r<192;r++){if(o->crows&&!((o->crows[r>>5]>>(r&31))&1u))continue;   /* the program never reads this row */
                    const float *a=pv+r*4,*b=cv+r*4;float *v=vbuf+r*4;
                    float d=fabsf(b[0]-a[0])+fabsf(b[1]-a[1])+fabsf(b[2]-a[2])+fabsf(b[3]-a[3]);
                    float m=fabsf(a[0])+fabsf(a[1])+fabsf(a[2])+fabsf(a[3])+fabsf(b[0])+fabsf(b[1])+fabsf(b[2])+fabsf(b[3]);
                    if(m>snap_min&&d>0.5f*m){snap++;memcpy(v,b,16);
                        {static int dg2=-1,nl2;if(dg2<0){const char *e=getenv("LEAN_INTERP_DIAG");dg2=e&&e[0]=='1';}
                         if(dg2&&nl2<40){nl2++;LOG("[LEAN-INTERP-SNAP] key %016llX nidx %u row %u (%.4g,%.4g,%.4g,%.4g) -> (%.4g,%.4g,%.4g,%.4g)\n",(unsigned long long)o->key,o->nidx,r,a[0],a[1],a[2],a[3],b[0],b[1],b[2],b[3]);}}}
                    else for(unsigned k=0;k<4;k++)v[k]=(a[k]==b[k])?b[k]:a[k]+(b[k]-a[k])*alpha;
                }
                interp_rotate(pv,cv,o->prows,alpha,vbuf,o->key,o->nidx);   /* the position transform of a turning object */
                memcpy(vbuf+192*4,cv+192*4,12*4);
                if(snap)ist.snapped++;else ist.matched++;
            }
            else memcpy(vbuf,cv,IV_FLOATS*4);
            {   /* LEAN_INTERP_DIAG=1 (diagnostic): which kinds of draw go unblended in in-between frames. */
                static int dg=-1;static struct {uint64_t key;unsigned nidx,nverts,unm,snp,tot;} tab[64];static uint64_t last;
                if(dg<0){const char *v=getenv("LEAN_INTERP_DIAG");dg=v&&v[0]=='1';last=iqpc();}
                if(dg&&alpha<1.0f){unsigned h;for(h=0;h<64;h++)if(!tab[h].tot||tab[h].key==o->key)break;
                    if(h<64){tab[h].key=o->key;tab[h].nidx=o->nidx;tab[h].nverts=o->nverts;tab[h].tot++;if(o->match<0)tab[h].unm++;else if(snap)tab[h].snp++;}
                    if(iqpc()-last>(uint64_t)interp_hz.QuadPart*5){last=iqpc();
                        for(unsigned k=0;k<64&&tab[k].tot;k++)if(tab[k].unm+tab[k].snp>tab[k].tot/10&&tab[k].tot>50)
                            LOG("[LEAN-INTERP-DIAG] key %016llX nidx %u nverts %u: %u draws, %u unmatched, %u snapped\n",(unsigned long long)tab[k].key,tab[k].nidx,tab[k].nverts,tab[k].tot,tab[k].unm,tab[k].snp);
                        memset(tab,0,sizeof tab);}}
            }
}

/* LEAN_INTERP_CBRING=1 (experimental, off): all draws' constants in one upload per
 * in-between frame instead of one UpdateSubresource per draw. Off because on this
 * laptop's driver (command lists emulated by the runtime) the offset bindings drew
 * with the wrong constants: run 20261008-102956 showed broken in-between frames,
 * 20261008-103134 (per-draw uploads) correct ones. The draws then bind their slice of
 * two large buffers (D3D11.1 constant-buffer offsets, 16-constant aligned).
 * Per-draw uploads were the bulk of the replay and of its command-list execution
 * on the game's context: about 3 KB each, 2000 draws a frame in the Action game. */
#define RING_V 208u   /* constants per draw slot (195 used) */
#define RING_P 32u    /* pixel constants per draw slot (24 used) */
static ID3D11Buffer *ringv,*ringp;static unsigned ring_slots;static int ring_on=-1;
static int part_quirk;   /* emulated command lists: UpdateSubresource box quirk applies */
static int ring_ready(unsigned n){
    if(ring_on<0){const char *v=getenv("LEAN_INTERP_CBRING");ring_on=v&&v[0]=='1';
        D3D11_FEATURE_DATA_D3D11_OPTIONS op={0};
        if(ring_on&&(!dc1||FAILED(ID3D11Device_CheckFeatureSupport(dev,D3D11_FEATURE_D3D11_OPTIONS,&op,sizeof op))||!op.ConstantBufferOffsetting))ring_on=0;
        LOG("[LEAN-INTERP] constants: %s\n",ring_on?"one upload per frame (offsets)":"one upload per draw");}
    if(!ring_on)return 0;
    if(n>ring_slots){unsigned want=n+n/4+256;
        if(ringv)ID3D11Buffer_Release(ringv);if(ringp)ID3D11Buffer_Release(ringp);
        ringv=mkbuf(want*RING_V*16,D3D11_BIND_CONSTANT_BUFFER,1);ringp=mkbuf(want*RING_P*16,D3D11_BIND_CONSTANT_BUFFER,1);
        ring_slots=(ringv&&ringp)?want:0;if(!ring_slots){ring_on=0;return 0;}}
    return 1;
}

static void replay_frame(IFrame *f,IFrame *p,float alpha,ID3D11Texture2D *back){
    for(unsigned i=0;i<MAX_SHADOW;i++)shadows[i].written=0;
    shadow_replays++;
    if(nfo_enabled())for(unsigned i=0;i<MAX_SHADOW;i++){Shadow *o=&shadows[i];   /* NF_OVERLAY: a surface not seen for 600 replays (its internal resolution changed) */
        if(!o->game||shadow_replays-o->used<600)continue;
        if(o->rtv)ID3D11RenderTargetView_Release(o->rtv);if(o->dsv)ID3D11DepthStencilView_Release(o->dsv);
        if(o->srv)ID3D11ShaderResourceView_Release(o->srv);if(o->tex)ID3D11Texture2D_Release(o->tex);ID3D11Texture2D_Release(o->game);
        memset(o,0,sizeof *o);}
    {static unsigned last;if(f->serial!=last){memset(srv_cache,0,sizeof srv_cache);last=f->serial;}} /* views may be reused */
    /* pass 1: every draw's constants into the rings */
    unsigned ndraw=0;for(unsigned i=0;i<f->nops;i++)if(f->ops[i].kind==0)ndraw++;
    int ring=ring_ready(ndraw);uint64_t b0=iqpc();
    if(ring){D3D11_MAPPED_SUBRESOURCE mv,mp;
        if(SUCCEEDED(ID3D11DeviceContext_Map(dc,(ID3D11Resource*)ringv,0,D3D11_MAP_WRITE_DISCARD,0,&mv))){
            if(SUCCEEDED(ID3D11DeviceContext_Map(dc,(ID3D11Resource*)ringp,0,D3D11_MAP_WRITE_DISCARD,0,&mp))){
                unsigned j=0;
                for(unsigned i=0;i<f->nops;i++){IOp *o=&f->ops[i];if(o->kind!=0)continue;
                    blend_constants(f,p,o,alpha,(float*)((uint8_t*)mv.pData+(size_t)j*RING_V*16));
                    memcpy((uint8_t*)mp.pData+(size_t)j*RING_P*16,f->mem+o->coff+IV_FLOATS*4,IP_FLOATS*4);j++;}
                ID3D11DeviceContext_Unmap(dc,(ID3D11Resource*)ringp,0);
            }else ring=0;
            ID3D11DeviceContext_Unmap(dc,(ID3D11Resource*)ringv,0);
        }else ring=0;}
    ist.blend_ticks+=iqpc()-b0;
    /* pass 2: the draws; geometry: the renderer's rings still hold this frame's vertices */
    float vbuf[IV_FLOATS];unsigned j=0;
    /* State already set on the deferred context (it starts each command list
     * empty): consecutive draws mostly share targets, shaders, textures and
     * render states, so only changes are issued. LEAN_INTERP_STATECACHE=0: all. */
    static int sc_on=-1;if(sc_on<0){const char *v=getenv("LEAN_INTERP_STATECACHE");sc_on=!(v&&v[0]=='0');}
    struct {int valid,srv_bound;void *rtv,*dsv,*il,*vs,*gs,*ps,*bs,*dss,*rs;ID3D11ShaderResourceView *srv[4];ID3D11SamplerState *smp[4];float bf[4];UINT sref,vw,vh,topo;} rc;
    memset(&rc,0,sizeof rc);
    #define RC_UNBIND() do{if(rc.srv_bound){ID3D11ShaderResourceView *none_[4]={0};ID3D11DeviceContext_PSSetShaderResources(dc,0,4,none_);memset(rc.srv,0,sizeof rc.srv);rc.srv_bound=0;}}while(0)
    for(unsigned i=0;i<f->nops;i++){IOp *o=&f->ops[i];
        if(o->kind!=0){RC_UNBIND();rc.valid=0;}
        if(o->kind==0){
            unsigned slot=j++;
            Shadow *cs=o->ct?shadow_for(o->ct,1):NULL,*zs=o->dt?shadow_for(o->dt,1):NULL;
            ID3D11RenderTargetView *rtv=cs?cs->rtv:NULL;ID3D11DepthStencilView *dsv=zs?zs->dsv:NULL;
            if(!rtv&&!dsv)continue;
            if(!sc_on)rc.valid=0;
            if(!rc.valid||rc.rtv!=(void*)rtv||rc.dsv!=(void*)dsv){RC_UNBIND();ID3D11DeviceContext_OMSetRenderTargets(dc,1,&rtv,dsv);rc.rtv=rtv;rc.dsv=dsv;}
            if(!rc.valid||rc.vw!=o->vw||rc.vh!=o->vh){D3D11_VIEWPORT vp={0,0,(float)o->vw,(float)o->vh,0,1};ID3D11DeviceContext_RSSetViewports(dc,1,&vp);rc.vw=o->vw;rc.vh=o->vh;}
            ID3D11ShaderResourceView *srv[4];
            for(unsigned k=0;k<4;k++){srv[k]=o->srv[k];
                ID3D11Resource *res=srv[k]?srv_resource(srv[k]):NULL;
                if(res){Shadow *s=shadow_for((ID3D11Texture2D*)res,0);if(s&&s->written&&s!=cs)srv[k]=s->srv;}}
            const float *cv=(const float*)(f->mem+o->coff);
            UINT vfirst=slot*RING_V,vnum=RING_V,pfirst=slot*RING_P,pnum=RING_P;
            if(!ring){blend_constants(f,p,o,alpha,vbuf);
                /* LEAN_INTERP_PARTIALCB=1 (experimental, off; =2 without the quirk
                 * adjustment): both variants drew wrong in-between frames on this
                 * laptop's driver (runs/cap12, cap13-1, cap13-2 in nightfire-port-native),
                 * so the whole buffer is uploaded by default.
                 * Upload only the rows the program reads (its lowest to highest row,
                 * plus the 3 renderer rows after row 191) when the driver supports
                 * partial constant-buffer updates; LEAN_INTERP_PARTIALCB=0: whole
                 * buffer. The deferred context's emulated command lists take the
                 * source pointer relative to the box start (the documented D3D11
                 * UpdateSubresource deferred-context quirk), hence src - box.left. */
                static int part=-1;
                if(part<0){const char *v=getenv("LEAN_INTERP_PARTIALCB");D3D11_FEATURE_DATA_D3D11_OPTIONS op={0};
                    part=(v&&(v[0]=='1'||v[0]=='2'))&&dc1&&SUCCEEDED(ID3D11Device_CheckFeatureSupport(dev,D3D11_FEATURE_D3D11_OPTIONS,&op,sizeof op))&&op.ConstantBufferPartialUpdate;
                    {D3D11_FEATURE_DATA_THREADING th={0};ID3D11Device_CheckFeatureSupport(dev,D3D11_FEATURE_THREADING,&th,sizeof th);part_quirk=!th.DriverCommandLists&&!(v&&v[0]=='2');}
                    LOG("[LEAN-INTERP] constant uploads: %s\n",part?"used rows only":"whole buffer");}
                unsigned lo=0,hi=191;
                if(part&&o->crows){lo=192;hi=0;for(unsigned w=0;w<6;w++)if(o->crows[w]){unsigned long b;
                        _BitScanForward(&b,o->crows[w]);if(w*32+b<lo)lo=w*32+b;_BitScanReverse(&b,o->crows[w]);if(w*32+b>hi)hi=w*32+b;}
                    if(lo>hi){lo=0;hi=0;}}
                if(part&&(lo>0||hi<191)){
                    D3D11_BOX b1={lo*16,0,0,(hi+1)*16,1,1},b2={192*16,0,0,195*16,1,1};
                    const uint8_t *s1=(const uint8_t*)(vbuf+lo*4),*s2=(const uint8_t*)(vbuf+192*4);
                    if(part_quirk){s1-=b1.left;s2-=b2.left;}
                    ID3D11DeviceContext1_UpdateSubresource1(dc1,(ID3D11Resource*)rvcb,0,&b1,s1,0,0,0);
                    ID3D11DeviceContext1_UpdateSubresource1(dc1,(ID3D11Resource*)rvcb,0,&b2,s2,0,0,0);
                }else ID3D11DeviceContext_UpdateSubresource(dc,(ID3D11Resource*)rvcb,0,NULL,vbuf,0,0);
                ID3D11DeviceContext_UpdateSubresource(dc,(ID3D11Resource*)rpcb,0,NULL,cv+IV_FLOATS,0,0);}
            UINT vofs=(UINT)o->voff;
            if(!rc.valid||rc.il!=(void*)o->il){ID3D11DeviceContext_IASetInputLayout(dc,o->il);rc.il=o->il;}
            {ID3D11Buffer *vbb=o->vbuf?o->vbuf:vb;ID3D11DeviceContext_IASetVertexBuffers(dc,0,1,&vbb,&o->stride,&vofs);}
            ID3D11DeviceContext_IASetIndexBuffer(dc,ib,DXGI_FORMAT_R32_UINT,(UINT)o->ioff);
            if(!rc.valid||rc.topo!=o->topo){ID3D11DeviceContext_IASetPrimitiveTopology(dc,o->topo==1?D3D11_PRIMITIVE_TOPOLOGY_LINELIST:o->topo==2?D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);rc.topo=o->topo;}
            if(!rc.valid||rc.vs!=(void*)o->vs){ID3D11DeviceContext_VSSetShader(dc,o->vs,NULL,0);rc.vs=o->vs;}
            {void *gs=o->sprite?(void*)sprite_gs:NULL;if(!rc.valid||rc.gs!=gs){ID3D11DeviceContext_GSSetShader(dc,(ID3D11GeometryShader*)gs,NULL,0);rc.gs=gs;}}
            if(!rc.valid||rc.ps!=(void*)o->ps){ID3D11DeviceContext_PSSetShader(dc,o->ps,NULL,0);rc.ps=o->ps;}
            if(ring){ID3D11DeviceContext1_VSSetConstantBuffers1(dc1,0,1,&ringv,&vfirst,&vnum);
                if(o->sprite)ID3D11DeviceContext1_GSSetConstantBuffers1(dc1,0,1,&ringv,&vfirst,&vnum);
                ID3D11DeviceContext1_PSSetConstantBuffers1(dc1,0,1,&ringp,&pfirst,&pnum);}
            else{ID3D11DeviceContext_VSSetConstantBuffers(dc,0,1,&rvcb);if(o->sprite)ID3D11DeviceContext_GSSetConstantBuffers(dc,0,1,&rvcb);
                ID3D11DeviceContext_PSSetConstantBuffers(dc,0,1,&rpcb);}
            if(!rc.valid||memcmp(rc.srv,srv,sizeof rc.srv)){ID3D11DeviceContext_PSSetShaderResources(dc,0,4,srv);memcpy(rc.srv,srv,sizeof rc.srv);rc.srv_bound=1;}
            if(!rc.valid||memcmp(rc.smp,o->smp,sizeof rc.smp)){ID3D11DeviceContext_PSSetSamplers(dc,0,4,o->smp);memcpy(rc.smp,o->smp,sizeof rc.smp);}
            if(!rc.valid||rc.bs!=(void*)o->bs||memcmp(rc.bf,o->bf,sizeof rc.bf)){ID3D11DeviceContext_OMSetBlendState(dc,o->bs,o->bf,0xffffffffu);rc.bs=o->bs;memcpy(rc.bf,o->bf,sizeof rc.bf);}
            if(!rc.valid||rc.dss!=(void*)o->dss||rc.sref!=o->sref){ID3D11DeviceContext_OMSetDepthStencilState(dc,o->dss,o->sref);rc.dss=o->dss;rc.sref=o->sref;}
            if(!rc.valid||rc.rs!=(void*)o->rs){ID3D11DeviceContext_RSSetState(dc,o->rs);rc.rs=o->rs;}
            ID3D11DeviceContext_DrawIndexed(dc,o->nidx,0,0);
            rc.valid=1;if(!sc_on)RC_UNBIND();
            if(cs)cs->written=1;if(zs)zs->written=1;ist.draws++;
        }else if(o->kind==1){
            Shadow *cs=o->ct?shadow_for(o->ct,1):NULL,*zs=o->dt?shadow_for(o->dt,1):NULL;
            if(cs&&o->full&&o->cmask==15){ID3D11DeviceContext_ClearRenderTargetView(dc,cs->rtv,o->col);cs->written=1;cs=NULL;}
            else if(cs&&dc1&&o->cmask==15){ID3D11DeviceContext1_ClearView(dc1,(ID3D11View*)cs->rtv,o->col,&o->rect,1);cs->written=1;cs=NULL;}
            if(zs&&o->full){UINT fl=((o->flags&1)?D3D11_CLEAR_DEPTH:0)|((o->flags&2)?D3D11_CLEAR_STENCIL:0);ID3D11DeviceContext_ClearDepthStencilView(dc,zs->dsv,fl,o->zv,(UINT8)o->sv);zs->written=1;zs=NULL;}
            if(!cs&&!zs)continue;
            ID3D11RenderTargetView *rtv=cs?cs->rtv:NULL;ID3D11DepthStencilView *dsv=zs?zs->dsv:NULL;
            ID3D11DeviceContext_OMSetRenderTargets(dc,1,&rtv,dsv);
            D3D11_VIEWPORT vp={0,0,(float)o->vw,(float)o->vh,0,1};ID3D11DeviceContext_RSSetViewports(dc,1,&vp);
            ID3D11DeviceContext_RSSetScissorRects(dc,1,&o->rect);
            float cb[8]={o->col[0],o->col[1],o->col[2],o->col[3],o->zv,0,0,0};ID3D11DeviceContext_UpdateSubresource(dc,(ID3D11Resource*)rclear_cb,0,NULL,cb,0,0);
            ID3D11DeviceContext_IASetInputLayout(dc,NULL);ID3D11DeviceContext_IASetPrimitiveTopology(dc,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ID3D11DeviceContext_VSSetShader(dc,clear_vs,NULL,0);ID3D11DeviceContext_VSSetConstantBuffers(dc,0,1,&rclear_cb);
            ID3D11DeviceContext_GSSetShader(dc,NULL,NULL,0);
            ID3D11DeviceContext_PSSetShader(dc,clear_ps,NULL,0);ID3D11DeviceContext_PSSetConstantBuffers(dc,0,1,&rclear_cb);
            ID3D11DeviceContext_OMSetBlendState(dc,clear_bs[cs?o->cmask:0],NULL,0xffffffffu);
            ID3D11DeviceContext_OMSetDepthStencilState(dc,clear_ds[(zs&&(o->flags&1)?1:0)|(zs&&(o->flags&2)?2:0)],o->sv);
            ID3D11DeviceContext_RSSetState(dc,clear_rs);
            ID3D11DeviceContext_Draw(dc,3,0);
            if(cs)cs->written=1;if(zs)zs->written=1;
        }else if(o->kind==2){
            Shadow *ds=shadow_for(o->ct,1),*ss=shadow_for(o->src,0);if(!ds)continue;
            ID3D11Texture2D *src=(ss&&ss->written)?ss->tex:o->src;
            ID3D11DeviceContext_CopySubresourceRegion(dc,(ID3D11Resource*)ds->tex,0,o->dx,o->dy,0,(ID3D11Resource*)src,0,&o->box);
            ds->written=1;
        }else if(o->kind==3){   /* filtered copy between surfaces of different internal resolution (lean_d3d.c rt_stretch) */
            Shadow *ds=shadow_for(o->ct,1),*ss=shadow_for(o->src,0);if(!ds||!ds->rtv)continue;
            rt_stretch(dc,1,ds->rtv,o->rect,(ss&&ss->written)?ss->srv:o->ssrv,o->uv[0],o->uv[1],o->uv[2],o->uv[3]);
            ds->written=1;
        }
    }
    RC_UNBIND();
    #undef RC_UNBIND
    /* what the window shows: the replayed scanout surface */
    ID3D11Texture2D *scan=f->scanout;Shadow *ss=scan?shadow_for(scan,0):NULL;
    ID3D11ShaderResourceView *ssrv=NULL;
    if(ss&&ss->written){scan=ss->tex;ssrv=ss->srv;}
    else if(scan)ssrv=f->scanout_srv;
    static ID3D11ShaderResourceView *cpu_srv;static ID3D11Texture2D *cpu_srv_tex;
    if(scan&&!ssrv){if(cpu_srv_tex!=scan){if(cpu_srv)ID3D11ShaderResourceView_Release(cpu_srv);cpu_srv=NULL;
        ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)scan,NULL,&cpu_srv);cpu_srv_tex=scan;}ssrv=cpu_srv;}
    if(!scan)return;
    if(nfo_enabled()){nfo_present(dc,back,ssrv);return;}   /* NF_OVERLAY (lean_overlay.inc) */
    if(present_gamma>0&&present_gamma!=1.0f&&gamma_vs&&ssrv){
        ID3D11RenderTargetView *rtv=NULL;
        if(SUCCEEDED(ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)back,NULL,&rtv))){
            D3D11_VIEWPORT vp={0,0,640,480,0,1};
            ID3D11DeviceContext_OMSetRenderTargets(dc,1,&rtv,NULL);ID3D11DeviceContext_RSSetViewports(dc,1,&vp);
            ID3D11DeviceContext_OMSetBlendState(dc,NULL,NULL,0xffffffffu);ID3D11DeviceContext_OMSetDepthStencilState(dc,NULL,0);
            ID3D11DeviceContext_RSSetState(dc,gamma_rs);ID3D11DeviceContext_IASetInputLayout(dc,NULL);
            ID3D11DeviceContext_IASetPrimitiveTopology(dc,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ID3D11DeviceContext_VSSetShader(dc,gamma_vs,NULL,0);ID3D11DeviceContext_GSSetShader(dc,NULL,NULL,0);
            ID3D11DeviceContext_PSSetShader(dc,gamma_ps,NULL,0);ID3D11DeviceContext_PSSetConstantBuffers(dc,0,1,&gamma_cb);
            ID3D11DeviceContext_PSSetShaderResources(dc,0,1,&ssrv);ID3D11DeviceContext_Draw(dc,3,0);
            ID3D11ShaderResourceView *none=NULL;ID3D11DeviceContext_PSSetShaderResources(dc,0,1,&none);
            ID3D11RenderTargetView_Release(rtv);
        }
    }else{
        D3D11_TEXTURE2D_DESC td;ID3D11Texture2D_GetDesc(scan,&td);
        if(td.Width==640&&td.Height==480)ID3D11DeviceContext_CopyResource(dc,(ID3D11Resource*)back,(ID3D11Resource*)scan);
    }
}

static volatile LONG interp_quit,interp_quit_done;   /* lean_d3d_quiesce */
static DWORD WINAPI interp_thread(void *unused){
    (void)unused;
    if(FAILED(ID3D11Device_CreateDeferredContext(dev,0,&dc))){LOG("[LEAN-INTERP] no deferred context; off\n");interp_on=0;return 0;}
    ID3D11DeviceContext_QueryInterface(dc,&IID_ID3D11DeviceContext1,(void**)&dc1);
    rvb=mkbuf(rvb_size,D3D11_BIND_VERTEX_BUFFER,1);rib=mkbuf(rib_size,D3D11_BIND_INDEX_BUFFER,1);
    rvcb=mkbuf(192*16+48,D3D11_BIND_CONSTANT_BUFFER,0);rpcb=mkbuf(sizeof(float)*4*(8+8+4+4),D3D11_BIND_CONSTANT_BUFFER,0);rclear_cb=mkbuf(32,D3D11_BIND_CONSTANT_BUFFER,0);
    QueryPerformanceFrequency(&interp_hz);
    const char *v=getenv("LEAN_INTERP_HZ");double env_target=v?atof(v):120.0;if(env_target<1)env_target=0;
    v=getenv("LEAN_PRESENT_UNCAPPED");int env_uncapped=v&&*v=='1';
    v=getenv("LEAN_PRESENT_VSYNC");int env_vsync=v&&*v=='1'&&!env_uncapped;
    /* The frame rate (display only): the launcher's switches, or with NF_OVERLAY=1 the
     * settings, re-read every picture (nfo_frame_mode). */
    int uncapped=-1;double target=0;int fmode=-2,fhz=0;
    #define FRAME_MODE() do{int hz_=0,m_=nfo_frame_mode(&hz_);if(m_!=fmode||hz_!=fhz){ \
        fmode=m_;fhz=hz_;int unc_=m_<0?env_uncapped:m_==2;int vs_=m_<0?env_vsync:m_==0; \
        target=unc_?0:m_==1?(double)hz_:env_target;swap_vsync=vs_;next=iqpc(); \
        if(unc_!=uncapped){uncapped=unc_;unc_gap_ms=0;unc_best=0;unc_maxed=0;unc_t=iqpc();unc_w=lean_game_lock_wait;unc_f=if_serial; \
            /* Uncapped must not slow the game: the display thread runs below it. */ \
            SetThreadPriority(GetCurrentThread(),unc_?THREAD_PRIORITY_BELOW_NORMAL:THREAD_PRIORITY_NORMAL);} \
        LOG("[LEAN-INTERP] frame rate (display only): %s\n",unc_?"uncapped":vs_?"match the screen (vsync)":m_==1?"cap":"timer"); \
        if(!unc_&&!vs_)LOG("[LEAN-INTERP]   %.0f pictures a second\n",target);}}while(0)
    /* Uncapped self-limit: every 250 ms compare the game's lock wait per game
     * frame; above 2.5 ms the pause between in-between frames grows, below
     * 1.0 ms it shrinks (90 Hz smooth mode costs the game 1.3-2 ms without
     * slowing it). The presented rate is then the headroom left while
     * the game keeps its own speed. */
    double unc_gap_ms=0,unc_best=0,unc_gfps=0;int unc_maxed=0;uint64_t unc_t=iqpc();LONG64 unc_w=lean_game_lock_wait;unsigned unc_f=if_serial;
    HANDLE unc_timer=CreateWaitableTimerExW(NULL,NULL,2,TIMER_ALL_ACCESS);   /* uncapped can be chosen later (NF_OVERLAY) */
    {D3D11_FEATURE_DATA_THREADING th={0};ID3D11Device_CheckFeatureSupport(dev,D3D11_FEATURE_THREADING,&th,sizeof th);
     LOG("[LEAN-INTERP] driver command lists %s, concurrent creates %s\n",th.DriverCommandLists?"native":"emulated by the runtime",th.DriverConcurrentCreates?"yes":"no");}
    LOG("[LEAN-INTERP] display thread: launcher %s, target %.0f Hz%s\n",env_uncapped?"uncapped":env_vsync?"vsync":"timer",env_vsync?0.0:env_target,
        nfo_enabled()?" (the settings overlay chooses while the game runs)":"");
    extern int lean_vblank_hz(void);   /* LEAN_VBLANK_HZ: the game tick follows the vblank rate */
    uint64_t tick=interp_hz.QuadPart/lean_vblank_hz(),last_report=iqpc(),next=iqpc();unsigned presents=0;
    for(;;){
        if(interp_quit){InterlockedExchange(&interp_quit_done,1);return 0;}
        if(swap)FRAME_MODE();   /* after the swap chain exists (it reads LEAN_PRESENT_VSYNC when made) */
        if(uncapped>0){uint64_t t=iqpc();
            if(t-unc_t>=(uint64_t)interp_hz.QuadPart/4){LONG64 w=lean_game_lock_wait;unsigned fr=if_serial-unc_f;
                double per=fr?(double)(w-unc_w)*1000.0/interp_hz.QuadPart/fr:0.0;
                /* The game's own frames a second: a drop of more than 15% below the
                 * recent best (GPU or CPU shared with the in-between frames, which the
                 * lock wait does not show; 10-09 Paris: 62 -> 30 game frames a second
                 * while 95 were presented) also backs off. If backing off all the way
                 * does not bring it back within 2 s, the game is simply slower there (a
                 * heavier scene, menus at the screen rate then play at 60): that becomes
                 * the new best. */
                double gfps=(double)fr*(double)interp_hz.QuadPart/(double)(t-unc_t);unc_gfps=gfps;
                if(gfps>unc_best)unc_best=gfps;else unc_best=unc_best*0.998+gfps*0.002;
                int game_slow=unc_best>20&&gfps<unc_best*0.85;
                if(game_slow&&unc_gap_ms>=20){if(++unc_maxed>=8){unc_best=gfps;unc_maxed=0;game_slow=0;}}else unc_maxed=0;
                if(per>2.5||game_slow)unc_gap_ms=unc_gap_ms*1.5+0.2;else if(per<1.0&&gfps>=unc_best*0.95)unc_gap_ms*=0.8;
                if(unc_gap_ms>20)unc_gap_ms=20;if(unc_gap_ms<0.05)unc_gap_ms=0;
                unc_t=t;unc_w=w;unc_f=if_serial;}
            if(unc_gap_ms>0&&unc_timer){LARGE_INTEGER due;due.QuadPart=-(LONGLONG)(unc_gap_ms*10000.0);
                if(SetWaitableTimer(unc_timer,&due,0,NULL,NULL,FALSE))WaitForSingleObject(unc_timer,50);}}
        ihb_stage=1;ihb_t=iqpc();
        LONG cur=if_latest;
        if(cur<0||!swap){WaitForSingleObject(interp_event,20);continue;}
        IFrame *f=&ifr[cur];InterlockedIncrement(&f->inuse);
        if(f->serial!=ifr[cur].serial||ifr[cur].serial==0){InterlockedDecrement(&f->inuse);continue;}
        IFrame *p=NULL;for(unsigned i=0;i<IF_SLOTS;i++)if(ifr[i].serial==f->serial-1&&ifr[i].serial){p=&ifr[i];break;}
        if(p)InterlockedIncrement(&p->inuse);
        if(p&&p->serial!=f->serial-1){InterlockedDecrement(&p->inuse);p=NULL;}
        /* Spread the step N-1 -> N over the time the game actually took
         * between them (nominally 20 ms), so a late frame does not hold. */
        /* Show the game state of one tick ago, between the two frames' vblank
         * stamps; a late frame slows the motion briefly instead of freezing. */
        uint64_t now=iqpc();float alpha=1.0f;
        if(p&&f->t>p->t){   /* step starts on arrival, lasts the game-clock interval */
            uint64_t span=f->t-p->t;if(span>tick*3)span=tick*3;
            alpha=(float)((double)(now-f->arrive)/(double)span);if(alpha>1)alpha=1;if(alpha<0)alpha=0;}
        {uint64_t m0=iqpc();if(p)interp_match(f,p);ist.match_ticks+=iqpc()-m0;}
        uint64_t r0=iqpc();static int dbg;if(dbg<6)LOG("[LEAN-INTERP] replay %d start f=%u p=%u ops=%u alpha=%.2f\n",dbg,f->serial,p?p->serial:0,f->nops,alpha);
        if(nfo_enabled()){CTX_LOCK();swap_fit_window();CTX_UNLOCK();}   /* NF_OVERLAY: buffers follow the window */
        ID3D11Texture2D *back=NULL;
        if(SUCCEEDED(IDXGISwapChain1_GetBuffer(swap,0,&IID_ID3D11Texture2D,(void**)&back))){
            ihb_stage=2;replay_frame(f,p?p:f,p?alpha:1.0f,back);ihb_stage=3;
            ID3D11CommandList *cl=NULL;
            if(SUCCEEDED(ID3D11DeviceContext_FinishCommandList(dc,FALSE,&cl))){
                uint64_t e0=iqpc();
                ihb_stage=4;CTX_LOCK();ihb_stage=5;ID3D11DeviceContext_ExecuteCommandList(ctx,cl,TRUE);CTX_UNLOCK();ihb_stage=6;ID3D11CommandList_Release(cl);
                ist.exec_ticks+=iqpc()-e0;}
            {   /* LEAN_INTERP_DUMP=N: save presents N..N+5 (what the window shows) */
                static int want=-1;static unsigned count;
                if(want<0){const char *v=getenv("LEAN_INTERP_DUMP");want=v?atoi(v):0;}
                const char *dir=getenv("DRIVING_CAPTURE_DIR");count++;
                /* LEAN_INTERP_DUMP_MS=t1,t2,...: also 6 presents from each time
                 * (ms since the first replay) */
                static int tparsed;static uint64_t tstart;static unsigned tms[16],nt,tleft;
                if(!tparsed){tparsed=1;tstart=iqpc();const char *v=getenv("LEAN_INTERP_DUMP_MS");
                    while(v&&*v&&nt<16){tms[nt++]=(unsigned)strtoul(v,(char**)&v,10);if(*v==',')v++;}}
                {uint64_t ms=(iqpc()-tstart)*1000/interp_hz.QuadPart;
                 for(unsigned k=0;k<nt;k++)if(tms[k]&&ms>=tms[k]){tms[k]=0;tleft=6;}}
                int tdump=tleft>0;if(tleft)tleft--;
                if(dir&&(tdump||(want>0&&count>=(unsigned)want&&count<(unsigned)want+6))){
                    D3D11_TEXTURE2D_DESC sd;ID3D11Texture2D_GetDesc(back,&sd);sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;sd.MiscFlags=0;
                    ID3D11Texture2D *stg=NULL;D3D11_MAPPED_SUBRESOURCE m;
                    if(SUCCEEDED(ID3D11Device_CreateTexture2D(dev,&sd,NULL,&stg))){
                        CTX_LOCK();ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)stg,(ID3D11Resource*)back);
                        if(SUCCEEDED(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)stg,0,D3D11_MAP_READ,0,&m))){
                            char path[MAX_PATH];snprintf(path,sizeof path,"%s/interp-%06u-game%06u-a%03d.bmp",dir,count,f->serial,(int)(alpha*100+0.5f));
                            FILE *fo=fopen(path,"wb");
                            if(fo){uint32_t W=sd.Width,H=sd.Height,img=W*H*4;uint8_t hd[54]={'B','M'};
                                *(uint32_t*)(hd+2)=54+img;*(uint32_t*)(hd+10)=54;*(uint32_t*)(hd+14)=40;*(int32_t*)(hd+18)=(int32_t)W;*(int32_t*)(hd+22)=-(int32_t)H;
                                *(uint16_t*)(hd+26)=1;*(uint16_t*)(hd+28)=32;*(uint32_t*)(hd+34)=img;fwrite(hd,1,54,fo);
                                for(uint32_t y=0;y<H;y++)fwrite((uint8_t*)m.pData+(size_t)y*m.RowPitch,1,W*4,fo);fclose(fo);}
                            ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)stg,0);
                        }
                        CTX_UNLOCK();ID3D11Texture2D_Release(stg);
                    }
                }
            }
            ID3D11Texture2D_Release(back);
        }
        if(p)InterlockedDecrement(&p->inuse);InterlockedDecrement(&f->inuse);
        ist.replay_ms+=(double)(iqpc()-r0)*1000.0/interp_hz.QuadPart;ist.replays++;
        if(dbg<6)LOG("[LEAN-INTERP] replay %d done %.2f ms\n",dbg,(double)(iqpc()-r0)*1000.0/interp_hz.QuadPart);
        /* Never block inside Present: it holds the device lock the game
         * thread also needs. Retry for up to ~25 ms if the queue is full. */
        {uint64_t pall=iqpc();int busy=0,done=0;   /* NF_PRESENT_STATS: whole retry loop, including the Sleep(1)s */
        for(int tries=0;tries<25;tries++){
            if(dbg<6)LOG("[LEAN-INTERP] present call %d try %d\n",dbg,tries);
            uint64_t p0=iqpc();ihb_stage=7;CTX_LOCK();ihb_stage=8;HRESULT phr=IDXGISwapChain1_Present(swap,swap_vsync?1:0,DXGI_PRESENT_DO_NOT_WAIT|((swap_tear&&!swap_vsync&&uncapped>0)?DXGI_PRESENT_ALLOW_TEARING:0));CTX_UNLOCK();   /* a cap shows whole pictures; only uncapped may tear */
            if(dbg<6)LOG("[LEAN-INTERP] present returned %08lX after %.2f ms\n",(unsigned long)phr,(double)(iqpc()-p0)*1000.0/interp_hz.QuadPart);
            if(phr!=DXGI_ERROR_WAS_STILL_DRAWING){if(SUCCEEDED(phr))InterlockedIncrement(&lean_screen_presents);done=1;break;}busy++;Sleep(1);}
        if(nf_pstats){nf_present_note(iqpc()-pall,busy,!done);nf_present_report("display thread (smooth mode)");}}
        if(uncapped>0)SwitchToThread();   /* let the game thread in between frames */
        presents++;if(dbg<6)LOG("[LEAN-INTERP] present %d ok\n",dbg++);
        now=iqpc();
        if(now-last_report>=(uint64_t)interp_hz.QuadPart*5){
            double secs=(double)(now-last_report)/interp_hz.QuadPart;
            LOG("[LEAN-INTERP] 5s: presented %.1f FPS, game frames %.1f/s, replay %.2f ms, draws matched %.0f%% (snapped %.1f%%), recording %.2f ms per game frame, execute %.2f ms per present, %.0f ops and %.1f MB per game frame\n",
                presents/secs,ist.frames/secs,ist.replays?ist.replay_ms/ist.replays:0.0,ist.draws?100.0*ist.matched/ist.draws:0.0,ist.draws?100.0*ist.snapped/ist.draws:0.0,
                ist.frames?(double)ist.rec_ticks*1000.0/interp_hz.QuadPart/ist.frames:0.0,ist.replays?(double)ist.exec_ticks*1000.0/interp_hz.QuadPart/ist.replays:0.0,ist.frames?(double)ist.rec_ops/ist.frames:0.0,ist.frames?(double)ist.rec_bytes/ist.frames/1048576.0:0.0);
            {static LONG64 lw;LONG64 w=lean_game_lock_wait;   /* how long the game waited for the renderer lock */
             LOG("[LEAN-INTERP] game waited %.2f ms per game frame for the renderer; per present: constants %.2f ms, matching %.2f ms\n",ist.frames?(double)(w-lw)*1000.0/interp_hz.QuadPart/ist.frames:0.0,
                 ist.replays?(double)ist.blend_ticks*1000.0/interp_hz.QuadPart/ist.replays:0.0,ist.replays?(double)ist.match_ticks*1000.0/interp_hz.QuadPart/ist.replays:0.0);lw=w;}
            if(uncapped>0)LOG("[LEAN-INTERP] uncapped: pause between in-between frames %.2f ms (game %.0f frames a second, recent best %.0f)\n",unc_gap_ms,unc_gfps,unc_best);
            if(irs.rotated||irs.not_rigid||irs.jump)LOG("[LEAN-INTERP] turning draws: %llu blended by angle, %llu not rigid, %llu screen jumps, %llu below 2 degrees\n",
                (unsigned long long)irs.rotated,(unsigned long long)irs.not_rigid,(unsigned long long)irs.jump,(unsigned long long)irs.tiny);
            memset(&irs,0,sizeof irs);
            presents=0;memset(&ist,0,sizeof ist);last_report=now;
        }
        if(!swap_vsync&&target>0){
            next+=(uint64_t)(interp_hz.QuadPart/target);
            if((int64_t)(next-now)<0)next=now;
            lean_wait_until(next,unc_timer,interp_hz.QuadPart);   /* high-resolution timer: a cap is kept to ~0.5 ms */
        }
    }
    return 0;
}
/* Orderly stop before the process ends at the time limit (watchdog): let the presenter
 * thread finish its current frame and leave, then hold the context lock so no thread
 * touches D3D11 during exit, flush and wait (bounded) for the GPU to go idle. */
/* Window close: stop the presenter thread before the window goes away, then flush.
 * Unlike lean_d3d_quiesce the context lock is released, so the game thread can still
 * reach its own exit path. */
void lean_d3d_stop_presenter(void){
    if(interp_on){InterlockedExchange(&interp_quit,1);for(int i=0;i<100&&!interp_quit_done;i++)Sleep(5);}
    if(!ctx)return;
    CTX_LOCK();ID3D11DeviceContext_Flush(ctx);CTX_UNLOCK();
    LOG("[LEAN] presenter stopped for window close (%s)\n",interp_quit_done?"stopped":"not running");
}
void lean_d3d_quiesce(void){
    if(interp_on){InterlockedExchange(&interp_quit,1);for(int i=0;i<100&&!interp_quit_done;i++)Sleep(5);}
    if(!ctx||!dev)return;
    CTX_LOCK();   /* deliberately kept: the process is about to exit */
    ID3D11DeviceContext_ClearState(ctx);ID3D11DeviceContext_Flush(ctx);
    D3D11_QUERY_DESC qd={D3D11_QUERY_EVENT,0};ID3D11Query *q=NULL;
    if(SUCCEEDED(ID3D11Device_CreateQuery(dev,&qd,&q))){ID3D11DeviceContext_End(ctx,(ID3D11Asynchronous*)q);
        for(int i=0;i<200&&ID3D11DeviceContext_GetData(ctx,(ID3D11Asynchronous*)q,NULL,0,0)==S_FALSE;i++)Sleep(1);
        ID3D11Query_Release(q);}
    LOG("[LEAN] D3D quiesced for exit (presenter %s)\n",interp_quit_done?"stopped":"not running");
}
