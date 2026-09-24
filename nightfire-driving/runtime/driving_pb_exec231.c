/* Compile the pinned executor with Driving-local diagnostic
 * policies: counter lookup263 and cached verbose logging, like the action
 * renderer. Sampling228 located repeated CRT environment-lock contention at
 * its per-method getenv call. Other original environment lookups remain unchanged.
 * The executor itself is serialized by driving_scanout152's consumer lock. */
#include <stdlib.h>
#include <string.h>
static char *driving_executor_getenv231(const char *name)
{
    if (!strcmp(name,"RECOMP_PB_EXEC_VERBOSE")) {
        static int enabled=-1;
        if(enabled<0)enabled=getenv("RECOMP_PB_EXEC_VERBOSE")!=NULL;
        return enabled ? "1" : NULL;
    }
    return getenv(name);
}
#define getenv driving_executor_getenv231
#include "generic263/nv2a_pb_exec263.inc"
#undef getenv
