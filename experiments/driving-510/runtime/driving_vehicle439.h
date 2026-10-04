/*439 Default-OFF material admission for existing exact shader families.
 * Original values pass untouched to fog217/material221 and D3D state mapping.
 * Does not admit new shader code, unknown methods, pointers or target formats. */
#ifndef DRIVING_VEHICLE439_H
#define DRIVING_VEHICLE439_H
static int vehicle439_enabled(void){
 static int enabled=-1;if(enabled<0){DWORD saved=GetLastError();const char *v=getenv("DRIVING_VEHICLE439");enabled=v&&!strcmp(v,"1");SetLastError(saved);}return enabled;
}
static int vehicle439_fog(const uint32_t*s,const unsigned char*k){
 if(!k[0x2a4/4]||s[0x2a4/4]>1)return 0;
 if(!s[0x2a4/4])return 1;
 if(!k[0x29c/4]||s[0x29c/4]!=0x2601||!k[0x2a0/4]||s[0x2a0/4]!=2||
    !k[0x9c0/4]||!k[0x9c4/4]||!k[0x9c8/4]||s[0x9c8/4])return 0;
 float bias,slope;memcpy(&bias,&s[0x9c0/4],4);memcpy(&slope,&s[0x9c4/4],4);
 return isfinite(bias)&&isfinite(slope);
}
static int vehicle439_field(const uint32_t*s,const unsigned char*k,unsigned m,uint32_t expected){
 if(!vehicle439_enabled())return 0;uint32_t v=s[m/4];
 if(m==0x2a8||m==0x9c0||m==0x9c4)return vehicle439_fog(s,k);
 /* Both original comparison faces are mapped by the existing backend. */
 if(m==0x39c)return (v==0x404||v==0x405)&&(expected==0x404||expected==0x405);
 if((m==0x300||m==0x304)&&v<=1&&expected<=1)return 1;
 /* Only MAX_ANISOTROPY bits0x30 vary; LOD clamps/enable stay exact.
  * Existing material sampler supports1/2/4/8 and consumes actual control. */
 if(m>=0x1b0c&&m<=0x1bcc&&(m-0x1b0c)%64==0){
  unsigned stage=(m-0x1b0c)/64;
  return ((s[0x1e70/4]>>(stage*5))&31)==1&&((v^expected)&~0x30u)==0;
 }
 return 0;
}
#endif
