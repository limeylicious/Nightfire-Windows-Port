/* Read-only bounded aggregate of the actual methods forcing batch publication.
 * No admission/order change. Consumer-thread only; explicitly opt-in. */
#ifndef DRIVING_BARRIER242_H
#define DRIVING_BARRIER242_H
static void driving_barrier242(unsigned method,unsigned value)
{
 static int enabled=-1;
 static unsigned total,count[2048],last[2048];
 if(enabled<0){const char*v=getenv("DRIVING_BARRIER242");enabled=v&&!strcmp(v,"1");}
 if(!enabled||(method&3)||method>=8192)return;
 unsigned at=method/4;count[at]++;last[at]=value;total++;
 if(total!=1&&total%600)return;
 unsigned chosen[6]={2048,2048,2048,2048,2048,2048};
 fprintf(stderr,"[BARRIER242] total=%u top-methods=count:last-value",total);
 for(unsigned k=0;k<6;k++){
  unsigned best=2048;
  for(unsigned i=0;i<2048;i++){
   unsigned seen=0;for(unsigned j=0;j<k;j++)seen|=chosen[j]==i;
   if(!seen&&count[i]&&(best==2048||count[i]>count[best]))best=i;
  }
  if(best==2048)break;chosen[k]=best;
  fprintf(stderr," %04X=%u:%08X",best*4,count[best],last[best]);
 }
 fputc('\n',stderr);
}
#endif
