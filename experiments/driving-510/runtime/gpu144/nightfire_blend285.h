/* Exact observed Driving material blend306/303. No Action/generic admission. */
#ifndef NIGHTFIRE_BLEND285_H
#define NIGHTFIRE_BLEND285_H
static int nf_blend285_enabled(void){
 static int setting=-1;DWORD e=GetLastError();
 if(setting<0){const char*v=getenv("DRIVING_ADMISSION283");setting=v&&!strcmp(v,"1");}
 SetLastError(e);return setting;
}
#endif
