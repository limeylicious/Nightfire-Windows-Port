/*275: omit only the optional private248 first-lane submission hint.*/
#ifndef NIGHTFIRE_LANE_FLUSH275_H
#define NIGHTFIRE_LANE_FLUSH275_H
static int lane_flush275_omitted(void){
 static int on=-1;static uint64_t omitted;
 if(on<0){DWORD e=GetLastError();const char*v=getenv("DRIVING_NO_LANE_FLUSH275");on=v&&!strcmp(v,"1");SetLastError(e);}
 if(!on)return 0;
 if(++omitted==1||!(omitted%120)){DWORD e=GetLastError();fprintf(stderr,"[LANE-FLUSH275] omitted=%llu final_blocking_maps=1 failure_flush_retained=1\n",(unsigned long long)omitted);SetLastError(e);}
 return 1;
}
#endif
