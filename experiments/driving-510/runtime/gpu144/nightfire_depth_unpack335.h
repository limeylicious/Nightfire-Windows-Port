#ifndef NIGHTFIRE_DEPTH_UNPACK335_H
#define NIGHTFIRE_DEPTH_UNPACK335_H
/*335 exact q24->D32 representation, only under the admitted FP controls.
 * Source q is at most24 bits, so CVTDQ2PS is exact; integer bit adjustment
 * reproduces the existing double-division rounding for every q. */
static int unpack335_mode(void){
 unsigned c=_mm_getcsr();return (c&_MM_ROUND_MASK)==_MM_ROUND_NEAREST&&
  (c&_MM_MASK_MASK)==_MM_MASK_MASK&&(c&_MM_EXCEPT_INEXACT)!=0;
}
static void unpack335_fast(float*dst,const uint32_t*src,unsigned n){
 const __m128i zero=_mm_setzero_si128(),adjust=_mm_set1_epi32(0x0bffffff);
 for(unsigned i=0;i<n;i+=4){
  __m128i q=_mm_srli_epi32(_mm_loadu_si128((const __m128i*)(src+i)),8);
  __m128i z=_mm_sub_epi32(_mm_castps_si128(_mm_cvtepi32_ps(q)),adjust);
  z=_mm_andnot_si128(_mm_cmpeq_epi32(q,zero),z);
  _mm_storeu_ps(dst+i,_mm_castsi128_ps(z));
 }
}

static int depth335_enabled(void){static int on=-1;if(on<0){NBGuard334 g;nb334_save(&g);const char*v=getenv("DRIVING_DEPTH_UNPACK335");on=v&&!strcmp(v,"1");nb334_restore(&g);}return on;}
static uint64_t depth335_selected,depth335_fallback,depth335_fast_rows,depth335_original_rows;
static void unpack335_row(float*dst,const uint32_t*src,unsigned n){
 /* A clear precision flag retains the complete ORIGINAL row. If that work
  * naturally sets precision, later rows may use the already-proven fast mode.
  * No status bit is synthesized; all-endpoint rows can stay original forever. */
 if(_mm_getcsr()&_MM_EXCEPT_INEXACT){unpack335_fast(dst,src,n);depth335_fast_rows++;}
 else{nf_depth_unpack(dst,src,n);depth335_original_rows++;}
}
static int depth335_choose(unsigned n,int admitted){
 if(!depth335_enabled())return 0;
 unsigned csr=_mm_getcsr();int controls=(csr&_MM_ROUND_MASK)==_MM_ROUND_NEAREST&&(csr&_MM_MASK_MASK)==_MM_MASK_MASK;
 int yes=admitted&&n&&!(n&3)&&controls;depth335_selected+=yes;depth335_fallback+=!yes;
 uint64_t calls=depth335_selected+depth335_fallback;
 if(calls==1||calls%1024==0){NBGuard334 g;nb334_save(&g);fprintf(stderr,"[DEPTH-UNPACK335] selected=%llu fallback=%llu fast_rows=%llu original_precision_rows=%llu exact_SSE2=1 private_import_only=1\n",(unsigned long long)depth335_selected,(unsigned long long)depth335_fallback,(unsigned long long)depth335_fast_rows,(unsigned long long)depth335_original_rows);nb334_restore(&g);}
 return yes;
}
#endif
