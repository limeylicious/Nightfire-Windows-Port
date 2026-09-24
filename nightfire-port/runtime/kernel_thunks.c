/* Local output-path adapter; the tested toolkit source stays unchanged.
 * The pinned kernel has XBOX_LOG_LEVEL, but no log-path environment option. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
static FILE *nightfire_kernel_fopen(const char *path, const char *mode)
{
    if (!strcmp(path, "xbox_kernel.log")) {
        const char *configured = getenv("NIGHTFIRE_KERNEL_LOG");
        _mkdir("logs");
        path = configured && *configured ? configured : "logs/xbox_kernel.log";
    }
    return fopen(path, mode);
}
#define fopen nightfire_kernel_fopen
#include NIGHTFIRE_TOOLKIT_KERNEL_THUNKS
#undef fopen
