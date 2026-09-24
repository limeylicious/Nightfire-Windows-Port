static int resident313_blocked(void);
/* Optional native D3D11 rasterization. The existing game/vertex execution
 * supplies triangles. Synchronization copies real render targets back to guest
 * RAM before software rendering, unsupported clears, or presentation. The
 * verified full maximum-depth clear stays on GPU within the current drain.
 * No replacement scene. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nightfire_hardware.h"
#ifdef NIGHTFIRE_CONSUMER135
#include "nightfire_consumer135.h"
#endif
#ifdef NIGHTFIRE_SHADOW_LAYOUT131_DIAGNOSTIC
#define NF_SHADOW131_IMPLEMENTATION
#include "nightfire_shadow_layout131.h"
#endif
#include "nightfire_rgba_mips129.h"
#include "nightfire_bytes_equal.h"
#include "nightfire_constant_mask.h"
static NFConstantMaskCache constant_mask;
static int constant_mask_enabled(void){static int on=-1;if(on<0){const char *v=getenv("NIGHTFIRE_CONSTANT_MASK");on=!v || strcmp(v,"0");}return on;}
#ifdef NIGHTFIRE_TEXTURE_EQUAL114
#include "nightfire_texture_equal114.h"
static int texture114_enabled(void){static int on=-1;if(on<0){const char *v=getenv("NIGHTFIRE_TEXTURE_EQUAL114");on=v && !strcmp(v,"1");}return on;}
#endif
static int texture_bytes_equal(const void *a,const void *b,size_t length){
    static int enabled=-1;
    if(enabled<0){const char *v=getenv("NIGHTFIRE_TEXTURE_EQUAL");enabled=!v || strcmp(v,"0");}
#ifdef NIGHTFIRE_TEXTURE_EQUAL114
    if(enabled && length>=128 && texture114_enabled())return nf_texture_equal114(a,b,length);
#endif
    return enabled?nf_bytes_equal(a,b,length):!memcmp(a,b,length);
}
#ifdef NIGHTFIRE_COLOR_REUSE110
#ifdef NF_COLOR_REUSE110_BACKEND_TEST
#define NF_COLOR_REUSE110_TEST
#endif
#include "nightfire_color_reuse110.h"
static uint64_t color110_generation,color110_next_generation;
static int color110_on=-1;
static int color110_enabled(void){
 if(color110_on<0){const char *v=getenv("NIGHTFIRE_COLOR_REUSE110");color110_on=v && !strcmp(v,"1");}
 return color110_on;
}
#ifdef NF_COLOR_REUSE110_BACKEND_TEST
void nf_hw_color110_test_set(int on){nf_color_reuse110_test_reset();color110_on=!!on;}
uint64_t nf_hw_color110_test_hits(void){return nf_color_reuse110.exact;}
#endif
#endif
#include "nightfire_depth_transfer.h"
#include "nightfire_depth_observe83.h"
#include "nightfire_surface_probe83.h"
#include "nightfire_gpu_timing84.h"
#include "nightfire_gpu_sample254.h"
#include "nightfire_gpu_sample297.h"
#if defined(NIGHTFIRE_GPU_TIMING_DIAGNOSTIC) || defined(NIGHTFIRE_VERTEX_REUSE105_DIAGNOSTIC) || defined(NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC)
extern unsigned nightfire_gpu_timing_frame(void);
#endif
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
#include <intrin.h>
#include "nightfire_color_reupload109.h"
static uint64_t color109_generation,color109_next_generation;
static const char *sync109_reason="external";
static int sync_reason109(const char *reason);
#define HW_SYNC109(reason) sync_reason109(reason)
static int color109_ready(unsigned frame){
 static int initialized,armed;static const char *path;static unsigned seen,last;
 if(!initialized){initialized=1;path=getenv("NIGHTFIRE_COLOR_REUPLOAD109_TRIGGER");}
 if(!armed && path && (!seen || last!=frame)){
  seen=1;last=frame;unsigned token=0;FILE *f=fopen(path,"r");
  if(f){if(fscanf(f,"%u",&token)!=1)token=0;fclose(f);}
  if(token){armed=1;nf_color_reupload109_begin(frame);fprintf(stderr,"[COLOR-REUPLOAD109] trigger_frame=%u token=%u\n",frame,token);}
 }
 return armed && nf_color_reupload109_wants(frame);
}
#else
#define HW_SYNC109(reason) nf_hw_sync()
#endif
#ifdef NIGHTFIRE_VERTEX_REUSE105_DIAGNOSTIC
#include "nightfire_vertex_reuse105.h"
/* Diagnostic only: preserve the bytes actually uploaded, never read mapped
 * write-combined GPU memory. The extra snapshot copy invalidates FPS claims. */
static unsigned char *reuse105_scratch;
static uint64_t reuse105_scratch_failures;
static int reuse105_ready(unsigned frame){
 static int initialized,armed;static const char *path;
 static unsigned seen,last_frame;
 if(!initialized){initialized=1;path=getenv("NIGHTFIRE_VERTEX_REUSE105_TRIGGER");}
 if(!armed && path && (!seen || frame!=last_frame)){
  seen=1;last_frame=frame;unsigned token=0;FILE *f=fopen(path,"r");
  if(f){if(fscanf(f,"%u",&token)!=1)token=0;fclose(f);}
  if(token){armed=1;nf_vertex_reuse105_begin(frame);fprintf(stderr,"[GPU-REUSE105] trigger_frame=%u token=%u\n",frame,token);}
 }
 return armed && nf_vertex_reuse105_wants(frame);
}
#endif
#include "nightfire_vertex_hlsl.h"
#include "nightfire_shader_cache99.h"
#include "nightfire_fog.h"
#include "d3d8_swizzle.h"
#include "rgb565148.h"
#ifdef NIGHTFIRE_MOVIE_RGB232
#include "movie_rgb232.h"
static MovieRGB232Entry movie_rgb232[4];
static unsigned movie_rgb232_next;
#endif
#include "palette175.h"
#include "nightfire_host_spans261.h"
#ifdef NIGHTFIRE_TEXTURE_DECODE100
#include "nightfire_texture_decode100.h"
static uint64_t decode100_calls,decode100_bytes,decode100_ticks;
static int decode100_enabled(void){static int on=-1;if(on<0){const char *v=getenv("NIGHTFIRE_TEXTURE_DECODE100");on=v && !strcmp(v,"1");}return on;}
#endif
#define RELEASE(p) do{if(p){(p)->lpVtbl->Release(p);p=NULL;}}while(0)
#if defined(NIGHTFIRE_VERTEX_BATCH92) && (!defined(NIGHTFIRE_DIRECT_VERTEX88) || !defined(NIGHTFIRE_DIRECT_DEFAULT90))
#error NIGHTFIRE_VERTEX_BATCH92 requires direct88 and direct90
#endif
#define CALL_OK(x) do{HRESULT hr=(x);if(FAILED(hr)){fprintf(stderr,"[HW-GPU] %s failed %08lX\n",#x,(unsigned long)hr);return 0;}}while(0)
static ID3D11Device *dev;
static ID3D11DeviceContext *ctx;
static ID3D11VertexShader *vs;
static ID3D11VertexShader *projective_vs182;
static ID3D11PixelShader *projective_ps182;
static ID3D11PixelShader *resolve_ps193[2];
static ID3D11PixelShader *movie_ps200[2];
static ID3D11InputLayout *projective_layout182;
static ID3D11Buffer *projective_vb182;
typedef NFHardwareTransformed218 ProjectiveVertex182;
static ID3D11PixelShader *ps,*video_ps;
static ID3D11InputLayout *layout;
static ID3D11Buffer *vb,*cb;
static ID3D11Buffer *input_vb,*kelvin_cb,*input_ib;
static unsigned index_cursor;
static ID3D11InputLayout *input_layout;
static unsigned input_cursor; /* bytes: program input strides can differ */
static unsigned char saved_params[64],saved_kelvin[192*16+32];
static int params_valid,kelvin_valid;
static uint64_t input_bytes,constant_uploads;
#ifdef NIGHTFIRE_VERTEX_BATCH92
static uint64_t raw92_draws,raw92_strips,raw92_indices,raw92_triangles;
#endif
static int program_active;
static unsigned program_inputs;
static unsigned program_dynamic;
#ifdef NIGHTFIRE_DIRECT_VERTEX88
static int direct28_selected;
static uint64_t direct28_draws,direct28_bytes;
static int direct28_format_supported(void){
    static int supported=-1;
    if(supported<0){UINT flags=0;HRESULT hr=ID3D11Device_CheckFormatSupport(dev,DXGI_FORMAT_B8G8R8A8_UNORM,&flags);supported=SUCCEEDED(hr) && (flags&D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER)!=0;}
    return supported;
}
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
static unsigned direct90_selected;
static uint64_t direct90_draws,direct90_bytes;
static ID3D11Buffer *default90_vb;
static unsigned direct90_bit(unsigned mask){return mask==0xf?NF_DIRECT90_F28:mask==0x1f?NF_DIRECT90_1F32:mask==0x7f9f?NF_DIRECT90_7F9F32:0;}
static int default90_buffer(void){
 if(default90_vb)return 1;
 static const float value[4]={0,0,0,1};
 D3D11_BUFFER_DESC d={0};d.ByteWidth=sizeof value;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_VERTEX_BUFFER;
 D3D11_SUBRESOURCE_DATA initial={0};initial.pSysMem=value;
 return SUCCEEDED(ID3D11Device_CreateBuffer(dev,&d,&initial,&default90_vb));
}
#endif
static ID3D11GeometryShader *validated_geometry,*projective_geometry182;
static uint64_t input_draws,input_vertices,program_compiles,program_ticks;
static unsigned vertex_cursor;
static ID3D11RasterizerState *rs,*color_only_rs96;
/* Ordinary cull/winding states retain slots0..5. Explicit color-only states
 * use slots6..11 so alternating depth attachments cannot inherit Z clipping. */
static ID3D11RasterizerState *program_raster[12];
static ID3D11Texture2D *color,*depth,*color_read,*depth_transfer;
static ID3D11RenderTargetView *rtv;
static ID3D11DepthStencilView *dsv;
static unsigned width,height;
static int initialized,pending;
int nf_hw_clear_pending;
static uint64_t gpu_depth_clears,clear_alias_syncs;
static uint64_t depth_upload_clears,depth_upload_copies;
static uint64_t color_only96_begins,point_clamp96_begins,surface96_creates,surface96_reuses;
#ifdef NIGHTFIRE_RGBA_MIPS129
static uint64_t rgba129_begins;
#endif
#ifdef NIGHTFIRE_SWIZZLED_SHADOW131
static uint8_t swizzled131_upload[256*256*4];
static uint64_t swizzled131_imports,swizzled131_exports,swizzled131_begins;
static uint64_t swizzled132_imports,swizzled132_exports,swizzled132_begins;
#endif
static uint64_t reverse_subtract97_begins;
int nf_hw_blend97_enabled(void){
#ifdef NIGHTFIRE_GPU_BLEND97
    static int enabled=-1;
    if(enabled<0){const char *v=getenv("NIGHTFIRE_GPU_BLEND97");enabled=v && !strcmp(v,"1");}
    return enabled;
#else
    return 0;
#endif
}
int nf_hw_fallback96_enabled(void){
#ifdef NIGHTFIRE_GPU_FALLBACK96
    static int enabled=-1;
    if(enabled<0){const char *v=getenv("NIGHTFIRE_GPU_FALLBACK96");enabled=v && !strcmp(v,"1");}
    return enabled;
#else
    return 0;
#endif
}
#ifdef NIGHTFIRE_DEPTH_UPLOAD_TEST
static int test_depth_upload_clear=1;
#endif
static int depth_upload_clear_enabled(void){
#ifdef NIGHTFIRE_DEPTH_UPLOAD_TEST
    return test_depth_upload_clear;
#else
    static int enabled=-1;
    if(enabled<0){const char *v=getenv("NIGHTFIRE_GPU_DEPTH_UPLOAD_CLEAR");enabled=!v || strcmp(v,"0");}
    return enabled;
#endif
}
static NFHardwareState active;
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
#if !defined(NIGHTFIRE_SURFACE_COHERENCE108) || !defined(NIGHTFIRE_GPU_FALLBACK96)
#error NIGHTFIRE_DEFER_MAIN128 requires coherence108 and fallback96
#endif
#ifdef NIGHTFIRE_GPU_TIMING_DIAGNOSTIC
#error The single-interval GPU timing observer does not support deferred128
#endif
/* One queued paired image, pinned independently of the currently bound target.
 * No guest execution, presentation, or unguarded CPU consumer may cross it. */
static struct {
    int valid,resident130;
    NFHardwareState state,shadow;
    ID3D11Texture2D *color,*depth,*color_read,*depth_transfer;
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
    uint64_t generation109;
#endif
} deferred128;
static uint64_t defer128_queued,defer128_published,defer128_paired,defer128_early;
static int defer128_active_dirty;
static int defer128_on=-1;
static int defer128_enabled(void){
    if(defer128_on<0){const char *v=getenv("NIGHTFIRE_DEFER_MAIN128");defer128_on=v && !strcmp(v,"1");}
    return defer128_on;
}
#ifdef NF_DEFER128_TEST
void nf_hw_defer128_test_set(int enabled){defer128_on=!!enabled;}
void nf_hw_defer128_test_stats(uint64_t out[6]){
    out[0]=defer128_queued;out[1]=defer128_published;out[2]=defer128_paired;
    out[3]=defer128_early;out[4]=deferred128.valid;out[5]=pending;
}
#endif
#ifdef NIGHTFIRE_RESIDENT_MAIN130
static int resident130_on=-1;
static uint64_t resident130_retained,resident130_resumed,resident130_published,resident130_shadow_maps;
static int resident130_enabled(void){
    if(resident130_on<0){const char *v=getenv("NIGHTFIRE_RESIDENT_MAIN130");resident130_on=v && !strcmp(v,"1");}
    return resident130_on && !defer128_enabled();
}
#ifdef NF_RESIDENT130_TEST
void nf_hw_resident130_test_set(int on){resident130_on=!!on;}
void nf_hw_resident130_test_stats(uint64_t out[6]){
    out[0]=resident130_retained;out[1]=resident130_resumed;out[2]=resident130_published;
    out[3]=resident130_shadow_maps;out[4]=deferred128.valid;out[5]=pending;
}
#endif
static int resident130_main(const NFHardwareState *s){
    const NFHardwareState *p=&deferred128.state;
    return deferred128.valid && deferred128.resident130 && !s->color_only && !s->color_layout && !p->color_layout &&
        s->color==p->color && s->depth==p->depth && s->width==p->width && s->height==p->height &&
        s->pitch==p->pitch && s->depth_pitch==p->depth_pitch;
}
static int resident130_shadow_sync(void);
#endif
static int overlap128(const void *a,size_t an,const void *b,size_t bn){
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    if(!an || !bn)return 0;
    if(an>UINTPTR_MAX-x || bn>UINTPTR_MAX-y)return 1;
    return x<y+bn && y<x+an;
}
static int deferred128_alias(const void *address,size_t length){
    return deferred128.valid &&
        (overlap128(address,length,deferred128.state.color,(size_t)deferred128.state.pitch*deferred128.state.height) ||
         overlap128(address,length,deferred128.state.depth,(size_t)deferred128.state.depth_pitch*deferred128.state.height));
}
static int deferred128_shadow(const NFHardwareState *s){
    const NFHardwareState *p=&deferred128.shadow;
    /* Full uniform pitched CPU clear and the proven swizzled shadow draw cover
     * exactly the same backing bytes. Permit that one explicit transition. */
    return !p->color_layout && (s->color_layout==NF_COLOR_PITCHED ||
        (s->color_layout==NF_COLOR_SWIZZLED_256_131 && nf_swizzled131_enabled())) &&
        s->color_only && !s->depth && !s->depth_pitch && !s->depth_enable &&
        s->color==p->color && s->width==p->width && s->height==p->height && s->pitch==p->pitch;
}
#endif
#ifdef NIGHTFIRE_SURFACE_COHERENCE108
static int coherence108_enabled(void){static int on=-1;if(on<0){const char *v=getenv("NIGHTFIRE_SURFACE_COHERENCE108");on=v && !strcmp(v,"1");}return on;}
#endif
#ifdef NIGHTFIRE_EARLY_SUBMIT113
static unsigned early113_interval,early113_draws;
static int early113_initialized;
static uint64_t early113_flushes;
/* CP135 candidate: submit once after the second actual Morton256 shadow draw.
 * Copies, Maps and guest publication retain their original locations. */
static unsigned shadow135_draws;
static uint64_t shadow135_flushes;
static int shadow135_enabled=-1;
static int shadow135_setting(void){
 if(shadow135_enabled<0){const char *v=getenv("NIGHTFIRE_SHADOW_SUBMIT135");shadow135_enabled=v && !strcmp(v,"1");}
 return shadow135_enabled;
}
static void early113_reset_interval(void){early113_draws=0;shadow135_draws=0;}
static unsigned early113_setting(void){
 if(!early113_initialized){
  early113_initialized=1;const char *v=getenv("NIGHTFIRE_EARLY_SUBMIT113");
  if(v && *v){char *end;unsigned long n=strtoul(v,&end,10);if(!*end && n<=4096)early113_interval=(unsigned)n;}
 }
 return early113_interval;
}
static void early113_drawn(void){
 unsigned interval=early113_setting();
 unsigned legacy=interval && ++early113_draws>=interval;
 unsigned shadow=0;
 if(shadow135_setting() && active.color_only && !active.depth && !active.depth_pitch && !active.depth_enable &&
    active.color_layout==NF_COLOR_SWIZZLED_256_131 && active.width==256 && active.height==256 && active.pitch==1024){
  if(shadow135_draws<2 && ++shadow135_draws==2)shadow=1;
 }else shadow135_draws=0;
 if(legacy || shadow){
  /* Asynchronous submission only. Coalesce coincident reasons into one call.
   * A shadow-only submission does not change the old512-draw cadence. */
  ID3D11DeviceContext_Flush(ctx);early113_flushes++;
  if(legacy)early113_draws=0;
  if(shadow)shadow135_flushes++;
 }
}
#ifdef NF_EARLY_SUBMIT113_TEST
void nf_hw_early113_test_set(unsigned interval){early113_initialized=1;early113_interval=interval;early113_reset_interval();early113_flushes=0;shadow135_flushes=0;}
void nf_hw_shadow135_test_set(int on){shadow135_enabled=on!=0;early113_reset_interval();shadow135_flushes=0;}
uint64_t nf_hw_shadow135_test_flushes(void){return shadow135_flushes;}
uint64_t nf_hw_early113_test_flushes(void){return early113_flushes;}
#endif
#endif
#if defined(NIGHTFIRE_GPU_FALLBACK96) || defined(NIGHTFIRE_GPU_BLEND97) || defined(NIGHTFIRE_SURFACE_COHERENCE108) || defined(NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC) || defined(NIGHTFIRE_COLOR_REUSE110) || defined(NIGHTFIRE_EARLY_SUBMIT113)
static void fallback96_drawn(void){
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
 defer128_active_dirty=1;
#endif
#ifdef NIGHTFIRE_EARLY_SUBMIT113
 early113_drawn();
#endif
#ifdef NIGHTFIRE_COLOR_REUSE110
 if(color110_enabled())nf_color_reuse110_invalidate(color);
#endif
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
    nf_color_reupload109_invalidate(color);
#endif
    /* Arm after submission, not begin: the current draw's fetch_attr calls
     * happen after prepare and must not unbind its freshly prepared target.
     * The next draw's existing preflight sees this before it prepares. */
#ifdef NIGHTFIRE_SURFACE_COHERENCE108
    /* Ordinary paired draws also change GPU memory. The next batch must
     * resolve overlapping CPU reads before hardware_prepare, just as for
     * the previously guarded color-only/reverse-subtract paths. */
    if(coherence108_enabled()){nf_hw_clear_pending=1;return;}
#endif
    if(active.color_only || active.compat_point_clamp || active.reverse_subtract97)nf_hw_clear_pending=1;
}
#else
#define fallback96_drawn() ((void)0)
#endif
static uint64_t draws,triangles,transfers,uploads;
#ifdef NF_EARLY_SUBMIT113_TEST
uint64_t nf_hw_shadow135_test_transfers(void){return transfers;}
#endif
#ifdef NIGHTFIRE_SURFACE_PROBE_DIAGNOSTIC
extern ptrdiff_t xbox_GetMemoryOffset(void);
extern uint32_t xbox_ContiguousAllocatedBytes(void);
static uint64_t probe_drain_transfers;
void nf_hw_surface_probe_begin_drain(void){probe_drain_transfers=transfers;}
void nf_hw_surface_probe_arm(unsigned frame){
    static int attempted;
    if(attempted || frame<100 || transfers==probe_drain_transfers || pending || initialized<=0 || !width || !height || active.color_only)return;
    attempted=1;
    uintptr_t base=(uintptr_t)xbox_GetMemoryOffset();
    fprintf(stderr,"[SURFACE-PROBE] frame=%u color=%p depth=%p size=%ux%u pitches=%u/%u transfers=%llu\n",
        frame,active.color,active.depth,width,height,active.pitch,active.depth_pitch,
        (unsigned long long)(transfers-probe_drain_transfers));
    nf_surface_probe_arm(active.color,(size_t)active.pitch*height,
        active.depth,(size_t)active.depth_pitch*height,base+0x80000000ull,
        base+0xf0000000ull,xbox_ContiguousAllocatedBytes());
}
#endif
static uint64_t begin_ticks,draw_ticks,sync_ticks,texture_ticks;
/* This immediate context is private to this backend. Remember actual bindings,
 * including explicit render-target unbinds in sync/surface replacement. */
static struct {
 ID3D11RasterizerState *raster;ID3D11DepthStencilState *depth;
 ID3D11BlendState *blend;ID3D11InputLayout *layout;
 ID3D11VertexShader *vertex;ID3D11GeometryShader *geometry;
 ID3D11PixelShader *pixel;
 ID3D11ShaderResourceView *texture;ID3D11SamplerState *sampler;
 D3D11_VIEWPORT viewport;D3D11_RECT scissor;
 unsigned valid,targets,fixed,kelvin,default90,topology;
} bound;
static uint64_t binding_checks,binding_calls;
static int binding_cache(void){static int enabled=-1;if(enabled<0){const char *v=getenv("NIGHTFIRE_GPU_STATE_CACHE");enabled=!v || strcmp(v,"0");}return enabled;}
#define BIND_IF(condition,call) do{binding_checks++;if(!binding_cache() || (condition)){call;binding_calls++;}}while(0)
#ifdef NIGHTFIRE_VERTEX_BATCH92
/* Every draw route names its topology. A native strip may be followed by a
 * fixed movie/UI draw or the retained dense/list route without a new context. */
static void topology92(unsigned topology){
 D3D11_PRIMITIVE_TOPOLOGY next=topology==NF_HW_TRIANGLE_STRIP?D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
 BIND_IF(bound.topology!=(unsigned)next,ID3D11DeviceContext_IASetPrimitiveTopology(ctx,next));bound.topology=(unsigned)next;
}
#endif
static uint64_t hw_clock(void){LARGE_INTEGER t;QueryPerformanceCounter(&t);return (uint64_t)t.QuadPart;}
#include "nightfire_batch_timing244.h"
typedef struct {unsigned used,start,count,inputs,dynamic,coordinate_key174;uint32_t code[136][4],constants[6];ID3D11VertexShader *shader;ID3D11InputLayout *layout;
#ifdef NIGHTFIRE_DIRECT_VERTEX88
 ID3D11InputLayout *direct28_layout;
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
 ID3D11InputLayout *direct90_layout;
#endif
} ProgramEntry;
static ProgramEntry programs[64];static unsigned program_next;
static int geometry_validator(void){
 if(validated_geometry)return 1;
 static const char code[]="struct P{float4 p:SV_POSITION;float2 uv:TEXCOORD0;float4 c:COLOR0;float fog:TEXCOORD2;float ok:TEXCOORD1;};[maxvertexcount(3)]void geometry(triangle P v[3],inout TriangleStream<P> stream){if(v[0].ok<0.5 || v[1].ok<0.5 || v[2].ok<0.5)return;stream.Append(v[0]);stream.Append(v[1]);stream.Append(v[2]);}";
 ID3DBlob *blob=NULL,*errors=NULL;
 HRESULT hr=D3DCompile(code,sizeof code-1,"nightfire-valid-triangle",NULL,NULL,"geometry","gs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);
 if(FAILED(hr)){fprintf(stderr,"[GPU-VERTEX] geometry fallback: %s\n",errors?(char*)errors->lpVtbl->GetBufferPointer(errors):"unknown");RELEASE(errors);RELEASE(blob);return 0;}RELEASE(errors);
 hr=ID3D11Device_CreateGeometryShader(dev,blob->lpVtbl->GetBufferPointer(blob),blob->lpVtbl->GetBufferSize(blob),NULL,&validated_geometry);RELEASE(blob);return SUCCEEDED(hr);
}
static int geometry_validator182(void){
 if(projective_geometry182)return 1;
 static const char code[]="struct P{float4 p:SV_POSITION;float4 uv:TEXCOORD0;float4 c:COLOR0;float fog:TEXCOORD2;float ok:TEXCOORD1;};[maxvertexcount(3)]void geometry(triangle P v[3],inout TriangleStream<P> stream){if(v[0].ok<0.5 || v[1].ok<0.5 || v[2].ok<0.5)return;stream.Append(v[0]);stream.Append(v[1]);stream.Append(v[2]);}";
 ID3DBlob *blob=NULL,*errors=NULL;
 HRESULT hr=D3DCompile(code,sizeof code-1,"nightfire-valid-triangle",NULL,NULL,"geometry","gs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);
 if(FAILED(hr)){fprintf(stderr,"[GPU-VERTEX] geometry fallback: %s\n",errors?(char*)errors->lpVtbl->GetBufferPointer(errors):"unknown");RELEASE(errors);RELEASE(blob);return 0;}RELEASE(errors);
 hr=ID3D11Device_CreateGeometryShader(dev,blob->lpVtbl->GetBufferPointer(blob),blob->lpVtbl->GetBufferSize(blob),NULL,&projective_geometry182);RELEASE(blob);return SUCCEEDED(hr);
}
static ID3D11VertexShader *program_shader_selected174(const NFVertexProgram *s,unsigned direct28,unsigned direct90,unsigned stage174,unsigned raw174){
 unsigned key174=nf_vertex_coordinate_key174(stage174,raw174);if(key174==~0u)return NULL;
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
 direct90_selected=0;
#else
 (void)direct90;
#endif
#ifdef NIGHTFIRE_DIRECT_VERTEX88
 direct28_selected=0;
#else
 (void)direct28;
#endif
 if(!s || (s->mode&3)!=2 || s->start>=136)return NULL;
 unsigned count=0;for(unsigned i=s->start;i<136;i++){if(s->valid[i]!=15)return NULL;count++;if(s->code[i][3]&1)break;}
 if(!count || !(s->code[s->start+count-1][3]&1))return NULL;
 ProgramEntry *e=NULL;
 for(unsigned i=0;i<64;i++)if(programs[i].used && programs[i].coordinate_key174==key174 && programs[i].start==s->start && programs[i].count==count && !memcmp(programs[i].code,s->code+s->start,count*16)){e=&programs[i];break;}
 if(!e){
  e=&programs[program_next++%64];RELEASE(e->shader);RELEASE(e->layout);
#ifdef NIGHTFIRE_DIRECT_VERTEX88
  RELEASE(e->direct28_layout);
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
  RELEASE(e->direct90_layout);
#endif
  memset(e,0,sizeof *e);e->used=1;e->coordinate_key174=key174;e->start=s->start;e->count=count;memcpy(e->code,s->code+s->start,count*16);
  NFVertexHlsl *b=malloc(sizeof *b);if(!b)return NULL;
  if(!nf_vertex_hlsl_core174(b,s,stage174,raw174)){free(b);return NULL;}
  memcpy(e->constants,b->constants,sizeof e->constants);
  e->inputs=b->inputs;
  e->dynamic=b->dynamic;
  ID3DBlob *code=NULL,*errors=NULL;int cache_hit=0;
#ifdef NIGHTFIRE_SHADER_CACHE99
  uint64_t started=nf_shader99_stats.compile_ticks;
#else
  uint64_t started=hw_clock();
#endif
  HRESULT hr=nf_shader99_compile(b->text,b->length,&code,&errors,0,&cache_hit);
#ifdef NIGHTFIRE_SHADER_CACHE99
  program_ticks+=nf_shader99_stats.compile_ticks-started;
#else
  program_ticks+=hw_clock()-started;
#endif
  if(FAILED(hr)){fprintf(stderr,"[GPU-VERTEX] compile fallback: %s\n",errors?(char*)errors->lpVtbl->GetBufferPointer(errors):"unknown error");RELEASE(errors);RELEASE(code);free(b);return NULL;}RELEASE(errors);
  hr=ID3D11Device_CreateVertexShader(dev,code->lpVtbl->GetBufferPointer(code),code->lpVtbl->GetBufferSize(code),NULL,&e->shader);
  if(FAILED(hr) && cache_hit){
   /* Byte-valid DXBC can still be rejected by the current device. Retry once
    * through the real compiler; cache I/O never decides render eligibility. */
   nf_shader99_device_reject();RELEASE(e->shader);RELEASE(code);
#ifdef NIGHTFIRE_SHADER_CACHE99
   started=nf_shader99_stats.compile_ticks;
#else
   started=hw_clock();
#endif
   hr=nf_shader99_compile(b->text,b->length,&code,&errors,1,&cache_hit);
#ifdef NIGHTFIRE_SHADER_CACHE99
   program_ticks+=nf_shader99_stats.compile_ticks-started;
#else
   program_ticks+=hw_clock()-started;
#endif
   if(FAILED(hr)){fprintf(stderr,"[GPU-VERTEX] compile fallback: %s\n",errors?(char*)errors->lpVtbl->GetBufferPointer(errors):"unknown error");RELEASE(errors);RELEASE(code);free(b);return NULL;}RELEASE(errors);
   hr=ID3D11Device_CreateVertexShader(dev,code->lpVtbl->GetBufferPointer(code),code->lpVtbl->GetBufferSize(code),NULL,&e->shader);
  }
  free(b);
  if(SUCCEEDED(hr)){
   D3D11_INPUT_ELEMENT_DESC elements[16]={0};
   unsigned offset=0;
   for(unsigned i=0;i<16;i++){elements[i].SemanticName="TEXCOORD";elements[i].SemanticIndex=i;elements[i].Format=DXGI_FORMAT_R32G32B32A32_FLOAT;elements[i].AlignedByteOffset=e->inputs&(1u<<i)?offset:0;elements[i].InputSlotClass=D3D11_INPUT_PER_VERTEX_DATA;if(e->inputs&(1u<<i))offset+=16;}
   hr=ID3D11Device_CreateInputLayout(dev,elements,16,code->lpVtbl->GetBufferPointer(code),code->lpVtbl->GetBufferSize(code),&e->layout);
#ifdef NIGHTFIRE_DIRECT_VERTEX88
   /* Same compiled shader; only the verified interleaved IA layout differs.
    * A raw-layout failure must not invalidate a successfully created dense one. */
   if(SUCCEEDED(hr) && e->inputs==0xdu && direct28_format_supported()){
    /* The compiled guest shader retains all16 signature declarations, even
     * unused semantics. Give unused entries a bounded dummy within the record,
     * just as the dense layout does; e->inputs proves they are never consumed. */
    D3D11_INPUT_ELEMENT_DESC raw[16]={0};
    for(unsigned i=0;i<16;i++){raw[i].SemanticName="TEXCOORD";raw[i].SemanticIndex=i;raw[i].Format=DXGI_FORMAT_R32G32B32A32_FLOAT;raw[i].InputSlotClass=D3D11_INPUT_PER_VERTEX_DATA;}
    raw[0].Format=DXGI_FORMAT_R32G32B32_FLOAT;
    raw[2].Format=DXGI_FORMAT_B8G8R8A8_UNORM;raw[2].AlignedByteOffset=16;
    raw[3].Format=DXGI_FORMAT_R32G32_FLOAT;raw[3].AlignedByteOffset=20;
    HRESULT raw_hr=ID3D11Device_CreateInputLayout(dev,raw,16,code->lpVtbl->GetBufferPointer(code),code->lpVtbl->GetBufferSize(code),&e->direct28_layout);
    if(FAILED(raw_hr)){RELEASE(e->direct28_layout);fprintf(stderr,"[GPU-DIRECT28] layout fallback %08lX\n",(unsigned long)raw_hr);}
   }
#endif
  }
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
  if(SUCCEEDED(hr) && direct90_bit(e->inputs) && direct28_format_supported() && default90_buffer()){
   /* Keep the current executor's unsupported type5/type6 values exactly:
    * float4(0,0,0,1), supplied by one immutable stride-zero second stream. */
   D3D11_INPUT_ELEMENT_DESC raw[16]={0};
   for(unsigned i=0;i<16;i++){raw[i].SemanticName="TEXCOORD";raw[i].SemanticIndex=i;raw[i].Format=DXGI_FORMAT_R32G32B32A32_FLOAT;raw[i].InputSlotClass=D3D11_INPUT_PER_VERTEX_DATA;}
   raw[0].Format=DXGI_FORMAT_R32G32B32_FLOAT;
   raw[1].InputSlot=1;
   raw[2].Format=DXGI_FORMAT_B8G8R8A8_UNORM;raw[2].AlignedByteOffset=16;
   raw[3].Format=DXGI_FORMAT_R32G32_FLOAT;raw[3].AlignedByteOffset=20;
   if(e->inputs&16){raw[4].Format=DXGI_FORMAT_B8G8R8A8_UNORM;raw[4].AlignedByteOffset=28;}
   for(unsigned i=7;i<=14;i++)if(e->inputs&(1u<<i))raw[i].InputSlot=1;
   HRESULT raw_hr=ID3D11Device_CreateInputLayout(dev,raw,16,code->lpVtbl->GetBufferPointer(code),code->lpVtbl->GetBufferSize(code),&e->direct90_layout);
   if(FAILED(raw_hr)){RELEASE(e->direct90_layout);fprintf(stderr,"[GPU-DIRECT90] layout fallback %08lX\n",(unsigned long)raw_hr);}
  }
#endif
  RELEASE(code);if(FAILED(hr) || (e->dynamic && !(raw174?geometry_validator182():geometry_validator()))){RELEASE(e->shader);return NULL;}
  /* Legacy shaders/cache counts successful program preparations. CP99 reports
   * real compiler calls separately; keep the existing log prefix/fields. */
  program_compiles++;fprintf(stderr,"[GPU-VERTEX] compiled program start=%u instructions=%u cache=%llu disk_hit=%d\n",s->start,count,(unsigned long long)program_compiles,cache_hit);
 }
 if(!e->shader)return NULL;
 if(constant_mask_enabled()){
  if(!nf_constants_available(nf_constant_mask(&constant_mask,s->constant_valid),e->constants))return NULL;
 }else for(unsigned i=0;i<192;i++)if((e->constants[i/32]&(1u<<(i%32))) && s->constant_valid[i]!=15)return NULL;
 input_layout=e->layout;program_inputs=e->inputs;program_dynamic=e->dynamic;
#ifdef NIGHTFIRE_DIRECT_VERTEX88
 if(direct28 && e->inputs==0xdu && e->direct28_layout){input_layout=e->direct28_layout;direct28_selected=1;}
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
 if((direct90&direct90_bit(e->inputs)) && e->direct90_layout && default90_vb){input_layout=e->direct90_layout;direct90_selected=e->inputs==0xf?28:32;}
#endif
 return e->shader;
}

/* Selected float4 path remains disabled until matching GS/PS/fallback support. */
static ID3D11VertexShader *program_shader(const NFVertexProgram *s,unsigned a,unsigned b){return program_shader_selected174(s,a,b,0,0);}
static const char shader[]= 
NF_FOG_PARAMS_HLSL
"struct V{float4 p:POSITION;float2 uv:TEXCOORD0;float4 c:COLOR0;float fog:TEXCOORD2;};"
"struct P{float4 p:SV_POSITION;float2 uv:TEXCOORD0;float4 c:COLOR0;float fog:TEXCOORD2;};"
"P vertex(V v){P o;o.p=float4((v.p.x*2/surface.x-1)*v.p.w,(1-v.p.y*2/surface.y)*v.p.w,v.p.z/16777215*v.p.w,v.p.w);o.uv=v.uv;o.c=v.c.bgra;o.fog=v.fog;return o;}"
"Texture2D tex:register(t0);SamplerState samp:register(s0);"
/* Load each original YUY2/UYVY pair without filtering shared chroma. Integer
 * BT.601 arithmetic matches the existing software sampler, including rounding. */
"float4 packedVideo(float2 uv){uint2 size=(uint2)abs(packedSize);uint2 q=(uint2)floor(max(uv,0)*size);q.x=packedSize.x<0?q.x%size.x:min(q.x,size.x-1);q.y=packedSize.y<0?q.y%size.y:min(q.y,size.y-1);"
"int4 b=(int4)floor(tex.Load(int3(q.x/2,q.y,0))*255+0.5);int yo=mode>=5?1:0;int y=b[(q.x&1)*2+yo]-16,u=b[1-yo]-128,v=b[3-yo]-128;"
"int3 rgb=int3(298*y+409*v+128,298*y-100*u-208*v+128,298*y+516*u+128)>>8;return float4(clamp(rgb,0,255)/255.0,1);}"
"float4 pixel(P p):SV_TARGET{\n#ifdef PACKED_VIDEO\nfloat4 c=packedVideo(p.uv);if(mode==4 || mode==6)c=saturate(c*p.c*float4(rgbScale,rgbScale,rgbScale,1));\n#else\n"
"float4 c=mode==2?p.c:tex.Sample(samp,p.uv);if(mode==1)c=saturate(c*p.c*float4(rgbScale,rgbScale,rgbScale,1));\n#endif\n"
"if(fogOn!=0)c.rgb=lerp(fogColor.rgb,c.rgb,saturate(p.fog));"
"if(alphaOn!=0){float a=floor(c.a*255+0.5);bool ok=alphaFunc==0?false:alphaFunc==1?a<alphaRef:alphaFunc==2?a==alphaRef:alphaFunc==3?a<=alphaRef:alphaFunc==4?a>alphaRef:alphaFunc==5?a!=alphaRef:alphaFunc==6?a>=alphaRef:true;if(!ok)discard;}return c;}";
static const char projective_shader182[]= 
NF_FOG_PARAMS_HLSL
"struct V{float4 p:POSITION;float4 uv:TEXCOORD0;float4 c:COLOR0;float fog:TEXCOORD2;};"
"struct P{float4 p:SV_POSITION;float4 uv:TEXCOORD0;float4 c:COLOR0;float fog:TEXCOORD2;};"
"P vertex(V v){P o;o.p=float4((v.p.x*2/surface.x-1)*v.p.w,(1-v.p.y*2/surface.y)*v.p.w,v.p.z/16777215*v.p.w,v.p.w);o.uv=v.uv;o.c=v.c;o.fog=v.fog;return o;}"
"Texture2D tex:register(t0);SamplerState samp:register(s0);"
/* Load each original YUY2/UYVY pair without filtering shared chroma. Integer
 * BT.601 arithmetic matches the existing software sampler, including rounding. */
"float4 packedVideo(float2 uv){uint2 size=(uint2)abs(packedSize);uint2 q=(uint2)floor(max(uv,0)*size);q.x=packedSize.x<0?q.x%size.x:min(q.x,size.x-1);q.y=packedSize.y<0?q.y%size.y:min(q.y,size.y-1);"
"int4 b=(int4)floor(tex.Load(int3(q.x/2,q.y,0))*255+0.5);int yo=mode>=5?1:0;int y=b[(q.x&1)*2+yo]-16,u=b[1-yo]-128,v=b[3-yo]-128;"
"int3 rgb=int3(298*y+409*v+128,298*y-100*u-208*v+128,298*y+516*u+128)>>8;return float4(clamp(rgb,0,255)/255.0,1);}"
"float4 pixel(P p):SV_TARGET{float2 uv=p.uv.xy/p.uv.w*packedSize;\n#ifdef PACKED_VIDEO\nfloat4 c=packedVideo(uv);if(mode==4 || mode==6)c=saturate(c*p.c*float4(rgbScale,rgbScale,rgbScale,1));\n#else\n"
"float4 c=mode==2?p.c:tex.Sample(samp,uv);if(mode==1)c=saturate(c*p.c*float4(rgbScale,rgbScale,rgbScale,1));\n#endif\n"
"if(fogOn!=0)c.rgb=lerp(fogColor.rgb,c.rgb,saturate(p.fog));"
"if(alphaOn!=0){float a=floor(c.a*255+0.5);bool ok=alphaFunc==0?false:alphaFunc==1?a<alphaRef:alphaFunc==2?a==alphaRef:alphaFunc==3?a<=alphaRef:alphaFunc==4?a>alphaRef:alphaFunc==5?a!=alphaRef:alphaFunc==6?a>=alphaRef:true;if(!ok)discard;}return c;}";
static const char movie_shader200[]= 
NF_FOG_PARAMS_HLSL
"struct V{float4 p:POSITION;float4 uv:TEXCOORD0;float4 c:COLOR0;float fog:TEXCOORD2;};"
"struct P{float4 p:SV_POSITION;float4 uv:TEXCOORD0;float4 c:COLOR0;float fog:TEXCOORD2;};"
"P vertex(V v){P o;o.p=float4((v.p.x*2/surface.x-1)*v.p.w,(1-v.p.y*2/surface.y)*v.p.w,v.p.z/16777215*v.p.w,v.p.w);o.uv=v.uv;o.c=v.c;o.fog=v.fog;return o;}"
"Texture2D tex:register(t0);SamplerState samp:register(s0);"
/* Load each original YUY2/UYVY pair without filtering shared chroma. Integer
 * BT.601 arithmetic matches the existing software sampler, including rounding. */
"float4 packedVideo(float2 uv){uint2 size=(uint2)abs(packedSize);uint2 q=(uint2)floor(max(uv,0)*size);q.x=packedSize.x<0?q.x%size.x:min(q.x,size.x-1);q.y=packedSize.y<0?q.y%size.y:min(q.y,size.y-1);"
"int4 b=(int4)floor(tex.Load(int3(q.x/2,q.y,0))*255+0.5);int yo=mode>=5?1:0;int y=b[(q.x&1)*2+yo]-16,u=b[1-yo]-128,v=b[3-yo]-128;"
"int3 rgb=int3(298*y+409*v+128,298*y-100*u-208*v+128,298*y+516*u+128)>>8;return float4(clamp(rgb,0,255)/255.0,1);}"
"float4 pixel(P p):SV_TARGET{if(((uint)floor(p.p.x)&1)!=MOVIE_LANE)discard;float2 uv=p.uv.xy/p.uv.w*packedSize;\n#ifdef PACKED_VIDEO\nfloat4 c=packedVideo(uv);if(mode==4 || mode==6)c=saturate(c*p.c*float4(rgbScale,rgbScale,rgbScale,1));\n#else\n"
"float4 c=mode==2?p.c:tex.Sample(samp,uv);if(mode==1)c=saturate(c*p.c*float4(rgbScale,rgbScale,rgbScale,1));\n#endif\n"
"if(fogOn!=0)c.rgb=lerp(fogColor.rgb,c.rgb,saturate(p.fog));"
"if(alphaOn!=0){float a=floor(c.a*255+0.5);bool ok=alphaFunc==0?false:alphaFunc==1?a<alphaRef:alphaFunc==2?a==alphaRef:alphaFunc==3?a<=alphaRef:alphaFunc==4?a>alphaRef:alphaFunc==5?a!=alphaRef:alphaFunc==6?a>=alphaRef:true;if(!ok)discard;}return c;}";
static const char resolve_shader193[]= 
NF_FOG_PARAMS_HLSL
"struct V{float4 p:POSITION;float4 uv:TEXCOORD0;float4 c:COLOR0;float fog:TEXCOORD2;};"
"struct P{float4 p:SV_POSITION;float4 uv:TEXCOORD0;float4 c:COLOR0;float fog:TEXCOORD2;};"
"P vertex(V v){P o;o.p=float4((v.p.x*2/surface.x-1)*v.p.w,(1-v.p.y*2/surface.y)*v.p.w,v.p.z/16777215*v.p.w,v.p.w);o.uv=v.uv;o.c=v.c;o.fog=v.fog;return o;}"
"Texture2D tex:register(t0);SamplerState samp:register(s0);"
"uint4 sample193(int2 at){return (uint4)floor(tex.Load(int3(at,0))*255.0+0.5);}"
"float4 resolve193(float4 raw){uint sw,h;tex.GetDimensions(sw,h);int2 last=int2(sw/2-1,h-1);"
/* Explicit profile origin: original U=2X+.5,V=Y at D3D half-pixel centers. */
"float2 uv=raw.xy/raw.w;int2 at=clamp((int2)floor(float2((uv.x-.5)*.5,uv.y)),int2(0,0),last);"
"int2 next=min(at+1,last);uint4 sum=4*sample193(int2(2*at.x+CENTER_LANE,at.y));"
"sum+=sample193(int2(2*at.x+1-CENTER_LANE,at.y));"
"sum+=sample193(int2(2*next.x+1-CENTER_LANE,at.y));"
"sum+=sample193(int2(2*at.x+1-CENTER_LANE,next.y));"
"sum+=sample193(int2(2*next.x+1-CENTER_LANE,next.y));"
"return (float4)((sum+4)/8)/255.0;}"

/* Load each original YUY2/UYVY pair without filtering shared chroma. Integer
 * BT.601 arithmetic matches the existing software sampler, including rounding. */
"float4 packedVideo(float2 uv){uint2 size=(uint2)abs(packedSize);uint2 q=(uint2)floor(max(uv,0)*size);q.x=packedSize.x<0?q.x%size.x:min(q.x,size.x-1);q.y=packedSize.y<0?q.y%size.y:min(q.y,size.y-1);"
"int4 b=(int4)floor(tex.Load(int3(q.x/2,q.y,0))*255+0.5);int yo=mode>=5?1:0;int y=b[(q.x&1)*2+yo]-16,u=b[1-yo]-128,v=b[3-yo]-128;"
"int3 rgb=int3(298*y+409*v+128,298*y-100*u-208*v+128,298*y+516*u+128)>>8;return float4(clamp(rgb,0,255)/255.0,1);}"
"float4 pixel(P p):SV_TARGET{float2 uv=p.uv.xy/p.uv.w*packedSize;\n#ifdef PACKED_VIDEO\nfloat4 c=packedVideo(uv);if(mode==4 || mode==6)c=saturate(c*p.c*float4(rgbScale,rgbScale,rgbScale,1));\n#else\n"
"float4 c=mode==2?p.c:resolve193(p.uv);if(mode==1)c=saturate(c*p.c*float4(rgbScale,rgbScale,rgbScale,1));\n#endif\n"
"c.a=p.c.a;"
"if(fogOn!=0)c.rgb=lerp(fogColor.rgb,c.rgb,saturate(p.fog));"
"if(alphaOn!=0){float a=floor(c.a*255+0.5);bool ok=alphaFunc==0?false:alphaFunc==1?a<alphaRef:alphaFunc==2?a==alphaRef:alphaFunc==3?a<=alphaRef:alphaFunc==4?a>alphaRef:alphaFunc==5?a!=alphaRef:alphaFunc==6?a>=alphaRef:true;if(!ok)discard;}return c;}";
static int initialize(void){if(resident313_blocked()){return 0;}
    if(initialized)return initialized>0;
    initialized=-1;D3D_FEATURE_LEVEL level;
    CALL_OK(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,NULL,0,D3D11_SDK_VERSION,&dev,&level,&ctx));
    if(level<D3D_FEATURE_LEVEL_10_1)return 0;
    ID3DBlob *vcode=NULL,*pcode=NULL,*errors=NULL;
    HRESULT hr=D3DCompile(shader,sizeof(shader)-1,"nightfire-native",NULL,NULL,"vertex","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vcode,&errors);
    if(FAILED(hr)){if(errors)fprintf(stderr,"[HW-GPU] shader: %s\n",(char*)errors->lpVtbl->GetBufferPointer(errors));RELEASE(errors);return 0;}RELEASE(errors);
    hr=D3DCompile(shader,sizeof(shader)-1,"nightfire-native",NULL,NULL,"pixel","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&pcode,&errors);
    if(FAILED(hr)){if(errors)fprintf(stderr,"[HW-GPU] shader: %s\n",(char*)errors->lpVtbl->GetBufferPointer(errors));RELEASE(errors);RELEASE(vcode);return 0;}RELEASE(errors);
    CALL_OK(ID3D11Device_CreateVertexShader(dev,vcode->lpVtbl->GetBufferPointer(vcode),vcode->lpVtbl->GetBufferSize(vcode),NULL,&vs));
    CALL_OK(ID3D11Device_CreatePixelShader(dev,pcode->lpVtbl->GetBufferPointer(pcode),pcode->lpVtbl->GetBufferSize(pcode),NULL,&ps));
    D3D11_INPUT_ELEMENT_DESC elements[]={
        {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",2,DXGI_FORMAT_R32_FLOAT,0,28,D3D11_INPUT_PER_VERTEX_DATA,0}};
    CALL_OK(ID3D11Device_CreateInputLayout(dev,elements,4,vcode->lpVtbl->GetBufferPointer(vcode),vcode->lpVtbl->GetBufferSize(vcode),&layout));
    RELEASE(vcode);RELEASE(pcode);
    const D3D_SHADER_MACRO movie_defines[]={{"PACKED_VIDEO","1"},{NULL,NULL}};
    hr=D3DCompile(shader,sizeof(shader)-1,"nightfire-movie",movie_defines,NULL,"pixel","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&pcode,&errors);
    if(FAILED(hr)){if(errors)fprintf(stderr,"[HW-GPU] movie shader: %s\n",(char*)errors->lpVtbl->GetBufferPointer(errors));RELEASE(errors);return 0;}RELEASE(errors);
    CALL_OK(ID3D11Device_CreatePixelShader(dev,pcode->lpVtbl->GetBufferPointer(pcode),pcode->lpVtbl->GetBufferSize(pcode),NULL,&video_ps));RELEASE(pcode);
    D3D11_BUFFER_DESC bd={0};bd.ByteWidth=16384*sizeof(NFHardwareVertex);bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    CALL_OK(ID3D11Device_CreateBuffer(dev,&bd,NULL,&vb));
    bd.ByteWidth=16384*sizeof(NFHardwareInputVertex);CALL_OK(ID3D11Device_CreateBuffer(dev,&bd,NULL,&input_vb));
    bd.ByteWidth=16384*sizeof(uint16_t);bd.BindFlags=D3D11_BIND_INDEX_BUFFER;CALL_OK(ID3D11Device_CreateBuffer(dev,&bd,NULL,&input_ib));
    bd.ByteWidth=sizeof saved_params;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;CALL_OK(ID3D11Device_CreateBuffer(dev,&bd,NULL,&cb));
    bd.ByteWidth=sizeof saved_kelvin;CALL_OK(ID3D11Device_CreateBuffer(dev,&bd,NULL,&kelvin_cb));
    D3D11_RASTERIZER_DESC rd={0};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;rd.ScissorEnable=TRUE;
    CALL_OK(ID3D11Device_CreateRasterizerState(dev,&rd,&rs));
    initialized=1;fprintf(stderr,"[HW-GPU] Native D3D11 rasterizer initialized, feature level %X\n",level);return 1;
}
static int initialize_projective182(void){
 if(projective_vs182 && projective_ps182 && projective_layout182 && projective_vb182)return 1;
 RELEASE(projective_vs182);RELEASE(projective_ps182);RELEASE(projective_layout182);RELEASE(projective_vb182);
 ID3DBlob *v=NULL,*p=NULL,*errors=NULL;HRESULT hr;
 hr=D3DCompile(projective_shader182,sizeof projective_shader182-1,"projective182",NULL,NULL,"vertex","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&v,&errors);
 if(FAILED(hr))goto done;RELEASE(errors);
 hr=D3DCompile(projective_shader182,sizeof projective_shader182-1,"projective182",NULL,NULL,"pixel","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&p,&errors);
 if(FAILED(hr))goto done;
 hr=ID3D11Device_CreateVertexShader(dev,v->lpVtbl->GetBufferPointer(v),v->lpVtbl->GetBufferSize(v),NULL,&projective_vs182);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreatePixelShader(dev,p->lpVtbl->GetBufferPointer(p),p->lpVtbl->GetBufferSize(p),NULL,&projective_ps182);if(FAILED(hr))goto done;
 D3D11_INPUT_ELEMENT_DESC e[]={
 {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"TEXCOORD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"TEXCOORD",2,DXGI_FORMAT_R32_FLOAT,0,48,D3D11_INPUT_PER_VERTEX_DATA,0}};
 hr=ID3D11Device_CreateInputLayout(dev,e,4,v->lpVtbl->GetBufferPointer(v),v->lpVtbl->GetBufferSize(v),&projective_layout182);if(FAILED(hr))goto done;
 D3D11_BUFFER_DESC d={0};d.ByteWidth=16384*sizeof(ProjectiveVertex182);d.Usage=D3D11_USAGE_DYNAMIC;d.BindFlags=D3D11_BIND_VERTEX_BUFFER;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
 hr=ID3D11Device_CreateBuffer(dev,&d,NULL,&projective_vb182);
done:
 if(FAILED(hr)){fprintf(stderr,"[GPU-PROJECTIVE182] preparation failed %08lX %s\n",(unsigned long)hr,errors?(char*)errors->lpVtbl->GetBufferPointer(errors):"");RELEASE(projective_vs182);RELEASE(projective_ps182);RELEASE(projective_layout182);RELEASE(projective_vb182);}
 RELEASE(v);RELEASE(p);RELEASE(errors);return SUCCEEDED(hr);
}

static int initialize_resolve193(unsigned profile){
 unsigned lane=profile-1;if(lane>1)return 0;if(resolve_ps193[lane])return 1;
 D3D_SHADER_MACRO macros[]={{"CENTER_LANE",lane?"1":"0"},{NULL,NULL}};ID3DBlob *code=NULL,*errors=NULL;
 HRESULT hr=D3DCompile(resolve_shader193,sizeof resolve_shader193-1,"experimental-resolve193",macros,NULL,"pixel","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_IEEE_STRICTNESS,0,&code,&errors);
 if(SUCCEEDED(hr))hr=ID3D11Device_CreatePixelShader(dev,code->lpVtbl->GetBufferPointer(code),code->lpVtbl->GetBufferSize(code),NULL,&resolve_ps193[lane]);
 if(FAILED(hr)){fprintf(stderr,"[GPU-RESOLVE193] experimental profile preparation failed %08lX %s\n",(unsigned long)hr,errors?(char*)errors->lpVtbl->GetBufferPointer(errors):"");RELEASE(resolve_ps193[lane]);}
 RELEASE(code);RELEASE(errors);return SUCCEEDED(hr);
}

static int initialize_movie200(unsigned profile){
 unsigned lane=profile;if(lane>1)return 0;if(movie_ps200[lane])return 1;
 D3D_SHADER_MACRO macros[]={{"MOVIE_LANE",lane?"1":"0"},{NULL,NULL}};ID3DBlob *code=NULL,*errors=NULL;
 HRESULT hr=D3DCompile(movie_shader200,sizeof movie_shader200-1,"experimental-movie200",macros,NULL,"pixel","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_IEEE_STRICTNESS,0,&code,&errors);
 if(SUCCEEDED(hr))hr=ID3D11Device_CreatePixelShader(dev,code->lpVtbl->GetBufferPointer(code),code->lpVtbl->GetBufferSize(code),NULL,&movie_ps200[lane]);
 if(FAILED(hr)){fprintf(stderr,"[GPU-MOVIE200] experimental profile preparation failed %08lX %s\n",(unsigned long)hr,errors?(char*)errors->lpVtbl->GetBufferPointer(errors):"");RELEASE(movie_ps200[lane]);}
 RELEASE(code);RELEASE(errors);return SUCCEEDED(hr);
}

#ifdef NIGHTFIRE_GPU_FALLBACK96
/* Allocation cache only. Switching sets always completes the old pending
 * interval, and the next begin uploads current guest bytes. These slots own
 * the references; the ordinary resource globals borrow the selected set. */
typedef struct {
    unsigned width,height;
    uint64_t used;
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
    uint64_t generation109;
#endif
#ifdef NIGHTFIRE_COLOR_REUSE110
    uint64_t generation110;
#endif
    ID3D11Texture2D *color,*depth,*color_read,*depth_transfer;
    ID3D11RenderTargetView *rtv;
    ID3D11DepthStencilView *dsv;
} SurfaceSet96;
static SurfaceSet96 surface96[4];
static uint64_t surface96_clock;
static void surface96_release(SurfaceSet96 *s){
#ifdef NIGHTFIRE_COLOR_REUSE110
    if(color110_enabled())nf_color_reuse110_invalidate(s->color);
#endif
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
    nf_color_reupload109_invalidate(s->color);
#endif
    RELEASE(s->rtv);RELEASE(s->dsv);RELEASE(s->color);RELEASE(s->depth);
    RELEASE(s->color_read);RELEASE(s->depth_transfer);memset(s,0,sizeof *s);
}
static int surface96_create(SurfaceSet96 *s,unsigned w,unsigned h){
    HRESULT hr;
    D3D11_TEXTURE2D_DESC d={0};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;d.SampleDesc.Count=1;
#define SURFACE96_CREATE(call) do{hr=(call);if(FAILED(hr))goto fail;}while(0)
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;
    SURFACE96_CREATE(ID3D11Device_CreateTexture2D(dev,&d,NULL,&s->color));
    SURFACE96_CREATE(ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)s->color,NULL,&s->rtv));
    d.Format=DXGI_FORMAT_R32_TYPELESS;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    SURFACE96_CREATE(ID3D11Device_CreateTexture2D(dev,&d,NULL,&s->depth));
    D3D11_DEPTH_STENCIL_VIEW_DESC dd={0};dd.Format=DXGI_FORMAT_D32_FLOAT;dd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
    SURFACE96_CREATE(ID3D11Device_CreateDepthStencilView(dev,(ID3D11Resource*)s->depth,&dd,&s->dsv));
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ|D3D11_CPU_ACCESS_WRITE;
    SURFACE96_CREATE(ID3D11Device_CreateTexture2D(dev,&d,NULL,&s->depth_transfer));
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    SURFACE96_CREATE(ID3D11Device_CreateTexture2D(dev,&d,NULL,&s->color_read));
#undef SURFACE96_CREATE
    s->width=w;s->height=h;surface96_creates++;
#ifdef NIGHTFIRE_COLOR_REUSE110
    s->generation110=++color110_next_generation;
#endif
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
    s->generation109=++color109_next_generation;
#endif
    return 1;
fail:
    fprintf(stderr,"[HW-GPU] CP96 surface allocation failed %08lX\n",(unsigned long)hr);
    surface96_release(s);return 0;
}
static int surfaces_cached96(unsigned w,unsigned h){
    if(width==w && height==h)return 1;
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
    /* begin_impl has admitted only the recorded, disjoint color-only target.
     * The old staging images remain pinned; no guest read occurs here. */
    if(!(deferred128.valid && !pending))
#endif
    if(!HW_SYNC109("surface-size"))return 0;
    ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);bound.targets=0;
    SurfaceSet96 *selected=NULL;
    for(unsigned i=0;i<4;i++)if(surface96[i].width==w && surface96[i].height==h){selected=&surface96[i];break;}
    if(selected)surface96_reuses++;
    else{
        unsigned slot=4;
        for(unsigned i=0;i<4;i++){
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
            if(deferred128.valid && surface96[i].color==deferred128.color)continue;
#endif
            if(!surface96[i].width){slot=i;break;}
            if(slot==4 || surface96[i].used<surface96[slot].used)slot=i;
        }
        if(slot==4)return 0;
        /* Unpublish before eviction: failure must not leave borrowed dangling
         * pointers or dimensions that would make a retry look initialized. */
        color=depth=color_read=depth_transfer=NULL;rtv=NULL;dsv=NULL;width=height=0;
        surface96_release(&surface96[slot]);
        if(!surface96_create(&surface96[slot],w,h))return 0;
        selected=&surface96[slot];
    }
    selected->used=++surface96_clock;
#ifdef NIGHTFIRE_COLOR_REUSE110
    color110_generation=selected->generation110;
#endif
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
    color109_generation=selected->generation109;
#endif
    color=selected->color;depth=selected->depth;color_read=selected->color_read;depth_transfer=selected->depth_transfer;
    rtv=selected->rtv;dsv=selected->dsv;width=w;height=h;return 1;
}
#endif
static int surfaces(unsigned w,unsigned h){if(resident313_blocked()){return 0;}
#ifdef NIGHTFIRE_GPU_FALLBACK96
    if(nf_hw_fallback96_enabled())return surfaces_cached96(w,h);
#endif
    if(width==w && height==h)return 1;
    if(!HW_SYNC109("surface-size"))return 0;
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
    nf_color_reupload109_invalidate(color);color109_generation=++color109_next_generation;
#endif
#ifdef NIGHTFIRE_COLOR_REUSE110
    if(color110_enabled())nf_color_reuse110_invalidate(color);
    color110_generation=++color110_next_generation;
#endif
    ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);bound.targets=0;
    RELEASE(rtv);RELEASE(dsv);RELEASE(color);RELEASE(depth);RELEASE(color_read);RELEASE(depth_transfer);width=height=0;
    D3D11_TEXTURE2D_DESC d={0};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;
    CALL_OK(ID3D11Device_CreateTexture2D(dev,&d,NULL,&color));CALL_OK(ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)color,NULL,&rtv));
    d.Format=DXGI_FORMAT_R32_TYPELESS;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    CALL_OK(ID3D11Device_CreateTexture2D(dev,&d,NULL,&depth));
    D3D11_DEPTH_STENCIL_VIEW_DESC dd={0};dd.Format=DXGI_FORMAT_D32_FLOAT;dd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
    CALL_OK(ID3D11Device_CreateDepthStencilView(dev,(ID3D11Resource*)depth,&dd,&dsv));
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ|D3D11_CPU_ACCESS_WRITE;
    CALL_OK(ID3D11Device_CreateTexture2D(dev,&d,NULL,&depth_transfer));
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    CALL_OK(ID3D11Device_CreateTexture2D(dev,&d,NULL,&color_read));width=w;height=h;surface96_creates++;return 1;
}
#if defined(NIGHTFIRE_BATCH234) && (defined(NIGHTFIRE_RESIDENT_MAIN130) || defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_GPU_FALLBACK96))
#error Driving236 must not enable Action resident/deferred/fallback state
#endif
#if defined(NIGHTFIRE_RESIDENT_MAIN130) || defined(NIGHTFIRE_BATCH234)
/* Observe only quant130's Dispatch; restore the original SDK macro afterward. */
#ifdef NIGHTFIRE_GPU_SAMPLE254
#pragma push_macro("ID3D11DeviceContext_Dispatch")
#undef ID3D11DeviceContext_Dispatch
#define ID3D11DeviceContext_Dispatch(c,x,y,z) gt254_dispatch(c,x,y,z)
#endif
#include "nightfire_depth_roundtrip130.h"
#ifdef NIGHTFIRE_GPU_SAMPLE254
#pragma pop_macro("ID3D11DeviceContext_Dispatch")
#endif
#endif
#include "nightfire_fragment_depth291.h"
#include "nightfire_native_depth302.h"
#include "nightfire_batch234.h"
#include "nightfire_depth_import268.h"
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
int nf_hw_defer_clear128(const NFHardwareState *s){if(resident313_blocked()){return 0;}
    int resident=0;
#ifdef NIGHTFIRE_RESIDENT_MAIN130
    resident=resident130_enabled();
#endif
    if((!defer128_enabled() && !resident) || !coherence108_enabled() || !nf_hw_fallback96_enabled() ||
       !s || deferred128.valid || initialized<=0 || !pending || active.color_only || active.color_layout || s->color_layout ||
       !color || !depth || !color_read || !depth_transfer || !active.color || !active.depth ||
       !s->color || !s->color_only || s->depth || s->depth_pitch || s->depth_enable ||
       s->width!=256 || s->height!=256 || s->pitch!=1024 || s->left || s->top ||
       s->right!=256 || s->bottom!=256 || (width==256 && height==256))return 0;
#ifdef NIGHTFIRE_COLOR_REUSE110
    if(color110_enabled())return 0; /* Independently rejected cache trial. */
#endif
    size_t cb=(size_t)active.pitch*height,zb=(size_t)active.depth_pitch*height;
    size_t sb=(size_t)s->pitch*s->height;
    if(overlap128(active.color,cb,active.depth,zb) ||
       overlap128(s->color,sb,active.color,cb) || overlap128(s->color,sb,active.depth,zb))return 0;
#ifdef NIGHTFIRE_RESIDENT_MAIN130
    /* Host SIMD pack operates complete groups, avoiding scalar-tail NaN rules.
     * All allocation/compile failures precede acquiring guest ownership. */
    if(resident && ((width&3) || !quant130_prepare(width,height)))return 0;
#endif
    deferred128.resident130=resident;
    deferred128.state=active;deferred128.shadow=*s;
    deferred128.color=color;deferred128.depth=depth;
    deferred128.color_read=color_read;deferred128.depth_transfer=depth_transfer;
    ID3D11Texture2D_AddRef(color);ID3D11Texture2D_AddRef(depth);
    ID3D11Texture2D_AddRef(color_read);ID3D11Texture2D_AddRef(depth_transfer);
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
    deferred128.generation109=color109_generation;
#endif
    ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);bound.targets=0;
    if(!resident){
    ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)color_read,(ID3D11Resource*)color);
    ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)depth_transfer,(ID3D11Resource*)depth);
    /* The baseline Map submitted this work immediately. Keep that overlap
     * opportunity while omitting the wait until a relevant CPU consumer. */
    ID3D11DeviceContext_Flush(ctx);
    }
#ifdef NIGHTFIRE_RESIDENT_MAIN130
    if(resident)resident130_retained++;
#endif
#ifdef NIGHTFIRE_EARLY_SUBMIT113
    early113_reset_interval();
#endif
    deferred128.valid=1;pending=0;defer128_active_dirty=0;nf_hw_clear_pending=1;defer128_queued++;
    return 1;
}
static int deferred128_publish(void){
    if(!deferred128.valid)return 1;
    const NFHardwareState *s=&deferred128.state;D3D11_MAPPED_SUBRESOURCE m;
#ifdef NIGHTFIRE_RESIDENT_MAIN130
    if(deferred128.resident130){
        ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);bound.targets=0;
        ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)deferred128.color_read,(ID3D11Resource*)deferred128.color);
        ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)deferred128.depth_transfer,(ID3D11Resource*)deferred128.depth);
    }
#endif
    CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)deferred128.color_read,0,D3D11_MAP_READ,0,&m));
    for(unsigned y=0;y<s->height;y++)
        memcpy(s->color+(size_t)y*s->pitch,(uint8_t*)m.pData+(size_t)y*m.RowPitch,s->width*4);
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
    if(color109_ready(nightfire_gpu_timing_frame()))
        nf_color_reupload109_capture(nightfire_gpu_timing_frame(),deferred128.color,deferred128.generation109,s->width,s->height,m.pData,m.RowPitch);
#endif
    ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)deferred128.color_read,0);
    CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)deferred128.depth_transfer,0,D3D11_MAP_READ,0,&m));
    for(unsigned y=0;y<s->height;y++)
        nf_depth_pack((uint32_t*)(s->depth+(size_t)y*s->depth_pitch),
                      (float*)((uint8_t*)m.pData+(size_t)y*m.RowPitch),s->width);
    ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)deferred128.depth_transfer,0);
    depth_observe_sync(s->width,s->height);
    RELEASE(deferred128.color);RELEASE(deferred128.depth);
    RELEASE(deferred128.color_read);RELEASE(deferred128.depth_transfer);
#ifdef NIGHTFIRE_RESIDENT_MAIN130
    if(deferred128.resident130)resident130_published++;
#endif
    deferred128.valid=0;deferred128.resident130=0;transfers++;defer128_published++;
    if(pending)defer128_paired++;else defer128_early++;
    nf_hw_clear_pending=pending && defer128_active_dirty;return 1;
}
#endif
static int sync_impl_mode(unsigned shadow_only){if(resident313_blocked()){return 0;}
    if(!pending){
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
        return shadow_only?1:deferred128_publish();
#else
        return 1;
#endif
    }
#ifdef NIGHTFIRE_EARLY_SUBMIT113
    early113_reset_interval();
#endif
    nf_gpu_time84_stamp(ctx,2);
    ID3D11DeviceContext_OMSetRenderTargets(ctx,0,NULL,NULL);bound.targets=0;
    ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)color_read,(ID3D11Resource*)color);
    if(!active.color_only)ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)depth_transfer,(ID3D11Resource*)depth);
    nf_gpu_time84_stamp(ctx,3);nf_gpu_time84_end(ctx);
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
    /* Queue both intervals before waiting, then preserve original publication
     * order: old main color/depth first, current shadow color second. */
    if(!shadow_only && !deferred128_publish())return 0;
#endif
    D3D11_MAPPED_SUBRESOURCE m;
    uint64_t timing84=nf_gpu_time84_start();
    BT244_START(bt_cmap244);
    CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)color_read,0,D3D11_MAP_READ,0,&m));
    BT244_END(bt_cmap244,BT244_COLOR_MAP);BT244_START(bt_ccopy244);
    nf_gpu_time84_cpu(NF_GPU_TIME84_COLOR_MAP_READ,timing84);timing84=nf_gpu_time84_start();
#ifdef NIGHTFIRE_SWIZZLED_SHADOW131
    if(active.color_layout){nf_swizzled131_export_size(active.color,m.pData,m.RowPitch,width);swizzled131_exports++;swizzled132_exports+=active.color_layout==NF_COLOR_SWIZZLED_128_132;}else
#endif
    for(unsigned y=0;y<height;y++)memcpy(active.color+(size_t)y*active.pitch,(uint8_t*)m.pData+(size_t)y*m.RowPitch,width*4);
#ifdef NIGHTFIRE_SHADOW_LAYOUT131_DIAGNOSTIC
    nf_shadow131_publish(active.color,m.pData,m.RowPitch,width,height);
#endif
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
    if(color109_ready(nightfire_gpu_timing_frame()))nf_color_reupload109_capture(nightfire_gpu_timing_frame(),color,color109_generation,width,height,m.pData,m.RowPitch);
#endif
#ifdef NIGHTFIRE_COLOR_REUSE110
    /* Snapshot GPU rows before guest depth packing, which may alias color. */
    if(color110_enabled())nf_color_reuse110_capture(color,color110_generation,width,height,m.pData,m.RowPitch);
#endif
    ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)color_read,0);BT244_END(bt_ccopy244,BT244_COLOR_COPY);
    nf_gpu_time84_cpu(NF_GPU_TIME84_COLOR_ROW_COPY,timing84);timing84=nf_gpu_time84_start();
    if(!active.color_only){
        BT244_START(bt_zmap244);
        CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)depth_transfer,0,D3D11_MAP_READ,0,&m));
        BT244_END(bt_zmap244,BT244_DEPTH_MAP);BT244_START(bt_zpack244);
        nf_gpu_time84_cpu(NF_GPU_TIME84_DEPTH_MAP_READ,timing84);timing84=nf_gpu_time84_start();
        for(unsigned y=0;y<height;y++){
            uint32_t *dst=(uint32_t*)(active.depth+(size_t)y*active.depth_pitch);float *src=(float*)((uint8_t*)m.pData+(size_t)y*m.RowPitch);
            nf_depth_pack(dst,src,width);
        }
        ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)depth_transfer,0);BT244_END(bt_zpack244,BT244_DEPTH_PACK);
        nf_gpu_time84_cpu(NF_GPU_TIME84_DEPTH_PACK,timing84);
        depth_observe_sync(width,height);
    }
    nf_gpu_time84_finish_cpu();
    pending=0;nf_hw_clear_pending=0;
#ifdef NIGHTFIRE_RESIDENT_MAIN130
    if(deferred128.valid && deferred128.resident130){nf_hw_clear_pending=1;if(shadow_only)resident130_shadow_maps++;}
#endif
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
    defer128_active_dirty=0;
#endif
    transfers++;return 1;
}
static int sync_impl(void){return sync_impl_mode(0);}
#ifdef NIGHTFIRE_RESIDENT_MAIN130
/* Only the recorded disjoint shadow can be published without its older main.
 * Unknown consumers always enter nf_hw_sync and publish both before returning. */
static int resident130_shadow_sync(void){
    if(!deferred128.valid || !deferred128.resident130 || (pending && !deferred128_shadow(&active)))return sync_impl();
    uint64_t t=hw_clock();int ok=sync_impl_mode(1);sync_ticks+=hw_clock()-t;return ok;
}
static int resident130_resume(const NFHardwareState *s){
    if(!resident130_main(s) || pending || color!=deferred128.color || depth!=deferred128.depth)return 0;
    /* Exactly the old pack/unpack boundary, without publishing or reading RAM.
     * No D3D11 state used by cached VS/PS/IA bindings changes except targets. */
    quant130_apply(depth);
    RELEASE(deferred128.color);RELEASE(deferred128.depth);
    RELEASE(deferred128.color_read);RELEASE(deferred128.depth_transfer);
    deferred128.valid=deferred128.resident130=0;
    active=*s;pending=1;defer128_active_dirty=1;nf_hw_clear_pending=1;resident130_resumed++;
    return 1;
}
#endif
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
__declspec(noinline) int nf_hw_sync(void){if(resident313_blocked())return 0;
 static uint64_t previous_draws;unsigned frame=nightfire_gpu_timing_frame();
 int observed=pending && color109_ready(frame);uint64_t prior=transfers;
 uint64_t t=hw_clock();int ok=sync_impl();if(!ok)nf_gpu_time84_abandon(ctx);sync_ticks+=hw_clock()-t;
 if(transfers!=prior){
  if(observed)fprintf(stderr,"[COLOR-SYNC109] frame=%u reason=%s caller_rva=%llX resource=%p generation=%llu guest=%p depth=%p size=%ux%u pitch=%u color_only=%u draws=%llu\n",frame,sync109_reason,(unsigned long long)((uintptr_t)_ReturnAddress()-(uintptr_t)GetModuleHandleW(NULL)),color,(unsigned long long)color109_generation,active.color,active.depth,width,height,active.pitch,active.color_only,(unsigned long long)(draws-previous_draws));
  previous_draws=draws;
 }
 return ok;
}
static int sync_reason109(const char *reason){if(resident313_blocked()){return 0;}const char *prior=sync109_reason;sync109_reason=reason;int ok=nf_hw_sync();sync109_reason=prior;return ok;}
int nf_hw_sync_tag109(const char *reason){if(resident313_blocked())return 0;return sync_reason109(reason?reason:"external");}
int nf_hw_clear111_wants(void){
 static unsigned count,last;
 unsigned frame=nightfire_gpu_timing_frame();
 if(!color109_ready(frame))return 0;
 if(!count || frame!=last){if(count==4)return 0;last=frame;count++;}
 return 1;
}
void nf_hw_clear111_observe(const NFHardwareClear111 *c){
 if(!c || !nf_hw_clear111_wants())return;
 /* raw81 describes command data only, not runtime HW/depth enable toggles.
  * Do not initialize those executor options or invoke the actual clear API. */
 unsigned raw81=c->mask==3 && c->z_value==0xffffff00u && c->depth_valid &&
  !c->clip_x && !c->clip_y && !(c->clear_x&0xffff) && !(c->clear_y&0xffff) &&
  (c->clear_x>>16)+1>=c->width && (c->clear_y>>16)+1>=c->height;
 const char *match="match";
 if(initialized<=0 || !dsv)match="backend-unready";
 else if(!pending)match="no-pending";
 else if(active.color_only)match="pending-color-only";
 else if(!c->depth_valid || !c->color_host || !c->depth_host)match="invalid-clear-target";
 else if(c->color_host!=(uintptr_t)active.color || c->depth_host!=(uintptr_t)active.depth)match="target-mismatch";
 else if(c->width!=width || c->height!=height)match="size-mismatch";
 else if(c->pitch!=active.pitch || c->depth_pitch!=active.depth_pitch)match="pitch-mismatch";
 else {
  uintptr_t cb=(uintptr_t)c->pitch*c->height,zb=(uintptr_t)c->depth_pitch*c->height;
  if(cb>UINTPTR_MAX-c->color_host || zb>UINTPTR_MAX-c->depth_host ||
     (c->color_host<c->depth_host+zb && c->depth_host<c->color_host+cb))match="overlap-or-range";
 }
 fprintf(stderr,"[CLEAR111] frame=%u mask=%08X color=%08X z=%08X raw=%08X/%08X resolved=%08X/%08X host=%p/%p size=%ux%u pitch=%u/%u clip=%u,%u rect=%08X/%08X format=%08X control0=%08X depth_valid=%u gpu_clear=%u raw81=%u descriptor=%s pending=%d active=%p/%p active_size=%ux%u active_pitch=%u/%u active_color_only=%u resource=%p generation=%llu\n",
  nightfire_gpu_timing_frame(),c->mask,c->color_value,c->z_value,c->raw_color,c->raw_depth,c->resolved_color,c->resolved_depth,
  (void*)c->color_host,(void*)c->depth_host,c->width,c->height,c->pitch,c->depth_pitch,c->clip_x,c->clip_y,c->clear_x,c->clear_y,c->format,c->control0,c->depth_valid,c->gpu_clear_enabled,raw81,match,pending,
  active.color,active.depth,width,height,active.pitch,active.depth_pitch,active.color_only,color,(unsigned long long)color109_generation);
}
#else
int nf_hw_sync(void){if(resident313_blocked())return 0;uint64_t t=hw_clock();int ok=sync_impl();if(!ok)nf_gpu_time84_abandon(ctx);sync_ticks+=hw_clock()-t;return ok;}
#endif
int nf_hw_clear_depth(const NFHardwareState *s){if(resident313_blocked())return 0;
    if(!s || initialized<=0 || !pending || active.color_only || s->color_only || active.color_layout || s->color_layout || !dsv || !s->color || !s->depth ||
       s->color!=active.color || s->depth!=active.depth || s->width!=width || s->height!=height ||
       s->pitch!=active.pitch || s->depth_pitch!=active.depth_pitch ||
       !width || !height || s->left || s->top || s->right!=width || s->bottom!=height)return 0;
    uintptr_t c=(uintptr_t)s->color,z=(uintptr_t)s->depth;
    size_t cb=(size_t)s->pitch*height,zb=(size_t)s->depth_pitch*height;
    if(cb>UINTPTR_MAX-c || zb>UINTPTR_MAX-z || (c<z+zb && z<c+cb))return 0;
    /* Every old depth is discarded. 1.0f is the exact existing upload for
     * guest depth 0xFFFFFF; no retained-depth quantization is skipped. */
    ID3D11DeviceContext_ClearDepthStencilView(ctx,dsv,D3D11_CLEAR_DEPTH,1.0f,0);
    depth_observe_clear();
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
    defer128_active_dirty=1;
#endif
    nf_hw_clear_pending=1;gpu_depth_clears++;return 1;
}
int nf_hw_clear_read(const void *address,size_t length){if(resident313_blocked()){return 0;}
    if(!nf_hw_clear_pending || !length)return 1;
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
    if(deferred128_alias(address,length)){
        clear_alias_syncs++;return HW_SYNC109("deferred128-source-alias");
    }
    if(!pending || !defer128_active_dirty)return 1;
#endif
    uintptr_t a=(uintptr_t)address,c=(uintptr_t)active.color,z=(uintptr_t)active.depth;
    size_t cb=(size_t)active.pitch*height,zb=(size_t)active.depth_pitch*height;
    if(length>UINTPTR_MAX-a || (a<c+cb && c<a+length) || (active.depth && a<z+zb && z<a+length)){
        clear_alias_syncs++;
#ifdef NIGHTFIRE_RESIDENT_MAIN130
        if(deferred128.valid && deferred128.resident130)return resident130_shadow_sync();
#endif
        return HW_SYNC109("source-or-command-alias");
    }
    return 1;
}
typedef struct {const uint8_t *address;unsigned identity228;unsigned format;size_t length;uint8_t *copy;ID3D11ShaderResourceView *view;
#ifdef NIGHTFIRE_TEXTURE_REUSE133
ID3D11Texture2D *texture;
#endif
} TextureEntry;
static TextureEntry textures[256];static unsigned replacement;
#ifdef NIGHTFIRE_TEXTURE_REUSE133
static int texture133_on=-1,texture133_timing=-1;
#ifdef NF_TEXTURE_REUSE133_TEST
static int texture133_fail_mip=-1;
void nf_hw_texture133_test_fail(int mip){texture133_fail_mip=mip;}
#endif
static struct {uint64_t hits,changed,misses,reused,creates,views,updates,bytes,evictions;
    uint64_t compare_ticks,decode_ticks,create_ticks,view_ticks,update_ticks,alias_ticks;} texture133;
static int texture133_enabled(void){if(texture133_on<0){const char *v=getenv("NIGHTFIRE_TEXTURE_REUSE133");texture133_on=v && !strcmp(v,"1");}return texture133_on;}
static uint64_t texture133_clock(void){if(texture133_timing<0){const char *v=getenv("NIGHTFIRE_TEXTURE_REUSE133_TIMING");texture133_timing=v && !strcmp(v,"1");}return texture133_timing?hw_clock():0;}
#define T133_START(name) uint64_t name=texture133_clock()
#define T133_END(name,field) do{if(name)texture133.field+=hw_clock()-(name);}while(0)
#define T133_COUNT(field) (++texture133.field)
/* Prepare every mip before mutating a reusable GPU resource. The old raw
 * snapshot/view remain authoritative on allocation or decode failure. */
static ID3D11ShaderResourceView *texture133_replace(const NFHardwareState *s,TextureEntry *e,
    size_t length,unsigned w,unsigned h,unsigned levels,unsigned bc,unsigned f,const void *identity228){
    uint8_t *copy=malloc(length),*decoded[16]={0};D3D11_SUBRESOURCE_DATA initial[16]={0};
    ID3D11Texture2D *fresh=NULL;ID3D11ShaderResourceView *view=NULL;
    if(!copy)return NULL;memcpy(copy,s->texture,length);
    unsigned mw=w,mh=h;size_t offset=0;
    for(unsigned i=0;i<levels;i++){
#ifdef NF_TEXTURE_REUSE133_TEST
        if(texture133_fail_mip==(int)i)goto fail;
#endif
        unsigned pitch=bc?((mw+3)/4)*bc:mw*4,rows=bc?(mh+3)/4:mh;
        initial[i].pSysMem=copy+offset;initial[i].SysMemPitch=pitch;
        if(!bc){
            decoded[i]=malloc((size_t)pitch*rows);if(!decoded[i])goto fail;
            T133_START(started);
#ifdef NIGHTFIRE_TEXTURE_DECODE100
            uint64_t decode_started=hw_clock();
            if(decode100_enabled())nf_texture_decode100(decoded[i],copy+offset,mw,mh,f==7);else
#endif
            {xbox_unswizzle_rect(decoded[i],copy+offset,mw,mh,4);
             if(f==7)for(unsigned j=0;j<mw*mh;j++)((uint32_t*)decoded[i])[j]|=0xff000000;}
#ifdef NIGHTFIRE_TEXTURE_DECODE100
            decode100_ticks+=hw_clock()-decode_started;decode100_calls++;decode100_bytes+=(uint64_t)mw*mh*4;
#endif
            T133_END(started,decode_ticks);initial[i].pSysMem=decoded[i];
        }
        offset+=(size_t)pitch*rows;if(mw>1)mw/=2;if(mh>1)mh/=2;
    }
    if(!identity228 && !e->identity228 && e->texture && e->view && e->address==s->texture && e->format==s->texture_format && e->length==length){
        for(unsigned i=0;i<levels;i++){
            T133_START(started);ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)e->texture,i,NULL,initial[i].pSysMem,initial[i].SysMemPitch,0);
            T133_END(started,update_ticks);texture133.updates++;
        }
        texture133.reused++;
    }else{
        D3D11_TEXTURE2D_DESC d={0};d.Width=w;d.Height=h;d.MipLevels=levels;d.ArraySize=d.SampleDesc.Count=1;
        d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        d.Format=f==0xc?DXGI_FORMAT_BC1_UNORM:f==0xe?DXGI_FORMAT_BC2_UNORM:f==0xf?DXGI_FORMAT_BC3_UNORM:DXGI_FORMAT_B8G8R8A8_UNORM;
        T133_START(started);HRESULT hr=ID3D11Device_CreateTexture2D(dev,&d,initial,&fresh);T133_END(started,create_ticks);texture133.creates++;
        if(FAILED(hr))goto fail;
        T133_START(view_started);hr=ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)fresh,NULL,&view);T133_END(view_started,view_ticks);texture133.views++;
        if(FAILED(hr))goto fail;
        RELEASE(e->view);RELEASE(e->texture);e->view=view;e->texture=fresh;view=NULL;fresh=NULL;
    }
    free(e->copy);e->copy=copy;e->length=length;e->address=identity228?(const uint8_t*)identity228:s->texture;e->identity228=identity228!=NULL;e->format=s->texture_format;
    for(unsigned i=0;i<levels;i++)free(decoded[i]);texture133.bytes+=length;uploads++;return e->view;
fail:
    RELEASE(view);RELEASE(fresh);for(unsigned i=0;i<levels;i++)free(decoded[i]);free(copy);return NULL;
}
#ifdef NF_TEXTURE_REUSE133_TEST
/* Test mode switches complete work and explicitly drop cache ownership. */
void nf_hw_texture133_test_set(int on){
    if(!nf_hw_sync())abort();
    if(ctx){ID3D11ShaderResourceView *v=NULL;ID3D11DeviceContext_PSSetShaderResources(ctx,0,1,&v);bound.texture=NULL;}
    for(unsigned i=0;i<256;i++){RELEASE(textures[i].view);RELEASE(textures[i].texture);free(textures[i].copy);memset(&textures[i],0,sizeof textures[i]);}
    replacement=0;texture133_on=!!on;
}
void nf_hw_texture133_test_stats(uint64_t out[8]){
    out[0]=texture133.hits;out[1]=texture133.changed;out[2]=texture133.misses;out[3]=texture133.reused;
    out[4]=texture133.creates;out[5]=texture133.views;out[6]=texture133.updates;out[7]=texture133.evictions;
}
#endif
#else
#define T133_START(name)
#define T133_END(name,field) ((void)0)
#define T133_COUNT(field) ((void)0)
#endif

typedef struct {const uint8_t *address;unsigned width,height,pitch;size_t length;uint8_t *copy;ID3D11Texture2D *texture;ID3D11ShaderResourceView *view;} VideoTextureEntry;
static VideoTextureEntry video_textures[8];static unsigned video_replacement;static uint64_t video_uploads;
static ID3D11ShaderResourceView *video_texture_view(const NFHardwareState *s){
    unsigned w=s->texture_width,h=s->texture_height,pitch=s->texture_pitch;
    if(!w || (w&1) || !h || w>2048 || h>2048 || pitch<w*2 || pitch>16384 || s->filtered || s->program)return NULL;
    size_t length=(size_t)pitch*h;if(length>s->texture_available)return NULL;
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
    if(deferred128_alias(s->texture,length) && !HW_SYNC109("deferred128-movie-alias"))return NULL;
#endif
    if(pending){uintptr_t a=(uintptr_t)s->texture,b=a+length,c=(uintptr_t)active.color,d=(uintptr_t)active.depth;
        if((a<c+(size_t)active.pitch*height && b>c)||(active.depth && a<d+(size_t)active.depth_pitch*height && b>d)){
#ifdef NIGHTFIRE_RESIDENT_MAIN130
            if(deferred128.valid && deferred128.resident130){if(!resident130_shadow_sync())return NULL;}else
#endif
            if(!HW_SYNC109("movie-texture-alias"))return NULL;
        }}
    VideoTextureEntry *e=NULL;
    for(unsigned i=0;i<8;i++)if(video_textures[i].address==s->texture){e=&video_textures[i];break;}
    if(!e)e=&video_textures[video_replacement++%8];
    if(e->address!=s->texture || e->width!=w || e->height!=h || e->pitch!=pitch){
        RELEASE(e->view);RELEASE(e->texture);free(e->copy);memset(e,0,sizeof *e);
        D3D11_TEXTURE2D_DESC desc={0};desc.Width=w/2;desc.Height=h;desc.MipLevels=desc.ArraySize=1;desc.SampleDesc.Count=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        HRESULT hr=ID3D11Device_CreateTexture2D(dev,&desc,NULL,&e->texture);
        if(SUCCEEDED(hr))hr=ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)e->texture,NULL,&e->view);
        if(FAILED(hr)){RELEASE(e->texture);return NULL;}
        e->copy=malloc(length);if(!e->copy){RELEASE(e->view);RELEASE(e->texture);return NULL;}
        e->address=s->texture;e->width=w;e->height=h;e->pitch=pitch;
    }
    if(e->length!=length || !texture_bytes_equal(e->copy,s->texture,length)){
        memcpy(e->copy,s->texture,length);e->length=length;
        ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)e->texture,0,NULL,e->copy,pitch,0);uploads++;video_uploads++;
    }
    return e->view;
}
/* These caches own at most4 movie/128 indexed texture snapshots and resources.
 * Callers must keep their selected guest mappings stable through begin(). */
static RGB565148Entry textures_rgb179[4];static Palette175Entry textures_pal179[128];
static unsigned rgb_next179,pal_next179;static uint64_t texture179_stats[7];
#ifdef NF_TEXTURE179_TEST
static unsigned alias_fail179;
#ifdef PALETTE175_TEST
void nf_hw_texture179_test_palettefail(unsigned stage){palette175_fail_stage=stage;}
#endif
void nf_hw_texture179_test_fail(unsigned on){alias_fail179=on;}
void nf_hw_texture179_test_stats(uint64_t *out){memcpy(out,texture179_stats,sizeof texture179_stats);}
#endif
static int range179(const void *a,size_t an,const void *b,size_t bn){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if(!a||!b||!an||!bn)return 0;
 if(an>UINTPTR_MAX-x||bn>UINTPTR_MAX-y)return 1;
 return x<y+bn&&y<x+an;
}
static int texture_range179(const void *address,size_t bytes){
 /* Unconditional draw-alias completion, not nf_hw_clear_pending-gated. */
 int alias=0;
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
 alias=deferred128_alias(address,bytes);
#endif
 if(pending)alias|=range179(address,bytes,active.color,(size_t)active.pitch*height)||
                      range179(address,bytes,active.depth,(size_t)active.depth_pitch*height);
 if(!alias)return 1;
#ifdef NF_TEXTURE179_TEST
 if(alias_fail179)return 0;
#endif
 texture179_stats[0]++;
 /* Full completion publishes every retained target, including resident and
  * deferred intervals. Do not return a cached view before that publication. */
 return HW_SYNC109("selected179-source-or-palette-alias");
}
static ID3D11ShaderResourceView *selected_texture179(const NFHardwareState *s,unsigned f,unsigned w,unsigned h,unsigned levels){
 size_t bytes;RGB565148Layout layout;ID3D11ShaderResourceView *view;
 if((s->texture_format&0xfc)!=0x28)return NULL;
 if(f==5||f==0x11){
  unsigned pitch=0;if(f==0x11){w=s->texture_width;h=s->texture_height;pitch=s->texture_pitch;}
  if(!rgb565148_layout(&layout,f,w,h,pitch,levels,s->texture_available)||!palette175_readable(s->texture,layout.span))return NULL;
  if(!texture_range179(s->texture,layout.span))return NULL;
#ifdef NIGHTFIRE_MOVIE_RGB232
  /* Separate mutable resources: ordinary material SRVs remain immutable.
   * Complete prior work before updating an old movie generation. */
  if(s->experimental_movie200){
   if(f!=0x11 || (pending && !HW_SYNC109("movie232-source-update")))return NULL;
   MovieRGB232Entry *movie=NULL;
   for(unsigned i=0;i<4;i++)if(movie_rgb232[i].source==s->texture && movie_rgb232[i].layout.width==w && movie_rgb232[i].layout.height==h && movie_rgb232[i].layout.pitch==pitch){movie=&movie_rgb232[i];break;}
   if(!movie){movie=&movie_rgb232[movie_rgb232_next++%4];movie_rgb232_release(movie);}
   uint64_t hits=movie->hits,up=movie->uploads;
   view=movie_rgb232_get(movie,dev,ctx,s->texture,s->texture_available,w,h,pitch);
   texture179_stats[1]+=movie->hits-hits;texture179_stats[2]+=movie->uploads-up;uploads+=movie->uploads-up;return view;
  }
#endif
  RGB565148Entry *e=NULL;
  for(unsigned i=0;i<4;i++)if(textures_rgb179[i].source==s->texture&&textures_rgb179[i].layout.format==f&&textures_rgb179[i].layout.width==w&&textures_rgb179[i].layout.height==h&&textures_rgb179[i].layout.pitch==layout.pitch){e=&textures_rgb179[i];break;}
  if(!e){e=&textures_rgb179[rgb_next179++%4];if(e->view)texture179_stats[5]++;rgb565148_release(e);}
  uint64_t hits=e->hits,up=e->uploads;view=rgb565148_get(e,dev,s->texture,s->texture_available,f,w,h,pitch,levels);
  texture179_stats[1]+=e->hits-hits;texture179_stats[2]+=e->uploads-up;uploads+=e->uploads-up;return view;
 }
 if(f!=0x0b||levels!=1||w!=64||(h!=32&&h!=64))return NULL;
 bytes=(size_t)w*h;
 /* Validate BOTH selected spans before finishing aliases or touching bytes.
  * A malformed palette cannot cause even the index snapshot to be read. */
 if(bytes>s->texture_available||s->texture_palette_available<1024||
    !palette175_readable(s->texture,bytes)||!palette175_readable(s->texture_palette,1024))return NULL;
 if(!texture_range179(s->texture,bytes)||!texture_range179(s->texture_palette,1024))return NULL;
 Palette175Entry *e=NULL;
 for(unsigned i=0;i<128;i++)if(textures_pal179[i].source==s->texture&&textures_pal179[i].palette==s->texture_palette&&textures_pal179[i].width==w&&textures_pal179[i].height==h){e=&textures_pal179[i];break;}
 if(!e){e=&textures_pal179[pal_next179++%128];if(e->view)texture179_stats[6]++;palette175_release(e);}
 uint64_t hits=e->hits,up=e->uploads;view=palette175_get(e,dev,s->texture,s->texture_available,s->texture_palette,s->texture_palette_available,f,w,h,levels);
 texture179_stats[3]+=e->hits-hits;texture179_stats[4]+=e->uploads-up;uploads+=e->uploads-up;return view;
}

#include "linear_rgba189.h"
static LinearEntry189 linear_entries189[4];static unsigned linear_next189;static uint64_t linear_stats189[4];
#ifdef NF_LINEAR189_TEST
void nf_hw_linear189_test_fail(unsigned stage){linear189_failure=stage;}
void nf_hw_linear189_test_stats(uint64_t out[4]){memcpy(out,linear_stats189,sizeof linear_stats189);}
#endif
static int movie200_admitted(const NFHardwareState *s){
 static const uint32_t original[9][4]={{0x0,0xec001b,0x836186c,0x20708800},{0x0,0xec201b,0x836186c,0x20704800},{0x0,0xec401b,0x836186c,0x20702800},{0x0,0xec601b,0x836186c,0x20701800},{0x0,0x20121b,0x836106c,0x2070f848},{0x0,0x20061b,0x836106c,0x2070f818},{0x0,0x2000ff,0xc436106c,0x20708828},{0x0,0x647401b,0xc4361bff,0x1078e800},{0x0,0x87601b,0xc400286c,0x3070e801}};
 if(!s->program||s->program->mode!=6||s->program->start||memcmp(s->program->code,original,sizeof original))return 0;
 for(unsigned i=0;i<9;i++)if(s->program->valid[i]!=15)return 0;
 if(s->experimental_movie200<1||s->experimental_movie200>2||s->experimental_resolve193||
    s->selected_coordinates182!=1||s->cpu_vertex182!=1||s->texture_stage182||!s->program||
    s->texture_format!=0x11129||!s->texture_width||!s->texture_height||s->texture_width>1024||s->texture_height>2048||
    s->width!=2*s->texture_width||s->height!=s->texture_height||s->texture_pitch<s->texture_width*2||
    s->color_only||s->color_layout||!s->depth||s->depth_pitch<s->width*4||
    s->fog_enable||s->combiner!=1||s->scale!=1||s->reverse_subtract97||s->compat_point_clamp||
    s->cull_enable||s->direct28||s->direct90||s->left||s->top||s->right!=s->width||s->bottom!=s->height||
    s->alpha_enable>1||s->alpha_func!=0x204||s->alpha_ref!=16||s->blend_enable>1||s->blend_src!=1||s->blend_dst||
    s->depth_enable>1||s->depth_write>1||s->depth_func!=0x203||s->texture_filter!=0x02062000||
    (s->texture_address!=0x30303&&s->texture_address!=0x10303))return 0;
 return 1;
}
/* Explicit world222 admission shares the already experimental LOD0 profile.
 * The only additional filter word differs in its signed13-bit LOD bias. */
static int resolve_filter222(unsigned filter){
#ifdef NIGHTFIRE_WORLD_RESOLVE222
 if(filter==0x04073f01)return 1;
#endif
 return filter==0x04072000;
}
static int resolve193_admitted(const NFHardwareState *s){
 static const uint32_t original[2][4]={{0,0x0020001b,0x0836106c,0x2070f800},{0,0x0020121b,0x0836106c,0x2070f849}};
 if(s->experimental_resolve193<1||s->experimental_resolve193>2||!s->selected_coordinates182||s->texture_stage182||
    !s->program||s->program->mode!=6||s->program->start||s->program->valid[0]!=15||s->program->valid[1]!=15||memcmp(s->program->code,original,sizeof original)||
    !s->width||!s->height||s->width>1024||s->height>2048||s->texture_width!=2*s->width||s->texture_height!=s->height||
    s->texture_format!=0x00011229||!resolve_filter222(s->texture_filter)||(s->texture_address!=0x00010303&&s->texture_address!=0x00030303)||
    !s->color_only||s->color_layout||s->depth||s->depth_pitch||s->depth_enable||s->depth_write>1||
    s->alpha_enable||s->blend_enable||s->fog_enable||s->combiner||s->scale!=1||s->reverse_subtract97||s->compat_point_clamp||
    s->cull_enable||s->direct28||s->direct90||s->left||s->top||s->right!=s->width||s->bottom!=s->height)return 0;
 return 1;
}
static int resolve193_vertices(const NFHardwareInputVertex *v,unsigned n){
 if(!v||n!=3)return 0;float q=v[0].attributes[9][3];if(!isfinite(q)||q<=0)return 0;
 for(unsigned i=0;i<3;i++){
  const float *p=v[i].attributes[0],*t=v[i].attributes[9];
  float x=i==1?4.f*active.width:0,y=i==2?4.f*active.height:0;
  if(p[0]!=x||p[1]!=y||p[2]!=0||p[3]!=1||t[3]!=q||t[2]!=0||t[0]/q!=2*x+.5f||t[1]/q!=y)return 0;
 }
 return 1;
}

static ID3D11ShaderResourceView *linear_texture189(const NFHardwareState *s,unsigned levels){
 LinearLayout189 layout;LinearEntry189 *e=NULL;int replacement=0;
 /* Explicitly refuse the original convolution draw until that operation has
  * an implementation; support for its source layout is not resolve support. */
 if((s->texture_format&0xfc)!=0x28||(((s->texture_filter>>16)&63)==7&&!resolve193_admitted(s))||
    !linear189_layout(&layout,s->texture_width,s->texture_height,s->texture_pitch,levels,s->texture_available)||
    !palette175_readable(s->texture,layout.span))return NULL;
 if(!texture_range179(s->texture,layout.span))return NULL;
 for(unsigned i=0;i<4;i++)if(linear_entries189[i].source==s->texture&&linear_entries189[i].layout.width==layout.width&&linear_entries189[i].layout.height==layout.height&&linear_entries189[i].layout.pitch==layout.pitch){e=&linear_entries189[i];break;}
 if(!e){e=&linear_entries189[linear_next189%4];replacement=1;}
 uint64_t hits=e->hits,uploads_before=e->uploads;int had_view=e->view!=NULL;
 ID3D11ShaderResourceView *view=linear189_get(e,dev,s->texture,&layout);
 linear_stats189[0]+=e->hits-hits;linear_stats189[1]+=e->uploads-uploads_before;uploads+=e->uploads-uploads_before;
 if(!view){linear_stats189[3]++;return NULL;}
 if(replacement){linear_next189++;if(had_view)linear_stats189[2]++;}return view;
}

static ID3D11ShaderResourceView *texture_view_impl(const NFHardwareState *s,const void *identity228){
    if(!s->material221)identity228=NULL;
    const uint8_t *key228=identity228?(const uint8_t*)identity228:s->texture;
    unsigned f=(s->texture_format>>8)&255,w=1u<<((s->texture_format>>20)&15),h=1u<<((s->texture_format>>24)&15),levels=(s->texture_format>>16)&15;
    if(s->texture && f==0x12)return linear_texture189(s,levels);
    if(s->texture && ((f==5 && !s->material221) || f==0x11 || f==0x0b))return selected_texture179(s,f,w,h,levels);
    if(s->texture && (f==0x24 || f==0x25))return video_texture_view(s);
    if(!s->texture || !levels || levels>1+(((s->texture_format>>20)&15)>((s->texture_format>>24)&15)?((s->texture_format>>20)&15):((s->texture_format>>24)&15)) || w>2048 || h>2048 || (s->texture_format&0xfc)!=0x28)return NULL;
    unsigned bc=d3d8_format_dxt_block_bytes(f);if(!bc && f!=6 && f!=7 && !(s->material221 && f==5))return NULL;
    unsigned texel_bytes221=f==5?2:4;
    size_t length=0;unsigned mw=w,mh=h;
    for(unsigned i=0;i<levels;i++){length+=bc?(size_t)((mw+3)/4)*((mh+3)/4)*bc:(size_t)mw*mh*texel_bytes221;if(mw>1)mw/=2;if(mh>1)mh/=2;}
    if(length>s->texture_available)return NULL;
    T133_START(alias_started);
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
    if(deferred128_alias(s->texture,length) && !HW_SYNC109("deferred128-texture-alias"))return NULL;
#endif
    if(pending){
        uintptr_t a=(uintptr_t)s->texture,b=a+length,c=(uintptr_t)active.color,d=(uintptr_t)active.depth;
        if((a<c+(size_t)active.pitch*height && b>c)||(active.depth && a<d+(size_t)active.depth_pitch*height && b>d))
            {
#ifdef NIGHTFIRE_RESIDENT_MAIN130
                if(deferred128.valid && deferred128.resident130){if(!resident130_shadow_sync())return NULL;}else
#endif
                if(!HW_SYNC109("texture-alias"))return NULL;
            }
    }
    T133_END(alias_started,alias_ticks);
#ifdef NIGHTFIRE_SHADOW_LAYOUT131_DIAGNOSTIC
    nf_shadow131_texture(s->texture,s->texture_format,s->texture_filter,s->texture_address,length);
    nf_material132_texture(s->texture,s->texture_format,length);
#endif
    TextureEntry *e=NULL;
    for(unsigned i=0;i<256;i++)if(textures[i].address==key228 && textures[i].identity228==(identity228!=NULL) && textures[i].format==s->texture_format){e=&textures[i];break;}
    if(e && e->length==length){T133_START(compare_started);int equal=texture_bytes_equal(e->copy,s->texture,length);T133_END(compare_started,compare_ticks);
        if(equal){T133_COUNT(hits);return e->view;}}
    if(e)T133_COUNT(changed);else{T133_COUNT(misses);e=&textures[replacement++%256];if(e->address)T133_COUNT(evictions);}
#ifdef NIGHTFIRE_TEXTURE_REUSE133
    if(texture133_enabled() && f!=5)return texture133_replace(s,e,length,w,h,levels,bc,f,identity228);
    RELEASE(e->texture);
#endif
    RELEASE(e->view);free(e->copy);memset(e,0,sizeof *e);
    e->copy=malloc(length);if(!e->copy)return NULL;memcpy(e->copy,s->texture,length);e->length=length;
    D3D11_SUBRESOURCE_DATA initial[16]={0};uint8_t *decoded[16]={0};size_t offset=0;mw=w;mh=h;
    for(unsigned i=0;i<levels;i++){
        unsigned pitch=bc?((mw+3)/4)*bc:mw*texel_bytes221,rows=bc?(mh+3)/4:mh;
        initial[i].pSysMem=e->copy+offset;initial[i].SysMemPitch=pitch;
        if(!bc){decoded[i]=malloc((size_t)pitch*rows);if(!decoded[i])goto fail;
            T133_START(decode133_started);
#ifdef NIGHTFIRE_TEXTURE_DECODE100
            uint64_t decode_started=hw_clock();
            if(decode100_enabled() && f!=5)nf_texture_decode100(decoded[i],e->copy+offset,mw,mh,f==7);
            else
#endif
            {
                xbox_unswizzle_rect(decoded[i],e->copy+offset,mw,mh,texel_bytes221);
                if(f==7)for(unsigned j=0;j<mw*mh;j++)((uint32_t*)decoded[i])[j]|=0xff000000;
            }
#ifdef NIGHTFIRE_TEXTURE_DECODE100
            decode100_ticks+=hw_clock()-decode_started;decode100_calls++;decode100_bytes+=(uint64_t)mw*mh*4;
#endif
            T133_END(decode133_started,decode_ticks);initial[i].pSysMem=decoded[i];}
        offset+=(size_t)pitch*rows;if(mw>1)mw/=2;if(mh>1)mh/=2;
    }
    D3D11_TEXTURE2D_DESC d={0};d.Width=w;d.Height=h;d.MipLevels=levels;d.ArraySize=1;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    d.Format=f==5?DXGI_FORMAT_B5G6R5_UNORM:f==0xc?DXGI_FORMAT_BC1_UNORM:f==0xe?DXGI_FORMAT_BC2_UNORM:f==0xf?DXGI_FORMAT_BC3_UNORM:DXGI_FORMAT_B8G8R8A8_UNORM;
    ID3D11Texture2D *t=NULL;T133_START(create_started);HRESULT hr=ID3D11Device_CreateTexture2D(dev,&d,initial,&t);T133_END(create_started,create_ticks);T133_COUNT(creates);
    if(SUCCEEDED(hr)){T133_START(view_started);hr=ID3D11Device_CreateShaderResourceView(dev,(ID3D11Resource*)t,NULL,&e->view);T133_END(view_started,view_ticks);T133_COUNT(views);RELEASE(t);}
    for(unsigned i=0;i<levels;i++)free(decoded[i]);
    if(FAILED(hr)){fprintf(stderr,"[HW-GPU] texture rejected format=%08X hr=%08lX\n",s->texture_format,(unsigned long)hr);free(e->copy);memset(e,0,sizeof *e);return NULL;}
    e->address=key228;e->identity228=identity228!=NULL;e->format=s->texture_format;uploads++;return e->view;
fail:
    for(unsigned i=0;i<levels;i++)free(decoded[i]);free(e->copy);memset(e,0,sizeof *e);return NULL;
}
static ID3D11ShaderResourceView *texture_view_identity228(const NFHardwareState *s,const void *identity228){uint64_t t=hw_clock();ID3D11ShaderResourceView *v=texture_view_impl(s,identity228);texture_ticks+=hw_clock()-t;return v;}
static ID3D11ShaderResourceView *texture_view(const NFHardwareState *s){return texture_view_identity228(s,NULL);}
static ID3D11DepthStencilState *depth_states[32];
#include "nightfire_blend285.h"
static ID3D11BlendState *blend_states[6*16];
static ID3D11SamplerState *samplers[8*17];
static ID3D11VertexShader *dual_vs220;
static ID3D11PixelShader *dual_ps220;
static ID3D11InputLayout *dual_layout220;
static ID3D11Buffer *dual_vb220;
static ID3D11SamplerState *dual_sampler1_220;
/* begin_impl has early-return failure paths. The public begin wrapper releases
 * these temporary references on every exit, after actual binding owns them. */
static ID3D11ShaderResourceView *dual_begin_hold0_220,*dual_begin_hold1_220;
static const char dual_shader220[]=
NF_FOG_PARAMS_HLSL
"struct V{float4 p:POSITION;float4 uv0:TEXCOORD0;float4 uv1:TEXCOORD1;float4 c:COLOR0;float fog:TEXCOORD2;};"
"struct P{float4 p:SV_POSITION;float4 uv0:TEXCOORD0;float4 uv1:TEXCOORD1;float4 c:COLOR0;float fog:TEXCOORD2;};"
"P vertex(V v){P o;o.p=float4((v.p.x*2/surface.x-1)*v.p.w,(1-v.p.y*2/surface.y)*v.p.w,v.p.z/16777215*v.p.w,v.p.w);o.uv0=v.uv0;o.uv1=v.uv1;o.c=v.c;o.fog=v.fog;return o;}"
"Texture2D tex0:register(t0);Texture2D tex1:register(t1);SamplerState sampler0:register(s0);SamplerState sampler1:register(s1);"
"float4 pixel(P p):SV_TARGET{float4 a=tex0.Sample(sampler0,p.uv0.xy/p.uv0.w);float4 b=tex1.Sample(sampler1,p.uv1.xy/p.uv1.w);"
"float3 r0=a.a*a.rgb+(1-a.a)*b.rgb;float3 rgb=saturate(2*p.c.rgb*r0);"
"if(fogOn!=0)rgb=lerp(fogColor.rgb,rgb,saturate(p.fog));return float4(rgb,1);}"
;
static int initialize_dual220(void){
 if(dual_vs220&&dual_ps220&&dual_layout220&&dual_vb220&&dual_sampler1_220)return 1;
 RELEASE(dual_vs220);RELEASE(dual_ps220);RELEASE(dual_layout220);RELEASE(dual_vb220);RELEASE(dual_sampler1_220);
 ID3DBlob *v=NULL,*p=NULL,*errors=NULL;HRESULT hr;
 hr=D3DCompile(dual_shader220,sizeof dual_shader220-1,"dual220",NULL,NULL,"vertex","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&v,&errors);if(FAILED(hr))goto done;RELEASE(errors);
 hr=D3DCompile(dual_shader220,sizeof dual_shader220-1,"dual220",NULL,NULL,"pixel","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&p,&errors);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreateVertexShader(dev,v->lpVtbl->GetBufferPointer(v),v->lpVtbl->GetBufferSize(v),NULL,&dual_vs220);if(FAILED(hr))goto done;
 hr=ID3D11Device_CreatePixelShader(dev,p->lpVtbl->GetBufferPointer(p),p->lpVtbl->GetBufferSize(p),NULL,&dual_ps220);if(FAILED(hr))goto done;
 D3D11_INPUT_ELEMENT_DESC e[]={
 {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"TEXCOORD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"TEXCOORD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,48,D3D11_INPUT_PER_VERTEX_DATA,0},
 {"TEXCOORD",2,DXGI_FORMAT_R32_FLOAT,0,64,D3D11_INPUT_PER_VERTEX_DATA,0}};
 hr=ID3D11Device_CreateInputLayout(dev,e,5,v->lpVtbl->GetBufferPointer(v),v->lpVtbl->GetBufferSize(v),&dual_layout220);if(FAILED(hr))goto done;
 D3D11_BUFFER_DESC d={0};d.ByteWidth=16384*sizeof(NFHardwareDual220);d.Usage=D3D11_USAGE_DYNAMIC;d.BindFlags=D3D11_BIND_VERTEX_BUFFER;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
 hr=ID3D11Device_CreateBuffer(dev,&d,NULL,&dual_vb220);if(FAILED(hr))goto done;
 D3D11_SAMPLER_DESC sd={0};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=D3D11_TEXTURE_ADDRESS_WRAP;sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MipLODBias=-255.0f/256;sd.MaxLOD=15;sd.MaxAnisotropy=1;sd.ComparisonFunc=D3D11_COMPARISON_ALWAYS;
 hr=ID3D11Device_CreateSamplerState(dev,&sd,&dual_sampler1_220);
done:
 if(FAILED(hr)){fprintf(stderr,"[DUAL220] preparation failed %08lX %s\n",(unsigned long)hr,errors?(char*)errors->lpVtbl->GetBufferPointer(errors):"");RELEASE(dual_vs220);RELEASE(dual_ps220);RELEASE(dual_layout220);RELEASE(dual_vb220);RELEASE(dual_sampler1_220);}
 RELEASE(v);RELEASE(p);RELEASE(errors);return SUCCEEDED(hr);
}
#ifdef NIGHTFIRE_MATERIAL221
static ID3D11VertexShader *material_vs221;
static ID3D11InputLayout *material_layout221;
static ID3D11Buffer *material_vb221;
#ifdef NIGHTFIRE_MATERIAL_RING246
#define MATERIAL_RING_CAPACITY246 65535u
static unsigned material_cursor246,material_capacity246;
static int material_ring246;
static uint64_t material_discards246,material_appends246,material_wraps246;
static int material_ring_enabled246(void){
 static int setting=-1;if(setting<0){const char*v=getenv("DRIVING_MATERIAL_RING246");setting=v&&!strcmp(v,"1");}return setting;
}
#endif

static ID3D11ShaderResourceView *material_holds221[4];
static ID3D11SamplerState *material_samplers221[32];
static NFHardwareMaterial221 material_active221;
typedef struct {NFPixel221 key;ID3D11PixelShader *shader;} NFMaterialShader221;
static NFMaterialShader221 material_shaders221[64];static unsigned material_next221;
static const char material_vertex221[]=
NF_FOG_PARAMS_HLSL
"struct V{float4 p:POSITION;float4 uv0:TEXCOORD0;float4 uv1:TEXCOORD1;float4 uv2:TEXCOORD2;float4 uv3:TEXCOORD3;float4 c:COLOR0;float4 spec:COLOR1;float fog:TEXCOORD4;};"
"struct P{float4 p:SV_POSITION;float4 uv0:TEXCOORD0;float4 uv1:TEXCOORD1;float4 uv2:TEXCOORD2;float4 uv3:TEXCOORD3;float4 c:COLOR0;float4 spec:COLOR1;float fog:TEXCOORD4;};"
"P vertex(V v){P o;o.p=float4((v.p.x*2/surface.x-1)*v.p.w,(1-v.p.y*2/surface.y)*v.p.w,v.p.z/16777215*v.p.w,v.p.w);o.uv0=v.uv0;o.uv1=v.uv1;o.uv2=v.uv2;o.uv3=v.uv3;o.c=v.c;o.spec=v.spec;o.fog=v.fog;return o;}";
static int material_texture_layout221(const NFHardwareMaterialTexture221 *t,size_t *length){
 unsigned f=t->format>>8&255,wx=t->format>>20&15,hy=t->format>>24&15,levels=t->format>>16&15;
 unsigned w=1u<<wx,h=1u<<hy,a=t->anisotropy,um=t->address&15,vm=t->address>>8&15;
 if(!t->data || (f!=5&&f!=6&&f!=12&&f!=14) || (t->format&0xfc)!=0x28 ||
    wx>11||hy>11||!levels||levels>1+(wx>hy?wx:hy)||
    (a!=1&&a!=2&&a!=4&&a!=8)||t->bias215>1||
    t->filter!=(t->bias215?0x02063f01u:0x02062000u)||
    (um!=1&&um!=3)||(vm!=1&&vm!=3)||((t->address&~0x00000f0fu)!=0x10000 && (t->address&~0x00000f0fu)!=0x30000))return 0;
 size_t n=0;for(unsigned i=0;i<levels;i++){
  n+=f==12||f==14?(size_t)((w+3)/4)*((h+3)/4)*(f==12?8:16):(size_t)w*h*(f==5?2:4);
  if(w>1)w/=2;if(h>1)h/=2;
 }
 if(n>t->available||n>UINTPTR_MAX-(uintptr_t)t->data||!(nf_host_current261?nf_host_span261(t->data,n,0):palette175_readable(t->data,n)))return 0;
 *length=n;return 1;
}
#ifdef NIGHTFIRE_BATCH_TIMING244
#define material_preflight221 material_preflight221_impl244
#endif
static int material_preflight221(const NFHardwareState *s){
 const NFHardwareMaterial221 *m=s->material221;
 if(!m||!nf_pixel221_valid(&m->pixel)||s->transformed218||s->dual220||s->program||s->texture||s->selected_coordinates182||s->texture_stage182||s->cpu_vertex182||
 s->experimental_resolve193||s->experimental_movie200||s->direct28||s->direct90||s->color_only||s->color_layout||s->compat_point_clamp||s->sampler_anisotropy213||s->sampler_bias215||
 s->alpha_func<0x200||s->alpha_func>0x207||!isfinite(s->fog_bias)||!isfinite(s->fog_slope)||
 (s->cull_enable && (s->cull_face!=0x404&&s->cull_face!=0x405))||(s->front_face!=0&&s->front_face!=0x900&&s->front_face!=0x901)||
 s->left>s->right||s->top>s->bottom||s->right>s->width||s->bottom>s->height)return 0;
 for(unsigned i=0;i<4;i++)if(((m->pixel.program>>(5*i))&31)==1){size_t n;
  if(!material_texture_layout221(&m->textures[i],&n)||
     range179(m->textures[i].data,n,s->color,(size_t)s->pitch*s->height)||
     range179(m->textures[i].data,n,s->depth,(size_t)s->depth_pitch*s->height))return 0;
 }
 if(m->pixel.white_stage2_242&&!nf_material_white242(m))return 0;
 return 1;
}
#ifdef NIGHTFIRE_BATCH_TIMING244
#undef material_preflight221
static int material_preflight221(const NFHardwareState *s){uint64_t t=bt244_start(1);int ok=material_preflight221_impl244(s);bt244_end(BT244_PREFLIGHT,t);return ok;}
#endif
static ID3D11PixelShader *material_prepare221(const NFHardwareMaterial221 *m){
 ID3DBlob *v=NULL,*p=NULL,*errors=NULL;HRESULT hr=S_OK;
 if(!material_vs221){
  hr=D3DCompile(material_vertex221,sizeof material_vertex221-1,"material221-vs",NULL,NULL,"vertex","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&v,&errors);if(FAILED(hr))goto fail;
  hr=ID3D11Device_CreateVertexShader(dev,v->lpVtbl->GetBufferPointer(v),v->lpVtbl->GetBufferSize(v),NULL,&material_vs221);if(FAILED(hr))goto fail;
  D3D11_INPUT_ELEMENT_DESC e[]={
  {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
  {"TEXCOORD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},
  {"TEXCOORD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0},
  {"TEXCOORD",2,DXGI_FORMAT_R32G32B32A32_FLOAT,0,48,D3D11_INPUT_PER_VERTEX_DATA,0},
  {"TEXCOORD",3,DXGI_FORMAT_R32G32B32A32_FLOAT,0,64,D3D11_INPUT_PER_VERTEX_DATA,0},
  {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,80,D3D11_INPUT_PER_VERTEX_DATA,0},
  {"COLOR",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,96,D3D11_INPUT_PER_VERTEX_DATA,0},
  {"TEXCOORD",4,DXGI_FORMAT_R32_FLOAT,0,112,D3D11_INPUT_PER_VERTEX_DATA,0}};
  hr=ID3D11Device_CreateInputLayout(dev,e,8,v->lpVtbl->GetBufferPointer(v),v->lpVtbl->GetBufferSize(v),&material_layout221);if(FAILED(hr))goto fail;
  unsigned material_vertices246=16384;
#ifdef NIGHTFIRE_MATERIAL_RING246
  material_ring246=material_ring_enabled246();material_cursor246=0;
  material_capacity246=material_ring246?MATERIAL_RING_CAPACITY246:16384;
  material_vertices246=material_capacity246;
#endif
  D3D11_BUFFER_DESC d={0};d.ByteWidth=material_vertices246*sizeof(NFHardwareMaterialVertex221);d.Usage=D3D11_USAGE_DYNAMIC;d.BindFlags=D3D11_BIND_VERTEX_BUFFER;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
  hr=ID3D11Device_CreateBuffer(dev,&d,NULL,&material_vb221);if(FAILED(hr))goto fail;
  RELEASE(v);RELEASE(errors);
 }
 for(unsigned i=0;i<64;i++)if(material_shaders221[i].shader&&!memcmp(&m->pixel,&material_shaders221[i].key,sizeof m->pixel))return material_shaders221[i].shader;
 {char source[32768];size_t prefix=sizeof material_vertex221-1;memcpy(source,material_vertex221,prefix);
  if(!nf_pixel221_emit(&m->pixel,source+prefix,sizeof source-prefix))return NULL;
  hr=D3DCompile(source,strlen(source),"material221-ps",NULL,NULL,"pixel","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&p,&errors);if(FAILED(hr))goto fail;
 }
 {ID3D11PixelShader *shader=NULL;
  hr=ID3D11Device_CreatePixelShader(dev,p->lpVtbl->GetBufferPointer(p),p->lpVtbl->GetBufferSize(p),NULL,&shader);if(FAILED(hr))goto fail;
  NFMaterialShader221 *entry=&material_shaders221[material_next221++%64];RELEASE(entry->shader);entry->shader=shader;entry->key=m->pixel;RELEASE(p);RELEASE(errors);return shader;
 }
fail:
 fprintf(stderr,"[MATERIAL221] preparation failed %08lX %s\n",(unsigned long)hr,errors?(char*)errors->lpVtbl->GetBufferPointer(errors):"");
 if(!material_vb221){RELEASE(material_vs221);RELEASE(material_layout221);}
 RELEASE(v);RELEASE(p);RELEASE(errors);return NULL;
}
static NFMaterialShader221 material_shaders291[64];static unsigned material_next291;
static ID3D11PixelShader *material_prepare291_impl(const NFHardwareMaterial221 *m){
 if(!material_prepare221(m))return NULL;
 ID3DBlob *v=NULL,*p=NULL,*errors=NULL;HRESULT hr=S_OK;
 for(unsigned i=0;i<64;i++)if(material_shaders291[i].shader&&!memcmp(&m->pixel,&material_shaders291[i].key,sizeof m->pixel))return material_shaders291[i].shader;
 {char source[32768];size_t prefix=sizeof material_vertex221-1;memcpy(source,material_vertex221,prefix);
  const char rename291[]="\n#define pixel raw_pixel291\n";memcpy(source+prefix,rename291,sizeof rename291-1);prefix+=sizeof rename291-1;
  if(!nf_pixel221_emit(&m->pixel,source+prefix,sizeof source-prefix))return NULL;
  const char suffix[]="\n#undef pixel\nuint unpackz291(uint q){if(!q)return 0;uint k=firstbithigh(q);return((k+103)<<23)+((q<<(23-k))-0x800000)+1;}"
   "struct DepthResult291{float4 color:SV_Target;float z:SV_Depth;};DepthResult291 pixel(P p){DepthResult291 o;o.color=raw_pixel291(p);o.z=asfloat(unpackz291((uint)floor(saturate(p.p.z)*16777215.0)));return o;}";
  if(strlen(source)+sizeof suffix>sizeof source)return NULL;strcat(source,suffix);
  hr=D3DCompile(source,strlen(source),"fragment291-ps",NULL,NULL,"pixel","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_IEEE_STRICTNESS,0,&p,&errors);if(FAILED(hr))goto fail;
 }
 {ID3D11PixelShader *shader=NULL;
  hr=ID3D11Device_CreatePixelShader(dev,p->lpVtbl->GetBufferPointer(p),p->lpVtbl->GetBufferSize(p),NULL,&shader);if(FAILED(hr))goto fail;
  NFMaterialShader221 *entry=&material_shaders291[material_next291++%64];RELEASE(entry->shader);entry->shader=shader;entry->key=m->pixel;RELEASE(p);RELEASE(errors);return shader;
 }
fail:
 fprintf(stderr,"[MATERIAL221] preparation failed %08lX %s\n",(unsigned long)hr,errors?(char*)errors->lpVtbl->GetBufferPointer(errors):"");

 RELEASE(v);RELEASE(p);RELEASE(errors);return NULL;
}
/* Shader compilation is host setup, not guest arithmetic. D3DCompile
 * clears MXCSR sticky flags on this driver/toolchain. Preserve the complete
 * caller environment across this extra preparation on success AND refusal. */
static ID3D11PixelShader *material_prepare291(const NFHardwareMaterial221 *m){
 unsigned csr=_mm_getcsr();ID3D11PixelShader *p=material_prepare291_impl(m);_mm_setcsr(csr);return p;
}
static ID3D11SamplerState *material_sampler221(const NFHardwareMaterialTexture221 *t){
 unsigned a=t->anisotropy,um=t->address&15,vm=t->address>>8&15;
 unsigned key=(a==1?0:a==2?1:a==4?2:3)+4*t->bias215+8*(um==3)+16*(vm==3);
 if(!material_samplers221[key]){
  D3D11_SAMPLER_DESC d={0};d.Filter=a==1?D3D11_FILTER_MIN_MAG_MIP_LINEAR:D3D11_FILTER_ANISOTROPIC;
  d.AddressU=um==1?D3D11_TEXTURE_ADDRESS_WRAP:D3D11_TEXTURE_ADDRESS_CLAMP;d.AddressV=vm==1?D3D11_TEXTURE_ADDRESS_WRAP:D3D11_TEXTURE_ADDRESS_CLAMP;d.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
  d.MaxAnisotropy=a;d.MaxLOD=15;d.MipLODBias=t->bias215?-255.f/256:0;d.ComparisonFunc=D3D11_COMPARISON_ALWAYS;
  if(FAILED(ID3D11Device_CreateSamplerState(dev,&d,&material_samplers221[key])))return NULL;
 }
 return material_samplers221[key];
}
#endif

#include "nightfire_validation267.h"
#include "nightfire_color_seed276.h"
#include "nightfire_depth_seed278.h"
#include "nightfire_begin_probe334.h"
#include "nightfire_import_probe335.h"
#include "nightfire_depth_unpack335.h"
static int begin_impl(const NFHardwareState *s,const NFValidation267 *proof267){
    if(!s||!fragment291_begin(s)||!native302_begin(s))return 0;
#ifdef NIGHTFIRE_MATERIAL221
    if(s->material221){
        int reuse267=validation267_begin(proof267,s);
        if(proof267){if(reuse267)validation267.begin_reused++;else validation267.begin_fallback++;}
        if(!reuse267 && !material_preflight221(s))return 0;
    }
    ID3D11PixelShader *material_pixel221=NULL;
    ID3D11ShaderResourceView *material_views221[4]={0};
    ID3D11SamplerState *material_ss221[4]={0};
#else
    if(s->material221)return 0;
#endif
    unsigned sampler_group213=0;
    if(s->dual220){
        if(s->dual220!=1 || s->transformed218 || s->program || s->selected_coordinates182 ||
           s->texture_stage182 || s->cpu_vertex182 || s->experimental_resolve193 || s->experimental_movie200 ||
           s->direct28 || s->direct90 || s->color_only || s->color_layout || s->compat_point_clamp ||
           s->sampler_anisotropy213 || s->sampler_bias215 || s->combiner!=1 || s->scale!=2 ||
           s->alpha_enable || s->blend_enable || s->reverse_subtract97 || s->cull_enable ||
           !s->texture || s->texture_format!=0x08860e29u || s->texture_available<87360 ||
           !s->filtered || s->texture_filter!=0x02063f01u || s->texture_address!=0x10101u ||
           !s->texture1_220 || s->texture1_available220<65536 || s->texture1_format220!=0x07710629u ||
           s->texture1_filter220!=0x02063f01u || s->texture1_address220!=0x10101u)return 0;
    }
    if(s->transformed218){
        unsigned f=(s->texture_format>>8)&255;
        if(s->transformed218!=1 || !s->texture || (f!=0xc && f!=0xe) ||
           s->program || s->selected_coordinates182 || s->texture_stage182 || s->cpu_vertex182 ||
           s->experimental_resolve193 || s->experimental_movie200 || s->direct28 || s->direct90 ||
           s->color_only || s->color_layout || s->compat_point_clamp || s->combiner>1 ||
           (s->scale!=1 && s->scale!=2 && s->scale!=4))return 0;
    }
    if(s->sampler_bias215>1 || (s->sampler_bias215 && !s->sampler_anisotropy213))return 0;
    if(s->sampler_anisotropy213){
        unsigned a=s->sampler_anisotropy213;
        if(a!=1 && a!=2 && a!=4 && a!=8)return 0;
        if(!s->texture || !s->filtered || (!s->transformed218 &&
           (((s->texture_format>>8)&255)!=6 || ((s->texture_format>>16)&15)!=1)) ||
           s->texture_filter!=(s->sampler_bias215?0x02063f01u:0x02062000u))return 0;
        sampler_group213=a==1?1:a==2?2:a==4?3:4;
        if(s->sampler_bias215)sampler_group213+=4;
        if(s->transformed218)sampler_group213+=8; /* Distinct mip-capable BC cache. */
    }
    if(s->color_write_mask212 && (s->color_write_mask212<0x10 || s->color_write_mask212>0x1f))return 0;
    unsigned write_mask212=s->color_write_mask212?(s->color_write_mask212&15):15;
    if(s->experimental_movie200 && !movie200_admitted(s))return 0;
    if(s->experimental_resolve193 && !resolve193_admitted(s))return 0;
    if(s->selected_coordinates182>1 || (s->selected_coordinates182 && (s->texture_stage182!=0 && s->texture_stage182!=3)) || (s->cpu_vertex182 && !s->selected_coordinates182))return 0;
    if(s->selected_coordinates182){unsigned f=(s->texture_format>>8)&255;if(!s->program || !s->texture || (f!=0x0b && f!=0x11 && f!=0x12) || ((f==0x11 || f==0x12) && (!s->texture_width || !s->texture_height)))return 0;}
    unsigned color_size131=nf_swizzled131_size(s->color_layout);
    if(s->color_layout && (!nf_swizzled131_enabled() || !color_size131 ||
       (s->color_layout==NF_COLOR_SWIZZLED_128_132 && !nf_swizzled132_enabled()) ||
       !s->color_only || s->depth || s->depth_pitch || s->depth_enable || s->width!=color_size131 || s->height!=color_size131 ||
       s->pitch!=color_size131*4 || s->left>s->right || s->top>s->bottom || s->right>color_size131 || s->bottom>color_size131))return 0;
    if(!s->color || !s->width || !s->height || s->width>2048 || s->height>2048 || s->pitch<s->width*4 || s->depth_func<0x200 || s->depth_func>0x207)return 0;
    if(s->color_only){
        if((!nf_hw_fallback96_enabled() && !s->experimental_resolve193) || s->depth_enable || s->depth || s->depth_pitch)return 0;
    }else if(!s->depth || s->depth_pitch<s->width*4)return 0;
    unsigned blend=0;
    if(s->blend_enable){if(s->blend_src==0x302 && s->blend_dst==0x303)blend=1;else if(s->blend_src==1 && s->blend_dst==1)blend=2;else if(s->blend_src==0x302 && s->blend_dst==1)blend=3;else if(s->material221 && s->blend_src==0x306 && s->blend_dst==0x303 && nf_blend285_enabled())blend=5;else if(s->blend_src!=1 || s->blend_dst!=0)return 0;}
    if(s->reverse_subtract97){
        if(!nf_hw_blend97_enabled() || !s->blend_enable || s->blend_src!=0x302 || s->blend_dst!=1)return 0;
        blend=4;
    }
    unsigned um=s->texture_address&15,vm=(s->texture_address>>8)&15;
    unsigned filtered=s->filtered!=0;
    if(s->compat_point_clamp){
        unsigned format=(s->texture_format>>8)&255,levels=(s->texture_format>>16)&15;
        int mip_compatible=levels==1 || nf_rgba_mips129_match(s->texture_format,s->texture_address);
        if(!nf_hw_fallback96_enabled() || !s->texture || (format!=6 && format!=7) || !mip_compatible ||
           (s->texture_format&0xfc)!=0x28 || s->texture_filter!=0x02063f01 || um!=4 || vm!=4)return 0;
        /* Match the existing RGBA CPU fallback: base-level nearest sampling
         * with clamped coordinates. Do not treat address4 as Xbox border proof. */
        um=vm=3;filtered=0;
    }
    if(s->texture && ((um!=1 && um!=3)||(vm!=1 && vm!=3)))return 0;
    nb334_mark(NB334_SHADER);
    if(!initialize())return 0;
    if(s->dual220 && !initialize_dual220())return 0;
#ifdef NIGHTFIRE_MATERIAL221
    if(s->material221){
        /* Native integer import removes the old CPU division that set inexact.
         * Preserve entry MXCSR around the unchanged shader/cache preparation. */
        unsigned csr302=native302_current?_mm_getcsr():0;
        material_pixel221=fragment291_current?material_prepare291(s->material221):material_prepare221(s->material221);
        if(native302_current)_mm_setcsr(csr302);
        if(!material_pixel221)return 0;
    }
#endif
    if(s->experimental_movie200 && (!initialize_movie200(0)||!initialize_movie200(1)))return 0;
    if(s->experimental_resolve193 && !initialize_resolve193(s->experimental_resolve193))return 0;
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
    /* Only the exact CPU-cleared disjoint shadow may run ahead of publication.
     * Main reload, another target, or depth use completes both intervals first. */
    if(deferred128.valid && !deferred128_shadow(s)
#ifdef NIGHTFIRE_RESIDENT_MAIN130
       && !resident130_main(s)
#endif
    ){
        if(!HW_SYNC109("deferred128-target-boundary"))return 0;
    }
#endif
    unsigned direct28=0,direct90=0;
#ifdef NIGHTFIRE_DIRECT_VERTEX88
    direct28=s->direct28;
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
    direct90=s->direct90;
#endif
    if((s->selected_coordinates182 || s->transformed218) && !initialize_projective182())return 0;
    ID3D11VertexShader *program=s->selected_coordinates182?(s->cpu_vertex182?NULL:program_shader_selected174(s->program,0,0,s->texture_stage182,1)):program_shader(s->program,direct28,direct90);
    program_active=program!=NULL;
    nb334_mark(NB334_TEXTURE);
    ID3D11ShaderResourceView *view=s->texture?texture_view(s):NULL;if(s->texture && !view)return 0;
    ID3D11ShaderResourceView *view1_220=NULL;
    if(s->dual220){NFHardwareState second=*s;
        ID3D11ShaderResourceView_AddRef(view);dual_begin_hold0_220=view;
        second.texture=s->texture1_220;second.texture_available=s->texture1_available220;
        second.texture_format=s->texture1_format220;second.texture_filter=s->texture1_filter220;second.texture_address=s->texture1_address220;
        view1_220=texture_view(&second);if(!view1_220)return 0;
        ID3D11ShaderResourceView_AddRef(view1_220);dual_begin_hold1_220=view1_220;
    }
#ifdef NIGHTFIRE_MATERIAL221
    if(s->material221)for(unsigned i=0;i<4;i++)if(((s->material221->pixel.program>>(5*i))&31)==1){
        const NFHardwareMaterialTexture221 *t=&s->material221->textures[i];NFHardwareState ts=*s;
        ts.texture=t->data;ts.texture_available=t->available;ts.texture_format=t->format;ts.texture_filter=t->filter;ts.texture_address=t->address;
        material_views221[i]=texture_view_identity228(&ts,t->identity228);if(!material_views221[i])return 0;
        ID3D11ShaderResourceView_AddRef(material_views221[i]);material_holds221[i]=material_views221[i];
        material_ss221[i]=material_sampler221(t);if(!material_ss221[i])return 0;
    }
#endif
    nb334_mark(NB334_IMPORT);
    if(pending && (s->color!=active.color || s->depth!=active.depth || s->color_only!=active.color_only || s->color_layout!=active.color_layout || s->width!=width || s->height!=height || s->pitch!=active.pitch || s->depth_pitch!=active.depth_pitch)){
#ifdef NIGHTFIRE_RESIDENT_MAIN130
        if(resident130_main(s)){if(!resident130_shadow_sync())return 0;}else
#endif
        if(!HW_SYNC109("target-change"))return 0;}
    if(!surfaces(s->width,s->height))return 0;
#ifdef NIGHTFIRE_RESIDENT_MAIN130
    if(resident130_main(s) && !resident130_resume(s)){
        /* Resource mismatch must never let the ordinary upload read stale RAM. */
        if(!HW_SYNC109("resident130-resume-refused"))return 0;
    }
#endif
    if(!pending){
        ni335_mark(NI335_COLOR);
#ifdef NIGHTFIRE_EARLY_SUBMIT113
        early113_reset_interval();
#endif
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
        defer128_active_dirty=0;
#endif
#ifdef NIGHTFIRE_GPU_TIMING_DIAGNOSTIC
        nf_gpu_time84_begin(dev,ctx,nightfire_gpu_timing_frame());
#endif
        uint64_t timing84=nf_gpu_time84_start();
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
        if(color109_ready(nightfire_gpu_timing_frame()))nf_color_reupload109_compare(nightfire_gpu_timing_frame(),color,color109_generation,s->width,s->height,s->color,s->pitch);
        nf_color_reupload109_invalidate(color);
#endif
#ifdef NIGHTFIRE_COLOR_REUSE110
        /* Skip only exact color re-uploads into the same unchanged GPU resource.
         * Depth conversion and every completion/readback remain unchanged. */
        if(!color110_enabled() || !nf_color_reuse110_same(color,color110_generation,s->width,s->height,s->color,s->pitch)){
            if(color110_enabled())nf_color_reuse110_invalidate(color);
#endif
#ifdef NIGHTFIRE_SWIZZLED_SHADOW131
        if(s->color_layout){
            nf_swizzled131_import_size(swizzled131_upload,s->width*4,s->color,s->width);
            ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)color,0,NULL,swizzled131_upload,s->width*4,0);swizzled131_imports++;swizzled132_imports+=s->color_layout==NF_COLOR_SWIZZLED_128_132;
        }else
#endif
        if(!color276_skip(s))ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)color,0,NULL,s->color,s->pitch,0);
#ifdef NIGHTFIRE_COLOR_REUSE110
        }
#endif
        nf_gpu_time84_cpu(NF_GPU_TIME84_COLOR_UPLOAD,timing84);timing84=nf_gpu_time84_start();
        if(!s->color_only){
        int depth_import278=0;
        ni335_mark(NI335_SCAN);
        int full_max=!native302_current&&depth_upload_clear_enabled();
        for(unsigned y=0;full_max && y<height;y++)
            full_max=nf_depth_is_max((const uint32_t*)(s->depth+(size_t)y*s->depth_pitch),width);
        nf_gpu_time84_cpu(NF_GPU_TIME84_DEPTH_SCAN,timing84);timing84=nf_gpu_time84_start();
        ni335_mark(NI335_SELECT);
        int native_import302=native302_import(s);
        if(native_import302<0)return 0;
        if(native_import302){depth_upload_copies++;}
        else if(full_max){
            /* Exactly the old FFFFFF / 16777215 upload, across the entire
             * resource regardless of draw scissor. No guest/stencil write and
             * no new deferral: normal sync still publishes every result. */
            ID3D11DeviceContext_ClearDepthStencilView(ctx,dsv,D3D11_CLEAR_DEPTH,1.0f,0);
            nf_gpu_time84_cpu(NF_GPU_TIME84_DEPTH_UPLOAD_COPY,timing84);
            depth_upload_clears++;depth278_max(s);
#ifdef NF_DEPTH_CLEAN269_AVAILABLE
            depth269_imported(depth,1);
#endif
        }else if((depth_import278=depth278_apply(s))!=0){
            if(depth_import278<0)return 0;
            nf_gpu_time84_cpu(NF_GPU_TIME84_DEPTH_UPLOAD_COPY,timing84);
            depth_upload_copies++;
#ifdef NF_DEPTH_IMPORT268_AVAILABLE
        }else if(depth268_apply(s)){
#ifdef NF_DEPTH_CLEAN269_AVAILABLE
            depth269_imported(depth,1);
#endif
            nf_gpu_time84_cpu(NF_GPU_TIME84_DEPTH_UPLOAD_COPY,timing84);
            depth_upload_copies++;
#endif
        }else{
            ni335_mark(NI335_MAP);
            D3D11_MAPPED_SUBRESOURCE m;CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)depth_transfer,0,D3D11_MAP_WRITE,0,&m));
            ni335_mark(NI335_UNPACK);
            nf_gpu_time84_cpu(NF_GPU_TIME84_DEPTH_MAP_WRITE,timing84);timing84=nf_gpu_time84_start();
#ifdef NF_DEPTH_CLEAN269_AVAILABLE
            const int import_nearest269=!depth269_current||batch_nearest236();
#endif
            int admitted335=0;
#ifdef NF_DEPTH_PING272_AVAILABLE
            admitted335=depth272_current&&depth272_current->private331;
#endif
            const int fast335=depth335_choose(width,admitted335);
            for(unsigned y=0;y<height;y++){
                float *dst=(float*)((uint8_t*)m.pData+(size_t)y*m.RowPitch);
                const uint32_t *src=(const uint32_t*)(s->depth+(size_t)y*s->depth_pitch);
                if(fast335)unpack335_row(dst,src,width);else nf_depth_unpack(dst,src,width);
            }
            ni335_mark(NI335_COPY);
            nf_gpu_time84_cpu(NF_GPU_TIME84_DEPTH_UNPACK,timing84);timing84=nf_gpu_time84_start();
#ifdef NF_DEPTH_CLEAN269_AVAILABLE
            depth269_imported(depth,import_nearest269&&(!depth269_current||batch_nearest236()));
#endif
            ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)depth_transfer,0);
            ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)depth,(ID3D11Resource*)depth_transfer);
            nf_gpu_time84_cpu(NF_GPU_TIME84_DEPTH_UPLOAD_COPY,timing84);
            depth_upload_copies++;
        }
        if(!fragment291_imported(s))return 0;
        depth_observe_begin(full_max);
        }
        nf_gpu_time84_stamp(ctx,1);
    }
    ni335_mark(NI335_TAIL);nb334_mark(NB334_STATE);
    active=*s;pending=1;
#ifdef NIGHTFIRE_MATERIAL221
    if(s->material221){material_active221=*s->material221;active.material221=&material_active221;}
#endif
#ifdef NIGHTFIRE_SWIZZLED_SHADOW131
    if(s->color_layout)swizzled131_begins++;
    swizzled132_begins+=s->color_layout==NF_COLOR_SWIZZLED_128_132;
#endif
    unsigned di=(s->depth_func-0x200)+8*(s->depth_write!=0)+16*(s->depth_enable!=0);
    if(!depth_states[di]){D3D11_DEPTH_STENCIL_DESC d={0};d.DepthEnable=s->depth_enable!=0;d.DepthWriteMask=s->depth_write?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;d.DepthFunc=(D3D11_COMPARISON_FUNC)(1+s->depth_func-0x200);CALL_OK(ID3D11Device_CreateDepthStencilState(dev,&d,&depth_states[di]));}
    unsigned blend_key212=blend*16+write_mask212;
    if(!blend_states[blend_key212]){D3D11_BLEND_DESC d={0};D3D11_RENDER_TARGET_BLEND_DESC *r=&d.RenderTarget[0];r->RenderTargetWriteMask=(UINT8)write_mask212;r->BlendEnable=blend!=0;r->BlendOp=r->BlendOpAlpha=D3D11_BLEND_OP_ADD;
        r->SrcBlend=r->SrcBlendAlpha=(blend==1 || blend==3 || blend==4)?D3D11_BLEND_SRC_ALPHA:D3D11_BLEND_ONE;
        r->DestBlend=r->DestBlendAlpha=blend==1?D3D11_BLEND_INV_SRC_ALPHA:blend==0?D3D11_BLEND_ZERO:D3D11_BLEND_ONE;
        if(blend==4)r->BlendOp=r->BlendOpAlpha=D3D11_BLEND_OP_REV_SUBTRACT;
        if(blend==5){r->SrcBlend=D3D11_BLEND_DEST_COLOR;r->SrcBlendAlpha=D3D11_BLEND_DEST_ALPHA;r->DestBlend=r->DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;}
        /* Alpha channels use the corresponding alpha factors. */
        CALL_OK(ID3D11Device_CreateBlendState(dev,&d,&blend_states[blend_key212]));}
    unsigned si=(filtered?4:0)+(um==3?1:0)+(vm==3?2:0)+sampler_group213*8;
    if(!samplers[si]){D3D11_SAMPLER_DESC d={0};d.Filter=s->sampler_anisotropy213>1?D3D11_FILTER_ANISOTROPIC:filtered?D3D11_FILTER_MIN_MAG_MIP_LINEAR:D3D11_FILTER_MIN_MAG_MIP_POINT;d.MaxAnisotropy=s->sampler_anisotropy213?s->sampler_anisotropy213:1;d.AddressU=um==3?D3D11_TEXTURE_ADDRESS_CLAMP:D3D11_TEXTURE_ADDRESS_WRAP;d.AddressV=vm==3?D3D11_TEXTURE_ADDRESS_CLAMP:D3D11_TEXTURE_ADDRESS_WRAP;d.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;d.MaxLOD=sampler_group213>=9?15:sampler_group213?0:filtered?15:0;d.ComparisonFunc=D3D11_COMPARISON_ALWAYS;
        /* Only the captured world bias is accepted by the caller for filtering. */
        d.MipLODBias=s->sampler_bias215?-255.0f/256.0f:sampler_group213?0:filtered?-255.0f/256.0f:0;CALL_OK(ID3D11Device_CreateSamplerState(dev,&d,&samplers[si]));}
    D3D11_MAPPED_SUBRESOURCE m;
    float values[16]={(float)width,(float)height,(float)s->alpha_enable,(float)(s->alpha_func-0x200),(float)(s->alpha_ref&255),(float)(s->texture?s->combiner:2),(float)s->scale,(float)s->fog_enable,
        (s->fog_color&255)/255.0f,((s->fog_color>>8)&255)/255.0f,((s->fog_color>>16)&255)/255.0f,0,s->fog_bias,s->fog_slope,0,0};
    unsigned tf=(s->texture_format>>8)&255;
    if(s->texture && (tf==0x24 || tf==0x25)){
        values[5]=(float)(3+(tf==0x25?2:0)+(s->combiner!=0));
        values[14]=(float)s->texture_width*(um==1?-1:1);values[15]=(float)s->texture_height*(vm==1?-1:1);
    }
    if(s->transformed218){values[14]=1;values[15]=1;}
    if(s->selected_coordinates182){values[14]=(tf==0x11 || tf==0x12)?1.0f/s->texture_width:1;values[15]=(tf==0x11 || tf==0x12)?1.0f/s->texture_height:1;}
    if(!params_valid || memcmp(saved_params,values,sizeof values)){
        CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)cb,0,D3D11_MAP_WRITE_DISCARD,0,&m));memcpy(m.pData,values,sizeof values);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)cb,0);memcpy(saved_params,values,sizeof values);params_valid=1;constant_uploads++;
    }
    D3D11_VIEWPORT viewport={0,0,(float)width,(float)height,0,1};D3D11_RECT scissor={(LONG)s->left,(LONG)s->top,(LONG)s->right,(LONG)s->bottom};
    ID3D11RasterizerState *raster=rs;
    if(!program && s->color_only){
        if(!color_only_rs96){
            D3D11_RASTERIZER_DESC d={0};d.FillMode=D3D11_FILL_SOLID;d.CullMode=D3D11_CULL_NONE;
            d.DepthClipEnable=FALSE;d.ScissorEnable=TRUE;
            CALL_OK(ID3D11Device_CreateRasterizerState(dev,&d,&color_only_rs96));
        }
        raster=color_only_rs96;
    }
    if(program || s->selected_coordinates182 || s->transformed218 || s->material221){
        unsigned ci=s->cull_enable?(s->cull_face==0x404?1:2):0;
        unsigned ri=ci*2+(s->front_face==0x901)+6*(s->color_only!=0);
        if(!program_raster[ri]){D3D11_RASTERIZER_DESC d={0};d.FillMode=D3D11_FILL_SOLID;d.CullMode=ci==1?D3D11_CULL_FRONT:ci==2?D3D11_CULL_BACK:D3D11_CULL_NONE;d.FrontCounterClockwise=s->front_face==0x901;d.DepthClipEnable=!s->color_only;d.ScissorEnable=TRUE;CALL_OK(ID3D11Device_CreateRasterizerState(dev,&d,&program_raster[ri]));}
        raster=program_raster[ri];
        if(program){
        uint32_t constants[192*4+8];memcpy(constants,s->program->constant_words,192*16);memset(constants+192*4,0,32);
        if(constant_mask_enabled())memcpy(constants+192*4,constant_mask.known,sizeof constant_mask.known);
        else for(unsigned i=0;i<192;i++)if(s->program->constant_valid[i]==15)constants[192*4+i/32]|=1u<<(i%32);
        if(!kelvin_valid || memcmp(saved_kelvin,constants,sizeof saved_kelvin)){
            CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)kelvin_cb,0,D3D11_MAP_WRITE_DISCARD,0,&m));memcpy(m.pData,constants,sizeof saved_kelvin);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)kelvin_cb,0);memcpy(saved_kelvin,constants,sizeof saved_kelvin);kelvin_valid=1;constant_uploads++;
        }
        BIND_IF(!bound.kelvin,ID3D11DeviceContext_VSSetConstantBuffers(ctx,1,1,&kelvin_cb));bound.kelvin=1;
        }
    }
    ID3D11InputLayout *next_layout=program?input_layout:(s->selected_coordinates182 || s->transformed218)?projective_layout182:layout;
    ID3D11VertexShader *next_vertex=program?program:(s->selected_coordinates182 || s->transformed218)?projective_vs182:vs;
    ID3D11GeometryShader *next_geometry=program && program_dynamic?(s->selected_coordinates182?projective_geometry182:validated_geometry):NULL;
    ID3D11PixelShader *next_pixel=s->experimental_resolve193?resolve_ps193[s->experimental_resolve193-1]:(s->selected_coordinates182 || s->transformed218)?projective_ps182:s->texture && (tf==0x24 || tf==0x25)?video_ps:ps;
    if(s->dual220){next_layout=dual_layout220;next_vertex=dual_vs220;next_pixel=dual_ps220;next_geometry=NULL;}
#ifdef NIGHTFIRE_MATERIAL221
    if(s->material221){next_layout=material_layout221;next_vertex=material_vs221;next_pixel=material_pixel221;next_geometry=NULL;view=material_views221[0];}
#endif
    ID3D11SamplerState *sampler0_221=samplers[si];
#ifdef NIGHTFIRE_MATERIAL221
    if(s->material221)sampler0_221=material_ss221[0];
#endif
    nb334_mark(NB334_BIND);
    BIND_IF(!bound.valid || bound.raster!=raster,ID3D11DeviceContext_RSSetState(ctx,raster));
    BIND_IF(!bound.valid || memcmp(&bound.viewport,&viewport,sizeof viewport),ID3D11DeviceContext_RSSetViewports(ctx,1,&viewport));
    BIND_IF(!bound.valid || memcmp(&bound.scissor,&scissor,sizeof scissor),ID3D11DeviceContext_RSSetScissorRects(ctx,1,&scissor));
    BIND_IF(!bound.targets,ID3D11DeviceContext_OMSetRenderTargets(ctx,1,&rtv,s->color_only?NULL:dsv));bound.targets=1;
    BIND_IF(!bound.valid || bound.depth!=depth_states[di],ID3D11DeviceContext_OMSetDepthStencilState(ctx,depth_states[di],0));
    BIND_IF(!bound.valid || bound.blend!=blend_states[blend_key212],ID3D11DeviceContext_OMSetBlendState(ctx,blend_states[blend_key212],NULL,~0u));
    BIND_IF(!bound.valid || bound.layout!=next_layout,ID3D11DeviceContext_IASetInputLayout(ctx,next_layout));
    BIND_IF(!bound.valid || bound.vertex!=next_vertex,ID3D11DeviceContext_VSSetShader(ctx,next_vertex,NULL,0));
    BIND_IF(!bound.valid || bound.geometry!=next_geometry,ID3D11DeviceContext_GSSetShader(ctx,next_geometry,NULL,0));
    BIND_IF(!bound.valid || bound.texture!=view,ID3D11DeviceContext_PSSetShaderResources(ctx,0,1,&view));
    BIND_IF(!bound.valid || bound.sampler!=sampler0_221,ID3D11DeviceContext_PSSetSamplers(ctx,0,1,&sampler0_221));
    /* Slot1 is deliberately uncached: every begin binds or unbinds it. The
     * existing slot0 cache remains accurate across dual/default transitions. */
    ID3D11SamplerState *sampler1_220=s->dual220?dual_sampler1_220:NULL;
    ID3D11ShaderResourceView *tail_views221[3]={view1_220,NULL,NULL};
    ID3D11SamplerState *tail_ss221[3]={sampler1_220,NULL,NULL};
#ifdef NIGHTFIRE_MATERIAL221
    if(s->material221){memcpy(tail_views221,material_views221+1,sizeof tail_views221);memcpy(tail_ss221,material_ss221+1,sizeof tail_ss221);}
#endif
    ID3D11DeviceContext_PSSetShaderResources(ctx,1,3,tail_views221);
    ID3D11DeviceContext_PSSetSamplers(ctx,1,3,tail_ss221);
#ifndef NIGHTFIRE_VERTEX_BATCH92
    BIND_IF(!bound.fixed,ID3D11DeviceContext_IASetPrimitiveTopology(ctx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
#endif
    BIND_IF(!bound.valid || bound.pixel!=next_pixel,ID3D11DeviceContext_PSSetShader(ctx,next_pixel,NULL,0));
    BIND_IF(!bound.fixed,ID3D11DeviceContext_VSSetConstantBuffers(ctx,0,1,&cb));
    BIND_IF(!bound.fixed,ID3D11DeviceContext_PSSetConstantBuffers(ctx,0,1,&cb));bound.fixed=1;
    bound.raster=raster;bound.viewport=viewport;bound.scissor=scissor;bound.depth=depth_states[di];bound.blend=blend_states[blend_key212];
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
    if(direct90_selected){unsigned zero=0;BIND_IF(!bound.default90,ID3D11DeviceContext_IASetVertexBuffers(ctx,1,1,&default90_vb,&zero,&zero));bound.default90=1;}
#endif
    bound.layout=next_layout;bound.vertex=next_vertex;bound.geometry=next_geometry;bound.pixel=next_pixel;bound.texture=view;bound.sampler=sampler0_221;bound.valid=1;
    color_only96_begins+=s->color_only!=0;point_clamp96_begins+=s->compat_point_clamp!=0;
#ifdef NIGHTFIRE_RGBA_MIPS129
    rgba129_begins+=s->compat_point_clamp && nf_rgba_mips129_match(s->texture_format,s->texture_address);
#endif
    reverse_subtract97_begins+=s->reverse_subtract97!=0;return 1;
}
static int nf_hw_begin_checked267(const NFHardwareState *s,const NFValidation267 *proof267){if(resident313_blocked()){return 0;}BT244_START(bt_begin244);uint64_t t=hw_clock();
int admitted334=0;
#ifdef NF_DEPTH_PING272_AVAILABLE
admitted334=s&&s->material221&&depth272_current&&depth272_current->private331;
#endif
NBScope334 scope334;NBScope334*saved334=nb334_enter(&scope334,admitted334);
NIScope335 scope335;NIScope335*saved335=ni335_enter(&scope335,admitted334&&!pending);
int ok=begin_impl(s,proof267);ni335_mark(NI335_TAIL);nb334_mark(NB334_CLEANUP);
RELEASE(dual_begin_hold0_220);RELEASE(dual_begin_hold1_220);
#ifdef NIGHTFIRE_MATERIAL221
for(unsigned i=0;i<4;i++){RELEASE(material_holds221[i]);}
#endif
if(!ok){nf_gpu_time84_abandon(ctx);
#ifdef NIGHTFIRE_DIRECT_VERTEX88
direct28_selected=0;
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
direct90_selected=0;
#endif
}ni335_leave(&scope335,saved335,ok);nb334_leave(&scope334,saved334,ok);begin_ticks+=hw_clock()-t;BT244_END(bt_begin244,BT244_BEGIN);return ok;}
int nf_hw_begin(const NFHardwareState *s){if(resident313_blocked())return 0;return nf_hw_begin_checked267(s,NULL);}
static int draw_impl(const NFHardwareVertex *vertices,unsigned count){
    if(!pending || active.selected_coordinates182 || active.transformed218 || active.dual220 || active.material221 || count>16384)return 0;if(!count)return 1;
#ifdef NIGHTFIRE_VERTEX_BATCH92
    topology92(NF_HW_TRIANGLE_LIST);
#endif
    if(vertex_cursor+count>16384)vertex_cursor=0;
    D3D11_MAPPED_SUBRESOURCE m;CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)vb,0,vertex_cursor?D3D11_MAP_WRITE_NO_OVERWRITE:D3D11_MAP_WRITE_DISCARD,0,&m));memcpy((NFHardwareVertex*)m.pData+vertex_cursor,vertices,count*sizeof *vertices);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)vb,0);
    unsigned stride=sizeof *vertices,offset=0;ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&vb,&stride,&offset);ID3D11DeviceContext_Draw(ctx,count,vertex_cursor);
    if(!active.color_only)depth_observe_draw(active.depth_enable,active.depth_write,count);
    fallback96_drawn();vertex_cursor+=count;draws++;triangles+=count/3;return 1;
}
int nf_hw_draw(const NFHardwareVertex *v,unsigned n){if(resident313_blocked())return 0;uint64_t t=hw_clock();int ok=draw_impl(v,n);draw_ticks+=hw_clock()-t;return ok;}
static int draw_transformed218(const NFHardwareTransformed218 *v,unsigned n){
    if(!pending || !active.transformed218 || active.dual220 || active.material221 || program_active || n>16384 || n%3 || (n && !v))return 0;
    if(!n)return 1;
    /* Validate the complete submission before upload. Original negative W is
     * retained for homogeneous clipping. Projective Q must not cross a pole. */
    for(unsigned i=0;i<n;i++){
        for(unsigned k=0;k<4;k++)if(!isfinite(v[i].position[k]) ||
            !isfinite(v[i].uv[k]) || !isfinite(v[i].color[k]))return 0;
        if(!isfinite(v[i].fog) || v[i].position[3]==0 || v[i].uv[3]==0)return 0;
        if((v[i].uv[3]<0)!=(v[i-i%3].uv[3]<0))return 0;
        float clip_x=(v[i].position[0]*2.0f/(float)width-1.0f)*v[i].position[3];
        float clip_y=(1.0f-v[i].position[1]*2.0f/(float)height)*v[i].position[3];
        float clip_z=(v[i].position[2]/16777215.0f)*v[i].position[3];
        if(!isfinite(clip_x) || !isfinite(clip_y) || !isfinite(clip_z))return 0;
    }
#ifdef NIGHTFIRE_VERTEX_BATCH92
    topology92(NF_HW_TRIANGLE_LIST);
#endif
    D3D11_MAPPED_SUBRESOURCE m;
    CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)projective_vb182,0,D3D11_MAP_WRITE_DISCARD,0,&m));
    memcpy(m.pData,v,n*sizeof *v);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)projective_vb182,0);
    unsigned stride=sizeof *v,offset=0;
    ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&projective_vb182,&stride,&offset);
    ID3D11DeviceContext_Draw(ctx,n,0);
    depth_observe_draw(active.depth_enable,active.depth_write,n);
    fallback96_drawn();draws++;triangles+=n/3;return 1;
}
int nf_hw_draw_transformed218(const NFHardwareTransformed218 *v,unsigned n){if(resident313_blocked())return 0;
    uint64_t t=hw_clock();int ok=draw_transformed218(v,n);draw_ticks+=hw_clock()-t;return ok;
}
int nf_hw_draw_dual220(const NFHardwareDual220 *v,unsigned n){if(resident313_blocked())return 0;
    if(!pending || !active.dual220 || active.material221 || program_active || n>16384 || n%3 || (n&&!v))return 0;if(!n)return 1;
    for(unsigned i=0;i<n;i++){
        for(unsigned k=0;k<4;k++)if(!isfinite(v[i].position[k])||!isfinite(v[i].uv0[k])||
            !isfinite(v[i].uv1[k])||!isfinite(v[i].color[k])||v[i].color[k]<0||v[i].color[k]>1)return 0;
        if(!isfinite(v[i].fog)||v[i].position[3]==0||v[i].uv0[3]==0||v[i].uv1[3]==0)return 0;
        if((v[i].uv0[3]<0)!=(v[i-i%3].uv0[3]<0)||(v[i].uv1[3]<0)!=(v[i-i%3].uv1[3]<0))return 0;
        float x=(v[i].position[0]*2/(float)width-1)*v[i].position[3];
        float y=(1-v[i].position[1]*2/(float)height)*v[i].position[3];
        float z=(v[i].position[2]/16777215.0f)*v[i].position[3];
        if(!isfinite(x)||!isfinite(y)||!isfinite(z))return 0;
    }
#ifdef NIGHTFIRE_VERTEX_BATCH92
    topology92(NF_HW_TRIANGLE_LIST);
#endif
    D3D11_MAPPED_SUBRESOURCE m;
    CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)dual_vb220,0,D3D11_MAP_WRITE_DISCARD,0,&m));
    memcpy(m.pData,v,n*sizeof*v);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)dual_vb220,0);
    unsigned stride=sizeof*v,offset=0;ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&dual_vb220,&stride,&offset);
    ID3D11DeviceContext_Draw(ctx,n,0);depth_observe_draw(active.depth_enable,active.depth_write,n);
    fallback96_drawn();draws++;triangles+=n/3;return 1;
}
static int tx339_draw(unsigned,unsigned);
static int draw_material221(const NFHardwareMaterialVertex221 *v,unsigned n,const NFValidation267 *proof267){
#ifndef NIGHTFIRE_MATERIAL221
    (void)v;(void)n;return 0;
#else
    if(!fragment291_draw(v,n)||!native302_draw(v,n))return 0;
    if(!pending || !active.material221 || program_active || n>16384 || n%3 || (n&&!v))return 0;if(!n)return 1;
    BT244_START(bt_validation244);
    int reuse267=validation267_draw(proof267,v,n);
    if(proof267){if(reuse267){validation267.draw_reused++;validation267.vertices_reused+=n;}else validation267.draw_fallback++;}
    /* Validate the entire dense triangle list before any upload/submission.
     * Original W, including its sign, remains available to homogeneous clipping. */
    for(unsigned i=0;i<n;i++){
        if(!reuse267){
        const float *all=(const float*)&v[i];for(unsigned k=0;k<29;k++)if(!isfinite(all[k]))return 0;
        if(v[i].position[3]==0)return 0;
        for(unsigned k=0;k<4;k++)if(v[i].color[k]<0||v[i].color[k]>1||v[i].specular[k]<0||v[i].specular[k]>1)return 0;
        for(unsigned stage=0;stage<4;stage++){
            unsigned mode=active.material221->pixel.program>>(5*stage)&31;const float *uv=v[i].uv[stage];
            if(active.material221->pixel.white_stage2_242&&stage==2){if(!nf_pixel_zero_uv242(&active.material221->pixel,stage,uv))return 0;}
            else if(mode==1 && (uv[3]==0 || (uv[3]<0)!=(v[i-i%3].uv[stage][3]<0) || !isfinite(uv[0]/uv[3]) || !isfinite(uv[1]/uv[3])))return 0;
            if(mode==4)for(unsigned k=0;k<4;k++)if(uv[k]<0||uv[k]>1)return 0;
        }
        } /*267: keep original draw-time clip arithmetic even with input proof. */
        float x=(v[i].position[0]*2/(float)width-1)*v[i].position[3];
        float y=(1-v[i].position[1]*2/(float)height)*v[i].position[3];
        float z=(v[i].position[2]/16777215.f)*v[i].position[3];
        if(!isfinite(x)||!isfinite(y)||!isfinite(z))return 0;
    }
    BT244_END(bt_validation244,BT244_DRAW_VALIDATION);
#ifdef NIGHTFIRE_VERTEX_BATCH92
    topology92(NF_HW_TRIANGLE_LIST);
#else
    ID3D11DeviceContext_IASetPrimitiveTopology(ctx,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
#endif
    D3D11_MAPPED_SUBRESOURCE m;
    unsigned material_start246=0;D3D11_MAP material_map246=D3D11_MAP_WRITE_DISCARD;
#ifdef NIGHTFIRE_MATERIAL_RING246
    if(material_ring246){
        if(material_capacity246!=MATERIAL_RING_CAPACITY246 || material_cursor246>material_capacity246 || n>material_capacity246)return 0;
        material_start246=material_cursor246>material_capacity246-n?0:material_cursor246;
        if(material_start246)material_map246=D3D11_MAP_WRITE_NO_OVERWRITE;
    }
#endif
    BT244_START(bt_vmap244);
    CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)material_vb221,0,material_map246,0,&m));
    BT244_END(bt_vmap244,BT244_VERTEX_MAP);BT244_START(bt_vcopy244);
    memcpy((NFHardwareMaterialVertex221*)m.pData+material_start246,v,n*sizeof*v);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)material_vb221,0);
    unsigned stride=sizeof*v,offset=0;ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&material_vb221,&stride,&offset);
    gt254_draw(ctx,0);gt297_op(ctx,GT297_MATERIAL,0,0);int handled339=tx339_draw(n,material_start246);if(handled339<0)return 0;if(!handled339)ID3D11DeviceContext_Draw(ctx,n,material_start246);gt297_op(ctx,GT297_MATERIAL,1,0);gt254_draw(ctx,1);depth_observe_draw(active.depth_enable,active.depth_write,n);
#ifdef NIGHTFIRE_MATERIAL_RING246
    if(material_ring246){
        if(material_start246)material_appends246++;else{material_discards246++;material_wraps246+=material_cursor246!=0;}
        material_cursor246=material_start246+n;
        uint64_t ring_draws246=material_discards246+material_appends246;
        if(ring_draws246==1 || !(ring_draws246%8192))fprintf(stderr,"[MATERIAL-RING246] draws=%llu capacity=%u cursor=%u discards=%llu appends=%llu wraps=%llu\n",(unsigned long long)ring_draws246,material_capacity246,material_cursor246,(unsigned long long)material_discards246,(unsigned long long)material_appends246,(unsigned long long)material_wraps246);
    }
#endif
    if(fragment291_current){fragment291_current->drawn=1;fragment291_counts.draws++;}
    if(native302_current){native302_current->drawn=1;native302_counts.draws++;}
    fallback96_drawn();draws++;triangles+=n/3;BT244_END(bt_vcopy244,BT244_VERTEX_COPY_SUBMIT);return 1;
#endif
}
static int nf_hw_draw_material_checked267(const NFHardwareMaterialVertex221 *v,unsigned n,const NFValidation267 *proof267){if(resident313_blocked()){return 0;}
 BT244_START(bt_draw244);uint64_t t=hw_clock();int ok=draw_material221(v,n,proof267);draw_ticks+=hw_clock()-t;BT244_END(bt_draw244,BT244_DRAW);return ok;
}
int nf_hw_draw_material221(const NFHardwareMaterialVertex221 *v,unsigned n){if(resident313_blocked())return 0;return nf_hw_draw_material_checked267(v,n,NULL);}
#include "nightfire_pair235.h"
#include "nightfire_shared339.h"
#include "nightfire_pairbatch248.h"
#include "nightfire_color_api276.h"
#include "nightfire_depth_api278.h"
#include "nightfire_resident315.h"
int nf_hw_program_active(void){return program_active;}
unsigned nf_hw_program_inputs(void){return program_active?program_inputs:0;}
int nf_hw_direct28_active(void){
#ifdef NIGHTFIRE_DIRECT_VERTEX88
 return program_active && direct28_selected;
#else
 return 0;
#endif
}
static int draw_input(const NFHardwareInputVertex *v,const float *packed,unsigned count,const uint16_t *indices,unsigned index_count,unsigned topology){
 if(active.experimental_resolve193 && (packed||indices||index_count||topology!=NF_HW_TRIANGLE_LIST||!resolve193_vertices(v,count)))return 0;
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
 if(direct90_selected)return 0;
#endif
#ifdef NIGHTFIRE_DIRECT_VERTEX88
 if(direct28_selected)return 0; /* Dense bytes cannot be consumed by the raw layout. */
#endif
 if(!pending || !program_active || count>16384 || index_count>16384)return 0;if(!count)return !index_count;
 if(indices)for(unsigned i=0;i<index_count;i++)if(indices[i]>=count)return 0;
#ifdef NIGHTFIRE_VERTEX_BATCH92
 if(topology>NF_HW_TRIANGLE_STRIP || (topology==NF_HW_TRIANGLE_STRIP && (!indices || index_count<3)))return 0;
 topology92(topology);
#else
 (void)topology;
#endif
 uint64_t started=hw_clock();
 unsigned used[16],attributes=0;for(unsigned a=0;a<16;a++)if(program_inputs&(1u<<a))used[attributes++]=a;
 unsigned stride=(attributes?attributes:1)*16,bytes=count*stride;
 if(input_cursor+bytes>16384*sizeof(NFHardwareInputVertex))input_cursor=0;
 D3D11_MAPPED_SUBRESOURCE m;CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)input_vb,0,input_cursor?D3D11_MAP_WRITE_NO_OVERWRITE:D3D11_MAP_WRITE_DISCARD,0,&m));
 unsigned char *output=(unsigned char*)m.pData+input_cursor;
 if(packed)memcpy(output,packed,bytes);
 else for(unsigned i=0;i<count;i++)for(unsigned a=0;a<attributes;a++)memcpy(output+(size_t)i*stride+a*16,v[i].attributes[used[a]],16);
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)input_vb,0);
 unsigned offset=input_cursor;ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&input_vb,&stride,&offset);
 if(indices){
  if(index_cursor+index_count>16384)index_cursor=0;
  CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)input_ib,0,index_cursor?D3D11_MAP_WRITE_NO_OVERWRITE:D3D11_MAP_WRITE_DISCARD,0,&m));memcpy((uint16_t*)m.pData+index_cursor,indices,index_count*2);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)input_ib,0);
  ID3D11DeviceContext_IASetIndexBuffer(ctx,input_ib,DXGI_FORMAT_R16_UINT,index_cursor*2);ID3D11DeviceContext_DrawIndexed(ctx,index_count,0,0);index_cursor+=index_count;
 }else ID3D11DeviceContext_Draw(ctx,count,0);
 unsigned primitive_count=(indices?index_count:count)/3;
#ifdef NIGHTFIRE_VERTEX_BATCH92
 if(topology==NF_HW_TRIANGLE_STRIP)primitive_count=index_count-2;
 if(!active.color_only)depth_observe_draw(active.depth_enable,active.depth_write,topology==NF_HW_TRIANGLE_STRIP?primitive_count*3:indices?index_count:count);
#else
 if(!active.color_only)depth_observe_draw(active.depth_enable,active.depth_write,indices?index_count:count);
#endif
 input_cursor+=bytes;input_bytes+=bytes;
 fallback96_drawn();draws++;triangles+=primitive_count;input_draws++;input_vertices+=count;draw_ticks+=hw_clock()-started;return 1;
}
int nf_hw_selected_cpu182(void){return pending && active.selected_coordinates182 && !program_active;}
static int submit_movie200(const ProjectiveVertex182 *original,unsigned n){
 /* Both lane copies are prepared before any GPU command; all original VP/UVRQ
  * outputs remain available. Experimental sample locations, not an NV2A oracle. */
 ProjectiveVertex182 *both=malloc(2*n*sizeof *both);if(!both)return 0;
 unsigned center=active.experimental_movie200-1;
 for(unsigned lane=0;lane<2;lane++)for(unsigned i=0;i<n;i++){
  ProjectiveVertex182 *v=&both[lane*n+i];*v=original[i];float sample=lane==center?.5f:0;
  v->position[0]=2*(v->position[0]-sample)+lane+.5f;
  v->position[1]=v->position[1]-sample+.5f;
 }
 D3D11_MAPPED_SUBRESOURCE m;
 HRESULT hr=ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)projective_vb182,0,D3D11_MAP_WRITE_DISCARD,0,&m);
 if(FAILED(hr)){free(both);return 0;}memcpy(m.pData,both,2*n*sizeof *both);free(both);
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)projective_vb182,0);
 unsigned stride=sizeof(ProjectiveVertex182),offset=0;
 ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&projective_vb182,&stride,&offset);
 for(unsigned lane=0;lane<2;lane++){
  ID3D11DeviceContext_PSSetShader(ctx,movie_ps200[lane],NULL,0);
  ID3D11DeviceContext_Draw(ctx,n,lane*n);
 }
 /* Keep binding cache honest: next begin may reuse the ordinary selected PS. */
 bound.pixel=movie_ps200[1];
 depth_observe_draw(active.depth_enable,active.depth_write,n*2);fallback96_drawn();draws+=2;triangles+=2*n/3;return 1;
}
static int draw_cpu182(const NFHardwareInputVertex *v,unsigned n){
 if(active.experimental_movie200 && n!=6)return 0;
 if(!nf_hw_selected_cpu182() || !active.program || n>16384 || n%3 || (n && !v))return 0;if(!n)return 1;
 ProjectiveVertex182 *out=malloc(n*sizeof *out);NFVertexProgram *p=malloc(sizeof *p);if(!out || !p){free(out);free(p);return 0;}
 /* Local decoded state: never mutate the caller's original program snapshot. */
 memcpy(p,active.program,sizeof *p);unsigned count=0;
 for(unsigned i=0;i<n;i+=3){ProjectiveVertex182 tri[3];int valid=1;
  for(unsigned j=0;j<3;j++){float o[16][4];if(!nf_vp_run(p,v[i+j].attributes,o)){free(out);free(p);return 0;}
   if(active.experimental_movie200){
    for(unsigned k=0;k<4;k++)if(!isfinite(o[0][k])||!isfinite(o[9][k])||!isfinite(o[3][k])){free(out);free(p);return 0;}
    if(o[0][3]<=0||o[9][3]<=0){free(out);free(p);return 0;}
   }
   memcpy(tri[j].position,o[0],16);memcpy(tri[j].uv,o[9+active.texture_stage182],16);
   for(unsigned k=0;k<4;k++)tri[j].color[k]=floorf(fminf(1,fmaxf(0,o[3][k]))*255)/255;
   tri[j].fog=active.fog_enable?nf_fog_exp(o[5][0],active.fog_bias,active.fog_slope):1;
  }
  /* No submission occurs until every vertex has been evaluated successfully. */
  if(valid){memcpy(out+count,tri,sizeof tri);count+=3;}
 }
 free(p);if(!count){free(out);return 1;}
 if(active.experimental_movie200){int ok=submit_movie200(out,count);free(out);return ok;}
#ifdef NIGHTFIRE_VERTEX_BATCH92
 topology92(NF_HW_TRIANGLE_LIST);
#endif
 D3D11_MAPPED_SUBRESOURCE m;HRESULT hr=ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)projective_vb182,0,D3D11_MAP_WRITE_DISCARD,0,&m);
 if(FAILED(hr)){free(out);return 0;}memcpy(m.pData,out,count*sizeof *out);free(out);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)projective_vb182,0);
 unsigned stride=sizeof(ProjectiveVertex182),offset=0;ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&projective_vb182,&stride,&offset);ID3D11DeviceContext_Draw(ctx,count,0);
 if(!active.color_only)depth_observe_draw(active.depth_enable,active.depth_write,count);fallback96_drawn();draws++;triangles+=count/3;return 1;
}

/* Isolated fixture seam: tests the actual resolve PS with explicit post-VP
 * color. It does not change original VP execution and is absent in production. */
#ifdef NF_RESOLVE193_TEST
int nf_hw_resolve193_test_processed(const NFHardwareInputVertex *v,unsigned n){
 if(!nf_hw_selected_cpu182() || !active.experimental_resolve193 || !resolve193_vertices(v,n))return 0;
 ProjectiveVertex182 out[3];memset(out,0,sizeof out);
 for(unsigned i=0;i<3;i++){
  memcpy(out[i].position,v[i].attributes[0],16);
  memcpy(out[i].uv,v[i].attributes[9],16);
  memcpy(out[i].color,v[i].attributes[3],16);out[i].fog=1;
 }
 D3D11_MAPPED_SUBRESOURCE m;
 CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)projective_vb182,0,D3D11_MAP_WRITE_DISCARD,0,&m));
 memcpy(m.pData,out,sizeof out);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)projective_vb182,0);
 unsigned stride=sizeof(ProjectiveVertex182),offset=0;
 ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&projective_vb182,&stride,&offset);
 ID3D11DeviceContext_Draw(ctx,3,0);fallback96_drawn();draws++;triangles++;return 1;
}
#endif

int nf_hw_draw_input(const NFHardwareInputVertex *v,unsigned n){if(resident313_blocked())return 0;if(active.experimental_resolve193 && !resolve193_vertices(v,n))return 0;if(pending && active.selected_coordinates182 && !program_active)return draw_cpu182(v,n);return draw_input(v,NULL,n,NULL,0,NF_HW_TRIANGLE_LIST);}
int nf_hw_draw_indexed_input(const NFHardwareInputVertex *v,unsigned n,const uint16_t *i,unsigned count){if(resident313_blocked())return 0;if(!i || !count)return 0;return draw_input(v,NULL,n,i,count,NF_HW_TRIANGLE_LIST);}
int nf_hw_draw_packed_input(const float *v,unsigned n,unsigned inputs,const uint16_t *i,unsigned count){if(resident313_blocked())return 0;if(!v || !i || !count || inputs!=program_inputs)return 0;return draw_input(NULL,v,n,i,count,NF_HW_TRIANGLE_LIST);}
int nf_hw_draw_packed_topology(const float *v,unsigned n,unsigned inputs,const uint16_t *i,unsigned count,unsigned topology){if(resident313_blocked())return 0;
#ifdef NIGHTFIRE_VERTEX_BATCH92
 if(!v || !i || !count || inputs!=program_inputs)return 0;
 return draw_input(NULL,v,n,i,count,topology);
#else
 (void)v;(void)n;(void)inputs;(void)i;(void)count;(void)topology;return 0;
#endif
}
int nf_hw_draw_direct28(const void *source,unsigned vertices,const uint16_t *indices,unsigned index_count){if(resident313_blocked())return 0;
#ifdef NIGHTFIRE_VERTEX_BATCH92
 if(!nf_hw_direct28_active())return 0;
 return nf_hw_draw_raw_indexed(source,vertices,28,indices,index_count,NF_HW_TRIANGLE_LIST);
#elif defined(NIGHTFIRE_DIRECT_VERTEX88)
 /* D3D11 reserves R16 index0xffff as a cut even for triangle lists. Xbox
  * source index65535 may still arrive safely after the executor rebases it. */
 if(!pending || !nf_hw_direct28_active() || !source || !indices || !vertices || vertices>65535 || !index_count || index_count>16384)return 0;
 unsigned bytes=vertices*28,capacity=16384*sizeof(NFHardwareInputVertex);
 if(bytes>capacity)return 0;
 for(unsigned i=0;i<index_count;i++)if(indices[i]>=vertices)return 0;
 uint64_t started=hw_clock();
 if(input_cursor+bytes>capacity)input_cursor=0;
 D3D11_MAPPED_SUBRESOURCE m;
 CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)input_vb,0,input_cursor?D3D11_MAP_WRITE_NO_OVERWRITE:D3D11_MAP_WRITE_DISCARD,0,&m));
 memcpy((unsigned char*)m.pData+input_cursor,source,bytes);
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)input_vb,0);
 unsigned stride=28,offset=input_cursor;
 ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&input_vb,&stride,&offset);
 if(index_cursor+index_count>16384)index_cursor=0;
 CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)input_ib,0,index_cursor?D3D11_MAP_WRITE_NO_OVERWRITE:D3D11_MAP_WRITE_DISCARD,0,&m));
 memcpy((uint16_t*)m.pData+index_cursor,indices,index_count*2);
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)input_ib,0);
 ID3D11DeviceContext_IASetIndexBuffer(ctx,input_ib,DXGI_FORMAT_R16_UINT,index_cursor*2);
 ID3D11DeviceContext_DrawIndexed(ctx,index_count,0,0);index_cursor+=index_count;
 if(!active.color_only)depth_observe_draw(active.depth_enable,active.depth_write,index_count);
 input_cursor+=bytes;input_bytes+=bytes;direct28_draws++;direct28_bytes+=bytes;
 fallback96_drawn();draws++;triangles+=index_count/3;input_draws++;input_vertices+=vertices;draw_ticks+=hw_clock()-started;return 1;
#else
 (void)source;(void)vertices;(void)indices;(void)index_count;return 0;
#endif
}
unsigned nf_hw_direct90_stride(void){
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
 return program_active?direct90_selected:0;
#else
 return 0;
#endif
}
int nf_hw_draw_direct90(const void *source,unsigned vertices,const uint16_t *indices,unsigned index_count){if(resident313_blocked())return 0;
#ifdef NIGHTFIRE_VERTEX_BATCH92
 unsigned stride=nf_hw_direct90_stride();if(!stride)return 0;
 return nf_hw_draw_raw_indexed(source,vertices,stride,indices,index_count,NF_HW_TRIANGLE_LIST);
#elif defined(NIGHTFIRE_DIRECT_DEFAULT90)
 /* D3D11 reserves R16 index0xffff as a cut even for triangle lists. Xbox
  * source index65535 may still arrive safely after the executor rebases it. */
 if(!pending || !nf_hw_direct90_stride() || !source || !indices || !vertices || vertices>65535 || !index_count || index_count>16384)return 0;
 unsigned bytes=vertices*direct90_selected,capacity=16384*sizeof(NFHardwareInputVertex);
 if(bytes>capacity)return 0;
 for(unsigned i=0;i<index_count;i++)if(indices[i]>=vertices)return 0;
 uint64_t started=hw_clock();
 if(input_cursor+bytes>capacity)input_cursor=0;
 D3D11_MAPPED_SUBRESOURCE m;
 CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)input_vb,0,input_cursor?D3D11_MAP_WRITE_NO_OVERWRITE:D3D11_MAP_WRITE_DISCARD,0,&m));
 memcpy((unsigned char*)m.pData+input_cursor,source,bytes);
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)input_vb,0);
 unsigned stride=direct90_selected,offset=input_cursor;
 ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&input_vb,&stride,&offset);
 if(index_cursor+index_count>16384)index_cursor=0;
 CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)input_ib,0,index_cursor?D3D11_MAP_WRITE_NO_OVERWRITE:D3D11_MAP_WRITE_DISCARD,0,&m));
 memcpy((uint16_t*)m.pData+index_cursor,indices,index_count*2);
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)input_ib,0);
 ID3D11DeviceContext_IASetIndexBuffer(ctx,input_ib,DXGI_FORMAT_R16_UINT,index_cursor*2);
 ID3D11DeviceContext_DrawIndexed(ctx,index_count,0,0);index_cursor+=index_count;
 if(!active.color_only)depth_observe_draw(active.depth_enable,active.depth_write,index_count);
 input_cursor+=bytes;input_bytes+=bytes;direct90_draws++;direct90_bytes+=bytes;
 fallback96_drawn();draws++;triangles+=index_count/3;input_draws++;input_vertices+=vertices;draw_ticks+=hw_clock()-started;return 1;
#else
 (void)source;(void)vertices;(void)indices;(void)index_count;return 0;
#endif
}
static int raw_indexed92(const void *source,unsigned vertices,unsigned stride,const uint16_t *source_indices,unsigned source_vertices,const uint16_t *indices,unsigned index_count,unsigned topology){
#ifdef NIGHTFIRE_VERTEX_BATCH92
 /* Keep all validation before resource writes. Both contiguous and compact
  * CPU packets enter the same byte-ring/index-ring submission implementation. */
 unsigned selected=nf_hw_direct28_active()?28:nf_hw_direct90_stride();
 if(!pending || !program_active || !selected || stride!=selected || !source || !indices ||
    !vertices || vertices>65535 || !index_count || index_count>16384 ||
    topology>NF_HW_TRIANGLE_STRIP || (topology==NF_HW_TRIANGLE_STRIP && index_count<3))return 0;
 unsigned bytes=vertices*stride,capacity=16384*sizeof(NFHardwareInputVertex);
 if(bytes>capacity)return 0;
 if(source_indices){
  if(!source_vertices || source_vertices>65535)return 0;
  for(unsigned i=0;i<vertices;i++)if(source_indices[i]>=source_vertices)return 0;
 }
 for(unsigned i=0;i<index_count;i++)if(indices[i]>=vertices)return 0;
 unsigned primitive_count=topology==NF_HW_TRIANGLE_STRIP?index_count-2:index_count/3;
 uint64_t started=hw_clock();
 if(input_cursor+bytes>capacity)input_cursor=0;
 D3D11_MAPPED_SUBRESOURCE m;
 CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)input_vb,0,input_cursor?D3D11_MAP_WRITE_NO_OVERWRITE:D3D11_MAP_WRITE_DISCARD,0,&m));
 unsigned char *output=(unsigned char*)m.pData+input_cursor;
#ifdef NIGHTFIRE_VERTEX_REUSE105_DIAGNOSTIC
 unsigned reuse105_frame=nightfire_gpu_timing_frame();
 unsigned char *reuse105_mapped=output;
 int reuse105_observe=reuse105_ready(reuse105_frame);
 if(reuse105_observe){
  if(!reuse105_scratch)reuse105_scratch=(unsigned char*)malloc(capacity);
  if(reuse105_scratch)output=reuse105_scratch;
  else{reuse105_observe=0;reuse105_scratch_failures++;}
 }
#endif
 if(source_indices){
  /* Fixed-size copies avoid a per-record variable-length library call. Reads
   * happen after the caller's full-span alias guard, with no guest callbacks. */
  if(stride==28)for(unsigned i=0;i<vertices;i++)memcpy(output+i*28,(const unsigned char*)source+(size_t)source_indices[i]*28,28);
  else for(unsigned i=0;i<vertices;i++)memcpy(output+i*32,(const unsigned char*)source+(size_t)source_indices[i]*32,32);
 }else memcpy(output,source,bytes);
#ifdef NIGHTFIRE_VERTEX_REUSE105_DIAGNOSTIC
 if(reuse105_observe){
  memcpy(reuse105_mapped,output,bytes);
  nf_vertex_reuse105_observe(reuse105_frame,(uintptr_t)source,stride,vertices,source_vertices,source_indices,output);
 }
#endif
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)input_vb,0);
 unsigned offset=input_cursor;
 ID3D11DeviceContext_IASetVertexBuffers(ctx,0,1,&input_vb,&stride,&offset);
 if(index_cursor+index_count>16384)index_cursor=0;
 CALL_OK(ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)input_ib,0,index_cursor?D3D11_MAP_WRITE_NO_OVERWRITE:D3D11_MAP_WRITE_DISCARD,0,&m));
 memcpy((uint16_t*)m.pData+index_cursor,indices,index_count*2);
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)input_ib,0);
 ID3D11DeviceContext_IASetIndexBuffer(ctx,input_ib,DXGI_FORMAT_R16_UINT,index_cursor*2);
 topology92(topology);
 ID3D11DeviceContext_DrawIndexed(ctx,index_count,0,0);
 index_cursor+=index_count;input_cursor+=bytes;input_bytes+=bytes;
 if(!active.color_only)depth_observe_draw(active.depth_enable,active.depth_write,primitive_count*3);
 if(nf_hw_direct28_active()){direct28_draws++;direct28_bytes+=bytes;}
 else{direct90_draws++;direct90_bytes+=bytes;}
 raw92_draws++;raw92_strips+=topology==NF_HW_TRIANGLE_STRIP;raw92_indices+=index_count;raw92_triangles+=primitive_count;
 fallback96_drawn();draws++;triangles+=primitive_count;input_draws++;input_vertices+=vertices;draw_ticks+=hw_clock()-started;return 1;
#else
 (void)source;(void)vertices;(void)stride;(void)source_indices;(void)source_vertices;(void)indices;(void)index_count;(void)topology;return 0;
#endif
}
int nf_hw_draw_raw_indexed(const void *source,unsigned vertices,unsigned stride,const uint16_t *indices,unsigned index_count,unsigned topology){if(resident313_blocked())return 0;
 return raw_indexed92(source,vertices,stride,NULL,vertices,indices,index_count,topology);
}
int nf_hw_draw_raw_compact_indexed(const void *source,unsigned source_vertices,unsigned stride,const uint16_t *source_indices,unsigned unique_vertices,const uint16_t *indices,unsigned index_count,unsigned topology){if(resident313_blocked())return 0;
 if(!source_indices)return 0;
 return raw_indexed92(source,unique_vertices,stride,source_indices,source_vertices,indices,index_count,topology);
}
static void direct28_report(void){
#ifdef NIGHTFIRE_TEXTURE_EQUAL114
 fprintf(stderr,"[TEXTURE-EQUAL114] enabled=%d avx2_available=%d\n",texture114_enabled(),nf_texture_equal114_avx2_available());
#endif
#ifdef NIGHTFIRE_EARLY_SUBMIT113
 fprintf(stderr,"[EARLY-SUBMIT113] interval=%u flushes=%llu\n",early113_setting(),(unsigned long long)early113_flushes);
fprintf(stderr,"[SHADOW-SUBMIT135] enabled=%d flushes=%llu policy=once-after-second-Morton256-draw\n",shadow135_setting(),(unsigned long long)shadow135_flushes);
#endif
#ifdef NIGHTFIRE_COLOR_REUSE110
 if(color110_enabled())nf_color_reuse110_report(stderr);
#endif
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
 (void)nf_color_reupload109_wants(nightfire_gpu_timing_frame());nf_color_reupload109_report(stderr);
#endif
#ifdef NIGHTFIRE_VERTEX_REUSE105_DIAGNOSTIC
 (void)nf_vertex_reuse105_wants(nightfire_gpu_timing_frame());
 nf_vertex_reuse105_report(stderr);
 if(reuse105_scratch_failures)fprintf(stderr,"[GPU-REUSE105] scratch_failures=%llu\n",(unsigned long long)reuse105_scratch_failures);
#endif
#ifdef NIGHTFIRE_TEXTURE_DECODE100
 LARGE_INTEGER decode_frequency;QueryPerformanceFrequency(&decode_frequency);
 fprintf(stderr,"[GPU-DECODE100] enabled=%d calls=%llu bytes=%llu decode_ms=%.3f (included in texture and begin)\n",decode100_enabled(),(unsigned long long)decode100_calls,(unsigned long long)decode100_bytes,1000.0*decode100_ticks/decode_frequency.QuadPart);
#endif
 nf_shader99_report(stderr);
#ifdef NIGHTFIRE_GPU_BLEND97
 if(initialized>0)fprintf(stderr,"[GPU-BLEND97] enabled=%d reverse_subtract_begins=%llu\n",nf_hw_blend97_enabled(),(unsigned long long)reverse_subtract97_begins);
#endif
#ifdef NIGHTFIRE_GPU_FALLBACK96
 if(initialized>0)fprintf(stderr,"[GPU-FALLBACK96] enabled=%d color_only_begins=%llu point_clamp_begins=%llu surface_creates=%llu surface_reuses=%llu\n",nf_hw_fallback96_enabled(),(unsigned long long)color_only96_begins,(unsigned long long)point_clamp96_begins,(unsigned long long)surface96_creates,(unsigned long long)surface96_reuses);
#endif
#ifdef NIGHTFIRE_VERTEX_BATCH92
 if(initialized>0)fprintf(stderr,"[GPU-RAW92] draws=%llu strips=%llu indices=%llu triangles=%llu\n",(unsigned long long)raw92_draws,(unsigned long long)raw92_strips,(unsigned long long)raw92_indices,(unsigned long long)raw92_triangles);
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
 if(initialized>0)fprintf(stderr,"[GPU-DIRECT90] draws=%llu bytes=%llu\n",(unsigned long long)direct90_draws,(unsigned long long)direct90_bytes);
#endif
#ifdef NIGHTFIRE_DIRECT_VERTEX88
 if(initialized>0)fprintf(stderr,"[GPU-DIRECT28] draws=%llu bytes=%llu\n",(unsigned long long)direct28_draws,(unsigned long long)direct28_bytes);
#endif
}
#ifdef NF_RESIDENT130_TEST
int nf_hw_resident130_test_quant(uint32_t *bits,unsigned w,unsigned h){
    if(!nf_hw_sync() || !initialize() || !quant130_prepare(w,h))return 0;
    ID3D11Texture2D *t=NULL,*read=NULL;
    D3D11_TEXTURE2D_DESC d={0};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R32_TYPELESS;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    HRESULT hr=ID3D11Device_CreateTexture2D(dev,&d,NULL,&t);
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    if(SUCCEEDED(hr))hr=ID3D11Device_CreateTexture2D(dev,&d,NULL,&read);
    if(FAILED(hr)){RELEASE(t);RELEASE(read);return 0;}
    ID3D11DeviceContext_UpdateSubresource(ctx,(ID3D11Resource*)t,0,NULL,bits,w*4,0);
    quant130_apply(t);ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)read,(ID3D11Resource*)t);
    D3D11_MAPPED_SUBRESOURCE m;hr=ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)read,0,D3D11_MAP_READ,0,&m);
    if(SUCCEEDED(hr)){for(unsigned y=0;y<h;y++)memcpy(bits+(size_t)y*w,(uint8_t*)m.pData+(size_t)y*m.RowPitch,w*4);ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)read,0);}
    RELEASE(t);RELEASE(read);return SUCCEEDED(hr);
}
#endif
void nf_hw_report(void){if(resident313_blocked())return;
#ifdef NIGHTFIRE_MATERIAL_RING246
if(material_vb221)fprintf(stderr,"[MATERIAL-RING246] enabled=%d capacity=%u cursor=%u discards=%llu appends=%llu wraps=%llu\n",material_ring246,material_capacity246,material_cursor246,(unsigned long long)material_discards246,(unsigned long long)material_appends246,(unsigned long long)material_wraps246);
#endif

#ifdef NIGHTFIRE_CONSUMER135
    nf_consumer135_report(stderr);
#endif
#ifdef NIGHTFIRE_TEXTURE_REUSE133
{LARGE_INTEGER f;QueryPerformanceFrequency(&f);double ms=1000.0/f.QuadPart;
fprintf(stderr,"[TEXTURE133] enabled=%d timing=%d hits=%llu changed=%llu misses=%llu reused=%llu creates=%llu views=%llu subresource_updates=%llu bytes=%llu evictions=%llu compare_ms=%.3f decode_ms=%.3f create_ms=%.3f view_ms=%.3f update_ms=%.3f alias_ms=%.3f (alias nested in texture/sync)\n",
texture133_enabled(),texture133_timing>0,(unsigned long long)texture133.hits,(unsigned long long)texture133.changed,(unsigned long long)texture133.misses,(unsigned long long)texture133.reused,(unsigned long long)texture133.creates,(unsigned long long)texture133.views,(unsigned long long)texture133.updates,(unsigned long long)texture133.bytes,(unsigned long long)texture133.evictions,ms*texture133.compare_ticks,ms*texture133.decode_ticks,ms*texture133.create_ticks,ms*texture133.view_ticks,ms*texture133.update_ticks,ms*texture133.alias_ticks);}
#endif
#ifdef NIGHTFIRE_SWIZZLED_SHADOW132
fprintf(stderr,"[SWIZZLED132] enabled=%d begins=%llu imports=%llu exports=%llu\n",nf_swizzled132_enabled(),(unsigned long long)swizzled132_begins,(unsigned long long)swizzled132_imports,(unsigned long long)swizzled132_exports);
#endif
#ifdef NIGHTFIRE_SWIZZLED_SHADOW131
fprintf(stderr,"[SWIZZLED131] enabled=%d begins=%llu imports=%llu exports=%llu\n",nf_swizzled131_enabled(),(unsigned long long)swizzled131_begins,(unsigned long long)swizzled131_imports,(unsigned long long)swizzled131_exports);
#endif
#ifdef NIGHTFIRE_RESIDENT_MAIN130
fprintf(stderr,"[RESIDENT130] enabled=%d retained=%llu resumed=%llu published=%llu shadow_maps=%llu outstanding=%d\n",resident130_enabled(),(unsigned long long)resident130_retained,(unsigned long long)resident130_resumed,(unsigned long long)resident130_published,(unsigned long long)resident130_shadow_maps,deferred128.valid && deferred128.resident130);
#endif
#ifdef NIGHTFIRE_RGBA_MIPS129
fprintf(stderr,"[GPU-RGBA129] enabled=%d mip5_begins=%llu\n",nf_rgba_mips129_enabled(),(unsigned long long)rgba129_begins);
#endif
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
fprintf(stderr,"[DEFER128] enabled=%d queued=%llu published=%llu paired=%llu early=%llu retained=%d\n",defer128_enabled(),(unsigned long long)defer128_queued,(unsigned long long)defer128_published,(unsigned long long)defer128_paired,(unsigned long long)defer128_early,deferred128.valid);
#endif
direct28_report();nf_gpu_time84_report(ctx,stderr);nf_surface_probe_report(stderr);depth_observe_report();if(initialized>0)fprintf(stderr,"[GPU-DEPTH-UPLOAD] clears=%llu copies=%llu\n",(unsigned long long)depth_upload_clears,(unsigned long long)depth_upload_copies);if(initialized>0)fprintf(stderr,"[GPU-CLEAR] depth=%llu alias_syncs=%llu\n",(unsigned long long)gpu_depth_clears,(unsigned long long)clear_alias_syncs);if(initialized>0)fprintf(stderr,"[GPU-MOVIE] packed_uploads=%llu\n",(unsigned long long)video_uploads);if(initialized>0)fprintf(stderr,"[GPU-STATE] checked=%llu submitted=%llu cache=%d\n",(unsigned long long)binding_checks,(unsigned long long)binding_calls,binding_cache());LARGE_INTEGER f;QueryPerformanceFrequency(&f);if(initialized>0)fprintf(stderr,"[GPU-UPLOAD] vertex_bytes=%llu constant_updates=%llu\n",(unsigned long long)input_bytes,(unsigned long long)constant_uploads);if(initialized>0)fprintf(stderr,"[GPU-VERTEX] draws=%llu vertices=%llu shaders=%llu compile_ms=%.1f\n",(unsigned long long)input_draws,(unsigned long long)input_vertices,(unsigned long long)program_compiles,1000.0*program_ticks/f.QuadPart);if(initialized>0)fprintf(stderr,"[HW-TIME] begin_ms=%.1f draw_ms=%.1f sync_ms=%.1f texture_ms=%.1f (texture included in begin)\n",1000.0*begin_ticks/f.QuadPart,1000.0*draw_ticks/f.QuadPart,1000.0*sync_ticks/f.QuadPart,1000.0*texture_ticks/f.QuadPart);if(initialized>0)fprintf(stderr,"[HW-GPU] draws=%llu triangles=%llu readbacks=%llu texture_uploads=%llu\n",(unsigned long long)draws,(unsigned long long)triangles,(unsigned long long)transfers,(unsigned long long)uploads);}


