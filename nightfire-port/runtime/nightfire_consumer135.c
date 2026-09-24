#include "nightfire_consumer135.h"
#ifdef NIGHTFIRE_CONSUMER135
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#define NF135_KINDS 48
#define NF135_DETAILS 4
#define NF135_NAME 40
typedef struct {
 uint32_t a,b,c,thread; unsigned type,shadow,invalid;
 uint64_t calls,first_ms,last_ms;
} NF135Detail;
typedef struct {
 char name[NF135_NAME]; unsigned details,type;
 uint64_t calls,unrepresented,reads,writes,bytes,shadow256,shadow128,invalid;
 NF135Detail detail[NF135_DETAILS];
} NF135Kind;
typedef struct {
 NF135Kind kinds[NF135_KINDS]; unsigned used;
 uint64_t calls,unknown_kind,apu_calls,apu_active,apu_inactive,apu_changes;
 unsigned apu_seen;uint32_t apu[6];
} NF135Data;
static NF135Data data;
static SRWLOCK guard=SRWLOCK_INIT;
static INIT_ONCE once=INIT_ONCE_STATIC_INIT;
static int enabled;
static BOOL CALLBACK nf135_init(PINIT_ONCE o,PVOID p,PVOID *ctx){
 const char *s=getenv("NIGHTFIRE_CONSUMER135");(void)o;(void)p;(void)ctx;
 enabled=s && s[0]=='1' && !s[1];return TRUE;
}
static int nf135_on(void){InitOnceExecuteOnce(&once,nf135_init,NULL,NULL);return enabled;}
static unsigned nf135_shadow(uint32_t va,uint32_t bytes,unsigned *invalid){
 uint64_t end=(uint64_t)va+bytes;unsigned mask=0;
 *invalid=end>0x100000000ull;
 if(!bytes || *invalid)return 0;
 /* Compare full intervals, including an invalid request that starts outside
  * an alias and extends into it. Low guest addresses alone are not physical
  * aliases. No mapping or readability is asserted by this classifier. */
 for(unsigned i=0;i<2;i++){
  uint64_t base=i?0xf0000000ull:0x80000000ull;
  if((uint64_t)va<base+0x04000000u && end>base && ((uint64_t)va<base || end>base+0x04000000u))*invalid=1;
  if((uint64_t)va<base+0x005c9000u && end>base+0x00589000u)mask|=1;
  if((uint64_t)va<base+0x005d9000u && end>base+0x005c9000u)mask|=2;
 }
 return mask;
}
static void nf135_record(const char *kind,uint32_t a,uint32_t b,uint32_t c,unsigned type){
 NF135Kind *k=NULL;NF135Detail line={0};char name[NF135_NAME];unsigned emit=0,shadow=0,invalid=0;
 DWORD tid=GetCurrentThreadId();uint64_t now=GetTickCount64();
 if(!kind)kind="(null)";
 /* Refuse names too long instead of merging indistinguishable prefixes. */
 size_t len=strnlen(kind,NF135_NAME);
 if(type)shadow=nf135_shadow(a,b,&invalid);
 AcquireSRWLockExclusive(&guard);data.calls++;
 if(len<NF135_NAME){
  for(unsigned i=0;i<data.used;i++)if(data.kinds[i].type==type && !strcmp(data.kinds[i].name,kind)){k=&data.kinds[i];break;}
  if(!k && data.used<NF135_KINDS){k=&data.kinds[data.used++];memcpy(k->name,kind,len+1);k->type=type;}
 }
 if(k){
  k->calls++;if(type){k->reads+=!c;k->writes+=c!=0;k->bytes+=b;k->shadow256+=(shadow&1)!=0;k->shadow128+=(shadow&2)!=0;k->invalid+=invalid;}
  NF135Detail *d=NULL;
  for(unsigned i=0;i<k->details;i++){NF135Detail *v=&k->detail[i];if(v->a==a && v->b==b && v->c==c && v->thread==tid){d=v;break;}}
  if(!d && k->details<NF135_DETAILS){d=&k->detail[k->details++];d->a=a;d->b=b;d->c=c;d->thread=tid;d->type=type;d->shadow=shadow;d->invalid=invalid;d->first_ms=now;emit=1;}
  if(d){d->calls++;d->last_ms=now;if(emit){line=*d;memcpy(name,k->name,sizeof name);}}
  else k->unrepresented++;
 }else data.unknown_kind++;
 ReleaseSRWLockExclusive(&guard);
 /* Bounded at 4 details per48 categories. No stdio while holding our lock. */
 if(emit)fprintf(stderr,"[CONSUMER135] first kind=%s type=%s tid=%lu a=%08X b=%08X c=%08X shadow=%u invalid=%u ms=%llu\n",name,type?"range":"event",(unsigned long)line.thread,line.a,line.b,line.c,line.shadow,line.invalid,(unsigned long long)line.first_ms);
}
void nf_consumer135_event(const char *kind,uint32_t a,uint32_t b,uint32_t c){
 DWORD error=GetLastError();if(nf135_on())nf135_record(kind,a,b,c,0);SetLastError(error);
}
void nf_consumer135_range(const char *kind,uint32_t va,uint32_t bytes,unsigned write){
 DWORD error=GetLastError();if(nf135_on())nf135_record(kind,va,bytes,write!=0,1);SetLastError(error);
}
void nf_consumer135_apu(unsigned vp_active,uint32_t sectl,uint32_t fectl,uint32_t vpv,uint32_t vpsge,uint32_t vpssl){
 DWORD error=GetLastError();
 if(nf135_on()){
  uint32_t values[6]={vp_active!=0,sectl,fectl,vpv,vpsge,vpssl};unsigned changed;
  AcquireSRWLockExclusive(&guard);data.apu_calls++;data.apu_active+=vp_active!=0;data.apu_inactive+=!vp_active;
  changed=!data.apu_seen || memcmp(data.apu,values,sizeof values)!=0;
  if(changed){data.apu_seen=1;data.apu_changes++;memcpy(data.apu,values,sizeof values);}
  ReleaseSRWLockExclusive(&guard);
  if(changed){nf135_record("apu.branch",values[0],sectl,fectl,0);nf135_record("apu.registers",vpv,vpsge,vpssl,0);}
 }
 SetLastError(error);
}
void nf_consumer135_report(FILE *stream){
 DWORD error=GetLastError();
 if(nf135_on() && stream){
  /* Heap snapshot avoids a large caller stack frame; diagnostic allocation
   * failure cannot alter game actions or leave the data lock held. */
  NF135Data *s=(NF135Data*)malloc(sizeof *s);
  if(!s){fprintf(stream,"[CONSUMER135] report unavailable: host allocation failed; ownership UNKNOWN\n");SetLastError(error);return;}
  AcquireSRWLockShared(&guard);memcpy(s,&data,sizeof *s);ReleaseSRWLockShared(&guard);
  fprintf(stream,"[CONSUMER135] summary calls=%llu kinds=%u unknown_kind_calls=%llu apu_calls=%llu apu_active=%llu apu_inactive=%llu apu_changes=%llu coverage=instrumented-sites-only ownership=UNKNOWN\n",(unsigned long long)s->calls,s->used,(unsigned long long)s->unknown_kind,(unsigned long long)s->apu_calls,(unsigned long long)s->apu_active,(unsigned long long)s->apu_inactive,(unsigned long long)s->apu_changes);
  for(unsigned i=0;i<s->used;i++){
   NF135Kind *k=&s->kinds[i];fprintf(stream,"[CONSUMER135] total kind=%s type=%s calls=%llu detail_unrepresented_calls=%llu reads=%llu writes=%llu bytes=%llu shadow256=%llu shadow128=%llu invalid_ranges=%llu\n",k->name,k->type?"range":"event",(unsigned long long)k->calls,(unsigned long long)k->unrepresented,(unsigned long long)k->reads,(unsigned long long)k->writes,(unsigned long long)k->bytes,(unsigned long long)k->shadow256,(unsigned long long)k->shadow128,(unsigned long long)k->invalid);
  }
  free(s);fflush(stream);
 }
 SetLastError(error);
}
#endif
