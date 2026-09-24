#ifndef NIGHTFIRE_SHADER_CACHE99_H
#define NIGHTFIRE_SHADER_CACHE99_H
/* Only generated Kelvin vertex HLSL, fixed entry/profile/options. DXBC remains
 * driver-portable input, not a native GPU pipeline cache. A rejected cached
 * shader must be retried by the caller with force_compile=1. */
#include <windows.h>
#include <d3dcompiler.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define NF_SHADER99_FLAGS (D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_IEEE_STRICTNESS)
static int nf_shader99_enabled(void){
#ifdef NIGHTFIRE_SHADER_CACHE99
    static int on=-1;if(on<0){const char *v=getenv("NIGHTFIRE_SHADER_CACHE99");on=v && !strcmp(v,"1");}return on;
#else
    return 0;
#endif
}
#ifdef NIGHTFIRE_SHADER_CACHE99
#include <wincrypt.h>
enum { NF_SHADER99_SCHEMA=1,NF_SHADER99_SOURCE_MAX=65536,NF_SHADER99_BLOB_MAX=4*1024*1024 };
typedef struct {
    uint32_t magic,schema,flags,source_bytes,blob_bytes;
    unsigned char source_hash[32],blob_hash[32],compiler_hash[32];
} NFShader99Header;
static struct {
    uint64_t compiles,hits,misses,rejects,writes,io_failures;
    uint64_t load_ticks,compile_ticks,write_ticks;
} nf_shader99_stats;
static uint64_t nf_shader99_clock(void){LARGE_INTEGER q;QueryPerformanceCounter(&q);return (uint64_t)q.QuadPart;}
static int nf_shader99_hash(const void *bytes,size_t length,unsigned char digest[32]){
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;DWORD size=32;int ok=0;
    if(length>MAXDWORD || !CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))return 0;
    if(CryptCreateHash(provider,CALG_SHA_256,0,0,&hash) && CryptHashData(hash,(const BYTE*)bytes,(DWORD)length,0))
        ok=CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0) && size==32;
    if(hash)CryptDestroyHash(hash);CryptReleaseContext(provider,0);return ok;
}
static int nf_shader99_compiler(unsigned char digest[32]){
    static int ready;static unsigned char saved[32];
    if(!ready){
        ready=-1;HMODULE module=NULL;WCHAR path[MAX_PATH];
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)(uintptr_t)&D3DCompile,&module))return 0;
        DWORD n=GetModuleFileNameW(module,path,MAX_PATH);if(!n || n>=MAX_PATH)return 0;
        const WCHAR *name=path;for(const WCHAR *p=path;*p;p++)if(*p==L'\\' || *p==L'/')name=p+1;
        /* SDK D3DCompiler.lib may expose an EXE import thunk. Its declared
         * compiler DLL must already be loaded; never load a guessed library. */
        if(_wcsnicmp(name,L"d3dcompiler_",12) || n<4 || _wcsicmp(path+n-4,L".dll")){
            module=GetModuleHandleW(D3DCOMPILER_DLL_W);if(!module)return 0;
            n=GetModuleFileNameW(module,path,MAX_PATH);if(!n || n>=MAX_PATH)return 0;
        }
        HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
        if(file==INVALID_HANDLE_VALUE)return 0;
        LARGE_INTEGER length;HCRYPTPROV provider=0;HCRYPTHASH hash=0;DWORD got=0,size=32;BYTE bytes[16384];int ok=0;
        if(GetFileSizeEx(file,&length) && length.QuadPart>0 && length.QuadPart<=64*1024*1024 &&
           CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT) && CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
            LONGLONG remaining=length.QuadPart;ok=1;
            while(remaining){DWORD count=remaining>sizeof bytes?sizeof bytes:(DWORD)remaining;
                if(!ReadFile(file,bytes,count,&got,NULL) || got!=count || !CryptHashData(hash,bytes,got,0)){ok=0;break;}remaining-=got;}
            if(ok)ok=CryptGetHashParam(hash,HP_HASHVAL,saved,&size,0) && size==32;
        }
        if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);
        if(ok)ready=1;
    }
    if(ready!=1)return 0;memcpy(digest,saved,32);return 1;
}
static int nf_shader99_path(const char *source,size_t length,NFShader99Header *header,WCHAR file[MAX_PATH]){
    memset(header,0,sizeof *header);
    if(!length || length>NF_SHADER99_SOURCE_MAX)return 0;
    header->magic=0x3939534e;header->schema=NF_SHADER99_SCHEMA;header->flags=NF_SHADER99_FLAGS;header->source_bytes=(uint32_t)length;
    if(!nf_shader99_hash(source,length,header->source_hash) || !nf_shader99_compiler(header->compiler_hash))return 0;
    /* Fixed compiler entry/profile/flags2/source-name are included explicitly. */
    struct {NFShader99Header header;char options[48];} key={0};key.header=*header;
    static const char options[]="nightfire-kelvin|vertex|vs_4_0|flags2=0";
    memcpy(key.options,options,sizeof options);
    unsigned char digest[32];if(!nf_shader99_hash(&key,sizeof key,digest))return 0;
    WCHAR input[MAX_PATH],directory[MAX_PATH];DWORD n=GetEnvironmentVariableW(L"NIGHTFIRE_SHADER_CACHE99_DIR",input,MAX_PATH);
    if(!n)wcscpy_s(input,MAX_PATH,L"cache\\shaders");else if(n>=MAX_PATH)return 0;
    n=GetFullPathNameW(input,MAX_PATH,directory,NULL);if(!n || n>=MAX_PATH || n+70>=MAX_PATH)return 0;
    wcscpy_s(file,MAX_PATH,directory);if(file[n-1]!=L'\\' && file[n-1]!=L'/')file[n++]=L'\\';
    static const WCHAR hex[]=L"0123456789abcdef";
    for(unsigned i=0;i<32;i++){file[n++]=hex[digest[i]>>4];file[n++]=hex[digest[i]&15];}
    wcscpy_s(file+n,MAX_PATH-n,L".dxbc");return 1;
}
static int nf_shader99_read(HANDLE file,void *p,DWORD n){DWORD got=0;return ReadFile(file,p,n,&got,NULL) && got==n;}
static int nf_shader99_load(const char *source,size_t length,ID3DBlob **blob){
    NFShader99Header expected,stored;WCHAR path[MAX_PATH];
    if(!nf_shader99_path(source,length,&expected,path)){nf_shader99_stats.io_failures++;return 0;}
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE){if(GetLastError()!=ERROR_FILE_NOT_FOUND && GetLastError()!=ERROR_PATH_NOT_FOUND)nf_shader99_stats.io_failures++;return 0;}
    LARGE_INTEGER size;char *copy=NULL;ID3DBlob *result=NULL;int ok=0;unsigned char hash[32];
    if(!GetFileSizeEx(file,&size) || !nf_shader99_read(file,&stored,sizeof stored))goto done;
    if(stored.magic!=expected.magic || stored.schema!=expected.schema || stored.flags!=expected.flags ||
       stored.source_bytes!=length || !stored.blob_bytes || stored.blob_bytes>NF_SHADER99_BLOB_MAX ||
       size.QuadPart!=(LONGLONG)(sizeof stored+length+stored.blob_bytes) ||
       memcmp(stored.source_hash,expected.source_hash,32) || memcmp(stored.compiler_hash,expected.compiler_hash,32))goto done;
    copy=(char*)malloc(length);if(!copy || !nf_shader99_read(file,copy,(DWORD)length) || memcmp(copy,source,length))goto done;
    if(FAILED(D3DCreateBlob(stored.blob_bytes,&result)))goto done;
    void *data=result->lpVtbl->GetBufferPointer(result);
    if(!nf_shader99_read(file,data,stored.blob_bytes) || stored.blob_bytes<4 || memcmp(data,"DXBC",4) ||
       !nf_shader99_hash(data,stored.blob_bytes,hash) || memcmp(hash,stored.blob_hash,32))goto done;
    *blob=result;result=NULL;ok=1;
done:
    free(copy);if(result)result->lpVtbl->Release(result);CloseHandle(file);
    if(!ok)nf_shader99_stats.rejects++;return ok;
}
static int nf_shader99_write_bytes(HANDLE f,const void *p,DWORD n){DWORD wrote=0;return WriteFile(f,p,n,&wrote,NULL) && wrote==n;}
static void nf_shader99_store(const char *source,size_t length,ID3DBlob *blob){
    NFShader99Header header;WCHAR path[MAX_PATH],directory[MAX_PATH],temp[MAX_PATH];
    size_t bytes=blob->lpVtbl->GetBufferSize(blob);const void *data=blob->lpVtbl->GetBufferPointer(blob);
    if(!bytes || bytes>NF_SHADER99_BLOB_MAX || !nf_shader99_path(source,length,&header,path) ||
       !nf_shader99_hash(data,bytes,header.blob_hash)){nf_shader99_stats.io_failures++;return;}
    header.blob_bytes=(uint32_t)bytes;wcscpy_s(directory,MAX_PATH,path);
    WCHAR *last=wcsrchr(directory,L'\\');if(!last)last=wcsrchr(directory,L'/');if(!last){nf_shader99_stats.io_failures++;return;}*last=0;
    for(WCHAR *p=directory+3;*p;p++)if(*p==L'\\' || *p==L'/'){WCHAR c=*p;*p=0;CreateDirectoryW(directory,NULL);*p=c;}
    CreateDirectoryW(directory,NULL);
    static LONG serial;HANDLE f=INVALID_HANDLE_VALUE;
    for(unsigned attempt=0;attempt<16;attempt++){
        int n=_snwprintf_s(temp,MAX_PATH,_TRUNCATE,L"%ls.%lu.%ld.tmp",path,GetCurrentProcessId(),InterlockedIncrement(&serial));
        if(n<0)break;
        f=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(f!=INVALID_HANDLE_VALUE)break;if(GetLastError()!=ERROR_FILE_EXISTS)break;
    }
    if(f==INVALID_HANDLE_VALUE){nf_shader99_stats.io_failures++;return;}
    int ok=nf_shader99_write_bytes(f,&header,sizeof header) && nf_shader99_write_bytes(f,source,(DWORD)length) &&
        nf_shader99_write_bytes(f,data,(DWORD)bytes) && FlushFileBuffers(f);
    if(!CloseHandle(f))ok=0;
    if(ok)ok=MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(ok)nf_shader99_stats.writes++;else{DeleteFileW(temp);nf_shader99_stats.io_failures++;}
}
#endif
static HRESULT nf_shader99_compile(const char *source,size_t length,ID3DBlob **code,ID3DBlob **errors,int force_compile,int *cache_hit){
    if(!source || !code || !cache_hit)return E_INVALIDARG;
    *code=NULL;if(errors)*errors=NULL;*cache_hit=0;
#ifdef NIGHTFIRE_SHADER_CACHE99
    int enabled=nf_shader99_enabled();uint64_t started=nf_shader99_clock();
    if(enabled && !force_compile){
        int hit=nf_shader99_load(source,length,code);nf_shader99_stats.load_ticks+=nf_shader99_clock()-started;
        if(hit){nf_shader99_stats.hits++;*cache_hit=1;return S_OK;}nf_shader99_stats.misses++;
    }
    started=nf_shader99_clock();nf_shader99_stats.compiles++;
#else
    (void)force_compile;
#endif
    HRESULT hr=D3DCompile(source,length,"nightfire-kelvin",NULL,NULL,"vertex","vs_4_0",NF_SHADER99_FLAGS,0,code,errors);
#ifdef NIGHTFIRE_SHADER_CACHE99
    nf_shader99_stats.compile_ticks+=nf_shader99_clock()-started;
    if(enabled && SUCCEEDED(hr)){started=nf_shader99_clock();nf_shader99_store(source,length,*code);nf_shader99_stats.write_ticks+=nf_shader99_clock()-started;}
#endif
    return hr;
}
static void nf_shader99_device_reject(void){
#ifdef NIGHTFIRE_SHADER_CACHE99
    nf_shader99_stats.rejects++;
#endif
}
static void nf_shader99_report(FILE *file){
#ifdef NIGHTFIRE_SHADER_CACHE99
    LARGE_INTEGER f;QueryPerformanceFrequency(&f);
    fprintf(file,"[GPU-SHADER99] enabled=%d compiles=%llu hits=%llu misses=%llu rejects=%llu writes=%llu io_failures=%llu load_ms=%.3f compile_ms=%.3f write_ms=%.3f\n",nf_shader99_enabled(),
        (unsigned long long)nf_shader99_stats.compiles,(unsigned long long)nf_shader99_stats.hits,(unsigned long long)nf_shader99_stats.misses,
        (unsigned long long)nf_shader99_stats.rejects,(unsigned long long)nf_shader99_stats.writes,(unsigned long long)nf_shader99_stats.io_failures,
        1000.0*nf_shader99_stats.load_ticks/f.QuadPart,1000.0*nf_shader99_stats.compile_ticks/f.QuadPart,1000.0*nf_shader99_stats.write_ticks/f.QuadPart);
#else
    (void)file;
#endif
}
#endif
