/* WPM may temporarily change and restore page protection inside Windows.
 * No callback, GPU barrier or flush belongs inside this mutation scope. */
#ifndef DRIVING_WRITEGUARD270_H
#define DRIVING_WRITEGUARD270_H
#include <windows.h>
#include "driving_permissions264.h"
static BOOL driving_write_process270(HANDLE process,LPVOID destination,LPCVOID source,SIZE_T bytes,SIZE_T *written)
{
    unsigned token=driving_permissions264_write_begin();
    BOOL result=FALSE;
    __try {
        result=WriteProcessMemory(process,destination,source,bytes,written);
    } __finally {
        /* Also release on structured exceptions; do not swallow them.
         * The 264 helpers preserve the API's LastError on normal return. */
        driving_permissions264_write_end(token);
    }
    return result;
}
#endif
