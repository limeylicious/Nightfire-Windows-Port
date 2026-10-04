/* Opt-in observation only. No permission to retain GPU ownership. */
#ifndef DRIVING_ACCESS321_H
#define DRIVING_ACCESS321_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
enum {
 DA321_GENERATED_READ=1,DA321_GENERATED_WRITE,DA321_KERNEL_READ,DA321_KERNEL_WRITE,
 DA321_RENDER_READ,DA321_RENDER_WRITE,DA321_ATOMIC,DA321_COPY211_READ,
 DA321_COPY211_WRITE,DA321_SPLIT_READ,DA321_JOIN_WRITE,DA321_FILE_READ_DEST,
 DA321_FILE_WRITE_SOURCE,DA321_RPM_SOURCE,DA321_RPM_DEST,DA321_POINTER_ESCAPE,
 DA321_COLOR_CLEAR_READ,DA321_COLOR_CLEAR_WRITE,DA321_DEPTH_CLEAR_READ,DA321_DEPTH_CLEAR_WRITE
};
void driving_access321_init(uintptr_t offset);
void driving_access321_note(const void *pointer,size_t bytes,unsigned kind);
void driving_access321_publish(const uint32_t address[2],const uint32_t bytes[2],void *const native[2]);
void driving_access321_report(void);
int driving_access321_backing(uintptr_t offset);
void *driving_access321_memcpy(void *d,const void *s,size_t n,unsigned kind);
void *driving_access321_memmove(void *d,const void *s,size_t n,unsigned kind);
void *driving_access321_memset(void *d,int value,size_t n,unsigned kind);
BOOL driving_access321_readfile(HANDLE h,void *p,DWORD n,DWORD *done,OVERLAPPED *ov);
BOOL driving_access321_writefile(HANDLE h,const void *p,DWORD n,DWORD *done,OVERLAPPED *ov);
BOOL driving_access321_rpm(HANDLE h,const void *p,void *out,SIZE_T n,SIZE_T *done);
static __forceinline uintptr_t driving_access321_pointer(uintptr_t p,size_t n,unsigned kind){
 driving_access321_note((const void*)p,n,kind);return p;
}
#endif
