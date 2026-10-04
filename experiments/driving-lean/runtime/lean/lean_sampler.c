/* Lean diagnostic: sample every thread's host call stack (LEAN_SAMPLE_AT_MS,
 * LEAN_SAMPLE_FOR_MS) and print a histogram of recompiled function names.
 * Recompiled functions map one-to-one to guest functions (sub_XXXXXXXX). */
#include <windows.h>
#include <tlhelp32.h>
#include <dbghelp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma comment(lib,"dbghelp.lib")

typedef struct {char key[256];DWORD tid;unsigned count;} Bucket;
#define NB 4096
static Bucket buckets[NB];
static unsigned nbuckets;

static void add(DWORD tid,const char *key){
    for(unsigned i=0;i<nbuckets;i++)if(buckets[i].tid==tid&&!strcmp(buckets[i].key,key)){buckets[i].count++;return;}
    if(nbuckets<NB){buckets[nbuckets].tid=tid;strncpy(buckets[nbuckets].key,key,255);buckets[nbuckets].count=1;nbuckets++;}
}
static int cmp(const void *a,const void *b){const Bucket *x=a,*y=b;if(x->tid!=y->tid)return x->tid<y->tid?-1:1;return (int)y->count-(int)x->count;}

static void name(HANDLE p,DWORD64 a,char *out,size_t cap){
    char buf[sizeof(SYMBOL_INFO)+128];SYMBOL_INFO *s=(SYMBOL_INFO*)buf;memset(buf,0,sizeof buf);s->SizeOfStruct=sizeof(SYMBOL_INFO);s->MaxNameLen=127;
    DWORD64 d=0;if(SymFromAddr(p,a,&d,s))snprintf(out,cap,"%s",s->Name);else snprintf(out,cap,"%llX",(unsigned long long)a);
}
static DWORD WINAPI sampler(void *arg){
    (void)arg;
    const char *v=getenv("LEAN_SAMPLE_AT_MS");DWORD at=v?(DWORD)atoi(v):0;
    v=getenv("LEAN_SAMPLE_FOR_MS");DWORD span=v?(DWORD)atoi(v):3000;
    Sleep(at);
    HANDLE p=GetCurrentProcess();SymSetOptions(SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS);SymInitialize(p,NULL,TRUE);
    DWORD self=GetCurrentThreadId(),pid=GetCurrentProcessId();
    ULONGLONG end=GetTickCount64()+span;unsigned rounds=0;
    while(GetTickCount64()<end){
        HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);THREADENTRY32 te;te.dwSize=sizeof te;
        if(snap!=INVALID_HANDLE_VALUE&&Thread32First(snap,&te))do{
            if(te.th32OwnerProcessID!=pid||te.th32ThreadID==self)continue;
            HANDLE t=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,te.th32ThreadID);if(!t)continue;
            if(SuspendThread(t)!=(DWORD)-1){
                CONTEXT c;memset(&c,0,sizeof c);c.ContextFlags=CONTEXT_FULL;
                if(GetThreadContext(t,&c)){
                    STACKFRAME64 f;memset(&f,0,sizeof f);f.AddrPC.Offset=c.Rip;f.AddrPC.Mode=AddrModeFlat;f.AddrStack.Offset=c.Rsp;f.AddrStack.Mode=AddrModeFlat;f.AddrFrame.Offset=c.Rbp;f.AddrFrame.Mode=AddrModeFlat;
                    char key[256]="";size_t n=0;
                    for(int depth=0;depth<6;depth++){
                        if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,p,t,&f,&c,NULL,SymFunctionTableAccess64,SymGetModuleBase64,NULL)||!f.AddrPC.Offset)break;
                        char nm[96];name(p,f.AddrPC.Offset,nm,sizeof nm);
                        int w=snprintf(key+n,sizeof key-n,"%s%s",depth?" < ":"",nm);if(w<0||(size_t)w>=sizeof key-n)break;n+=(size_t)w;
                    }
                    add(te.th32ThreadID,key);
                }
                ResumeThread(t);
            }
            CloseHandle(t);
        }while(Thread32Next(snap,&te));
        if(snap!=INVALID_HANDLE_VALUE)CloseHandle(snap);
        rounds++;Sleep(10);
    }
    qsort(buckets,nbuckets,sizeof *buckets,cmp);
    fprintf(stderr,"[LEAN-SAMPLE] %u rounds over %lu ms\n",rounds,(unsigned long)span);
    DWORD last=0;unsigned shown=0;
    for(unsigned i=0;i<nbuckets;i++){
        if(buckets[i].tid!=last){last=buckets[i].tid;shown=0;}
        if(shown++<8)fprintf(stderr,"[LEAN-SAMPLE] tid=%lu n=%u %s\n",(unsigned long)buckets[i].tid,buckets[i].count,buckets[i].key);
    }
    fflush(stderr);
    return 0;
}
void lean_sampler_start(void){
    static int started;if(started||!getenv("LEAN_SAMPLE_AT_MS"))return;started=1;
    HANDLE h=CreateThread(NULL,0,sampler,NULL,0,NULL);if(h)CloseHandle(h);
}
