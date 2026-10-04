/*422 A successful explicit original import, not guest ownership, certifies
 * canonical D32 only inside one already-admitted private421 lane. */
#ifndef NIGHTFIRE_IMPORT422_H
#define NIGHTFIRE_IMPORT422_H
#ifdef NF_DEPTH_PING272_AVAILABLE
typedef struct {
 NFDepthScope272*scope;ID3D11Texture2D*texture;ID3D11DepthStencilView*view;
 uint64_t draws,clears,transfers,uploads,upload_clears;unsigned kind;
} NFDepthImport422;
static uint64_t depth422_clears,depth422_copies,depth422_refused;
#ifdef NF_PAIR234_TEST
static uint64_t depth422_test_captures;
#endif
static int depth422_enabled(void){static int on=-1;if(on<0){NBGuard334 g;nb334_save(&g);const char*v=getenv("DRIVING_DEPTH_IMPORT422");on=v&&!strcmp(v,"1");nb334_restore(&g);}return on;}
static int depth422_nearest(void){unsigned c=_mm_getcsr();return (c&(_MM_ROUND_MASK|_MM_MASK_MASK))==_MM_MASK_MASK;}
static void depth422_capture(NFDepthImport422*p,unsigned kind){
 NFDepthScope272*s=depth272_current;
 if(!s||!s->private331||!s->plain421||!depth422_enabled()||(kind!=1&&kind!=2)||(kind==2&&!depth422_nearest()))return;
 if(s->at>1||s->slots[s->at].texture!=depth||s->slots[s->at].dsv!=dsv||width!=640||height!=480)return;
 p->scope=s;p->texture=depth;p->view=dsv;p->draws=draws;p->clears=gpu_depth_clears;
 p->transfers=transfers;p->uploads=depth_upload_copies;p->upload_clears=depth_upload_clears;p->kind=kind;
#ifdef NF_PAIR234_TEST
 depth422_test_captures++;
#endif
}
static void depth422_commit(const NFDepthImport422*p){
 if(!p->kind)return;NFDepthScope272*s=depth272_current;
 if(s!=p->scope||!s||!s->private331||!s->plain421||s->in_draw331||!bound.valid||
    s->at>1||s->slots[s->at].texture!=p->texture||s->slots[s->at].dsv!=p->view||
    depth!=p->texture||dsv!=p->view||draws!=p->draws||gpu_depth_clears!=p->clears||
    transfers!=p->transfers||depth_upload_copies!=p->uploads||depth_upload_clears!=p->upload_clears||
    (p->kind==2&&!depth422_nearest())){depth422_refused++;return;}
 s->canonical331=1;s->partial332=0;s->delta333_known=s->write333_known=0;depth331_snapshot(s);
 depth422_clears+=p->kind==1;depth422_copies+=p->kind==2;
 uint64_t n=depth422_clears+depth422_copies;
 if(n==1||!(n%1024)){NBGuard334 g;nb334_save(&g);fprintf(stderr,"[DEPTH-IMPORT422] clear=%llu full_copy=%llu refused=%llu successful_begin_only=1 original_flush_only=1\n",(unsigned long long)depth422_clears,(unsigned long long)depth422_copies,(unsigned long long)depth422_refused);nb334_restore(&g);}
}
#else
typedef struct{unsigned unused;} NFDepthImport422;
static int depth422_nearest(void){return 0;}
static void depth422_capture(NFDepthImport422*p,unsigned kind){(void)p;(void)kind;}
static void depth422_commit(const NFDepthImport422*p){(void)p;}
#endif
#endif
