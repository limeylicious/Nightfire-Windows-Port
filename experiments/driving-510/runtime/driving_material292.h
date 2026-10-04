#ifndef DRIVING_MATERIAL292_H
#define DRIVING_MATERIAL292_H
#include <windows.h>
#include <stdlib.h>
#include <string.h>
static int driving_material292_setting=-1;
static int driving_material292_enabled(void){
 if(driving_material292_setting<0){DWORD saved=GetLastError();const char*v=getenv("DRIVING_MATERIAL292");driving_material292_setting=v&&!strcmp(v,"1");SetLastError(saved);}
 return driving_material292_setting;
}
#endif
