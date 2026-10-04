/*453: default-OFF failure repair for the experimental451 publication gate.
 * Requires452's permission-generation tracking. No change to rendering order,
 * ownership duration, or normal/frozen builds. */
#ifndef DRIVING_ACCESS453_H
#define DRIVING_ACCESS453_H
static int pc453_enabled(void){
 static int mode=-1;
 if(mode<0){DWORD e=GetLastError();int crt=errno;const char*v=getenv("DRIVING_PC453");
  mode=v&&!strcmp(v,"1")&&pc452_enabled();errno=crt;SetLastError(e);}
 return mode;
}
/* Only the consumer may enter publication. A second AV while it is restoring
 * pages/publishing is a real failure, not another request to retry the draw. */
static int pc453_publishing;
static void pc453_rollback(unsigned span,DWORD protection){
 DWORD previous=0;
 if(!pc452_protect(pc451.base[span],DRIVING_PAIR234,protection,&previous))
  fail143("pc453 protection rollback failed",(uint32_t)GetLastError(),span);
 if(previous!=PAGE_NOACCESS)
  fail143("pc453 protection changed during rollback",previous,span);
}
#endif
