/*400 Exact335 CPU representation only, scoped to one pair-owned import.
 * No depth272 scope is manufactured. Never a content/ownership certificate. */
#ifndef NIGHTFIRE_PAIR_IMPORT400_H
#define NIGHTFIRE_PAIR_IMPORT400_H
typedef struct {ID3D11Texture2D *depth,*transfer;} PairImport400;
static PairImport400 pair_import400;
static unsigned pair_profile400;
void nf_hw_pair_import_profile400(unsigned profile){pair_profile400=profile;}
static int pair_setting400=-1;
static int pair_enabled400(void){
 if(pair_setting400<0){NBGuard334 g;nb334_save(&g);const char*v=getenv("DRIVING_PAIR_IMPORT400");
  pair_setting400=v&&!strcmp(v,"1");nb334_restore(&g);
 }return pair_setting400;
}
static int pair_admitted400(void){
 return pair_import400.depth && pair_import400.depth==depth && pair_import400.transfer==depth_transfer && width==640 && height==480;
}
#endif
