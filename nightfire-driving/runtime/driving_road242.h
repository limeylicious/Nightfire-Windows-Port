#ifndef DRIVING_ROAD242_H
#define DRIVING_ROAD242_H
/* Experimental zero-projection compatibility, default off. The exact profile
 * was already matched and all textures copied into owned storage before this
 * helper. Matrix rows are original complete constants, not patched outputs. */
static int driving_road_enabled242(void){
 static int on=-1;if(on<0){const char *v=getenv("DRIVING_ROAD_WHITE242");on=v&&!strcmp(v,"1");}return on;
}
static int driving_road_zero_matrix242(unsigned profile,const NFVertexProgram *p){
 if(profile!=22||!p||p->mode!=6||p->start)return 0;
 for(unsigned pc=0;pc<53;pc++)if(p->valid[pc]!=15||memcmp(p->code[pc],code221_22[pc],16))return 0;
 for(unsigned r=13;r<=16;r++){
  if(p->constant_valid[r]!=15)return 0;
  for(unsigned c=0;c<4;c++)if(p->constant_words[r][c]!=0)return 0;
 }return 1;
}
static int driving_road_zero_matrix292(unsigned profile,const NFVertexProgram *p){
 if(!driving_material292_enabled()||profile!=23||!p||p->mode!=6||p->start)return 0;
 for(unsigned pc=0;pc<56;pc++)if(p->valid[pc]!=15||memcmp(p->code[pc],code221_23[pc],16))return 0;
 for(unsigned r=13;r<=16;r++){if(p->constant_valid[r]!=15)return 0;for(unsigned c=0;c<4;c++)if(p->constant_words[r][c])return 0;}
 return 1;
}
static void driving_road_specialize242(unsigned profile,const NFVertexProgram *p,NFHardwareMaterial221 *m){
 if(driving_road_enabled242()&&driving_road_zero_matrix242(profile,p)&&nf_material_white242(m))m->pixel.white_stage2_242=1;
 if(driving_road_zero_matrix292(profile,p)&&nf_pixel_white_contract292(&m->pixel)&&nf_material_white242(m))m->pixel.white_stage2_242=2;
}
#endif
