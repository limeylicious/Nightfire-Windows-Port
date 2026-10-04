/*352 SOURCE ONLY. Compute results have not been independently validated.
 * Mandatory CPU comparison: no option to silently promote GPU-only results.
 * All CPU reference work happens in the original vertex order; a CPU error
 * stops before this optional diagnostic dispatch. Known-good outputs remain
 * authoritative, including mismatches. This is NOT a faster-renderer claim. */
#ifndef DRIVING_TRANSFORM352_H
#define DRIVING_TRANSFORM352_H
#include <fenv.h>
#include <errno.h>
#include <xmmintrin.h>
static int renderer_compare352(const NFVertexProgram *program,
 const NFHardwareInputVertex *input,const NFHardwareVertexResult352 *expected,unsigned count)
{
 if(!renderer_compute352())return 1;
 DWORD saved=GetLastError();int crt=errno;fenv_t env;unsigned csr=_mm_getcsr();fegetenv(&env);
 static DrivingStorage228 storage;static unsigned calls,equal,unequal,refused;
 NFHardwareVertexResult352 *gpu=driving_storage228(&storage,(size_t)count*sizeof *gpu);
 uint32_t failure=0;int result=-1;
 if(!gpu)failure=(uint32_t)E_OUTOFMEMORY;
 else result=nf_hw_vertex_compute352(program,input,count,gpu,&failure);
 calls++;
 if(result>0){
  unsigned vertex=0;while(vertex<count&&!memcmp(&gpu[vertex],&expected[vertex],sizeof *gpu))vertex++;
  if(vertex==count)equal++;else{
   unequal++;
   if(unequal<=8){
    unsigned word=0;const unsigned char *a=(const unsigned char*)&gpu[vertex],*b=(const unsigned char*)&expected[vertex];
    while(word<sizeof *gpu/4&&!memcmp(a+word*4,b+word*4,4))word++;
    uint32_t av=0,bv=0;if(word<sizeof *gpu/4){memcpy(&av,a+word*4,4);memcpy(&bv,b+word*4,4);}
    fprintf(stderr,"[COMPUTE352] mismatch call=%u vertex=%u word=%u gpu=%08X cpu=%08X cpu_result_retained=1\n",calls,vertex,word,av,bv);
   }
  }
 }else if(!result)refused++;
 if(calls<=8||!(calls%120)||result<0)
  fprintf(stderr,"[COMPUTE352] compared=%u equal=%u unequal=%u refused=%u status=%d failure=%08X measured_speedup=0\n",calls,equal,unequal,refused,result,failure);
 driving_storage_release228(&storage,gpu);
 fesetenv(&env);_mm_setcsr(csr);errno=crt;SetLastError(saved);
 if(result<0)fail143("vertex compute352 backend failure",failure,count);
 return 1;
}
#endif
