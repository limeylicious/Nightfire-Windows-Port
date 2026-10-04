#ifndef DRIVING_DEPTH291_H
#define DRIVING_DEPTH291_H
/* Additional eligibility only AFTER the complete original material planner.
 * This does not admit a draw or replace any original state/program check. */
static unsigned driving_depth291(const uint32_t *s,const unsigned char *k){
 static int on=-1;if(on<0){DWORD error=GetLastError();const char *v=getenv("DRIVING_FRAGMENT_DEPTH291");on=v&&!strcmp(v,"1");SetLastError(error);}if(!on)return 0;
 static const uint32_t exact[][2]={
  {0x208,0x1128},{0x20c,0x14001400},{0x290,0x00100001},
  {0x32c,0},{0x330,0},{0x334,0},{0x338,0},
  {0x394,0},{0x398,0x4b7fffff},{0x1d78,1},{0x30c,1}
 };
 for(unsigned i=0;i<sizeof exact/sizeof exact[0];i++)
  if(!k[exact[i][0]/4]||s[exact[i][0]/4]!=exact[i][1])return 0;
 if(!k[0x384/4]||!k[0x388/4]||(s[0x384/4]&0x7fffffff)||(s[0x388/4]&0x7fffffff))return 0;
 if(!k[0x354/4]||(s[0x354/4]!=0x203&&s[0x354/4]!=0x207)||!k[0x35c/4]||s[0x35c/4]>1)return 0;
 return 291;
}
#endif
