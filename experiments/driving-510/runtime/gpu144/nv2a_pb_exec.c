#include "../driving_diag510.h" /*510 private default-OFF diagnostics*/
/**
 * Execute the parts of the title's pushbuffer that produce visible pixels.
 *
 * The title builds NV2A commands in guest RAM and advances DMA_PUT; without
 * something consuming them the framebuffer stays whatever it was, which is how
 * a fully booted title renders a black screen. This walks the same command
 * stream nv2a_pb_scan.c surveys and carries out the subset that decides what is
 * on screen: which surface is being drawn into, and clearing it.
 *
 * The default geometry path uses a screen-space heuristic for UI/video.
 * NIGHTFIRE_VERTEX_PROGRAM enables an experimental independent vertex-program
 * path for float3/4 world geometry. Depth, clipping and shading remain partial;
 * neither the heuristic nor the experimental path proves correct rendering.
 *
 * Everything this does not handle is counted and ranked by
 * nv2a_pb_exec_report(), so what remains is a list rather than a guess.
 *
 * Enabled with RECOMP_PB_EXEC. RECOMP_RASTER_TEST draws one known triangle
 * after every clear, which separates "the pixel path is broken" from "the title
 * has not given us any vertices". RECOMP_FB_DUMP=<prefix> writes the surface to
 * <prefix>NNN.bmp, so the result can be looked at without a display.
 */
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#if defined(NIGHTFIRE_DIRECT_VERTEX_SURVEY89) && !defined(NIGHTFIRE_DIRECT_VERTEX88)
#error NIGHTFIRE_DIRECT_VERTEX_SURVEY89 requires NIGHTFIRE_DIRECT_VERTEX88
#endif
#if defined(NIGHTFIRE_COMPACT_VERTEX91) && !defined(NIGHTFIRE_DIRECT_VERTEX88)
#error NIGHTFIRE_COMPACT_VERTEX91 requires NIGHTFIRE_DIRECT_VERTEX88
#endif
#if defined(NIGHTFIRE_VERTEX_BATCH92) && (!defined(NIGHTFIRE_DIRECT_VERTEX88) || !defined(NIGHTFIRE_DIRECT_DEFAULT90) || !defined(NIGHTFIRE_COMPACT_VERTEX91))
#error NIGHTFIRE_VERTEX_BATCH92 requires NIGHTFIRE_DIRECT_VERTEX88, NIGHTFIRE_DIRECT_DEFAULT90 and NIGHTFIRE_COMPACT_VERTEX91
#endif
#include "nightfire_surface_probe83.h"
#include <string.h>
#include "kernel.h"   /* XBOX_CONTIG_BASE / XBOX_CONTIG_SIZE */
/* The swizzle decoder the D3D8 layer already uses -- one implementation of
 * Morton order, not a second one that can disagree with it. */
#include "d3d8_swizzle.h"
#include "nightfire_vertex_program.h"
#include "nightfire_depth.h"
#include "nightfire_hardware.h"
#include "nightfire_rgba_mips129.h"
#include "nightfire_fog.h"
#include "nightfire_index_guard120.h"
extern ptrdiff_t xbox_GetMemoryOffset(void);
static int verbose_enabled(void){static int on=-1;if(on<0)on=getenv("RECOMP_PB_EXEC_VERBOSE")!=NULL;return on;}
static int hardware_enabled(void){static int on=-1;if(on<0){const char *v=getenv("NIGHTFIRE_HW_GPU");on=v && strcmp(v,"1")==0;}return on;}
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
void nightfire_gpu_complete_reason109(const char *reason){nf_surface_probe_end(1002);if(hardware_enabled() && !nf_hw_sync_tag109(reason)){fprintf(stderr,"[HW-GPU] readback failed; stopping before stale game-memory use\n");exit(4);}}
void nightfire_gpu_complete(void){nightfire_gpu_complete_reason109("external-unknown");}
#define NF_GPU_COMPLETE109(reason) nightfire_gpu_complete_reason109(reason)
#else
void nightfire_gpu_complete(void){nf_surface_probe_end(1002);if(hardware_enabled() && !nf_hw_sync()){fprintf(stderr,"[HW-GPU] readback failed; stopping before stale game-memory use\n");exit(4);}}
#define NF_GPU_COMPLETE109(reason) nightfire_gpu_complete()
#endif
/* The contiguous arena and its tiled aperture share storage; low game RAM
 * is separate. Guard input reads while a GPU clear extends pending drawing. */
void nightfire_gpu_read_guard(uint32_t va,size_t bytes){
    if(!nf_hw_clear_pending || !bytes)return;
    if(bytes>UINT32_MAX || (uint64_t)va+bytes>0x100000000ull){NF_GPU_COMPLETE109("readguard-range");return;}
    if(va<0xf0000000u && (uint64_t)va+bytes>0xf0000000ull){NF_GPU_COMPLETE109("readguard-range");return;}
    if(va>=0xf0000000u && va<0xf4000000u){
        if((uint64_t)va+bytes>0xf4000000ull){NF_GPU_COMPLETE109("readguard-range");return;}
        va=0x80000000u+(va-0xf0000000u);
    }
    if(!nf_hw_clear_read((const uint8_t*)xbox_GetMemoryOffset()+va,bytes)){
        fprintf(stderr,"[HW-GPU] clear input readback failed\n");exit(4);
    }
}
static int hardware_batch;
static unsigned hardware_count;
static NFHardwareVertex hardware_vertices[16384];
static float hardware_inputs[16384*16*4];
static unsigned hardware_program,hardware_input_mask,hardware_attributes[16],hardware_attribute_count,hardware_input_stride;
static uint16_t hardware_indices[16384];
static unsigned hardware_input_count;
static struct {unsigned generation,slot;} hardware_slots[65536];
static int gpu_vertex_enabled(void){static int on=-1;if(on<0){const char *v=getenv("NIGHTFIRE_GPU_VERTEX");on=v && !strcmp(v,"1");}return on;}
#include "nightfire_texture_cache.h"
#include "nightfire_texture_filter.h"
static NFTextureFilter world_filter;
static int texture_filter_enabled(void){static int on=-1;if(on<0){const char *v=getenv("NIGHTFIRE_TEXTURE_FILTER");on=v && strcmp(v,"0")!=0;}return on;}
static int texture_cache_enabled(void){static int on=-1;if(on<0){const char *v=getenv("NIGHTFIRE_TEXTURE_CACHE");on=v?strcmp(v,"0")!=0:texture_filter_enabled();}return on;}
static NFVertexProgram vertex_program;
static int vertex_program_enabled(void){static int on=-1;if(on<0)on=getenv("NIGHTFIRE_VERTEX_PROGRAM")!=NULL;return on;}
static int world_raster;
static int world_varyings;
static uint32_t world_colors[3];
static float world_fog[3];
static int depth_pipeline_enabled(void){static int on=-1;if(on<0)on=getenv("NIGHTFIRE_DEPTH")!=NULL;return on;}
static uint32_t depth_pending_address,depth_pending_value;

extern ptrdiff_t xbox_GetMemoryOffset(void);
extern uint32_t xbox_ContiguousAllocatedBytes(void);
extern void xbox_FramebufferWindowSet(uint32_t fb_va, uint32_t pitch);
extern void xbox_FramebufferPublish(uint32_t fb_va, uint32_t pitch);
static uint32_t shade_mode;
static uint32_t combiner_rgb,combiner_alpha,combiner_rgb_out,combiner_alpha_out,combiner_count,final0,final1;
static uint32_t fog_enable,fog_mode,fog_gen,fog_color,fog_params[3];
static uint32_t last_draw_address,last_draw_pitch;
static unsigned diagnostic_presents;
#ifdef NIGHTFIRE_COMPACT_VERTEX91
static uint64_t compact91_draws,compact91_vertices,compact91_bytes;
#endif
#ifdef NIGHTFIRE_VERTEX_BATCH92
static uint64_t batch92_draws,batch92_compact_draws,batch92_strip_draws,batch92_vertices,batch92_bytes,batch92_indices;
static uint64_t packed92_draws,packed92_vertices,packed92_bytes,packed92_indices,packed92_triangles;
#endif
#ifdef NIGHTFIRE_DIRECT_VERTEX_SURVEY89
static void direct_vertex89_report(void);
#endif
#ifdef NIGHTFIRE_DIRECT_VERTEX_SURVEY
static void direct_vertex_survey_report(void);
#endif
#if defined(NIGHTFIRE_GPU_TIMING_DIAGNOSTIC) || defined(NIGHTFIRE_VERTEX_REUSE105_DIAGNOSTIC) || defined(NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC)
unsigned nightfire_gpu_timing_frame(void){return diagnostic_presents;}
#endif
#ifdef NIGHTFIRE_SURFACE_PROBE_DIAGNOSTIC
unsigned nightfire_surface_probe_frame(void){return diagnostic_presents;}
#endif
static uint64_t vertex_ticks,raster_ticks,profile_vertices,profile_triangles;
static int world_profile(void){static int on=-1;if(on<0)on=getenv("NIGHTFIRE_PROFILE")!=NULL;return on;}
static uint64_t profile_clock(void){LARGE_INTEGER now;QueryPerformanceCounter(&now);return (uint64_t)now.QuadPart;}
static uint32_t diagnostic_vp_mode,diagnostic_vp_start;
static struct {uint32_t method,value;} transform_trace[3072];
static unsigned transform_trace_count;
static int gpu_diagnostics(void) {static int enabled=-1;if(enabled<0)enabled=getenv("NIGHTFIRE_GPU_DIAGNOSTICS")!=NULL;return enabled;}
static void diagnostic_draw_state(void);
void nv2a_pb_exec_report(void);
static void video_mapping_report(void);
void nightfire_gpu_present(void)
{
    NF_GPU_COMPLETE109("present");
    if(last_draw_address) xbox_FramebufferPublish(last_draw_address,last_draw_pitch);
    diagnostic_presents++;
#ifdef NIGHTFIRE_SHADOW_LAYOUT131_DIAGNOSTIC
    nf_shadow131_tick(diagnostic_presents);
#endif
#ifdef NIGHTFIRE_DIRECT_VERTEX_SURVEY89
    direct_vertex89_report();
#endif
#ifdef NIGHTFIRE_DIRECT_VERTEX_SURVEY
    direct_vertex_survey_report();
#endif
    if(hardware_enabled() && diagnostic_presents%100==0){nf_hw_report();video_mapping_report();}
#ifdef NIGHTFIRE_COMPACT_VERTEX91
    if(hardware_enabled() && diagnostic_presents%100==0)fprintf(stderr,"[GPU-COMPACT91] draws=%llu vertices=%llu bytes=%llu\n",(unsigned long long)compact91_draws,(unsigned long long)compact91_vertices,(unsigned long long)compact91_bytes);
#endif
#ifdef NIGHTFIRE_VERTEX_BATCH92
    if(hardware_enabled() && diagnostic_presents%100==0)fprintf(stderr,"[GPU-BATCH92] draws=%llu compact=%llu strips=%llu vertices=%llu bytes=%llu submitted_indices=%llu\n",(unsigned long long)batch92_draws,(unsigned long long)batch92_compact_draws,(unsigned long long)batch92_strip_draws,(unsigned long long)batch92_vertices,(unsigned long long)batch92_bytes,(unsigned long long)batch92_indices);
    if(hardware_enabled() && diagnostic_presents%100==0)fprintf(stderr,"[GPU-PACKED92] strips=%llu vertices=%llu bytes=%llu submitted_indices=%llu triangles=%llu\n",(unsigned long long)packed92_draws,(unsigned long long)packed92_vertices,(unsigned long long)packed92_bytes,(unsigned long long)packed92_indices,(unsigned long long)packed92_triangles);
#endif
    if(world_profile() && diagnostic_presents%100==0){LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
        fprintf(stderr,"[WORLD-PROFILE] present=%u vertices=%llu vertex_ms=%.3f triangles=%llu raster_ms=%.3f\n",diagnostic_presents,(unsigned long long)profile_vertices,1000.0*vertex_ticks/frequency.QuadPart,(unsigned long long)profile_triangles,1000.0*raster_ticks/frequency.QuadPart);}
    if(gpu_diagnostics() &&
       (diagnostic_presents==2500 || diagnostic_presents==2600 || diagnostic_presents==2700))
        nv2a_pb_exec_report();
}
/* Observed single-stage T0 * V0, R0 final output. Encodings and output scaling
 * are documented by the pinned d3d8_combiners component. Other shaders retain
 * the diagnostic fallback until implemented; this is not a general shader. */
static int supported_final(void){
    return final1==0x1c80 && (final0==0xc ||
        (final0==0x130c0300 && (!fog_enable || fog_mode==0x800)));
}
static float current_fog(float distance){
    if(!fog_enable || fog_mode!=0x800)return 1;
    float p[3];memcpy(p,fog_params,sizeof p);return nf_fog_exp(distance,p[0],p[1]);
}
static uint32_t fog_pixel(uint32_t c,float factor){
    if(!fog_enable || final0!=0x130c0300 || !supported_final() || combiner_count!=1 ||
       combiner_rgb!=0x08040000 || combiner_alpha!=0x18140000 ||
       (combiner_rgb_out!=0x10c00 && combiner_rgb_out!=0xc00) || combiner_alpha_out!=0xc00)return c;
    factor=fminf(1,fmaxf(0,factor));uint32_t result=c&0xff000000u;
    for(unsigned k=0;k<3;k++){
        unsigned shift=16-k*8;float src=(float)((c>>shift)&255),fog=(float)((fog_color>>(k*8))&255);
        result|=(uint32_t)(fog+(src-fog)*factor+0.5f)<<shift;
    }
    return result;
}
static uint32_t shade_texel(uint32_t texel,uint32_t diffuse)
{
    if(combiner_count!=1 || combiner_rgb!=0x08040000 || combiner_alpha!=0x18140000 ||
       (combiner_rgb_out!=0x10c00 && combiner_rgb_out!=0xc00) || combiner_alpha_out!=0xc00 ||
       !supported_final()) return texel;
    uint32_t result=0;
    for(unsigned shift=0;shift<32;shift+=8) {
        unsigned scale=shift==24?1:(combiner_rgb_out==0x10c00?2:1);
        unsigned v=(((texel>>shift)&255)*((diffuse>>shift)&255)*scale+127)/255;
        if(v>255)v=255;result|=v<<shift;
    }
    return result;
}
extern void xbox_FramebufferWindowStart(void);
extern uint32_t g_xbox_image_lo, g_xbox_image_hi;

/* Record the actual CPU destination at the PAL conversion call boundary.
 * The tiled aperture now shares the physical/contiguous arena. */
static SRWLOCK s_video_lock = SRWLOCK_INIT;
typedef struct { uint32_t va, bytes; } VideoBuffer;
static VideoBuffer s_video_buffers[8];
static volatile LONG64 s_video_generation;
static uint64_t s_video_resolves,s_video_snapshots;
static int video_mapping_cache(void){static int enabled=-1;if(enabled<0){const char *v=getenv("NIGHTFIRE_VIDEO_MAP_CACHE");enabled=!v || strcmp(v,"0");}return enabled;}
static void video_mapping_report(void){fprintf(stderr,"[VIDEO-MAP] resolves=%llu snapshots=%llu cache=%d\n",(unsigned long long)s_video_resolves,(unsigned long long)s_video_snapshots,video_mapping_cache());}
static unsigned s_video_next;
void nightfire_gpu_note_video_output(uint32_t va, uint32_t pitch, uint32_t block_rows)
{
    uint32_t cpu_va = va;
    uint64_t bytes = (uint64_t)pitch * block_rows * 16;
    /* LockRect returns the F alias of the contiguous physical allocation. */
    if (va >= 0xf0000000u && (uint64_t)va + bytes <= 0xf4000000ull)
        va -= 0x70000000u;
    if (!va || !pitch || pitch > 16384 || !block_rows || block_rows > 256
        || (va < 0x80000000u ? (uint64_t)va + bytes > 0x04000000u
                            : (uint64_t)va + bytes > 0x84000000u)) return;
    AcquireSRWLockExclusive(&s_video_lock);
    for (unsigned i=0; i<8; ++i) {
        if (s_video_buffers[i].va == va) {
            if(s_video_buffers[i].bytes!=(uint32_t)bytes){
                s_video_buffers[i].bytes = (uint32_t)bytes;
                InterlockedIncrement64(&s_video_generation);
            }
            ReleaseSRWLockExclusive(&s_video_lock);
            return;
        }
    }
    unsigned i = s_video_next++ % 8;
    s_video_buffers[i].va = va; s_video_buffers[i].bytes = (uint32_t)bytes;
    InterlockedIncrement64(&s_video_generation);
    fprintf(stderr, "[VIDEO-OUTPUT] cpu=%08X physical=%08X pitch=%u rows=%u bytes=%u\n",
            cpu_va, va & 0x7fffffffu, pitch, block_rows * 16, (uint32_t)bytes);
    ReleaseSRWLockExclusive(&s_video_lock);
}

static uint32_t texture_resolve(uint32_t offset);

/* Would writing this surface land on the title's own image?
 *
 * NV097_SET_SURFACE_COLOR_OFFSET is an offset within the colour DMA object,
 * not a guest virtual address, and this executor has always used it as one.
 * That is harmless while the two happen to agree and catastrophic when they do
 * not: the Xbox Dashboard names surface 0x00088000 at 1280x960x4, so clearing
 * it wrote 4.9 MB of opaque black from 0x00088000 to 0x00538000 -- straight
 * over its own code, its D3D context at 0x000BBFC0 and the register-block
 * pointer at 0x000BE2C4. The symptom was a title that submitted one perfect
 * frame and then spun forever in a pushbuffer-full loop, three layers away,
 * with every D3D global reading 0xFF000000: the clear colour.
 *
 * So refuse, and say so. Getting the address right needs the DMA object base
 * this ignores (NV097_SET_CONTEXT_DMA_COLOR); until that exists, writing
 * nothing is strictly better than writing over the guest, and a title that
 * cannot draw is easier to debug than one that has been overwritten.
 */
static int surface_hits_image(uint32_t base, uint32_t bytes)
{
    if (!g_xbox_image_hi || !bytes)
        return 0;
    return base < g_xbox_image_hi && base + bytes > g_xbox_image_lo;
}

/* Where a DMA-object offset actually lives.
 *
 * NV097_SET_SURFACE_COLOR_OFFSET is an offset inside the colour DMA object,
 * and for a framebuffer that object covers physical memory -- so the offset
 * is a physical address, not a guest VA. Those are the same number in this
 * runtime, which is why treating it as a VA works until it does not: on
 * hardware the image is mapped at VA 0x00010000 from arbitrary physical
 * pages, so a framebuffer at physical 0x84000 does not overlap it. Here it
 * would.
 *
 * The title tells us which it is by where it allocated. Half-Life 2's
 * framebuffer comes from MmAllocateContiguousMemory, which this runtime
 * serves from the window at XBOX_CONTIG_BASE, so physical P is visible at
 * XBOX_CONTIG_BASE + P -- clear of the image, and the same bytes the title's
 * own writes and the framebuffer window reach.
 *
 * So: use the offset as a VA when that is credible, and fall back to the
 * physical mirror exactly when it is not. Titles whose surfaces already sit
 * in ordinary RAM (Wreckless renders to the tiled alias of physical
 * 0x01954000) keep the first path and are unaffected.
 */
static uint32_t dma_resolve(uint32_t offset)
{
    extern uint32_t xbox_ContiguousAllocatedBytes(void);

    /* Did this runtime hand the offset out as contiguous memory? Then the
     * bytes live in the window, and that is not a guess: the arena is a bump
     * allocator from XBOX_CONTIG_BASE, so everything below its high-water
     * mark is memory some MmAllocateContiguousMemory call returned. The
     * title's own writes go through the window, so the executor's must too.
     *
     * Checking this BEFORE the image test is the whole point. The image test
     * only catches an offset that would land on the title's code, and whether
     * it does is an accident of where the image happens to end: Half-Life 2's
     * colour surface is physical 0x00A6C000, which clears the image by 700 KB.
     * So it looked like an ordinary VA, and the executor cleared 1.2 MB of
     * black straight through the guest heap -- which faulted the title three
     * frames later on a pointer that had been overwritten, while the real
     * framebuffer at 0x80A6C000 stayed untouched and the screen stayed black. */
    if (offset < xbox_ContiguousAllocatedBytes())
        return XBOX_CONTIG_BASE + offset;
    if (!surface_hits_image(offset, 1))
        return offset;
    if ((uint64_t)offset < XBOX_CONTIG_SIZE)
        return XBOX_CONTIG_BASE + offset;
    return offset;                         /* nothing better to offer */
}

static uint32_t texture_resolve(uint32_t offset)
{
    uint32_t result = offset;
    int found = 0;
    /* Only the synchronous GPU consumer reads this private snapshot. Writers
     * publish a new generation under the table lock after a mapping changes.
     * A hit can linearize at the acquire read, including concurrent updates.
     * Cache mappings only: fallback allocation bounds remain live below. */
    static VideoBuffer snapshot[8];static LONG64 generation=-1;
    const VideoBuffer *buffers=s_video_buffers;
    int cached=video_mapping_cache();s_video_resolves++;
    if(cached){
        if(generation!=ReadAcquire64(&s_video_generation)){
            AcquireSRWLockShared(&s_video_lock);
            memcpy(snapshot,s_video_buffers,sizeof snapshot);
            generation=ReadAcquire64(&s_video_generation);
            ReleaseSRWLockShared(&s_video_lock);s_video_snapshots++;
        }
        buffers=snapshot;
    }else AcquireSRWLockShared(&s_video_lock);
    for (unsigned i=0; i<8; ++i) {
        uint32_t physical = buffers[i].va & 0x7fffffffu;
        if (buffers[i].bytes && offset >= physical
            && (uint64_t)offset < (uint64_t)physical + buffers[i].bytes) {
            result = buffers[i].va + (offset - physical);
            found = 1;
            break;
        }
    }
    if(!cached)ReleaseSRWLockShared(&s_video_lock);
    return found ? result : dma_resolve(offset);
}

static int surface_write_refused(uint32_t base, uint32_t bytes, const char *what)
{
    static int said;

    if (!surface_hits_image(base, bytes))
        return 0;
    if (!said) {
        said = 1;
        fprintf(stderr,
                "  [GPU] REFUSING to %s surface 0x%08X..0x%08X: that overlaps "
                "the loaded image (0x%08X..0x%08X).\n"
                "  [GPU]   SET_SURFACE_COLOR_OFFSET is a DMA-object offset, not "
                "a guest VA, and this executor treats it as one. Writing here "
                "would destroy the title's own code and globals.\n",
                what, base, base + bytes, g_xbox_image_lo, g_xbox_image_hi);
        fflush(stderr);
    }
    return 1;
}

/* NV097 methods this executor acts on. */
#define NV097_SET_SURFACE_CLIP_HORIZONTAL 0x0200
#define NV097_SET_SURFACE_CLIP_VERTICAL   0x0204
#define NV097_SET_SURFACE_FORMAT          0x0208
#define NV097_SET_SURFACE_PITCH           0x020C
#define NV097_SET_SURFACE_COLOR_OFFSET    0x0210
#define NV097_SET_COLOR_CLEAR_VALUE       0x1D90
#define NV097_CLEAR_SURFACE               0x1D94
#define NV097_SET_VERTEX_DATA_ARRAY_OFFSET 0x1720   /* +i*4, 16 attributes */
#define NV097_SET_VERTEX_DATA_ARRAY_FORMAT 0x1760   /* +i*4 */
#define NV097_SET_BEGIN_END               0x17FC
#define NV097_SET_TEXTURE_OFFSET          0x1B00   /* +i*0x40 */
#define NV097_SET_TEXTURE_FORMAT          0x1B04
#define NV097_SET_TEXTURE_ADDRESS         0x1B08
#define NV097_SET_TEXTURE_CONTROL1        0x1B10
#define NV097_SET_TEXTURE_IMAGE_RECT      0x1B1C
/* The buffer flip. A title double-buffers by telling the GPU which buffer
 * the CRTC reads and which it draws into, advancing the write index and
 * then stalling until the flip has happened. Ignoring these means the
 * stall never clears: Half-Life 2's loader submits its initialisation,
 * asks for a flip, and waits for it in a loop that makes no kernel calls
 * and burns no dispatch, which reads as a hang with no cause.
 *
 * ponytail: the flip completes the moment it is asked for, because there is
 * no scanout to be in the middle of. That makes every frame land instantly
 * and a title that paces itself on the flip runs as fast as it can draw.
 * Pacing wants the vblank clock in the kernel, not a sleep in here. */
#define NV097_SET_FLIP_READ               0x0120
#define NV097_SET_FLIP_WRITE              0x0124
#define NV097_SET_FLIP_MODULO             0x0128
#define NV097_FLIP_INCREMENT_WRITE        0x012C
#define NV097_FLIP_STALL                  0x0130
#define NV097_ARRAY_ELEMENT16             0x1800
#define NV097_ARRAY_ELEMENT32             0x1808
#define NV097_INLINE_ARRAY                0x1818

#define NV097_CLEAR_COLOR_MASK            0xF0   /* R,G,B,A bits */

/* One vertex attribute stream, as the title describes it. Attribute 0 is
 * position; the rest are colours, texture coordinates and so on. */
typedef struct {
    uint32_t offset;      /* guest address of element 0 */
    uint32_t type;        /* NV097 data type nibble */
    uint32_t size;        /* components per element */
    uint32_t stride;      /* bytes between elements */
} VertexAttr;

#define NV_VERTEX_ATTRS 16
#define NV_MAX_INDICES  4096
#define NV_MAX_INLINE   4096            /* dwords of INLINE_ARRAY per batch */

/* Texture stage 0, decoded from what the title programmed.
 *
 * Only stage 0: it is the only one the dashboard configures, and a stage
 * nothing writes to is a stage nothing can be sampled from. The rest arrive
 * as unhandled methods and are counted as such, which is how the next title
 * that needs them will say so. */
typedef struct {
    uint32_t offset;                    /* guest address of texel (0,0)  */
    uint32_t width, height;             /* from IMAGE_RECT               */
    uint32_t pitch;                     /* bytes per row, from CONTROL1  */
    uint32_t color;                     /* NV097 colour-format code      */
    uint32_t addr_u, addr_v;            /* wrap mode per axis            */
    int      valid;
} Texture;

static struct {
    VertexAttr attr[NV_VERTEX_ATTRS];
    uint32_t   prim;                    /* SET_BEGIN_END parameter, 0 = ended */
    uint16_t   idx[NV_MAX_INDICES];
    uint32_t   idx_count;
    /* INLINE_ARRAY payload: vertices written straight into the pushbuffer
     * instead of into a buffer the title points at. Same vertex format, a
     * different place to read them from. */
    uint32_t   inline_buf[NV_MAX_INLINE];
    uint32_t   inline_count;
    int        inline_active;
    uint32_t   draws, verts, nonzero_draws;
    float      min_x, max_x, min_y, max_y;
    uint32_t color_offset, color_base, pitch, format;
    uint32_t clip_x, clip_w, clip_y, clip_h;
    uint32_t clear_color;
    uint32_t depth_enable,depth_func,depth_mask,zeta_offset,zeta_pitch,zclear,clear_x,clear_y,control0;
    uint32_t alpha_test, alpha_func, alpha_ref;
    uint32_t cull_enable,cull_face,front_face,culled;
    uint32_t blend_enable, blend_src, blend_dst, blend_equation, blend_color;
    uint32_t clears, unhandled_total;
    uint32_t flip_read, flip_write, flip_modulo, flips;
    uint32_t tris_drawn, tris_skipped_offscreen, batches_untransformed;
    /* Why a batch came out flat. "Untextured" has two causes that look
     * identical on screen and want opposite fixes: the batch carried no
     * texture coordinates, or it did and the stage was not usable. */
    uint32_t batches_textured, batches_no_uv, batches_no_tex;
    Texture  tex;
} s_gpu;

/* Unhandled methods, ranked. The interesting output is not that something was
 * skipped but which things dominate, because that is the order to implement
 * them in. */
#define PB_EXEC_MAX_UNHANDLED 2048
typedef struct { uint32_t method, count; } PbUnhandled;
static PbUnhandled s_unhandled[PB_EXEC_MAX_UNHANDLED];
static int s_unhandled_count;
/* Normal NV2A method addresses are aligned and below0x2000. Store slot+1;
 * validate the key because periodic reports reorder the ranked table. */
static uint16_t s_unhandled_slot[2048];

/* Every texture-stage register, as the title last set it. Descriptor contents
 * survive disabling a stage; they do not imply that sampling is enabled. */
#define NV_TEX_FIRST 0x1B00
#define NV_TEX_LAST  0x1BFC
static uint32_t s_tex_reg[(NV_TEX_LAST - NV_TEX_FIRST) / 4 + 1];
static uint8_t  s_tex_set[(NV_TEX_LAST - NV_TEX_FIRST) / 4 + 1];

/* Formats whose dimensions come from the format word and whose coordinates
 * arrive normalised, rather than from a pitch and SET_TEXTURE_IMAGE_RECT with
 * coordinates in texels. Swizzled and block-compressed are both in this group,
 * and every place that used to test only for swizzled needs the pair. */
static int tex_size_from_format(uint32_t fmt)
{
    return d3d8_format_is_swizzled(fmt) || d3d8_format_dxt_block_bytes(fmt);
}

static void record_tex_reg(uint32_t method, uint32_t param)
{
    s_tex_reg[(method - NV_TEX_FIRST) / 4] = param;
    s_tex_set[(method - NV_TEX_FIRST) / 4] = 1;
    /* A pitch is a linear texture's property. A swizzled one has no rows and
     * so no pitch, and requiring one here refused every swizzled texture --
     * which is nearly all of them, since swizzled is the Xbox default. That
     * left the title's own textures unsampled and every textured quad drawn in
     * flat vertex colour. */
    /* NV097_SET_TEXTURE_CONTROL0 bit30 (pinned NV2A register definitions).
     * Untextured glass must not sample the preceding moon/light-flare texture. */
    s_gpu.tex.valid = (s_tex_reg[3]&0x40000000u) && s_gpu.tex.offset && s_gpu.tex.width && s_gpu.tex.height
                   && (tex_size_from_format(s_gpu.tex.color)
                       || s_gpu.tex.pitch);
}

/* Every distinct texture a batch was drawn with, and how many batches used it.
 *
 * The per-draw verbose print shows the first few draws of the first frame,
 * which is enough to see that texturing works at all and not enough to answer
 * "is a font page ever bound". This is the same shape as the unhandled-method
 * table below it: a small set, ranked, printed with the rest of the report. */
#define PB_EXEC_MAX_TEXTURES 64
typedef struct {
    uint32_t offset, color, width, height, batches;
} PbTexUse;
static PbTexUse s_tex_use[PB_EXEC_MAX_TEXTURES];
static int s_tex_use_count;

/* Defined below, next to the sampler it goes through. */
static void dump_texture_bmp(uint32_t seq);

static void note_texture_use(void)
{
    int i;

    if (!s_gpu.tex.valid)
        return;
    for (i = 0; i < s_tex_use_count; i++) {
        if (s_tex_use[i].offset == s_gpu.tex.offset
         && s_tex_use[i].color  == s_gpu.tex.color) {
            s_tex_use[i].batches++;
            if (s_tex_use[i].batches == 60)
                dump_texture_bmp((uint32_t)i + PB_EXEC_MAX_TEXTURES);
            return;
        }
    }
    if (s_tex_use_count < PB_EXEC_MAX_TEXTURES) {
        s_tex_use[s_tex_use_count].offset  = s_gpu.tex.offset;
        s_tex_use[s_tex_use_count].color   = s_gpu.tex.color;
        s_tex_use[s_tex_use_count].width   = s_gpu.tex.width;
        s_tex_use[s_tex_use_count].height  = s_gpu.tex.height;
        s_tex_use[s_tex_use_count].batches = 1;
        s_tex_use_count++;
        dump_texture_bmp((uint32_t)s_tex_use_count - 1);
    }
}

static void note_unhandled(uint32_t method)
{
    int i;

    s_gpu.unhandled_total++;
    if(method<0x2000 && !(method&3)){
        unsigned slot=s_unhandled_slot[method/4];
        if(slot && slot<=(unsigned)s_unhandled_count && s_unhandled[slot-1].method==method){
            s_unhandled[slot-1].count++;return;
        }
    }
    for (i = 0; i < s_unhandled_count; i++) {
        if (s_unhandled[i].method == method) {
            s_unhandled[i].count++;
            if(method<0x2000 && !(method&3))s_unhandled_slot[method/4]=(uint16_t)(i+1);
            return;
        }
    }
    if (s_unhandled_count < PB_EXEC_MAX_UNHANDLED) {
        s_unhandled[s_unhandled_count].method = method;
        s_unhandled[s_unhandled_count].count = 1;
        s_unhandled_count++;
        if(method<0x2000 && !(method&3))s_unhandled_slot[method/4]=(uint16_t)s_unhandled_count;
    }
}

/* Read attribute `a` of vertex `index` as floats. Only the float and the
 * normalised-byte types appear in practice; anything else returns 0 so a
 * caller sees a degenerate vertex rather than reading past the array. */
#if defined(_MSC_VER)
/* CP60: eliminate the hot per-attribute call boundary so the
 * compiler can propagate each drawing path's inputs without changing math. */
static __forceinline int fetch_attr(const VertexAttr *a, uint32_t index, float out[4])
#else
static inline int fetch_attr(const VertexAttr *a, uint32_t index, float out[4])
#endif
{
    const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
    const uint8_t *p;
    uint32_t i;

    out[0] = out[1] = out[2] = 0.0f;
    out[3] = 1.0f;
    if (!a->size || !a->stride)
        return 0;
    if (s_gpu.inline_active) {
        /* The batch arrived as INLINE_ARRAY, so `offset` is a byte offset into
         * the buffered payload rather than a guest address -- and 0 is a legal
         * one there, which is why the offset test is on the other side of this
         * branch. */
        size_t at = (size_t)a->offset + (size_t)index * a->stride;
        if (at + 4 > (size_t)s_gpu.inline_count * 4)
            return 0;
        p = (const uint8_t *)s_gpu.inline_buf + at;
    } else {
        if (!a->offset)
            return 0;
        p = mem + a->offset + (size_t)index * a->stride;
        if(nf_hw_clear_pending){
            uint64_t address=(uint64_t)a->offset+(size_t)index*a->stride;
            if(address>UINT32_MAX)NF_GPU_COMPLETE109("readguard-range");
            else nightfire_gpu_read_guard((uint32_t)address,16);
        }
    }

    switch (a->type) {
    case 0:                                  /* D3DCOLOR */
        /* A DWORD 0xAARRGGBB, so little-endian bytes are B,G,R,A -- not the
         * component order of every other format here. Returned as R,G,B,A so
         * callers need not know which format the title chose. */
        out[0] = (float)p[2] / 255.0f;
        out[1] = (float)p[1] / 255.0f;
        out[2] = (float)p[0] / 255.0f;
        out[3] = (float)p[3] / 255.0f;
        return 1;
    case 2:                                  /* float */
        for (i = 0; i < a->size && i < 4; i++)
            out[i] = ((const float *)p)[i];
        return 1;
    case 4:                                  /* unsigned byte, normalised */
        for (i = 0; i < a->size && i < 4; i++)
            out[i] = (float)p[i] / 255.0f;
        return 1;
    default:
        return 0;
    }
}

static uint32_t surface_bpp(void)
{
#ifdef NIGHTFIRE_SWIZZLED_SHADOW131
    if(nf_swizzled131_layout(s_gpu.format,s_gpu.pitch,s_gpu.clip_x,s_gpu.clip_y,s_gpu.clip_w,s_gpu.clip_h,s_gpu.depth_enable,s_gpu.zeta_offset))return 4;
#endif
    /* The pitch and the clip width together give the pixel size, which is more
     * reliable than decoding the format field: the format's colour code is
     * only meaningful alongside a type the title also sets, while the pitch is
     * always exactly how many bytes a row occupies. */
    if (!s_gpu.clip_w)
        return 0;
    return s_gpu.pitch / s_gpu.clip_w;
}


/* Write the current surface out as a 24-bit BMP.
 *
 * A framebuffer window needs someone watching it. A file does not, which makes
 * this the only way to check what a title actually rendered on a machine you
 * are not sitting at -- and the only way to put a picture in a bug report.
 *
 * ponytail: bottom-up 24bpp BMP, no palette, no compression. That is the one
 * format every viewer reads and it is 30 lines; PNG would need a dependency.
 */
static void dump_surface_bmp_named(const char *named)
{
    NF_GPU_COMPLETE109("capture");
    const char *prefix = getenv("RECOMP_FB_DUMP");
    const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
    uint32_t bpp = surface_bpp();
    static int seq;
    char path[512];
    uint32_t w = s_gpu.clip_w, h = s_gpu.clip_h, y, x;
    uint32_t row_bytes, pad, filesz;
    uint8_t hdr[54];
    FILE *f;

    if (!prefix || !w || !h || (bpp != 2 && bpp != 4) || !s_gpu.color_offset)
        return;

    row_bytes = w * 3;
    pad = (4 - (row_bytes & 3)) & 3;
    filesz = 54 + (row_bytes + pad) * h;

    if(named)snprintf(path,sizeof path,"%s",named);else snprintf(path, sizeof path, "%s%03d.bmp", prefix, seq++);
    f = fopen(path, "wb");
    if (!f)
        return;

    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B'; hdr[1] = 'M';
    memcpy(hdr + 2, &filesz, 4);
    hdr[10] = 54;
    hdr[14] = 40;
    memcpy(hdr + 18, &w, 4);
    memcpy(hdr + 22, &h, 4);
    hdr[26] = 1;
    hdr[28] = 24;
    fwrite(hdr, 1, sizeof hdr, f);

    /* BMP rows run bottom-up. */
    for (y = h; y-- > 0; ) {
        const uint8_t *row = mem + dma_resolve(s_gpu.color_offset)
                           + (size_t)(s_gpu.clip_y + y) * s_gpu.pitch;
        for (x = 0; x < w; x++) {
            uint8_t bgr[3];
            if (bpp == 4) {
                uint32_t v = ((const uint32_t *)row)[s_gpu.clip_x + x];
#ifdef NIGHTFIRE_SWIZZLED_SHADOW131
                if(nf_swizzled131_layout(s_gpu.format,s_gpu.pitch,s_gpu.clip_x,s_gpu.clip_y,s_gpu.clip_w,s_gpu.clip_h,s_gpu.depth_enable,s_gpu.zeta_offset)){
                    uint32_t base=dma_resolve(s_gpu.color_offset);
                    unsigned size=nf_swizzled131_size(nf_swizzled131_layout(s_gpu.format,s_gpu.pitch,s_gpu.clip_x,s_gpu.clip_y,s_gpu.clip_w,s_gpu.clip_h,s_gpu.depth_enable,s_gpu.zeta_offset));
                    if(base>=XBOX_CONTIG_BASE && (uint64_t)base+size*size*4<=(uint64_t)XBOX_CONTIG_BASE+xbox_ContiguousAllocatedBytes())
                        v=((const uint32_t*)(mem+base))[nf_swizzled131_index(s_gpu.clip_x+x,s_gpu.clip_y+y)];
                }
#endif
                bgr[0] = (uint8_t)(v);
                bgr[1] = (uint8_t)(v >> 8);
                bgr[2] = (uint8_t)(v >> 16);
            } else {
                uint16_t v = ((const uint16_t *)row)[s_gpu.clip_x + x];
                bgr[0] = (uint8_t)(( v        & 0x1F) << 3);
                bgr[1] = (uint8_t)(((v >>  5) & 0x3F) << 2);
                bgr[2] = (uint8_t)(((v >> 11) & 0x1F) << 3);
            }
            fwrite(bgr, 1, 3, f);
        }
        if (pad) {
            static const uint8_t zero[3] = {0, 0, 0};
            fwrite(zero, 1, pad, f);
        }
    }
    fclose(f);
    if (seq == 1)
        fprintf(stderr, "  [GPU] framebuffer dump: %s (%ux%u from 0x%08X %ubpp)\n",
                path, w, h, s_gpu.color_offset, bpp);
}

/* Defined below, next to the rest of the rasteriser; the clear path uses it
 * for RECOMP_RASTER_TEST. */
static void raster_triangle(const float a[2], const float b[2],
                            const float c[2], uint32_t argb,
                            const float uv[3][2]);

static void dump_surface_bmp(void){dump_surface_bmp_named(NULL);}

static int depth_surface(uint32_t *base)
{
    uint64_t end=(uint64_t)s_gpu.zeta_offset+(uint64_t)(s_gpu.clip_y+s_gpu.clip_h)*s_gpu.zeta_pitch;
    /* Only the captured pitched integer Z24S8 layout, inside an allocation
     * from the runtime's contiguous arena. Never guess a depth-buffer VA. */
    if((s_gpu.format&0xff0)!=0x120 || (s_gpu.control0&0x1000) ||
       !s_gpu.zeta_offset || !s_gpu.clip_w || !s_gpu.clip_h ||
       (s_gpu.zeta_pitch&3) || s_gpu.zeta_pitch<(s_gpu.clip_x+s_gpu.clip_w)*4 ||
       end>xbox_ContiguousAllocatedBytes())return 0;
    *base=dma_resolve(s_gpu.zeta_offset);return 1;
}
#ifdef NIGHTFIRE_SWIZZLED_SHADOW131
static int swizzled_shadow131(void){
    return nf_swizzled131_layout(s_gpu.format,s_gpu.pitch,
        s_gpu.clip_x,s_gpu.clip_y,s_gpu.clip_w,s_gpu.clip_h,s_gpu.depth_enable,s_gpu.zeta_offset);
}
static int swizzled_backing131(uint32_t base){
    unsigned size=nf_swizzled131_size(swizzled_shadow131());
    return size && base>=XBOX_CONTIG_BASE && (uint64_t)base+size*size*4<=(uint64_t)XBOX_CONTIG_BASE+xbox_ContiguousAllocatedBytes();
}
#endif
static void clear_depth_surface(uint8_t *mem,unsigned mask)
{
    uint32_t base;
    if(!depth_pipeline_enabled() || !(mask&3) || !depth_surface(&base))return;
    unsigned left=s_gpu.clear_x&0xffff,right=(s_gpu.clear_x>>16)+1;
    unsigned top=s_gpu.clear_y&0xffff,bottom=(s_gpu.clear_y>>16)+1;
    if(left<s_gpu.clip_x)left=s_gpu.clip_x;if(right>s_gpu.clip_x+s_gpu.clip_w)right=s_gpu.clip_x+s_gpu.clip_w;
    if(top<s_gpu.clip_y)top=s_gpu.clip_y;if(bottom>s_gpu.clip_y+s_gpu.clip_h)bottom=s_gpu.clip_y+s_gpu.clip_h;
    for(unsigned y=top;y<bottom;y++){
        uint32_t *row=(uint32_t *)(mem+base+(size_t)y*s_gpu.zeta_pitch);
        for(unsigned x=left;x<right;x++)row[x]=nf_depth_clear(row[x],s_gpu.zclear,mask);
    }
}

#ifdef NIGHTFIRE_SHADOW_LAYOUT131_DIAGNOSTIC
/* Descriptor and host program reads only; never force surface completion. */
static unsigned material132_offset_enable,material132_offset_scale,material132_offset_bias;
static void material132_state(unsigned hardware,unsigned triangles){
    NFMaterial132 d={0};d.frame=diagnostic_presents;
    d.target=dma_resolve(s_gpu.color_offset);d.surface=s_gpu.format;
    d.width=s_gpu.clip_x+s_gpu.clip_w;d.height=s_gpu.clip_y+s_gpu.clip_h;
    d.hardware=hardware;d.triangles=triangles;
    d.texture=s_gpu.tex.valid?s_gpu.tex.offset:0;d.format=s_tex_reg[1];d.filter=s_tex_reg[5];d.address=s_tex_reg[2];d.control=s_tex_reg[3];
    d.depth_enable=s_gpu.depth_enable;d.depth_func=s_gpu.depth_func;d.depth_write=s_gpu.depth_mask;
    d.blend_enable=s_gpu.blend_enable;d.blend_src=s_gpu.blend_src;d.blend_dst=s_gpu.blend_dst;
    d.offset_enable=material132_offset_enable;d.offset_scale=material132_offset_scale;d.offset_bias=material132_offset_bias;
    nf_material132_observe(&d);
}
static void shadow131_state(unsigned clear,unsigned hardware,unsigned triangles){
    unsigned size=s_gpu.clip_x+s_gpu.clip_w;
    if((size!=128 && size!=256) || s_gpu.clip_y+s_gpu.clip_h!=size || s_gpu.pitch!=size*4 || !nf_shadow131_wants(diagnostic_presents))return;
    NFShadow131State d={0};d.frame=diagnostic_presents;d.clear=clear;
    d.target=dma_resolve(s_gpu.color_offset);d.host=(const uint8_t*)xbox_GetMemoryOffset()+d.target;
    d.format=s_gpu.format;d.control=s_gpu.control0;d.width=s_gpu.clip_x+s_gpu.clip_w;d.height=s_gpu.clip_y+s_gpu.clip_h;
    d.clip_x=s_gpu.clip_x;d.clip_y=s_gpu.clip_y;d.pitch=s_gpu.pitch;
    d.indices=clear?0:s_gpu.idx_count;d.triangles=triangles;d.hardware=hardware;
    d.program_start=vertex_program.start;d.program_mode=vertex_program.mode;
    d.texture=s_gpu.tex.valid?s_gpu.tex.offset:0;d.texture_format=s_tex_reg[1];
    d.texture_filter=s_tex_reg[5];d.texture_control=s_tex_reg[3];d.texture_address=s_tex_reg[2];
    uint64_t hash=14695981039346656037ull;
    if(!clear)for(unsigned pc=vertex_program.start;pc<136 && vertex_program.valid[pc]==15;pc++){
        for(unsigned w=0;w<4;w++)for(unsigned k=0;k<4;k++){hash^=(vertex_program.code[pc][w]>>(k*8))&255;hash*=1099511628211ull;}
        if(vertex_program.code[pc][3]&1)break;
    }
    d.program_hash=clear?0:hash;nf_shadow131_event(&d);
}
#endif
static void clear_surface(uint32_t param)
{
    static int gpu_clear=-1;
    if(gpu_clear<0){const char *v=getenv("NIGHTFIRE_GPU_DEPTH_CLEAR");gpu_clear=!v || strcmp(v,"0");}
#ifdef NIGHTFIRE_COLOR_REUPLOAD109_DIAGNOSTIC
    if(nf_hw_clear111_wants()){
        NFHardwareClear111 c={0};uint32_t zbase=0;
        c.mask=param;c.color_value=s_gpu.clear_color;c.z_value=s_gpu.zclear;
        c.raw_color=s_gpu.color_offset;c.raw_depth=s_gpu.zeta_offset;
        c.resolved_color=dma_resolve(s_gpu.color_offset);
        c.depth_valid=depth_surface(&zbase);c.resolved_depth=zbase;
        c.format=s_gpu.format;c.control0=s_gpu.control0;c.clear_x=s_gpu.clear_x;c.clear_y=s_gpu.clear_y;
        c.clip_x=s_gpu.clip_x;c.clip_y=s_gpu.clip_y;c.width=s_gpu.clip_w;c.height=s_gpu.clip_h;
        c.pitch=s_gpu.pitch;c.depth_pitch=s_gpu.zeta_pitch;c.gpu_clear_enabled=gpu_clear!=0;
        uintptr_t mem=(uintptr_t)xbox_GetMemoryOffset();c.color_host=mem+c.resolved_color;
        c.depth_host=c.depth_valid?mem+zbase:0;
        nf_hw_clear111_observe(&c);
    }
#endif
    if(gpu_clear && hardware_enabled() && depth_pipeline_enabled() && param==3 &&
       s_gpu.zclear==0xffffff00u && !s_gpu.clip_x && !s_gpu.clip_y &&
       !(s_gpu.clear_x&0xffff) && !(s_gpu.clear_y&0xffff) &&
       (s_gpu.clear_x>>16)+1>=s_gpu.clip_w && (s_gpu.clear_y>>16)+1>=s_gpu.clip_h){
        uint32_t zbase;
        if(depth_surface(&zbase)){
            uint8_t *mem=(uint8_t*)xbox_GetMemoryOffset();NFHardwareState clear={0};
            clear.color=mem+dma_resolve(s_gpu.color_offset);clear.depth=mem+zbase;
            clear.width=clear.right=s_gpu.clip_w;clear.height=clear.bottom=s_gpu.clip_h;
            clear.pitch=s_gpu.pitch;clear.depth_pitch=s_gpu.zeta_pitch;
            if(nf_hw_clear_depth(&clear)){clear_depth_surface(mem,param);return;}
        }
    }
#if defined(NIGHTFIRE_DEFER_MAIN128) || defined(NIGHTFIRE_RESIDENT_MAIN130)
    int deferred_clear128=0;
    /* This preserves the CPU clear itself. Only its preceding completion may
     * be queued ahead of one disjoint color-only GPU target, within this drain. */
    if(hardware_enabled() && param==NV097_CLEAR_COLOR_MASK && s_gpu.color_offset &&
       !s_gpu.clip_x && !s_gpu.clip_y && s_gpu.clip_w==256 && s_gpu.clip_h==256 &&
       s_gpu.pitch==1024){
        uint32_t unused_depth;
        if(!depth_surface(&unused_depth)){
            uint32_t base=dma_resolve(s_gpu.color_offset);
            uint64_t end=(uint64_t)XBOX_CONTIG_BASE+xbox_ContiguousAllocatedBytes();
            if(base>=XBOX_CONTIG_BASE && (uint64_t)base+256*1024<=end){
                NFHardwareState clear={0};
                clear.color=(uint8_t*)xbox_GetMemoryOffset()+base;
#ifdef NIGHTFIRE_SWIZZLED_SHADOW131
                if(swizzled_shadow131())clear.color_layout=swizzled_shadow131();
#endif
                clear.color_only=1;clear.width=clear.right=256;
                clear.height=clear.bottom=256;clear.pitch=1024;
                deferred_clear128=nf_hw_defer_clear128(&clear);
            }
        }
    }
    if(!deferred_clear128)
#endif
        NF_GPU_COMPLETE109("clear-fallback");
    uint8_t *mem = (uint8_t *)xbox_GetMemoryOffset();
    uint32_t bpp = surface_bpp();
    uint32_t y, x;

    clear_depth_surface(mem,param);
    if (!(param & NV097_CLEAR_COLOR_MASK))
        return;                            /* depth/stencil only */
    if (!s_gpu.color_offset || !s_gpu.pitch || !s_gpu.clip_h || bpp == 0)
        return;
    {
        uint32_t base = dma_resolve(s_gpu.color_offset);
        if (surface_write_refused(base,
                                  (s_gpu.clip_y + s_gpu.clip_h) * s_gpu.pitch,
                                  "clear"))
            return;
        s_gpu.color_base = base;
    }

#ifdef NIGHTFIRE_SWIZZLED_SHADOW131
    if(swizzled_shadow131()){
        unsigned size=nf_swizzled131_size(swizzled_shadow131());
        if(!swizzled_backing131(s_gpu.color_base) || surface_write_refused(s_gpu.color_base,size*size*4,"swizzled clear"))return;
        unsigned l=s_gpu.clear_x&65535,t=s_gpu.clear_y&65535,r=(s_gpu.clear_x>>16)+1,b=(s_gpu.clear_y>>16)+1;
        if(l<s_gpu.clip_x)l=s_gpu.clip_x;if(t<s_gpu.clip_y)t=s_gpu.clip_y;
        if(r>s_gpu.clip_x+s_gpu.clip_w)r=s_gpu.clip_x+s_gpu.clip_w;if(b>s_gpu.clip_y+s_gpu.clip_h)b=s_gpu.clip_y+s_gpu.clip_h;
        uint32_t mask=((param&0x10)?0x00ff0000:0)|((param&0x20)?0x0000ff00:0)|((param&0x40)?0x000000ff:0)|((param&0x80)?0xff000000:0);
        uint32_t *dst=(uint32_t*)(mem+s_gpu.color_base);
        for(y=t;y<b;y++)for(x=l;x<r;x++){unsigned q=nf_swizzled131_index(x,y);dst[q]=(dst[q]&~mask)|(s_gpu.clear_color&mask);}
    }else
#endif
    for (y = 0; y < s_gpu.clip_h; y++) {
        uint8_t *row = mem + s_gpu.color_base
                     + (size_t)(s_gpu.clip_y + y) * s_gpu.pitch;
        if (bpp == 4) {
            uint32_t *p = (uint32_t *)row + s_gpu.clip_x;
            for (x = 0; x < s_gpu.clip_w; x++)
                p[x] = s_gpu.clear_color;
        } else if (bpp == 2) {
            /* The clear value is always given as A8R8G8B8; a 16-bit surface
             * takes the same colour reduced to 5:6:5. */
            uint16_t v = (uint16_t)(((s_gpu.clear_color >> 8) & 0xF800)
                                  | ((s_gpu.clear_color >> 5) & 0x07E0)
                                  | ((s_gpu.clear_color >> 3) & 0x001F));
            uint16_t *p = (uint16_t *)row + s_gpu.clip_x;
            for (x = 0; x < s_gpu.clip_w; x++)
                p[x] = v;
        }
    }
#ifdef NIGHTFIRE_SHADOW_LAYOUT131_DIAGNOSTIC
    shadow131_state(param,0,0);
#endif
    s_gpu.clears++;
    /* Progress markers, interleaved with everything else in the log. The
     * summary says drawing stopped; only a marker next to the surrounding
     * activity says what the title was doing when it stopped. */
    if ((s_gpu.clears % 100) == 0)
        fprintf(stderr, "  [GPU] clear #%u\n", s_gpu.clears);
    /* Distinct clear colours actually used. "Cleared to black" and "the clear
     * never ran" look identical in the framebuffer, and only one of them is a
     * bug -- so record what was asked for, not just how often. */
    {
        static uint32_t seen[8];
        static int n;
        int i;
        for (i = 0; i < n; i++)
            if (seen[i] == s_gpu.clear_color) break;
        if (i == n && n < 8) {
            seen[n++] = s_gpu.clear_color;
            fprintf(stderr, "  [GPU] clear colour 0x%08X -> surface 0x%08X"
                            " (%ubpp)\n",
                    s_gpu.clear_color, s_gpu.color_offset, surface_bpp());
        }
    }

    /* Prove the pixel path end to end, independent of whether the title has
     * given us any geometry yet.
     *
     * "Nothing on screen" has three very different causes -- the surface
     * address or pitch is wrong, the rasteriser is broken, or the title's
     * vertex buffers are empty -- and they are indistinguishable from a black
     * window. RECOMP_RASTER_TEST draws one known triangle into the surface
     * just cleared, so a visible triangle rules out the first two and leaves
     * only the third. On Wreckless it is the third: attribute 0 decodes
     * correctly (float, size 2, stride 16) and the buffer it points at stays
     * zero.
     *
     * ponytail: bring-up aid, not a feature. It costs one branch per clear. */
    if (getenv("RECOMP_RASTER_TEST")) {
        static int announced;
        /* Every clear, not once: the title clears each frame and double-buffers,
         * so a triangle drawn a single time is erased before anyone sees it. */
        if (s_gpu.clip_w && s_gpu.clip_h) {
            float a[2], b[2], c[2];
            a[0] = s_gpu.clip_w * 0.5f; a[1] = s_gpu.clip_h * 0.15f;
            b[0] = s_gpu.clip_w * 0.85f; b[1] = s_gpu.clip_h * 0.85f;
            c[0] = s_gpu.clip_w * 0.15f; c[1] = s_gpu.clip_h * 0.85f;
            raster_triangle(a, b, c, 0xFFFF00FFu, NULL);  /* magenta: never a clear colour */
            if (announced++ == 0)
            fprintf(stderr, "  [GPU] raster self-test: triangle (%.0f,%.0f)"
                            " (%.0f,%.0f) (%.0f,%.0f) into 0x%08X %ubpp\n",
                    a[0], a[1], b[0], b[1], c[0], c[1],
                    s_gpu.color_offset, surface_bpp());
        }
    }

    /* Show the surface actually being drawn into. A title that double-buffers
     * renders into the back buffer, so following AvSetDisplayMode's address
     * would show the one nothing is writing. */
    /* The window has to read where the pixels actually are, which is the
     * resolved address rather than the DMA-object offset. */
    xbox_FramebufferWindowSet(dma_resolve(s_gpu.color_offset), s_gpu.pitch);

    /* And open the window, rather than waiting for AvSetDisplayMode to do it.
     *
     * That was the only caller, so a title which draws before setting a display
     * mode -- or never sets one at all -- got no window however much it
     * rendered. The Xbox Dashboard clears a 1280x960 surface at 0x00088000 on
     * its first frame and had not called AvSetDisplayMode by then, so
     * RECOMP_FB_WINDOW=1 was set, the executor knew the address and the pitch,
     * and nothing appeared.
     *
     * Here is the better trigger anyway: this runs when a surface address is
     * known to be real, because a clear just used it. Idempotent and gated on
     * RECOMP_FB_WINDOW, so the cost is one interlocked compare per clear. */
    xbox_FramebufferWindowStart();
}


/* ── Rasteriser ──────────────────────────────────────────────────────────
 *
 * Fills triangles straight into the guest framebuffer, the same memory
 * clear_surface() writes and the framebuffer window already shows. That is the
 * whole reason it is done on the CPU rather than through D3D: nothing new has
 * to be plumbed for the result to be visible.
 *
 * ponytail: flat-shaded, no depth buffer, no texturing, no perspective
 * correction, and only batches whose attribute 0 is already in screen space.
 * A title running a vertex program hands over object-space positions that mean
 * nothing without executing the program, so those batches are counted and
 * skipped rather than drawn somewhere wrong. Upgrade path is the D3D11
 * translator in src/nv2a/nv2a_pgraph_d3d11.c once vertex programs are
 * translated; this exists to get the first geometry on screen for every title,
 * which in practice is UI, HUD and 2D overlays -- all pre-transformed.
 */

/* One texel, in the title's own format.
 *
 * The codes are the NV097 colour field, which is the Xbox D3DFMT_ enum --
 * src/d3d/d3d8_xbox.h is the table, and it is the table to check against
 * rather than recollection: 0x1E is LIN_X8R8G8B8 and not, as this first read
 * it, a byte-reversed BGRA. Getting that one wrong turned an opaque black
 * render target into a screen of pure blue, which is the kind of wrong that
 * looks like content.
 *
 * Only the linear (LIN_) formats are read. A swizzled texture stores its
 * texels in Morton order rather than in rows, so reading one as if it had a
 * pitch does not give a slightly wrong colour, it gives a different image --
 * and inventing that image is exactly what this is not for. An unsupported
 * format samples nothing and the caller keeps the vertex colour, which is
 * visibly wrong rather than quietly wrong.
 *
 * ponytail: nearest texel, no filtering, whatever SET_TEXTURE_FILTER asked
 * for. Bilinear when a title's output actually depends on it.
 */
/* Off the edge of the texture, the way the title asked for.
 *
 * Refusing to sample instead is not neutral: it hands the caller back the
 * vertex colour, so a pass whose coordinates reach the last texel by half a
 * texel gets a bright line down the edge of the screen. The dashboard's
 * resolve does exactly that -- its last column and last row, 1119 pixels of
 * white on a black frame, from a rounding step at the boundary.
 */
static uint32_t wrap_coord(uint32_t c, uint32_t size, uint32_t mode)
{
    if (!size)
        return 0;
    if (mode == 1)                         /* wrap */
        return !(size & (size-1)) ? c & (size-1) : c % size;
    return c >= size ? size - 1 : c;       /* clamp, and everything else */
}

static uint32_t expand(uint32_t v, uint32_t bits)
{
    return d3d8_expand_channel(v, bits);
}

/* The linear format that decodes the same texels as a swizzled one.
 *
 * Swizzling changes where a texel lives, not what it says: A8R8G8B8 (0x06) and
 * LIN_A8R8G8B8 (0x12) are the same four bytes in the same order. So the whole
 * difference is the address calculation, and one of those lets every format
 * below serve both. Pairs read off the table in d3d8_xbox.h rather than
 * recalled -- the comment above this one is about getting exactly that wrong. */
static uint32_t linear_twin(uint32_t fmt)
{
    switch (fmt) {
    case 0x00: return 0x13;                /* L8        -> LIN_L8        */
    case 0x02: return 0x10;                /* A1R5G5B5  -> LIN_A1R5G5B5  */
    case 0x03: return 0x1C;                /* X1R5G5B5  -> LIN_X1R5G5B5  */
    case 0x04: return 0x1D;                /* A4R4G4B4  -> LIN_A4R4G4B4  */
    case 0x05: return 0x11;                /* R5G6B5    -> LIN_R5G6B5    */
    case 0x06: return 0x12;                /* A8R8G8B8  -> LIN_A8R8G8B8  */
    case 0x07: return 0x1E;                /* X8R8G8B8  -> LIN_X8R8G8B8  */
    case 0x19: return 0x1F;                /* A8        -> LIN_A8        */
    default:   return fmt;                 /* already linear, or unhandled */
    }
}

static int sample_texture(uint32_t u, uint32_t v, uint32_t *argb);
static int sample_texture_at(const uint8_t *mem, uint32_t u, uint32_t v, uint32_t *argb)
{
    const uint8_t *p;
    uint32_t fmt;

    if (!s_gpu.tex.valid)
        return 0;
    u = wrap_coord(u, s_gpu.tex.width,  s_gpu.tex.addr_u);
    v = wrap_coord(v, s_gpu.tex.height, s_gpu.tex.addr_v);

    fmt = s_gpu.tex.color;
    if (d3d8_format_dxt_block_bytes(fmt)) {
        if(world_raster && texture_cache_enabled())
            return nf_texture_cached(mem+s_gpu.tex.offset,fmt,u,v,s_gpu.tex.width,argb);
        return d3d8_dxt_decode_texel(mem + s_gpu.tex.offset, fmt, u, v,
                                     s_gpu.tex.width, argb);
    }
    if (d3d8_format_is_swizzled(fmt)) {
        /* Morton order: a texel's index is interleaved from x and y instead of
         * v*pitch + u, so index from the base of the image. The switch below
         * casts to each format's own width, which makes that index a texel
         * index for every one of them. */
        fmt = linear_twin(fmt);
        p = mem + s_gpu.tex.offset;
        u = swizzle_offset(u, v, s_gpu.tex.width, s_gpu.tex.height);
    } else {
        p = mem + s_gpu.tex.offset + (size_t)v * s_gpu.tex.pitch;
    }

    switch (fmt) {

    case 0x24:                                      /* YUY2: Y0 U Y1 V */
    case 0x25: {                                    /* UYVY: U Y0 V Y1 */
        /* Same BT.601 conversion as pinned d3d8_resources.c. Read the
         * shared chroma pair, respecting the submitted linear pitch. */
        uint32_t pair = u & ~1u;
        int yo = fmt == 0x24 ? 0 : 1;
        int c, cu, cv, r, g, b;
        if (pair + 1 >= s_gpu.tex.width || (size_t)(pair + 2) * 2 > s_gpu.tex.pitch)
            return 0;
        c = p[u * 2 + yo] - 16;
        cu = p[pair * 2 + 1 - yo] - 128;
        cv = p[pair * 2 + 3 - yo] - 128;
        r = (298 * c + 409 * cv + 128) >> 8;
        g = (298 * c - 100 * cu - 208 * cv + 128) >> 8;
        b = (298 * c + 516 * cu + 128) >> 8;
        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;
        *argb = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        return 1;
    }

    /* 32-bit, alpha-red-green-blue in the dword. */
    case 0x12:                                      /* LIN_A8R8G8B8 */
        *argb = ((const uint32_t *)p)[u];
        return 1;
    case 0x1E:                                      /* LIN_X8R8G8B8 */
        *argb = ((const uint32_t *)p)[u] | 0xFF000000u;
        return 1;

    /* 32-bit, other channel orders. The name gives the byte order from the
     * top of the dword down, so each is a permutation of the same four. */
    case 0x3F: {                                    /* LIN_A8B8G8R8 */
        uint32_t t = ((const uint32_t *)p)[u];
        *argb = (t & 0xFF00FF00u) | ((t & 0xFF) << 16) | ((t >> 16) & 0xFF);
        return 1;
    }
    case 0x40: {                                    /* LIN_B8G8R8A8 */
        uint32_t t = ((const uint32_t *)p)[u];
        *argb = ((t & 0xFFu) << 24)                 /* A, from the bottom */
              | (((t >>  8) & 0xFFu) << 16)         /* R */
              | (((t >> 16) & 0xFFu) <<  8)         /* G */
              |  ((t >> 24) & 0xFFu);               /* B, from the top */
        return 1;
    }
    case 0x41: {                                    /* LIN_R8G8B8A8 */
        uint32_t t = ((const uint32_t *)p)[u];
        *argb = ((t & 0xFFu) << 24) | (t >> 8);
        return 1;
    }

    /* 16-bit. */
    case 0x10: {                                    /* LIN_A1R5G5B5 */
        uint32_t t = ((const uint16_t *)p)[u];
        *argb = ((t & 0x8000u) ? 0xFF000000u : 0u)
              | (expand((t >> 10) & 0x1F, 5) << 16)
              | (expand((t >>  5) & 0x1F, 5) <<  8)
              |  expand( t        & 0x1F, 5);
        return 1;
    }
    case 0x1C: {                                    /* LIN_X1R5G5B5 */
        uint32_t t = ((const uint16_t *)p)[u];
        *argb = 0xFF000000u
              | (expand((t >> 10) & 0x1F, 5) << 16)
              | (expand((t >>  5) & 0x1F, 5) <<  8)
              |  expand( t        & 0x1F, 5);
        return 1;
    }
    case 0x11: {                                    /* LIN_R5G6B5 */
        uint32_t t = ((const uint16_t *)p)[u];
        *argb = 0xFF000000u
              | (expand((t >> 11) & 0x1F, 5) << 16)
              | (expand((t >>  5) & 0x3F, 6) <<  8)
              |  expand( t        & 0x1F, 5);
        return 1;
    }
    case 0x1D: {                                    /* LIN_A4R4G4B4 */
        uint32_t t = ((const uint16_t *)p)[u];
        *argb = (expand((t >> 12) & 0x0F, 4) << 24)
              | (expand((t >>  8) & 0x0F, 4) << 16)
              | (expand((t >>  4) & 0x0F, 4) <<  8)
              |  expand( t        & 0x0F, 4);
        return 1;
    }

    /* 8-bit. */
    case 0x13: {                                    /* LIN_L8 */
        uint32_t t = p[u];
        *argb = 0xFF000000u | (t << 16) | (t << 8) | t;
        return 1;
    }
    case 0x1F:                                      /* LIN_A8 */
        *argb = ((uint32_t)p[u] << 24) | 0x00FFFFFFu;
        return 1;

    default:
        return 0;
    }
}

/* Write a bound texture out as a BMP, through the sampler rather than around it.
 *
 * "Which texture is this" is not answerable from an address and a format, and
 * it is the question behind most of the ones that matter -- is that a font
 * page or an icon atlas, did the swizzle decode, is the alpha inverted. Going
 * through sample_texture means the file shows exactly what the rasteriser
 * sees, so a decode bug appears here rather than only as a wrong-looking
 * triangle.
 *
 * ponytail: RGB only, alpha dropped. A glyph page is alpha and would come out
 * black, so alpha is composited onto mid-grey to stay legible; that is a
 * viewing choice, not a decode. One file per distinct texture, first use only.
 */
static void dump_texture_bmp(uint32_t seq)
{
    const char *prefix = getenv("RECOMP_TEX_DUMP");
    uint32_t w = s_gpu.tex.width, h = s_gpu.tex.height, x, y;
    uint32_t row_bytes, pad, filesz;
    uint8_t hdr[54];
    char path[512];
    FILE *f;

    if (!prefix || !w || !h || w > 4096 || h > 4096)
        return;
    if ((s_gpu.tex.color == 0x24 || s_gpu.tex.color == 0x25)
        && s_gpu.tex.pitch >= w * 2 && s_gpu.tex.pitch <= 16384) {
        snprintf(path, sizeof path, "%s%02u_%08X_fmt%02X.raw",
                 prefix, seq, s_gpu.tex.offset, s_gpu.tex.color);
        f = fopen(path, "wb");
        if (f) {
            fwrite((const uint8_t *)xbox_GetMemoryOffset() + s_gpu.tex.offset,
                   s_gpu.tex.pitch, h, f);
            fclose(f);
        }
        /* Compare separate runtime RAM arenas; diagnostic only, never sampled. */
        if (s_gpu.tex.offset >= 0x80000000u && s_gpu.tex.offset < 0x84000000u) {
            snprintf(path, sizeof path, "%s%02u_%08X_low.raw",
                     prefix, seq, s_gpu.tex.offset);
            f = fopen(path, "wb");
            if (f) {
                fwrite((const uint8_t *)xbox_GetMemoryOffset() + (s_gpu.tex.offset - 0x80000000u),
                       s_gpu.tex.pitch, h, f);
                fclose(f);
            }
        }
    }
    row_bytes = w * 3;
    pad = (4 - (row_bytes & 3)) & 3;
    filesz = 54 + (row_bytes + pad) * h;

    snprintf(path, sizeof path, "%s%02u_%08X_fmt%02X.bmp",
             prefix, seq, s_gpu.tex.offset, s_gpu.tex.color);
    f = fopen(path, "wb");
    if (!f)
        return;
    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B'; hdr[1] = 'M';
    memcpy(hdr + 2, &filesz, 4);
    hdr[10] = 54; hdr[14] = 40;
    memcpy(hdr + 18, &w, 4);
    memcpy(hdr + 22, &h, 4);
    hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, sizeof hdr, f);

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            uint32_t argb = 0, a;
            uint8_t px[3];
            if (!sample_texture(x, h - 1 - y, &argb))
                argb = 0;
            a = (argb >> 24) & 0xFFu;
            /* over mid-grey, so an alpha-only page is visible either way */
            px[0] = (uint8_t)(((argb & 0xFFu) * a + 128u * (255u - a)) / 255u);
            px[1] = (uint8_t)((((argb >> 8) & 0xFFu) * a + 128u * (255u - a)) / 255u);
            px[2] = (uint8_t)((((argb >> 16) & 0xFFu) * a + 128u * (255u - a)) / 255u);
            fwrite(px, 1, 3, f);
        }
        if (pad) {
            static const uint8_t zero[3] = {0, 0, 0};
            fwrite(zero, 1, pad, f);
        }
    }
    fclose(f);
    fprintf(stderr, "  [TEXDUMP] %s (%ux%u fmt 0x%02X)\n",
            path, w, h, s_gpu.tex.color);
    fflush(stderr);
}

static int alpha_pass(uint32_t alpha)
{
    if(!s_gpu.alpha_test) return 1;
    uint32_t ref=s_gpu.alpha_ref & 255;
    switch(s_gpu.alpha_func) {
    case 0x200:return 0; case 0x201:return alpha<ref; case 0x202:return alpha==ref;
    case 0x203:return alpha<=ref; case 0x204:return alpha>ref; case 0x205:return alpha!=ref;
    case 0x206:return alpha>=ref; case 0x207:return 1; default:return 0;
    }
}

static int world_texture_coord(float coordinate,uint32_t size,uint32_t mode,uint32_t *result)
{
    if(!size || !isfinite(coordinate))return 0;
    float c=floorf(coordinate);
    if(mode==1){
        if(!(size&(size-1)) && c>=-2147483648.0f && c<=2147483520.0f){
            *result=(uint32_t)(int32_t)c&(size-1);return 1;
        }
        c=fmodf(c,(float)size);if(c<0)c+=(float)size;
    }
    else c=fminf((float)(size-1),fmaxf(0,c));
    *result=(uint32_t)c;return 1;
}

static int sample_texture(uint32_t u, uint32_t v, uint32_t *argb)
{
    return sample_texture_at((const uint8_t *)xbox_GetMemoryOffset(),u,v,argb);
}
static uint32_t blend_factor(uint32_t mode,uint32_t src,uint32_t dst,unsigned shift)
{
    switch(mode) {
    case 0:return 0; case 1:return 255;
    case 0x300:return (src>>shift)&255; case 0x301:return 255-((src>>shift)&255);
    case 0x302:return src>>24; case 0x303:return 255-(src>>24);
    case 0x304:return dst>>24; case 0x305:return 255-(dst>>24);
    case 0x306:return (dst>>shift)&255; case 0x307:return 255-((dst>>shift)&255);
    case 0x308:return shift==24 ? 255 : ((src>>24)<255-(dst>>24) ? src>>24 : 255-(dst>>24));
    case 0x8001:return (s_gpu.blend_color>>shift)&255;
    case 0x8002:return 255-((s_gpu.blend_color>>shift)&255);
    case 0x8003:return s_gpu.blend_color>>24;
    case 0x8004:return 255-(s_gpu.blend_color>>24);
    default:return 0;
    }
}
static uint32_t blend_pixel(uint32_t src,uint32_t dst)
{
    if(!s_gpu.blend_enable) return src;
    /* Exact endpoints of the existing integer blend, common for movie quads. */
    if(s_gpu.blend_equation==0x8006) {
        if(s_gpu.blend_src==1 && s_gpu.blend_dst==0) return src;
        if(s_gpu.blend_src==0x302 && s_gpu.blend_dst==0x303) {
            if((src>>24)==255) return src;
            if((src>>24)==0) return dst;
        }
    }
    uint32_t result=0;
    for(unsigned shift=0;shift<32;shift+=8) {
        int sc=(src>>shift)&255, dc=(dst>>shift)&255;
        int a=sc*blend_factor(s_gpu.blend_src,src,dst,shift);
        int b=dc*blend_factor(s_gpu.blend_dst,src,dst,shift), value;
        switch(s_gpu.blend_equation) {
        case 0x8006:value=(a+b+127)/255;break;
        case 0x800a:value=(a-b+127)/255;break;
        case 0x800b:value=(b-a+127)/255;break;
        case 0x8007:value=sc<dc ? sc:dc;break;
        case 0x8008:value=sc>dc ? sc:dc;break;
        default:value=sc;break;
        }
        if(value<0)value=0; if(value>255)value=255;
        result|=(uint32_t)value<<shift;
    }
    return result;
}
static void put_pixel(uint8_t *mem, uint32_t base, uint32_t bpp, int x, int y, uint32_t argb)
{
    uint8_t *row;

    if (x < (int)s_gpu.clip_x || x >= (int)(s_gpu.clip_x + s_gpu.clip_w))
        return;
    if (y < (int)s_gpu.clip_y || y >= (int)(s_gpu.clip_y + s_gpu.clip_h))
        return;
    if(!alpha_pass(argb>>24)) return;
    /* Same reason the clear checks: a rasterised triangle writes guest memory
     * too, and a surface address that lands on the image is no safer one pixel
     * at a time than 4.9 MB at once. */
    if (surface_hits_image(base,
                           (s_gpu.clip_y + s_gpu.clip_h) * s_gpu.pitch))
        return;
    if(world_raster && depth_pending_address && s_gpu.depth_mask){
        uint32_t *z=(uint32_t *)(mem+depth_pending_address);
        *z=nf_depth_write(*z,depth_pending_value);
    }
#ifdef NIGHTFIRE_SWIZZLED_SHADOW131
    if(swizzled_shadow131()){
        unsigned size=nf_swizzled131_size(swizzled_shadow131());
        if(!swizzled_backing131(base) || surface_hits_image(base,size*size*4))return;
        uint32_t *dst=(uint32_t*)(mem+base)+nf_swizzled131_index((unsigned)x,(unsigned)y);
        *dst=blend_pixel(argb,*dst);return;
    }
#endif
    row = mem + base + (size_t)y * s_gpu.pitch;
    if (bpp == 4) {
        argb=blend_pixel(argb,((uint32_t *)row)[x]);
        ((uint32_t *)row)[x] = argb;
    } else if (bpp == 2) {
        uint32_t d=((uint16_t *)row)[x];
        argb=blend_pixel(argb,0xff000000u | (expand(d>>11,5)<<16)
                        | (expand((d>>5)&63,6)<<8) | expand(d&31,5));
        ((uint16_t *)row)[x] = (uint16_t)(((argb >> 8) & 0xF800)
                                        | ((argb >> 5) & 0x07E0)
                                        | ((argb >> 3) & 0x001F));
    }
}

/* Half-space fill. Barycentric edge functions rather than scanline slopes:
 * the same test decides both windings, so a title that emits clockwise
 * triangles does not silently render nothing. */
static void raster_triangle(const float a[2], const float b[2],
                            const float c[2], uint32_t argb,
                            const float uv[3][2])
{
    uint8_t *mem = (uint8_t *)xbox_GetMemoryOffset();
    uint32_t bpp = surface_bpp();
    float area;
    int minx, maxx, miny, maxy, x, y;
    int textured = uv && s_gpu.tex.valid;
    /* Command state and the allocation mapping stay fixed during a triangle. */
    uint32_t base = dma_resolve(s_gpu.color_offset);
    uint32_t zbase=0;
    int use_depth=world_raster && depth_pipeline_enabled() && s_gpu.depth_enable;
    depth_pending_address=0;
    if(use_depth && !depth_surface(&zbase)){
        static unsigned failures;if(failures++<4)fprintf(stderr,"[DEPTH] unsupported or out-of-allocation surface format=%08X offset=%08X pitch=%u\n",s_gpu.format,s_gpu.zeta_offset,s_gpu.zeta_pitch);
        return;
    }

    if (bpp != 4 && bpp != 2)
        return;

    area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    if (area == 0.0f || !isfinite(area))
        return;                            /* degenerate */
    if(world_raster && s_gpu.cull_enable){
        /* This framebuffer has downward-increasing Y: a positive signed
         * area is clockwise on screen (NV097_FRONT_FACE_CW = 0x900). */
        int front=s_gpu.front_face==0x901 ? area<0 : area>0;
        if(s_gpu.cull_face==0x408 || (s_gpu.cull_face==0x404 && front) ||
           (s_gpu.cull_face==0x405 && !front)){s_gpu.culled++;return;}
    }

    minx = (int)floorf(fminf((float)(s_gpu.clip_x+s_gpu.clip_w),fmaxf((float)s_gpu.clip_x,fminf(a[0], fminf(b[0], c[0])))));
    maxx = (int)ceilf (fminf((float)(s_gpu.clip_x+s_gpu.clip_w),fmaxf((float)s_gpu.clip_x,fmaxf(a[0], fmaxf(b[0], c[0])))));
    miny = (int)floorf(fminf((float)(s_gpu.clip_y+s_gpu.clip_h),fmaxf((float)s_gpu.clip_y,fminf(a[1], fminf(b[1], c[1])))));
    maxy = (int)ceilf (fminf((float)(s_gpu.clip_y+s_gpu.clip_h),fmaxf((float)s_gpu.clip_y,fmaxf(a[1], fmaxf(b[1], c[1])))));

    if (minx < (int)s_gpu.clip_x) minx = (int)s_gpu.clip_x;
    if (miny < (int)s_gpu.clip_y) miny = (int)s_gpu.clip_y;
    if (maxx > (int)(s_gpu.clip_x + s_gpu.clip_w)) maxx = (int)(s_gpu.clip_x + s_gpu.clip_w);
    if (maxy > (int)(s_gpu.clip_y + s_gpu.clip_h)) maxy = (int)(s_gpu.clip_y + s_gpu.clip_h);
    if (minx >= maxx || miny >= maxy) {
        s_gpu.tris_skipped_offscreen++;
        return;
    }

    float dxq=0,dyq=0,dxu=0,dyu=0,dxv=0,dyv=0;
    if(world_raster && textured && world_filter.levels){
        const float *p[3]={a,b,c};
        for(unsigned i=0;i<3;i++){
            unsigned j=(i+1)%3,k=(i+2)%3;
            float dx=(p[j][1]-p[k][1])/p[i][3],dy=(p[k][0]-p[j][0])/p[i][3];
            dxq+=dx;dyq+=dy;dxu+=dx*uv[i][0];dyu+=dy*uv[i][0];dxv+=dx*uv[i][1];dyv+=dy*uv[i][1];
        }
    }
    for (y = miny; y < maxy; y++) {
        for (x = minx; x < maxx; x++) {
            float px = (float)x + 0.5f, py = (float)y + 0.5f;
            float w0 = (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]);
            float w1 = (c[0] - b[0]) * (py - b[1]) - (c[1] - b[1]) * (px - b[0]);
            float w2 = (a[0] - c[0]) * (py - c[1]) - (a[1] - c[1]) * (px - c[0]);
            if (!((w0 >= 0 && w1 >= 0 && w2 >= 0)
               || (w0 <= 0 && w1 <= 0 && w2 <= 0)))
                continue;
            if(use_depth){
                uint32_t z;
                if(!nf_depth_encode((w1*a[2]+w2*b[2]+w0*c[2])/area,&z))continue;
                uint32_t addr=zbase+(uint32_t)y*s_gpu.zeta_pitch+(uint32_t)x*4;
                if(!nf_depth_compare(s_gpu.depth_func,z,(*(uint32_t *)(mem+addr))>>8))continue;
                depth_pending_address=addr;depth_pending_value=z;
            }
            uint32_t diffuse=argb;float fog=1;
            if(world_raster && world_varyings){
                float weights[3]={w1/a[3],w2/b[3],w0/c[3]},q=weights[0]+weights[1]+weights[2];
                if(!isfinite(q) || q==0)continue;
                diffuse=0;fog=0;
                for(unsigned k=0;k<3;k++){weights[k]/=q;fog+=weights[k]*world_fog[k];}
                for(unsigned shift=0;shift<32;shift+=8){
                    float channel=0;for(unsigned k=0;k<3;k++)channel+=weights[k]*((world_colors[k]>>shift)&255);
                    diffuse|=(uint32_t)fminf(255,fmaxf(0,channel+0.5f))<<shift;
                }
            }
            if (textured) {
                /* Barycentric, straight from the edge functions already
                 * computed: w1 is the area opposite a, w2 opposite b, w0
                 * opposite c, and the three sum to the whole triangle.
                 *
                 * UI interpolation is affine; the experimental world path
                 * below uses reciprocal W for perspective-correct UVs. */
                uint32_t texel;
                float su = (w1 * uv[0][0] + w2 * uv[1][0] + w0 * uv[2][0]) / area;
                float sv = (w1 * uv[0][1] + w2 * uv[1][1] + w0 * uv[2][1]) / area;
                if(world_raster){float q=w1/a[3]+w2/b[3]+w0/c[3];
                    if(!isfinite(q) || q==0)continue;
                    su=(w1*uv[0][0]/a[3]+w2*uv[1][0]/b[3]+w0*uv[2][0]/c[3])/q;
                    sv=(w1*uv[0][1]/a[3]+w2*uv[1][1]/b[3]+w0*uv[2][1]/c[3])/q;}
                if(world_raster && world_filter.levels){
                    float q=w1/a[3]+w2/b[3]+w0/c[3];
                    float lod=nf_filter_lod((dxu-su*dxq)/q,(dxv-sv*dxq)/q,(dyu-su*dyq)/q,(dyv-sv*dyq)/q);
                    if(nf_filter_sample(&world_filter,su,sv,lod,&texel)){
                        put_pixel(mem,base,bpp,x,y,fog_pixel(shade_texel(texel,diffuse),fog));continue;
                    }
                }
                uint32_t tu,tv;
                if(world_raster){
                    if(!world_texture_coord(su,s_gpu.tex.width,s_gpu.tex.addr_u,&tu) ||
                       !world_texture_coord(sv,s_gpu.tex.height,s_gpu.tex.addr_v,&tv))continue;
                }else{
                    if (su < 0.0f) su = 0.0f;
                    if (sv < 0.0f) sv = 0.0f;
                    tu=(uint32_t)su;tv=(uint32_t)sv;
                }
                if (sample_texture_at(mem, tu, tv, &texel)) {
                    put_pixel(mem, base, bpp, x, y, fog_pixel(shade_texel(texel,diffuse),fog));
                    continue;
                }
            }
            put_pixel(mem, base, bpp, x, y, fog_pixel(diffuse,fog));
        }
    }
    last_draw_address=base;last_draw_pitch=s_gpu.pitch;
    s_gpu.tris_drawn++;
}

/* Attribute 3 is diffuse colour in every NV2A layout that sets one. Absent it,
 * white -- a visible wrong colour beats an invisible correct one during
 * bring-up. */
/* Which attribute carries the colour.
 *
 * Slot 3 is diffuse by convention and titles that follow it are read straight
 * from there. Half-Life 2 does not: its vertex is position, colour, texcoord
 * at stride 24, and the colour arrives in slot 5. So fall back to the format
 * rather than the slot number -- D3DCOLOR is the one attribute type that is
 * only ever a colour, which makes it a stronger signal than the convention. */
static const VertexAttr *color_attr(void)
{
    uint32_t a;

    if (s_gpu.attr[3].offset && s_gpu.attr[3].stride
        && s_gpu.attr[3].type == 0 && s_gpu.attr[3].size == 4)
        return &s_gpu.attr[3];
    for (a = 0; a < NV_VERTEX_ATTRS; a++)
        if (s_gpu.attr[a].type == 0 && s_gpu.attr[a].size == 4
                && s_gpu.attr[a].offset && s_gpu.attr[a].stride)
            return &s_gpu.attr[a];
    return &s_gpu.attr[3];
}

/* Attribute 9 is texture coordinate 0 in the NV2A vertex layout, the same way
 * 0 is position and 3 is diffuse -- for a title that follows the convention.
 *
 * Half-Life 2 does not, in either place. Its menu and HUD vertex is position,
 * colour, texcoord at stride 24, with the colour in slot 5 and the texcoords
 * in slot 7, so reading slot 9 found nothing and every batch drew untextured.
 * That is invisible rather than wrong-looking: the menu paints a full-screen
 * quad and then draws its text over it, and with no sampling both come out
 * white, so the screen is blank white and nothing suggests the text was ever
 * drawn.
 *
 * Falling back to the format works because the three attributes of such a
 * vertex are distinguishable: position is float3, colour is D3DCOLOR, and a
 * float2 is a texture coordinate and nothing else.
 *
 * ponytail: takes the first float2 it finds, so a title with two texcoord sets
 * gets stage 0's -- which is what this single-texture rasteriser samples
 * anyway. Multi-texture wants the D3D11 translator, not another heuristic. */
static const VertexAttr *texcoord_attr(void)
{
    uint32_t a;

    if (s_gpu.attr[9].offset && s_gpu.attr[9].stride)
        return &s_gpu.attr[9];
    for (a = 0; a < NV_VERTEX_ATTRS; a++)
        if (s_gpu.attr[a].type == 2 && s_gpu.attr[a].size == 2
                && s_gpu.attr[a].offset && s_gpu.attr[a].stride)
            return &s_gpu.attr[a];
    return &s_gpu.attr[9];
}

/* Texel coordinates, whichever convention the title used.
 *
 * The two are not interchangeable and the format decides which is in force: a
 * swizzled texture is addressed in [0,1], a linear one in texels. Both are
 * scaled to texels here so that everything downstream -- the barycentric
 * interpolation and the sampler -- works in one unit.
 *
 * This mattered the moment swizzled formats became samplable. Normalised
 * coordinates truncated to a texel index land on texel 0 for any coordinate
 * below 1.0, so a whole quad sampled a single texel and came out flat: the
 * background painted one near-black colour, which looks like a texture that
 * decoded wrong rather than one that was never indexed. */
static int fetch_texcoord(uint32_t index, float out[2])
{
    float t[4];

    if (!fetch_attr(texcoord_attr(), index, t))
        return 0;
    out[0] = t[0];
    out[1] = t[1];
    if (tex_size_from_format(s_gpu.tex.color)) {
        out[0] *= (float)s_gpu.tex.width;
        out[1] *= (float)s_gpu.tex.height;
    }
    return 1;
}

static uint32_t vertex_color(uint32_t index)
{
    float c[4];

    if (!fetch_attr(color_attr(), index, c))
        return 0xFFFFFFFFu;
    return ((uint32_t)(c[3] * 255.0f) << 24)
         | ((uint32_t)(c[0] * 255.0f) << 16)
         | ((uint32_t)(c[1] * 255.0f) <<  8)
         |  (uint32_t)(c[2] * 255.0f);
}

/* An untransformed batch drawn as if it were screen space smears a few pixels
 * into the corner, so the batch has to be classified before it is rasterised.
 *
 * This used to demand that every vertex land inside the surface, which is a
 * different question and the wrong one: geometry that extends past the
 * viewport is ordinary, and clipping it is raster_triangle's job (it clamps
 * its span to the clip rect). The dashboard is exactly the case that exposed
 * it -- a full-screen pass drawn as one oversized triangle, vertices at
 * (-0.5,-0.5), (2*w,-0.5), (-0.5,2*h), all correct and all rejected.
 *
 * What actually separates the two is scale. Object-space positions are model
 * units, a handful either side of the origin; screen-space ones are measured
 * in pixels of a surface hundreds of pixels wide. So: the batch has to be able
 * to touch the surface at all, and it has to be bigger than object space.
 *
 * ponytail: a genuinely tiny screen-space sprite reads as object space and is
 * skipped. It is counted as skipped rather than silently dropped, and the
 * unambiguous answer needs the vertex-program state, which is not tracked yet.
 */
#define OBJECT_SPACE_SPAN 8.0f

static int batch_is_screen_space(void)
{
    float p[4], lo_x, hi_x, lo_y, hi_y;
    uint32_t i;

    if (!s_gpu.clip_w || !s_gpu.clip_h || !s_gpu.idx_count)
        return 0;
    if (!fetch_attr(&s_gpu.attr[0], s_gpu.idx[0], p))
        return 0;
    lo_x = hi_x = p[0];
    lo_y = hi_y = p[1];
    for (i = 1; i < s_gpu.idx_count; i++) {
        if (!fetch_attr(&s_gpu.attr[0], s_gpu.idx[i], p))
            return 0;
        if (p[0] < lo_x) lo_x = p[0];
        if (p[0] > hi_x) hi_x = p[0];
        if (p[1] < lo_y) lo_y = p[1];
        if (p[1] > hi_y) hi_y = p[1];
    }

    /* Entirely off the surface: nothing to draw under either reading. */
    if (hi_x < (float)s_gpu.clip_x
     || lo_x > (float)(s_gpu.clip_x + s_gpu.clip_w)
     || hi_y < (float)s_gpu.clip_y
     || lo_y > (float)(s_gpu.clip_y + s_gpu.clip_h))
        return 0;

    /* Small enough to be model units rather than pixels. */
    if (hi_x - lo_x < OBJECT_SPACE_SPAN && hi_y - lo_y < OBJECT_SPACE_SPAN)
        return 0;

    return 1;
}

/* NV097 primitive types that are triangles under some winding. */
/* Values verified against pinned nv2a_regs.h, NV097_SET_BEGIN_END_OP_*. */
#define NV_PRIM_TRIANGLES      5
#define NV_PRIM_TRIANGLE_STRIP 6
#define NV_PRIM_TRIANGLE_FAN   7
#define NV_PRIM_QUADS          8
#define NV_PRIM_QUAD_STRIP     9

/* How many post-draw captures to keep: enough to see whether the geometry
 * is stable from frame to frame, few enough not to fill a directory. */
#define FB_DUMP_AFTER_DRAW 8
static int s_drawn_dumps;

static void dump_surface_bmp(void);

/* One triangle by vertex index: gather position and, if the batch has one,
 * texture coordinate 0. A vertex whose position cannot be read is not drawn;
 * a batch whose texcoords cannot be read is drawn untextured rather than not
 * at all, so a missing coordinate stream costs the colour and not the shape.
 */
static unsigned transformed_generation;
static struct {unsigned generation;int valid;float p[4],uv[2],fog;uint32_t color;} transformed[65536];
static int transform_vertex(unsigned index,float p[4],float uv[2],uint32_t *color)
{
    if(index>=65536)return 0;
    if(transformed[index].generation!=transformed_generation){
        uint64_t start=world_profile()?profile_clock():0;
        float in[16][4],out[16][4];for(unsigned a=0;a<16;a++)fetch_attr(&s_gpu.attr[a],index,in[a]);
        transformed[index].generation=transformed_generation;
        int ok=nf_vp_run(&vertex_program,(const float (*)[4])in,out);
        if(start){vertex_ticks+=profile_clock()-start;profile_vertices++;}
        transformed[index].valid=ok;
        if(!ok){static unsigned failures;
            if(failures++<8)fprintf(stderr,"[VERTEX-PROGRAM] unsupported or incomplete program start=%u pc=%u missing_constant=%u\n",vertex_program.start,vertex_program.error_pc,vertex_program.error_constant);
            if(failures==1 && gpu_diagnostics()){
                FILE *f=fopen("analysis/checkpoint-34/unsupported-program.txt","w");
                if(f){
                    fprintf(f,"START %u MODE %u PC %u\n",vertex_program.start,vertex_program.mode,vertex_program.error_pc);
                    for(unsigned j=vertex_program.start;j<136 && vertex_program.valid[j]==15;j++){
                        fprintf(f,"P %u %08X %08X %08X %08X\n",j,vertex_program.code[j][0],vertex_program.code[j][1],vertex_program.code[j][2],vertex_program.code[j][3]);
                        if(vertex_program.code[j][3]&1)break;
                    }
                    for(unsigned j=0;j<192;j++)if(vertex_program.constant_valid[j])fprintf(f,"C %u %X %08X %08X %08X %08X\n",j,vertex_program.constant_valid[j],vertex_program.constant_words[j][0],vertex_program.constant_words[j][1],vertex_program.constant_words[j][2],vertex_program.constant_words[j][3]);
                    for(unsigned j=0;j<16;j++)fprintf(f,"V %u %.9g %.9g %.9g %.9g\n",j,in[j][0],in[j][1],in[j][2],in[j][3]);
                    fclose(f);
                }
            }
            return 0;
        }
        memcpy(transformed[index].p,out[0],16);transformed[index].fog=current_fog(out[5][0]);
        float sx=tex_size_from_format(s_gpu.tex.color)?(float)s_gpu.tex.width:1.0f;
        float sy=tex_size_from_format(s_gpu.tex.color)?(float)s_gpu.tex.height:1.0f;
        transformed[index].uv[0]=out[9][0]*sx;transformed[index].uv[1]=out[9][1]*sy;
        unsigned c[4];for(unsigned a=0;a<4;a++)c[a]=(unsigned)(fminf(1.0f,fmaxf(0.0f,out[3][a]))*255.0f);
        transformed[index].color=(c[3]<<24)|(c[0]<<16)|(c[1]<<8)|c[2];
    }
    if(!transformed[index].valid)return 0;
    memcpy(p,transformed[index].p,16);memcpy(uv,transformed[index].uv,8);*color=transformed[index].color;return 1;
}
/* Gather a hardware program batch without repeating shared-corner lookups for
 * each expanded triangle. Retain first-use fetch order, topology and winding. */
#ifdef NIGHTFIRE_COMPACT_VERTEX91
static uint8_t compact28_vertices[NV_MAX_INDICES*28];
/* compact mode requires the validated source span and selected raw28 backend. */
static int gather_hardware_indices_mode(int compact)
#else
static int gather_hardware_indices(void)
#endif
{
    unsigned n=s_gpu.idx_count,used=0,count=0;
    if(n>NV_MAX_INDICES || hardware_count || hardware_input_count)return 0;
    switch(s_gpu.prim){
    case NV_PRIM_TRIANGLES:used=n/3*3;count=used;break;
    case NV_PRIM_TRIANGLE_STRIP:case NV_PRIM_TRIANGLE_FAN:if(n>=3){used=n;count=(n-2)*3;}break;
    case NV_PRIM_QUADS:used=n/4*4;count=used/4*6;break;
    case NV_PRIM_QUAD_STRIP:if(n>=4){used=n/2*2;count=(used-2)*3;}break;
    default:return 0;
    }
    if(count>16384 || used>16384)return 0;
    uint16_t slots[NV_MAX_INDICES];
    for(unsigned pos=0;pos<used;pos++){
        /* Quad strips encounter 0,1,3,2,5,4,... in the original expansion. */
        unsigned at=s_gpu.prim==NV_PRIM_QUAD_STRIP && pos>=2?pos^1:pos;
        unsigned index=s_gpu.idx[at],slot=hardware_slots[index].slot;
        if(hardware_slots[index].generation!=transformed_generation){
            slot=hardware_input_count++;hardware_slots[index].generation=transformed_generation;hardware_slots[index].slot=slot;
#ifdef NIGHTFIRE_COMPACT_VERTEX91
            if(compact)memcpy(compact28_vertices+slot*28,(const uint8_t*)xbox_GetMemoryOffset()+s_gpu.attr[0].offset+(size_t)index*28,28);
            else
#endif
            for(unsigned a=0;a<hardware_attribute_count;a++)fetch_attr(&s_gpu.attr[hardware_attributes[a]],index,hardware_inputs+slot*hardware_input_stride+a*4);
        }
        slots[at]=(uint16_t)slot;
    }
#define NF_EMIT(a,b,c) do{hardware_indices[hardware_count++]=slots[a];hardware_indices[hardware_count++]=slots[b];hardware_indices[hardware_count++]=slots[c];}while(0)
    switch(s_gpu.prim){
    case NV_PRIM_TRIANGLES:memcpy(hardware_indices,slots,count*sizeof *slots);hardware_count=count;break;
    case NV_PRIM_TRIANGLE_STRIP:for(unsigned i=0;i+2<used;i++)NF_EMIT(i,i+1+(i&1),i+2-(i&1));break;
    case NV_PRIM_TRIANGLE_FAN:for(unsigned i=1;i+1<used;i++)NF_EMIT(0,i,i+1);break;
    case NV_PRIM_QUADS:for(unsigned i=0;i+3<used;i+=4){NF_EMIT(i,i+1,i+2);NF_EMIT(i,i+2,i+3);}break;
    case NV_PRIM_QUAD_STRIP:for(unsigned i=0;i+3<used;i+=2){NF_EMIT(i,i+1,i+3);NF_EMIT(i,i+3,i+2);}break;
    }
#undef NF_EMIT
    s_gpu.tris_drawn+=count/3;return 1;
}
static int index_gather_enabled(void){static int enabled=-1;if(enabled<0){const char *v=getenv("NIGHTFIRE_INDEX_GATHER");enabled=!v || strcmp(v,"0");}return enabled;}
#ifdef NIGHTFIRE_COMPACT_VERTEX91
static int gather_hardware_indices(void){return gather_hardware_indices_mode(0);}
static int compact91_enabled(void){static int enabled=-1;if(enabled<0){const char *v=getenv("NIGHTFIRE_COMPACT_VERTEX91");enabled=v && !strcmp(v,"1");}return enabled;}
#endif
#ifdef NIGHTFIRE_VERTEX_BATCH92
/* Packed program fallback only. Fetch exactly the CP76 first-use sequence,
 * including its current defaults and inline handling, but emit one strip
 * index directly instead of staging and expanding each triangle corner. */
static int gather_packed_strip92(void)
{
    unsigned n=s_gpu.idx_count;
    if(s_gpu.prim!=NV_PRIM_TRIANGLE_STRIP || n<3 || n>NV_MAX_INDICES ||
       hardware_count || hardware_input_count)return 0;
    unsigned count=(n-2)*3;if(count>16384)return 0;
    for(unsigned pos=0;pos<n;pos++){
        unsigned index=s_gpu.idx[pos],slot=hardware_slots[index].slot;
        if(hardware_slots[index].generation!=transformed_generation){
            slot=hardware_input_count++;hardware_slots[index].generation=transformed_generation;hardware_slots[index].slot=slot;
            for(unsigned a=0;a<hardware_attribute_count;a++)fetch_attr(&s_gpu.attr[hardware_attributes[a]],index,hardware_inputs+slot*hardware_input_stride+a*4);
        }
        hardware_indices[pos]=(uint16_t)slot;
    }
    /* Legacy diagnostics count expanded corners; the draw API receives n. */
    hardware_count=count;s_gpu.tris_drawn+=count/3;return 1;
}
#endif
#ifdef NIGHTFIRE_DIRECT_VERTEX88
typedef struct {uint32_t address,bytes;unsigned used,count,min_index,vertices;} Direct28Plan;
static Direct28Plan direct28_plan;
#ifdef NIGHTFIRE_COMPACT_VERTEX91
static Direct28Plan compact28_plan;
#endif
static int direct28_enabled(void){static int enabled=-1;if(enabled<0){const char *v=getenv("NIGHTFIRE_DIRECT_VERTEX88");enabled=!v || !strcmp(v,"1");}return enabled;}
/* Descriptor/index reads only. Restrict to the measured layout and ordinary
 * contiguous RAM. Include every index hole and byte of padding in the guard.
 * A sparse upload may not exceed the old non-deduplicated float4 upper bound. */
static int plan_source28(Direct28Plan *out)
{
    Direct28Plan p={0};unsigned n=s_gpu.idx_count;
    const VertexAttr *a=&s_gpu.attr[0],*c=&s_gpu.attr[2],*uv=&s_gpu.attr[3];
    if(s_gpu.inline_active || n>NV_MAX_INDICES || !a->offset || (a->offset&3) ||
       a->type!=2 || a->size!=3 || a->stride!=28 || c->type!=0 || c->size!=4 || c->stride!=28 ||
       uv->type!=2 || uv->size!=2 || uv->stride!=28 ||
       (uint64_t)c->offset!=(uint64_t)a->offset+16 || (uint64_t)uv->offset!=(uint64_t)a->offset+20)return 0;
    switch(s_gpu.prim){
    case NV_PRIM_TRIANGLES:p.used=n/3*3;p.count=p.used;break;
    case NV_PRIM_TRIANGLE_STRIP:case NV_PRIM_TRIANGLE_FAN:if(n>=3){p.used=n;p.count=(n-2)*3;}break;
    case NV_PRIM_QUADS:p.used=n/4*4;p.count=p.used/4*6;break;
    case NV_PRIM_QUAD_STRIP:if(n>=4){p.used=n/2*2;p.count=(p.used-2)*3;}break;
    default:return 0;
    }
    if(!p.count || p.count>16384)return 0;
    unsigned lo=65535,hi=0;
    for(unsigned i=0;i<p.used;i++){unsigned v=s_gpu.idx[i];if(v<lo)lo=v;if(v>hi)hi=v;}
    p.min_index=lo;p.vertices=hi-lo+1;p.bytes=p.vertices*28;
    if(p.vertices>65535 || p.bytes>16384*sizeof(NFHardwareInputVertex))return 0;
    uint64_t address=(uint64_t)a->offset+(uint64_t)lo*28;
    uint64_t available=xbox_ContiguousAllocatedBytes();if(available>XBOX_CONTIG_SIZE)available=XBOX_CONTIG_SIZE;
    if(address<XBOX_CONTIG_BASE || address+p.bytes>(uint64_t)XBOX_CONTIG_BASE+available)return 0;
    p.address=(uint32_t)address;*out=p;return 1;
}
static int plan_direct28(Direct28Plan *out){
    Direct28Plan p;if(!plan_source28(&p) || p.bytes>p.used*48)return 0;*out=p;return 1;
}
static void gather_direct28(const Direct28Plan *p)
{
    uint16_t slots[NV_MAX_INDICES];
    for(unsigned i=0;i<p->used;i++)slots[i]=(uint16_t)(s_gpu.idx[i]-p->min_index);
#define NF_DIRECT_EMIT(a,b,c) do{hardware_indices[hardware_count++]=slots[a];hardware_indices[hardware_count++]=slots[b];hardware_indices[hardware_count++]=slots[c];}while(0)
    switch(s_gpu.prim){
    case NV_PRIM_TRIANGLES:memcpy(hardware_indices,slots,p->count*2);hardware_count=p->count;break;
    case NV_PRIM_TRIANGLE_STRIP:for(unsigned i=0;i+2<p->used;i++)NF_DIRECT_EMIT(i,i+1+(i&1),i+2-(i&1));break;
    case NV_PRIM_TRIANGLE_FAN:for(unsigned i=1;i+1<p->used;i++)NF_DIRECT_EMIT(0,i,i+1);break;
    case NV_PRIM_QUADS:for(unsigned i=0;i+3<p->used;i+=4){NF_DIRECT_EMIT(i,i+1,i+2);NF_DIRECT_EMIT(i,i+2,i+3);}break;
    case NV_PRIM_QUAD_STRIP:for(unsigned i=0;i+3<p->used;i+=2){NF_DIRECT_EMIT(i,i+1,i+3);NF_DIRECT_EMIT(i,i+3,i+2);}break;
    }
#undef NF_DIRECT_EMIT
    s_gpu.tris_drawn+=hardware_count/3;
}
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
#ifndef NIGHTFIRE_DIRECT_VERTEX88
#error NIGHTFIRE_DIRECT_DEFAULT90 requires NIGHTFIRE_DIRECT_VERTEX88
#endif
typedef struct {Direct28Plan span;unsigned allowed,stride;} Direct90Plan;
static Direct90Plan direct90_plan;
static int direct90_enabled(void){static int on=-1;if(on<0){const char *v=getenv("NIGHTFIRE_DIRECT_DEFAULT90");on=!v || !strcmp(v,"1");}return on;}
static int plan_direct90(Direct90Plan *out)
{
    Direct90Plan p={0};unsigned n=s_gpu.idx_count;
    const VertexAttr *a=&s_gpu.attr[0],*d=&s_gpu.attr[1],*c=&s_gpu.attr[2],*uv=&s_gpu.attr[3],*c1=&s_gpu.attr[4];
    unsigned stride=a->stride;
    if(s_gpu.inline_active || n>NV_MAX_INDICES || !a->offset || (a->offset&3) || (stride!=28 && stride!=32) ||
       a->type!=2 || a->size!=3 || d->type!=6 || d->size!=1 || d->stride!=stride ||
       c->type!=0 || c->size!=4 || c->stride!=stride || uv->type!=2 || uv->size!=2 || uv->stride!=stride ||
       (uint64_t)d->offset!=(uint64_t)a->offset+12 || (uint64_t)c->offset!=(uint64_t)a->offset+16 ||
       (uint64_t)uv->offset!=(uint64_t)a->offset+20)return 0;
    p.stride=stride;
    if(stride==28)p.allowed=NF_DIRECT90_F28;
    else{
        if(c1->type!=0 || c1->size!=4 || c1->stride!=32 || (uint64_t)c1->offset!=(uint64_t)a->offset+28)return 0;
        p.allowed=NF_DIRECT90_1F32;
        unsigned defaults=s_gpu.attr[7].offset;int match=defaults!=0;
        for(unsigned i=7;i<=14;i++){const VertexAttr *v=&s_gpu.attr[i];if(v->type!=5 || v->size!=3 || v->stride!=6 || v->offset!=defaults)match=0;}
        if(match)p.allowed|=NF_DIRECT90_7F9F32;
    }
    switch(s_gpu.prim){
    case NV_PRIM_TRIANGLES:p.span.used=n/3*3;p.span.count=p.span.used;break;
    case NV_PRIM_TRIANGLE_STRIP:case NV_PRIM_TRIANGLE_FAN:if(n>=3){p.span.used=n;p.span.count=(n-2)*3;}break;
    case NV_PRIM_QUADS:p.span.used=n/4*4;p.span.count=p.span.used/4*6;break;
    case NV_PRIM_QUAD_STRIP:if(n>=4){p.span.used=n/2*2;p.span.count=(p.span.used-2)*3;}break;
    default:return 0;
    }
    if(!p.span.count || p.span.count>16384)return 0;
    unsigned lo=65535,hi=0;
    for(unsigned i=0;i<p.span.used;i++){unsigned v=s_gpu.idx[i];if(v<lo)lo=v;if(v>hi)hi=v;}
    p.span.min_index=lo;p.span.vertices=hi-lo+1;p.span.bytes=p.span.vertices*stride;
    /* Use the smallest matching dense mask as the conservative copy budget. */
    unsigned packed_stride=stride==28?64:80;
    if(p.span.vertices>65535 || p.span.bytes>p.span.used*packed_stride || p.span.bytes>16384*sizeof(NFHardwareInputVertex))return 0;
    uint64_t address=(uint64_t)a->offset+(uint64_t)lo*stride;
    uint64_t available=xbox_ContiguousAllocatedBytes();if(available>XBOX_CONTIG_SIZE)available=XBOX_CONTIG_SIZE;
    if(address<XBOX_CONTIG_BASE || address+p.span.bytes>(uint64_t)XBOX_CONTIG_BASE+available)return 0;
    p.span.address=(uint32_t)address;*out=p;return 1;
}
#endif
#ifdef NIGHTFIRE_VERTEX_BATCH92
/* One descriptor and index-span pass for the supported raw layouts. The
 * backend still owns actual shader-mask/input-layout acceptance. */
typedef struct {
    Direct28Plan span;
    unsigned stride,allow28,allow90,compact,index_count,topology;
    const uint16_t *indices;
} Raw92Plan;
static Raw92Plan raw92_plan;
static uint16_t compact92_sources[NV_MAX_INDICES];
static uint16_t raw92_expansion_slots[NV_MAX_INDICES];
static int vertex_batch92_enabled(void){static int on=-1;if(on<0){const char *v=getenv("NIGHTFIRE_VERTEX_BATCH92");on=v && !strcmp(v,"1");}return on;}
static int plan_source92(Raw92Plan *out)
{
    Raw92Plan p={0};unsigned n=s_gpu.idx_count;
    const VertexAttr *a=&s_gpu.attr[0],*d=&s_gpu.attr[1],*c=&s_gpu.attr[2],*uv=&s_gpu.attr[3],*c1=&s_gpu.attr[4];
    unsigned stride=a->stride;
    if(s_gpu.inline_active || n>NV_MAX_INDICES || !a->offset || (a->offset&3) ||
       (stride!=28 && stride!=32) || a->type!=2 || a->size!=3 ||
       c->type!=0 || c->size!=4 || c->stride!=stride ||
       uv->type!=2 || uv->size!=2 || uv->stride!=stride ||
       (uint64_t)c->offset!=(uint64_t)a->offset+16 || (uint64_t)uv->offset!=(uint64_t)a->offset+20)return 0;
    p.stride=stride;p.allow28=stride==28 && direct28_enabled();
    if(direct90_enabled() && d->type==6 && d->size==1 && d->stride==stride &&
       (uint64_t)d->offset==(uint64_t)a->offset+12){
        if(stride==28)p.allow90=NF_DIRECT90_F28;
        else if(c1->type==0 && c1->size==4 && c1->stride==32 &&
                (uint64_t)c1->offset==(uint64_t)a->offset+28){
            p.allow90=NF_DIRECT90_1F32;
            unsigned defaults=s_gpu.attr[7].offset;int match=defaults!=0;
            for(unsigned i=7;i<=14;i++){const VertexAttr *v=&s_gpu.attr[i];
                if(v->type!=5 || v->size!=3 || v->stride!=6 || v->offset!=defaults)match=0;}
            if(match)p.allow90|=NF_DIRECT90_7F9F32;
        }
    }
    if(!p.allow28 && !p.allow90)return 0;
    switch(s_gpu.prim){
    case NV_PRIM_TRIANGLES:p.span.used=n/3*3;p.span.count=p.span.used;break;
    case NV_PRIM_TRIANGLE_STRIP:case NV_PRIM_TRIANGLE_FAN:if(n>=3){p.span.used=n;p.span.count=(n-2)*3;}break;
    case NV_PRIM_QUADS:p.span.used=n/4*4;p.span.count=p.span.used/4*6;break;
    case NV_PRIM_QUAD_STRIP:if(n>=4){p.span.used=n/2*2;p.span.count=(p.span.used-2)*3;}break;
    default:return 0;
    }
    if(!p.span.count || p.span.count>16384)return 0;
    unsigned lo=65535,hi=0;
    for(unsigned i=0;i<p.span.used;i++){unsigned v=s_gpu.idx[i];if(v<lo)lo=v;if(v>hi)hi=v;}
    p.span.min_index=lo;p.span.vertices=hi-lo+1;p.span.bytes=p.span.vertices*stride;
    /* Retain conservative whole-span/index-cut/capacity restrictions even
     * though sparse compaction subsequently reads only referenced records. */
    if(p.span.vertices>65535 || p.span.bytes>16384*sizeof(NFHardwareInputVertex))return 0;
    uint64_t address=(uint64_t)a->offset+(uint64_t)lo*stride;
    uint64_t available=xbox_ContiguousAllocatedBytes();if(available>XBOX_CONTIG_SIZE)available=XBOX_CONTIG_SIZE;
    if(address<XBOX_CONTIG_BASE || address+p.span.bytes>(uint64_t)XBOX_CONTIG_BASE+available)return 0;
    p.span.address=(uint32_t)address;
    p.topology=s_gpu.prim==NV_PRIM_TRIANGLE_STRIP?NF_HW_TRIANGLE_STRIP:NF_HW_TRIANGLE_LIST;
    p.index_count=p.topology==NF_HW_TRIANGLE_STRIP?p.span.used:p.span.count;
    *out=p;return 1;
}
static int raw92_backend_matches(void)
{
    if(!raw92_plan.span.count || !hardware_program)return 0;
    if(hardware_input_mask==0xD)return raw92_plan.allow28 && raw92_plan.stride==28 && nf_hw_direct28_active();
    unsigned allowed=hardware_input_mask==0xF?NF_DIRECT90_F28:
        hardware_input_mask==0x1F?NF_DIRECT90_1F32:
        hardware_input_mask==0x7F9F?NF_DIRECT90_7F9F32:0;
    return allowed && (raw92_plan.allow90&allowed) && nf_hw_direct90_stride()==raw92_plan.stride;
}
static int gather_raw92(void)
{
    Raw92Plan *p=&raw92_plan;
    if(!p->span.count || p->span.used>NV_MAX_INDICES || hardware_count || hardware_input_count)return 0;
    int ordered=s_gpu.prim==NV_PRIM_TRIANGLES || s_gpu.prim==NV_PRIM_TRIANGLE_STRIP;
    p->indices=hardware_indices;
    /* A zero-based contiguous packet already contains the exact GPU indices.
     * The backend copies them into its ring before this batch can be reused. */
    if(!p->compact && ordered && !p->span.min_index){
        p->indices=s_gpu.idx;
        hardware_count=p->span.count;s_gpu.tris_drawn+=hardware_count/3;return 1;
    }
    /* Strips/lists write directly to the submitted packet. Only topologies
     * needing expansion use separate slots; no per-draw 8KB stack array. */
    uint16_t *slots=ordered?hardware_indices:raw92_expansion_slots;
    for(unsigned pos=0;pos<p->span.used;pos++){
        /* Preserve the retained gather's first-use record order. Native
         * triangle strips retain the original index sequence and parity. */
        unsigned at=s_gpu.prim==NV_PRIM_QUAD_STRIP && pos>=2?pos^1:pos;
        unsigned index=s_gpu.idx[at],slot=index-p->span.min_index;
        if(p->compact){
            slot=hardware_slots[index].slot;
            if(hardware_slots[index].generation!=transformed_generation){
                slot=hardware_input_count++;hardware_slots[index].generation=transformed_generation;hardware_slots[index].slot=slot;
                /* Only build the first-use packet here. The backend copies
                 * fresh records straight into its mapped upload ring. */
                compact92_sources[slot]=(uint16_t)(index-p->span.min_index);
            }
        }
        slots[at]=(uint16_t)slot;
    }
    unsigned count=0;
#define NF_RAW92_EMIT(a,b,c) do{hardware_indices[count++]=slots[a];hardware_indices[count++]=slots[b];hardware_indices[count++]=slots[c];}while(0)
    switch(s_gpu.prim){
    case NV_PRIM_TRIANGLES:case NV_PRIM_TRIANGLE_STRIP:count=p->index_count;break;
    case NV_PRIM_TRIANGLE_FAN:for(unsigned i=1;i+1<p->span.used;i++)NF_RAW92_EMIT(0,i,i+1);break;
    case NV_PRIM_QUADS:for(unsigned i=0;i+3<p->span.used;i+=4){NF_RAW92_EMIT(i,i+1,i+2);NF_RAW92_EMIT(i,i+2,i+3);}break;
    case NV_PRIM_QUAD_STRIP:for(unsigned i=0;i+3<p->span.used;i+=2){NF_RAW92_EMIT(i,i+1,i+3);NF_RAW92_EMIT(i,i+3,i+2);}break;
    }
#undef NF_RAW92_EMIT
    if(count!=p->index_count)return 0;
    hardware_count=p->span.count;s_gpu.tris_drawn+=hardware_count/3;return 1;
}
#endif
static void raster_indexed(uint32_t i0, uint32_t i1, uint32_t i2, uint32_t argb)
{
    float p[3][4], uv[3][2];
    int textured;
    if(hardware_batch && hardware_program){
        if(i0>=65536 || i1>=65536 || i2>=65536)return;
        if(hardware_count+3>16384){fprintf(stderr,"[GPU-VERTEX] batch overflow\n");exit(4);}
        unsigned indices[3]={i0,i1,i2};
        for(unsigned k=0;k<3;k++){
            unsigned index=indices[k],slot=hardware_slots[index].slot;
            if(hardware_slots[index].generation!=transformed_generation){
                if(hardware_input_count==16384){fprintf(stderr,"[GPU-VERTEX] unique vertex overflow\n");exit(4);}
                slot=hardware_input_count++;hardware_slots[index].generation=transformed_generation;hardware_slots[index].slot=slot;
                for(unsigned a=0;a<hardware_attribute_count;a++)fetch_attr(&s_gpu.attr[hardware_attributes[a]],index,hardware_inputs+slot*hardware_input_stride+a*4);
            }
            hardware_indices[hardware_count++]=(uint16_t)slot;
        }
        s_gpu.tris_drawn++;return;
    }
    if(vertex_program_enabled() && s_gpu.attr[0].size>=3){
        uint32_t c1,c2;unsigned indices[3]={i0,i1,i2};
        if(!transform_vertex(i0,p[0],uv[0],&argb) || !transform_vertex(i1,p[1],uv[1],&c1) || !transform_vertex(i2,p[2],uv[2],&c2))return;
        if(hardware_batch){
            float area=(p[1][0]-p[0][0])*(p[2][1]-p[0][1])-(p[1][1]-p[0][1])*(p[2][0]-p[0][0]);
            if(!isfinite(area) || area==0)return;
            /* Projected winding flips for each negative W. Recover the
             * homogeneous orientation before D3D11 clips the triangle. */
            if((p[0][3]<0) ^ (p[1][3]<0) ^ (p[2][3]<0))area=-area;
            if(s_gpu.cull_enable){int front=s_gpu.front_face==0x901?area<0:area>0;
                if(s_gpu.cull_face==0x408 || (s_gpu.cull_face==0x404 && front) || (s_gpu.cull_face==0x405 && !front)){s_gpu.culled++;return;}}
            if(hardware_count+3>16384){fprintf(stderr,"[HW-GPU] batch overflow\n");exit(4);}
            for(unsigned k=0;k<3;k++){NFHardwareVertex *v=&hardware_vertices[hardware_count++];memcpy(v->position,p[k],16);
                v->uv[0]=s_gpu.tex.width?uv[k][0]/s_gpu.tex.width:0;v->uv[1]=s_gpu.tex.height?uv[k][1]/s_gpu.tex.height:0;v->color=shade_mode==0x1d01?(k==0?argb:k==1?c1:c2):argb;v->fog=transformed[indices[k]].fog;}
            s_gpu.tris_drawn++;return;
        }
        /* The legacy software rasterizer still lacks polygon clipping. */
        if(p[0][3]<=0 || p[1][3]<=0 || p[2][3]<=0)return;
        uint64_t start=world_profile()?profile_clock():0;
        world_colors[0]=argb;world_colors[1]=shade_mode==0x1d01?c1:argb;world_colors[2]=shade_mode==0x1d01?c2:argb;
        for(unsigned k=0;k<3;k++)world_fog[k]=transformed[indices[k]].fog;
        world_raster=world_varyings=1;raster_triangle(p[0],p[1],p[2],argb,(const float (*)[2])uv);world_raster=world_varyings=0;
        if(start){raster_ticks+=profile_clock()-start;profile_triangles++;}return;
    }

    if (!fetch_attr(&s_gpu.attr[0], i0, p[0])
     || !fetch_attr(&s_gpu.attr[0], i1, p[1])
     || !fetch_attr(&s_gpu.attr[0], i2, p[2]))
        return;

    textured = fetch_texcoord(i0, uv[0])
            && fetch_texcoord(i1, uv[1])
            && fetch_texcoord(i2, uv[2]);

    if(hardware_batch){
        float area=(p[1][0]-p[0][0])*(p[2][1]-p[0][1])-(p[1][1]-p[0][1])*(p[2][0]-p[0][0]);
        if(!isfinite(area) || area==0)return;
        if(hardware_count+3>16384){fprintf(stderr,"[GPU-SCREEN] batch overflow\n");exit(4);}
        for(unsigned k=0;k<3;k++){
            NFHardwareVertex *v=&hardware_vertices[hardware_count++];
            v->position[0]=p[k][0];v->position[1]=p[k][1];v->position[2]=0;v->position[3]=1;
            v->uv[0]=textured && s_gpu.tex.width?uv[k][0]/s_gpu.tex.width:0;
            v->uv[1]=textured && s_gpu.tex.height?uv[k][1]/s_gpu.tex.height:0;
            v->color=argb;v->fog=1;
        }
        s_gpu.tris_drawn++;return;
    }

    raster_triangle(p[0], p[1], p[2], argb,
                    textured ? (const float (*)[2])uv : NULL);
}

#ifdef NIGHTFIRE_SHADOW_LAYOUT131_DIAGNOSTIC
#include "nightfire_geometry133.h"
#endif

/* Bounded first-world-frame diagnostics; only enabled explicitly. */
static void capture_world_batch(int used_hardware){
    static int enabled=-1,started,done;static unsigned frame,batch;static FILE *f;
    if(enabled<0)enabled=getenv("NIGHTFIRE_WORLD_DRAW_CAPTURE")?1:getenv("NIGHTFIRE_SKY_TRACE")?2:0;
    if(!enabled || done || s_gpu.attr[0].size<3)return;
    if(!started){started=1;frame=diagnostic_presents;f=fopen(enabled==2?"analysis/checkpoint-34/sky-batches.txt":"analysis/checkpoint-34/world-batches.txt","w");}
    if(frame!=diagnostic_presents){done=1;if(f)fclose(f);return;}
    batch++;
    /* Select the captured sky texture/combiner states, not draw ordinals:
     * implementing a previously missing draw command changes those ordinals. */
    if(enabled==2 && !((s_tex_reg[1]==0x08810629 || s_tex_reg[1]==0x05640c29) && final0==0xc && final1==0x1c80))return;
    if(f){
        fprintf(f,"B %u indices=%u prim=%u tex=%08X fmt=%08X shade=%X depth=%u/%X/%u blend=%u/%X/%X hw=%d filter=%08X control=%08X address=%08X comb=%X/%X/%X/%X/%X/%X/%X\n",batch,s_gpu.idx_count,s_gpu.prim,s_gpu.tex.offset,s_tex_reg[1],shade_mode,s_gpu.depth_enable,s_gpu.depth_func,s_gpu.depth_mask,s_gpu.blend_enable,s_gpu.blend_src,s_gpu.blend_dst,used_hardware,s_tex_reg[5],s_tex_reg[3],s_tex_reg[2],combiner_count,combiner_rgb,combiner_alpha,combiner_rgb_out,combiner_alpha_out,final0,final1);
        fprintf(f,"FOG enable=%u mode=%X gen=%X color=%08X params=%08X/%08X/%08X\n",fog_enable,fog_mode,fog_gen,fog_color,fog_params[0],fog_params[1],fog_params[2]);
        for(unsigned j=0;j<s_gpu.idx_count && (enabled==2 || j<6);j++){float p[4],uv[2];uint32_t c;if(transform_vertex(s_gpu.idx[j],p,uv,&c))fprintf(f,"V %u %.7g %.7g %.7g %.7g UV %.7g %.7g C %08X\n",s_gpu.idx[j],p[0],p[1],p[2],p[3],uv[0],uv[1],c);}
        /* Track a pixel inside the persistent orange sky gap. A depth
         * change without a color change identifies an earlier invisible draw. */
        uint32_t zb;if(enabled==1 && depth_surface(&zb) && s_gpu.clip_w==640 && s_gpu.clip_h==480){
            NF_GPU_COMPLETE109("capture");uint8_t *mem=(uint8_t*)xbox_GetMemoryOffset();
            uint32_t col=*(uint32_t*)(mem+dma_resolve(s_gpu.color_offset)+10*s_gpu.pitch+310*4);
            uint32_t z=*(uint32_t*)(mem+zb+10*s_gpu.zeta_pitch+310*4);
            static uint32_t last_col,last_z;
            if(batch==1 || col!=last_col || z!=last_z){fprintf(f,"PIX batch=%u color=%08X depth=%08X alpha=%u/%X/%u cull=%u/%X/%X\n",batch,col,z,s_gpu.alpha_test,s_gpu.alpha_func,s_gpu.alpha_ref,s_gpu.cull_enable,s_gpu.cull_face,s_gpu.front_face);last_col=col;last_z=z;}
        }
        if(enabled==2){
            fprintf(f,"CULL %u %X %X ACCEPTED %u\n",s_gpu.cull_enable,s_gpu.cull_face,s_gpu.front_face,hardware_count);
            if(nf_hw_program_active())fprintf(f,"GPU_VERTEX raw inputs submitted; CPU HV list not applicable\n");
            else for(unsigned j=0;j<hardware_count;j++){NFHardwareVertex *v=&hardware_vertices[j];fprintf(f,"HV %.9g %.9g %.9g %.9g %.9g %.9g %08X\n",v->position[0],v->position[1],v->position[2],v->position[3],v->uv[0],v->uv[1],v->color);}
            unsigned bytes=d3d8_format_dxt_block_bytes(s_gpu.tex.color);
            size_t length=bytes?(size_t)((s_gpu.tex.width+3)/4)*((s_gpu.tex.height+3)/4)*bytes:(size_t)s_gpu.tex.width*s_gpu.tex.height*4;
            uint64_t end=(uint64_t)XBOX_CONTIG_BASE+xbox_ContiguousAllocatedBytes();
            if(s_gpu.tex.offset>=XBOX_CONTIG_BASE && s_gpu.tex.offset<end && length<=end-s_gpu.tex.offset){char path[128];snprintf(path,sizeof path,"analysis/checkpoint-34/sky-texture-%u.bin",batch);FILE *t=fopen(path,"wb");if(t){fwrite((uint8_t*)xbox_GetMemoryOffset()+s_gpu.tex.offset,1,length,t);fclose(t);}}
        }
        fflush(f);
    }
    if(getenv("NIGHTFIRE_WORLD_CAPTURE_IMAGES") && (batch<=16 || batch%64==0)){char name[128];snprintf(name,sizeof name,"analysis/checkpoint-34/batch-%04u.bmp",batch);dump_surface_bmp_named(name);}
}

static int hardware_prepare(void)
{
    static int screen_enabled=-1;
    if(screen_enabled<0){const char *v=getenv("NIGHTFIRE_GPU_SCREEN");screen_enabled=v && strcmp(v,"0");}
    int screen=screen_enabled && s_gpu.attr[0].type==2 && s_gpu.attr[0].size==2;
    if(!hardware_enabled() || (!screen && (!vertex_program_enabled() || s_gpu.attr[0].size<3)) || surface_bpp()!=4)return 0;
    unsigned color_layout131=0;
#ifdef NIGHTFIRE_SWIZZLED_SHADOW131
    if(swizzled_shadow131()){
        if(!swizzled_backing131(dma_resolve(s_gpu.color_offset)))return 0;
        color_layout131=swizzled_shadow131();
    }
#endif
    unsigned reverse_subtract97=0;
    if(s_gpu.blend_enable && s_gpu.blend_equation!=0x8006){
        if(!nf_hw_blend97_enabled() || s_gpu.blend_equation!=0x800b || s_gpu.blend_src!=0x302 || s_gpu.blend_dst!=1)return 0;
        reverse_subtract97=1;
    }
    uint32_t zbase=0;unsigned color_only96=0;
    if(!depth_surface(&zbase)){
        /* A disabled Z test does not require a guest depth attachment. Keep
         * valid paired targets on their existing path to avoid extra swaps. */
        if(s_gpu.depth_enable || !nf_hw_fallback96_enabled())return 0;
        color_only96=1;
    }
    uint32_t base=dma_resolve(s_gpu.color_offset);
    uint64_t end=(uint64_t)XBOX_CONTIG_BASE+xbox_ContiguousAllocatedBytes();
    if(base<XBOX_CONTIG_BASE || (uint64_t)base+(s_gpu.clip_y+s_gpu.clip_h)*s_gpu.pitch>end)return 0;
    if(s_gpu.tex.valid && (s_gpu.tex.offset<XBOX_CONTIG_BASE || s_gpu.tex.offset>=end))return 0;
    if(!screen && texture_filter_enabled() && s_gpu.tex.valid && (s_tex_reg[5]!=0x02063f01 || s_tex_reg[3]!=0x4003ffc0))return 0;
    /* Legacy 2D sampling clamps negative texel coordinates before wrapping.
     * Retain that fallback for negative/missing/nonfinite UVs. */
    if(screen && s_gpu.tex.valid)for(unsigned i=0;i<s_gpu.idx_count;i++){
        float uv[2];if(!fetch_texcoord(s_gpu.idx[i],uv) || !isfinite(uv[0]) || !isfinite(uv[1]) || uv[0]<0 || uv[1]<0)return 0;
    }
    NFHardwareState state={0};uint8_t *mem=(uint8_t*)xbox_GetMemoryOffset();
#ifdef NIGHTFIRE_DIRECT_VERTEX88
    state.direct28=direct28_plan.count!=0;
#ifdef NIGHTFIRE_COMPACT_VERTEX91
    state.direct28|=compact28_plan.count!=0;
#endif
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
    state.direct90=direct90_plan.allowed;
#endif
#ifdef NIGHTFIRE_VERTEX_BATCH92
    if(raw92_plan.span.count){state.direct28=raw92_plan.allow28;state.direct90=raw92_plan.allow90;}
#endif
    state.color=mem+base;state.depth=color_only96?NULL:mem+zbase;state.width=s_gpu.clip_x+s_gpu.clip_w;state.height=s_gpu.clip_y+s_gpu.clip_h;
    state.pitch=s_gpu.pitch;state.depth_pitch=color_only96?0:s_gpu.zeta_pitch;state.left=s_gpu.clip_x;state.top=s_gpu.clip_y;state.right=state.width;state.bottom=state.height;
    state.color_only=color_only96;
    state.color_layout=color_layout131;
    if(color_layout131){state.width=state.height=nf_swizzled131_size(color_layout131);}

    state.depth_enable=s_gpu.depth_enable;state.depth_func=s_gpu.depth_enable?s_gpu.depth_func:0x207;state.depth_write=s_gpu.depth_mask;
    state.alpha_enable=s_gpu.alpha_test;state.alpha_func=s_gpu.alpha_func;state.alpha_ref=s_gpu.alpha_ref;
    state.blend_enable=s_gpu.blend_enable;state.blend_src=s_gpu.blend_src;state.blend_dst=s_gpu.blend_dst;
    state.reverse_subtract97=reverse_subtract97;
    state.scale=combiner_rgb_out==0x10c00?2:1;
    state.combiner=combiner_count==1 && combiner_rgb==0x08040000 && combiner_alpha==0x18140000 &&
        (combiner_rgb_out==0x10c00 || combiner_rgb_out==0xc00) && combiner_alpha_out==0xc00 && supported_final();
    if(state.combiner && final0==0x130c0300 && fog_enable){
        state.fog_enable=1;state.fog_color=fog_color;
        memcpy(&state.fog_bias,fog_params,4);memcpy(&state.fog_slope,fog_params+1,4);
    }
    if(s_gpu.tex.valid){state.texture=mem+s_gpu.tex.offset;state.texture_available=(size_t)(end-s_gpu.tex.offset);}
    state.texture_format=s_tex_reg[1];state.texture_filter=s_tex_reg[5];state.texture_address=s_tex_reg[2];state.filtered=texture_filter_enabled();
    /* Preserve the current CPU nearest/clamp approximation for the captured
     * one-level RGBA address4 pass. This is not native border-mode emulation. */
    if(!screen && s_gpu.tex.valid && nf_hw_fallback96_enabled() &&
       (s_gpu.tex.color==6 || s_gpu.tex.color==7) &&
       (((s_tex_reg[1]>>16)&15)==1 || nf_rgba_mips129_match(s_tex_reg[1],s_tex_reg[2])) && (s_tex_reg[1]&0x7f)==0x29 &&
       s_tex_reg[2]==0x00010404 && s_tex_reg[5]==0x02063f01 && s_tex_reg[3]==0x4003ffc0)
        state.compat_point_clamp=1;
    if(screen && s_gpu.tex.valid && (s_gpu.tex.color==0x24 || s_gpu.tex.color==0x25)){
        static int movie_enabled=-1;
        if(movie_enabled<0){const char *v=getenv("NIGHTFIRE_GPU_MOVIE");movie_enabled=v && !strcmp(v,"1");}
        if(!movie_enabled)return 0;
        state.texture_width=s_gpu.tex.width;state.texture_height=s_gpu.tex.height;state.texture_pitch=s_gpu.tex.pitch;
    }
    /* Preserve the current screen-space path: affine UVs, base-level point
     * sampling, flat triangle color, and no depth reads/writes or culling. */
    if(screen){state.depth_enable=state.depth_write=0;state.depth_func=0x207;state.filtered=0;}
    if(!screen && gpu_vertex_enabled() && shade_mode==0x1d01 && (!s_gpu.cull_enable || s_gpu.cull_face==0x404 || s_gpu.cull_face==0x405) && (s_gpu.front_face==0x900 || s_gpu.front_face==0x901)){
        state.program=&vertex_program;state.cull_enable=s_gpu.cull_enable;state.cull_face=s_gpu.cull_face;state.front_face=s_gpu.front_face;
    }
    return nf_hw_begin(&state);
}
/* Opt-in bounded survey: identify CPU fallback states without changing draws.
 * Counts are workload leads, not elapsed time or a rendering-correctness test. */
static void survey_hardware_fallback(int hardware)
{
    static int enabled=-1,done;
    static unsigned gpu_batches,cpu_batches,used,overflow,first=100,last_token,seen_frame=~0u;
    static const char *trigger;
    static struct {unsigned key[16],batches,indices;} rows[32];
    if(enabled<0){enabled=getenv("NIGHTFIRE_GPU_FALLBACK_SURVEY")!=NULL;trigger=getenv("NIGHTFIRE_GPU_FALLBACK_TRIGGER");if(trigger)done=1;}
    if(!enabled)return;
    /* Optional rearm file: check once per published frame, never per draw.
     * A new positive token selects the next20 frames at the user's location. */
    if(trigger && seen_frame!=diagnostic_presents){
        seen_frame=diagnostic_presents;unsigned token=0;FILE *f=fopen(trigger,"r");
        if(f){if(fscanf(f,"%u",&token)!=1)token=0;fclose(f);}
        if(token && token!=last_token){last_token=token;first=diagnostic_presents;done=0;
            gpu_batches=cpu_batches=used=overflow=0;memset(rows,0,sizeof rows);
            fprintf(stderr,"[GPU-FALLBACK] token=%u start=%u\n",token,first);}
    }
    if(done || diagnostic_presents<first)return;
    if(diagnostic_presents-first>=20){
        done=1;
        fprintf(stderr,"[GPU-FALLBACK] frames=%u..%u gpu_batches=%u cpu_batches=%u overflow=%u\n",first,first+19,gpu_batches,cpu_batches,overflow);
        for(unsigned i=0;i<used;i++)fprintf(stderr,"[GPU-FALLBACK] batches=%u indices=%u attr=%u/%u bpp=%u texture=%08X filter=%08X control=%08X depth=%u/%X blend=%u/%X src=%X dst=%X address=%08X program=%u/%u texture_va=%08X\n",rows[i].batches,rows[i].indices,rows[i].key[0],rows[i].key[1],rows[i].key[2],rows[i].key[3],rows[i].key[4],rows[i].key[5],rows[i].key[6],rows[i].key[7],rows[i].key[8],rows[i].key[9],rows[i].key[10],rows[i].key[11],rows[i].key[12],rows[i].key[13],rows[i].key[14],rows[i].key[15]);
        return;
    }
    if(hardware){gpu_batches++;return;}
    cpu_batches++;
    unsigned key[]={s_gpu.attr[0].type,s_gpu.attr[0].size,surface_bpp(),s_gpu.tex.valid?s_tex_reg[1]:0,s_tex_reg[5],s_tex_reg[3],s_gpu.depth_enable,s_gpu.depth_func,s_gpu.blend_enable,s_gpu.blend_equation,s_gpu.blend_src,s_gpu.blend_dst,s_tex_reg[2],vertex_program.start,vertex_program.mode,s_gpu.tex.valid?s_gpu.tex.offset:0};
    unsigned i;for(i=0;i<used;i++)if(!memcmp(key,rows[i].key,sizeof key))break;
    if(i==used){if(used==32){overflow++;return;}memcpy(rows[used++].key,key,sizeof key);}
    rows[i].batches++;rows[i].indices+=s_gpu.idx_count;
}
#ifdef NIGHTFIRE_DIRECT_VERTEX_SURVEY
/* CP87 observation only: completed packed-program draws at presents 100..119.
 * Read cached descriptors/indices, never vertex bytes or GPU resources. A raw
 * span includes index holes and unused attribute bytes; eligibility is neither
 * a timing result nor proof that a future direct-input implementation is exact.
 * Rejection counters overlap when a batch fails more than one condition. */
enum { DV_INLINE, DV_NO_ATTRS, DV_FORMAT, DV_MISSING, DV_STRIDE,
       DV_ATTR_SPAN, DV_TOPOLOGY, DV_ARENA, DV_REASONS };
static const char *const direct_vertex_reasons[DV_REASONS]={
    "inline","no_attributes","not_float1_to4","zero_offset_or_stride",
    "different_strides","attribute_span_gt_stride","topology_or_count","outside_arena"};
typedef struct {
    unsigned mask,inline_active,reject;
    /* offset relative to lowest used offset, type, size, stride, by slot. */
    uint32_t attributes[16][4];
    uint64_t batches,triangles,packed,raw,max_raw;
    unsigned min_index,max_index;
} DirectVertexLayout;
static struct {
    unsigned done,layouts,overflow;
    uint64_t batches,triangles,packed,eligible_batches,eligible_triangles;
    uint64_t eligible_packed,raw,max_raw,rejected[DV_REASONS];
    DirectVertexLayout row[16];
} direct_vertex_survey;
static void direct_vertex_survey_report(void)
{
    if(direct_vertex_survey.done || diagnostic_presents<120)return;
    direct_vertex_survey.done=1;
    fprintf(stderr,"[DIRECT-VERTEX-SURVEY] frames=100..119 completed_program_batches=%llu triangles=%llu packed_bytes=%llu eligible_batches=%llu eligible_triangles=%llu eligible_packed_bytes=%llu eligible_raw_span_bytes=%llu max_raw_span_bytes=%llu layouts=%u layout_overflow_batches=%u rejection_counts_overlap=1\n",
        (unsigned long long)direct_vertex_survey.batches,(unsigned long long)direct_vertex_survey.triangles,
        (unsigned long long)direct_vertex_survey.packed,(unsigned long long)direct_vertex_survey.eligible_batches,
        (unsigned long long)direct_vertex_survey.eligible_triangles,(unsigned long long)direct_vertex_survey.eligible_packed,
        (unsigned long long)direct_vertex_survey.raw,(unsigned long long)direct_vertex_survey.max_raw,
        direct_vertex_survey.layouts,direct_vertex_survey.overflow);
    for(unsigned r=0;r<DV_REASONS;r++)
        fprintf(stderr,"[DIRECT-VERTEX-SURVEY] reject=%s batches=%llu\n",direct_vertex_reasons[r],(unsigned long long)direct_vertex_survey.rejected[r]);
    for(unsigned i=0;i<direct_vertex_survey.layouts;i++){
        const DirectVertexLayout *row=&direct_vertex_survey.row[i];
        fprintf(stderr,"[DIRECT-VERTEX-LAYOUT] row=%u mask=%04X inline=%u reject_bits=%X batches=%llu triangles=%llu packed_bytes=%llu eligible_raw_bytes=%llu max_raw_bytes=%llu used_source_index=%u..%u attributes=slot:relative_offset/type/size/stride",
            i,row->mask,row->inline_active,row->reject,(unsigned long long)row->batches,
            (unsigned long long)row->triangles,(unsigned long long)row->packed,
            (unsigned long long)row->raw,(unsigned long long)row->max_raw,row->min_index,row->max_index);
        for(unsigned a=0;a<16;a++)if(row->mask&(1u<<a))
            fprintf(stderr," %u:%u/%u/%u/%u",a,row->attributes[a][0],row->attributes[a][1],row->attributes[a][2],row->attributes[a][3]);
        fputc('\n',stderr);
    }
}
static void survey_direct_vertex_completed(void)
{
    if(direct_vertex_survey.done || diagnostic_presents<100 || diagnostic_presents>=120)return;
    unsigned reject=0,used=0,expected=0,n=s_gpu.idx_count;
    unsigned min_index=UINT32_MAX,max_index=0,stride=0,have_stride=0;
    uint64_t min_offset=UINT32_MAX,max_end=0,raw=0;
    DirectVertexLayout layout={0};
    uint64_t packed=(uint64_t)hardware_input_count*hardware_input_stride*4;
    direct_vertex_survey.batches++;direct_vertex_survey.triangles+=hardware_count/3;
    direct_vertex_survey.packed+=packed;
    if(s_gpu.inline_active)reject|=1u<<DV_INLINE;
    if(!hardware_attribute_count)reject|=1u<<DV_NO_ATTRS;
    for(unsigned a=0;a<16;a++)if(hardware_input_mask&(1u<<a)){
        const VertexAttr *v=&s_gpu.attr[a];
        if(v->type!=2 || v->size<1 || v->size>4)reject|=1u<<DV_FORMAT;
        if(!v->offset || !v->stride)reject|=1u<<DV_MISSING;
        if(!have_stride){stride=v->stride;have_stride=1;}
        else if(stride!=v->stride)reject|=1u<<DV_STRIDE;
        if(v->offset<min_offset)min_offset=v->offset;
        if((uint64_t)v->offset+(uint64_t)v->size*4>max_end)max_end=(uint64_t)v->offset+(uint64_t)v->size*4;
    }
    if(hardware_attribute_count && max_end-min_offset>stride)reject|=1u<<DV_ATTR_SPAN;
    /* Exactly the consumed source-index prefix from CP76. Odd quad-strip
     * tails and incomplete triangles/quads never contribute to the span. */
    switch(s_gpu.prim){
    case NV_PRIM_TRIANGLES:used=n/3*3;expected=used;break;
    case NV_PRIM_TRIANGLE_STRIP:case NV_PRIM_TRIANGLE_FAN:if(n>=3){used=n;expected=(n-2)*3;}break;
    case NV_PRIM_QUADS:used=n/4*4;expected=used/4*6;break;
    case NV_PRIM_QUAD_STRIP:if(n>=4){used=n/2*2;expected=(used-2)*3;}break;
    default:reject|=1u<<DV_TOPOLOGY;break;
    }
    if(n>NV_MAX_INDICES || !used || expected!=hardware_count)reject|=1u<<DV_TOPOLOGY;
    if(n<=NV_MAX_INDICES)for(unsigned i=0;i<used;i++){
        unsigned index=s_gpu.idx[i];
        if(index<min_index)min_index=index;
        if(index>max_index)max_index=index;
    }
    if(!(reject&((1u<<DV_NO_ATTRS)|(1u<<DV_TOPOLOGY)))){
        uint64_t start=min_offset+(uint64_t)min_index*stride;
        uint64_t end=max_end+(uint64_t)max_index*stride;
        uint64_t arena_bytes=xbox_ContiguousAllocatedBytes();
        if(arena_bytes>XBOX_CONTIG_SIZE)arena_bytes=XBOX_CONTIG_SIZE;
        if(start<XBOX_CONTIG_BASE || end<start || end>(uint64_t)XBOX_CONTIG_BASE+arena_bytes)
            reject|=1u<<DV_ARENA;
        else raw=end-start;
    }
    for(unsigned r=0;r<DV_REASONS;r++)if(reject&(1u<<r))direct_vertex_survey.rejected[r]++;
    if(!reject){
        direct_vertex_survey.eligible_batches++;direct_vertex_survey.eligible_triangles+=hardware_count/3;
        direct_vertex_survey.eligible_packed+=packed;direct_vertex_survey.raw+=raw;
        if(raw>direct_vertex_survey.max_raw)direct_vertex_survey.max_raw=raw;
    }
    layout.mask=hardware_input_mask;layout.inline_active=s_gpu.inline_active!=0;layout.reject=reject;
    for(unsigned a=0;a<16;a++)if(layout.mask&(1u<<a)){
        const VertexAttr *v=&s_gpu.attr[a];
        layout.attributes[a][0]=(uint32_t)((uint64_t)v->offset-min_offset);
        layout.attributes[a][1]=v->type;layout.attributes[a][2]=v->size;layout.attributes[a][3]=v->stride;
    }
    unsigned row;
    for(row=0;row<direct_vertex_survey.layouts;row++){
        DirectVertexLayout *old=&direct_vertex_survey.row[row];
        if(old->mask==layout.mask && old->inline_active==layout.inline_active && old->reject==layout.reject &&
           !memcmp(old->attributes,layout.attributes,sizeof layout.attributes))break;
    }
    if(row==direct_vertex_survey.layouts){
        if(row==16){direct_vertex_survey.overflow++;return;}
        direct_vertex_survey.row[row]=layout;direct_vertex_survey.row[row].min_index=UINT32_MAX;
        direct_vertex_survey.layouts++;
    }
    DirectVertexLayout *out=&direct_vertex_survey.row[row];
    out->batches++;out->triangles+=hardware_count/3;out->packed+=packed;
    if(min_index<out->min_index)out->min_index=min_index;
    if(max_index>out->max_index)out->max_index=max_index;
    if(!reject){out->raw+=raw;if(raw>out->max_raw)out->max_raw=raw;}
}
#endif
#ifdef NIGHTFIRE_DIRECT_VERTEX_SURVEY89
/* CP89 descriptor/index-only survey of successful hardware-program draws.
 * Main-stream spans include index holes; type5/type6 streams contribute no
 * source bytes because current fetch_attr returns default (0,0,0,1) for them.
 * No interpretation of those unsupported formats is introduced here. */
enum { V89_D28,V89_F28,V89_1F32,V89_7F9F32,V89_OTHER,V89_LAYOUTS };
enum { V89_LAYOUT,V89_TOPOLOGY,V89_INDEX_CUT,V89_SPARSE,V89_CAPACITY,
       V89_ARENA,V89_DISABLED,V89_BACKEND,V89_PLAN_MISMATCH,V89_REJECTIONS };
static const char *const vertex89_layout_names[V89_LAYOUTS]={"D28","F28","1F32","7F9F32","other"};
static const char *const vertex89_reject_names[V89_REJECTIONS]={"layout","topology_count","index_cut","sparse_budget","capacity","arena","disabled","backend_decline","plan_mismatch"};
typedef struct {
    uint64_t direct,dense,direct_triangles,dense_triangles,direct_bytes,unique,packed;
    uint64_t span_batches,span_bytes,max_span,span_vertices,used_entries;
    unsigned min_index,max_index;
} Vertex89Row;
static struct {
    unsigned done;
    Vertex89Row row[V89_LAYOUTS];
    struct {uint64_t batches,triangles,packed;} rejects[V89_REJECTIONS];
    struct {uint64_t batches,packed,raw,unique,span_vertices;} sparse[4];
} vertex89;
static int vertex89_attr(unsigned slot,unsigned type,unsigned size,unsigned stride,uint64_t address)
{
    const VertexAttr *a=&s_gpu.attr[slot];
    return a->type==type && a->size==size && a->stride==stride && (uint64_t)a->offset==address;
}
static unsigned vertex89_classify(void)
{
    unsigned mask=hardware_input_mask,stride;
    uint64_t base=s_gpu.attr[0].offset;
    if(s_gpu.inline_active || !base || (base&3))return V89_OTHER;
    if(mask==0xD || mask==0xF)stride=28;
    else if(mask==0x1F || mask==0x7F9F)stride=32;
    else return V89_OTHER;
    if(!vertex89_attr(0,2,3,stride,base) || !vertex89_attr(2,0,4,stride,base+16) ||
       !vertex89_attr(3,2,2,stride,base+20))return V89_OTHER;
    if(mask==0xD)return V89_D28;
    if(!vertex89_attr(1,6,1,stride,base+12))return V89_OTHER;
    if(mask==0xF)return V89_F28;
    if(!vertex89_attr(4,0,4,32,base+28))return V89_OTHER;
    if(mask==0x1F)return V89_1F32;
    uint64_t defaults=s_gpu.attr[7].offset;
    if(!defaults)return V89_OTHER;
    for(unsigned a=7;a<=14;a++)if(!vertex89_attr(a,5,3,6,defaults))return V89_OTHER;
    return V89_7F9F32;
}
static void direct_vertex89_completed(int direct)
{
    if(vertex89.done || diagnostic_presents<100 || diagnostic_presents>=120)return;
    unsigned group=vertex89_classify(),n=s_gpu.idx_count,used=0,expected=0;
    unsigned lo=65535,hi=0,vertices=0,reason=V89_LAYOUT;
    uint64_t raw=0,address=0,packed=direct?0:(uint64_t)hardware_input_count*hardware_input_stride*4;
    int topology_ok=0,arena_ok=0;
    Vertex89Row *row=&vertex89.row[group];
    if(!row->direct && !row->dense)row->min_index=UINT32_MAX;
    if(direct){
        uint64_t bytes=direct28_plan.bytes;
#ifdef NIGHTFIRE_VERTEX_BATCH92
        if(raw92_plan.span.count)bytes=raw92_plan.compact?(uint64_t)hardware_input_count*raw92_plan.stride:raw92_plan.span.bytes;
#endif
        row->direct++;row->direct_triangles+=hardware_count/3;row->direct_bytes+=bytes;
    }
    else{row->dense++;row->dense_triangles+=hardware_count/3;row->unique+=hardware_input_count;row->packed+=packed;}
    if(direct)return; /* Remaining-dense span metrics need no accepted rescans. */
    /* CP76 consumed prefix, including each repeated index but excluding tails. */
    if(n<=NV_MAX_INDICES)switch(s_gpu.prim){
    case NV_PRIM_TRIANGLES:used=n/3*3;expected=used;break;
    case NV_PRIM_TRIANGLE_STRIP:case NV_PRIM_TRIANGLE_FAN:if(n>=3){used=n;expected=(n-2)*3;}break;
    case NV_PRIM_QUADS:used=n/4*4;expected=used/4*6;break;
    case NV_PRIM_QUAD_STRIP:if(n>=4){used=n/2*2;expected=(used-2)*3;}break;
    }
    topology_ok=used && expected==hardware_count && expected<=16384;
    if(topology_ok){
        for(unsigned i=0;i<used;i++){unsigned index=s_gpu.idx[i];if(index<lo)lo=index;if(index>hi)hi=index;}
        vertices=hi-lo+1;row->used_entries+=used;
        if(lo<row->min_index)row->min_index=lo;if(hi>row->max_index)row->max_index=hi;
        if(group!=V89_OTHER){
            unsigned stride=group<=V89_F28?28:32;
            raw=(uint64_t)vertices*stride;address=(uint64_t)s_gpu.attr[0].offset+(uint64_t)lo*stride;
            uint64_t available=xbox_ContiguousAllocatedBytes();if(available>XBOX_CONTIG_SIZE)available=XBOX_CONTIG_SIZE;
            arena_ok=address>=XBOX_CONTIG_BASE && address+raw<=(uint64_t)XBOX_CONTIG_BASE+available;
            row->span_batches++;row->span_bytes+=raw;row->span_vertices+=vertices;
            if(raw>row->max_span)row->max_span=raw;
        }
    }
    /* Exclusive first rejection, mirroring the current CP88 gate order.
     * Recognized extra layouts still reject as layout under CP88. A backend
     * decline is reported only if its actual shader mask is D and a plan exists. */
    if(!direct28_enabled() || !index_gather_enabled())reason=V89_DISABLED;
    else if(group!=V89_D28)reason=V89_LAYOUT;
    else if(!topology_ok)reason=V89_TOPOLOGY;
    else if(vertices>65535)reason=V89_INDEX_CUT;
    else if(raw>(uint64_t)used*48)reason=V89_SPARSE;
    else if(raw>16384*sizeof(NFHardwareInputVertex))reason=V89_CAPACITY;
    else if(!arena_ok)reason=V89_ARENA;
    else if(direct28_plan.count)reason=V89_BACKEND;
    else reason=V89_PLAN_MISMATCH;
    vertex89.rejects[reason].batches++;vertex89.rejects[reason].triangles+=hardware_count/3;vertex89.rejects[reason].packed+=packed;
    if(reason==V89_SPARSE){
        unsigned bin=raw<=packed?0:raw*2<=packed*3?1:raw<=packed*2?2:3;
        vertex89.sparse[bin].batches++;vertex89.sparse[bin].packed+=packed;vertex89.sparse[bin].raw+=raw;
        vertex89.sparse[bin].unique+=hardware_input_count;vertex89.sparse[bin].span_vertices+=vertices;
    }
}
static void direct_vertex89_report(void)
{
    if(vertex89.done || diagnostic_presents<120)return;
    vertex89.done=1;
    uint64_t direct=0,dense=0,dt=0,pt=0,db=0,packed=0,unique=0;
    for(unsigned i=0;i<V89_LAYOUTS;i++){
        const Vertex89Row *r=&vertex89.row[i];direct+=r->direct;dense+=r->dense;dt+=r->direct_triangles;
        pt+=r->dense_triangles;db+=r->direct_bytes;packed+=r->packed;unique+=r->unique;
    }
    fprintf(stderr,"[VERTEX89] frames=100..119 completed_program_direct=%llu dense=%llu direct_triangles=%llu dense_triangles=%llu direct_bytes=%llu dense_unique=%llu dense_packed_bytes=%llu spans=main_stream_including_holes_no_default_stream_reads rejection_counts_exclusive=1\n",
        (unsigned long long)direct,(unsigned long long)dense,(unsigned long long)dt,(unsigned long long)pt,
        (unsigned long long)db,(unsigned long long)unique,(unsigned long long)packed);
    for(unsigned i=0;i<V89_LAYOUTS;i++){
        const Vertex89Row *r=&vertex89.row[i];
        fprintf(stderr,"[VERTEX89-LAYOUT] name=%s direct=%llu dense=%llu direct_triangles=%llu dense_triangles=%llu direct_bytes=%llu dense_unique=%llu dense_packed_bytes=%llu dense_span_batches=%llu dense_estimated_span_bytes=%llu dense_max_span_bytes=%llu dense_span_vertices=%llu dense_consumed_entries=%llu dense_source_indices=%u..%u\n",
            vertex89_layout_names[i],(unsigned long long)r->direct,(unsigned long long)r->dense,
            (unsigned long long)r->direct_triangles,(unsigned long long)r->dense_triangles,(unsigned long long)r->direct_bytes,
            (unsigned long long)r->unique,(unsigned long long)r->packed,(unsigned long long)r->span_batches,
            (unsigned long long)r->span_bytes,(unsigned long long)r->max_span,(unsigned long long)r->span_vertices,
            (unsigned long long)r->used_entries,r->used_entries?r->min_index:0,r->max_index);
    }
    for(unsigned i=0;i<V89_REJECTIONS;i++)fprintf(stderr,"[VERTEX89-REJECT] reason=%s batches=%llu triangles=%llu packed_bytes=%llu\n",
        vertex89_reject_names[i],(unsigned long long)vertex89.rejects[i].batches,
        (unsigned long long)vertex89.rejects[i].triangles,(unsigned long long)vertex89.rejects[i].packed);
    const char *const bins[]={"le1","gt1_le1.5","gt1.5_le2","gt2"};
    for(unsigned i=0;i<4;i++)fprintf(stderr,"[VERTEX89-SPARSE] raw_over_actual_packed=%s batches=%llu packed_bytes=%llu raw_span_bytes=%llu unique_vertices=%llu span_vertices=%llu\n",
        bins[i],(unsigned long long)vertex89.sparse[i].batches,(unsigned long long)vertex89.sparse[i].packed,
        (unsigned long long)vertex89.sparse[i].raw,(unsigned long long)vertex89.sparse[i].unique,(unsigned long long)vertex89.sparse[i].span_vertices);
}
#endif
static void raster_batch(void)
{
    uint32_t i;
    uint32_t before = s_gpu.tris_drawn;
#ifdef NIGHTFIRE_VERTEX_BATCH92
    memset(&raw92_plan,0,sizeof raw92_plan);
    if(vertex_batch92_enabled()){
        memset(&direct28_plan,0,sizeof direct28_plan);
        memset(&direct90_plan,0,sizeof direct90_plan);
        memset(&compact28_plan,0,sizeof compact28_plan);
        if(index_gather_enabled())plan_source92(&raw92_plan);
        if(nf_hw_clear_pending && raw92_plan.span.count)
            nightfire_gpu_read_guard(raw92_plan.span.address,raw92_plan.span.bytes);
    }else{
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
    memset(&direct90_plan,0,sizeof direct90_plan);
    if(direct90_enabled() && index_gather_enabled())plan_direct90(&direct90_plan);
    if(nf_hw_clear_pending && direct90_plan.span.count)
        nightfire_gpu_read_guard(direct90_plan.span.address,direct90_plan.span.bytes);
#endif
#ifdef NIGHTFIRE_DIRECT_VERTEX88
    memset(&direct28_plan,0,sizeof direct28_plan);
#ifdef NIGHTFIRE_COMPACT_VERTEX91
    memset(&compact28_plan,0,sizeof compact28_plan);
#endif
    if(direct28_enabled() && index_gather_enabled()){
#ifdef NIGHTFIRE_COMPACT_VERTEX91
        if(compact91_enabled()){
            Direct28Plan source;
            if(plan_source28(&source)){
                if(source.bytes<=source.used*48)direct28_plan=source;
                else compact28_plan=source;
            }
        }else
#endif
        plan_direct28(&direct28_plan);
    }
    if(nf_hw_clear_pending && direct28_plan.count)
        nightfire_gpu_read_guard(direct28_plan.address,direct28_plan.bytes);
#ifdef NIGHTFIRE_COMPACT_VERTEX91
    if(nf_hw_clear_pending && compact28_plan.count)
        nightfire_gpu_read_guard(compact28_plan.address,compact28_plan.bytes);
#endif
#endif
#ifdef NIGHTFIRE_VERTEX_BATCH92
    }
#endif
    /* Resolve alias reads before hardware_prepare binds this batch. A sync
     * during vertex gathering would unbind the targets of that prepared draw. */
    if(nf_hw_clear_pending && !s_gpu.inline_active){
        unsigned max_index=0;
        for(unsigned k=0;k<s_gpu.idx_count;k++)if(s_gpu.idx[k]>max_index)max_index=s_gpu.idx[k];
        for(unsigned a=0;a<NV_VERTEX_ATTRS && nf_hw_clear_pending;a++){
            const VertexAttr *v=&s_gpu.attr[a];
            if(v->offset && v->size && v->stride)
                nightfire_gpu_read_guard(v->offset,(size_t)max_index*v->stride+64);
        }
    }
    diagnostic_draw_state();
    world_filter.levels=0;
    if(texture_filter_enabled() && vertex_program_enabled() && s_gpu.attr[0].size>=3 && s_gpu.tex.valid){
        uint64_t end=(uint64_t)XBOX_CONTIG_BASE+xbox_ContiguousAllocatedBytes();
        if(s_gpu.tex.offset>=XBOX_CONTIG_BASE && s_gpu.tex.offset<end)
            nf_filter_init(&world_filter,(const uint8_t *)xbox_GetMemoryOffset()+s_gpu.tex.offset,
                (size_t)(end-s_gpu.tex.offset),s_tex_reg[1],s_tex_reg[5],s_tex_reg[3],s_tex_reg[2],texture_cache_enabled());
    }
    if(++transformed_generation==0){memset(transformed,0,sizeof transformed);memset(hardware_slots,0,sizeof hardware_slots);transformed_generation=1;}

    if (s_gpu.idx_count < 3)
        return;
    if (!(vertex_program_enabled() && s_gpu.attr[0].size>=3) && !batch_is_screen_space()) {
        s_gpu.batches_untransformed++;
        return;
    }

    /* Count why, once per batch: the texture stage cannot change inside one. */
    hardware_count=hardware_input_count=0;hardware_batch=hardware_prepare();
    survey_hardware_fallback(hardware_batch);
    /* Program/layout cannot change while this guest-thread batch is gathered. */
    hardware_program=hardware_batch && nf_hw_program_active();
    hardware_input_mask=hardware_program?nf_hw_program_inputs():0;hardware_attribute_count=0;
    int gathered_raw92=0,gathered_packed92=0;
#ifdef NIGHTFIRE_VERTEX_BATCH92
    gathered_raw92=raw92_backend_matches();
    if(gathered_raw92){
        unsigned budget=hardware_input_mask==0xD?48:hardware_input_mask==0xF?64:80;
        raw92_plan.compact=raw92_plan.span.bytes>raw92_plan.span.used*budget;
    }
#endif
    /* Raw inputs have no packed float4 attribute-list consumer. Retain that
     * metadata when a compiled survey explicitly observes the dense layout. */
#if defined(NIGHTFIRE_VERTEX_BATCH92) && !defined(NIGHTFIRE_DIRECT_VERTEX_SURVEY) && !defined(NIGHTFIRE_DIRECT_VERTEX_SURVEY89)
    if(!gathered_raw92)
#endif
        for(unsigned a=0;a<16;a++)if(hardware_input_mask&(1u<<a))hardware_attributes[hardware_attribute_count++]=a;
    hardware_input_stride=(hardware_attribute_count?hardware_attribute_count:1)*4;
    if(!hardware_batch)NF_GPU_COMPLETE109("cpu-fallback");
    {
        const VertexAttr *tc = texcoord_attr();

        if (!(tc->offset && tc->stride))
            s_gpu.batches_no_uv++;
        else if (!s_gpu.tex.valid)
            s_gpu.batches_no_tex++;
        else {
            s_gpu.batches_textured++;
            note_texture_use();
        }
    }

    int program_color=vertex_program_enabled() && s_gpu.attr[0].size>=3;
    int gathered_direct28=0,gathered_direct90=0,gathered_compact91=0;
#ifdef NIGHTFIRE_VERTEX_BATCH92
    if(gathered_raw92 && !gather_raw92()){fprintf(stderr,"[HW-GPU] batch92 gather contract failed\n");exit(4);}
#endif
#ifdef NIGHTFIRE_DIRECT_VERTEX88
    if(!gathered_raw92 && hardware_program && nf_hw_direct28_active()){
#ifdef NIGHTFIRE_COMPACT_VERTEX91
        if(compact28_plan.count){
            if(!gather_hardware_indices_mode(1)){fprintf(stderr,"[HW-GPU] compact91 gather contract failed\n");exit(4);}
            gathered_compact91=1;
        }else
#endif
        {
        gather_direct28(&direct28_plan);gathered_direct28=1;
        }
    }
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
    if(!gathered_raw92 && !gathered_direct28 && !gathered_compact91 && hardware_program && nf_hw_direct90_stride()){
        gather_direct28(&direct90_plan.span);gathered_direct90=1;
    }
#endif
#ifdef NIGHTFIRE_VERTEX_BATCH92
    if(!gathered_raw92 && !gathered_direct28 && !gathered_direct90 && !gathered_compact91 &&
       hardware_program && vertex_batch92_enabled() && index_gather_enabled() && s_gpu.prim==NV_PRIM_TRIANGLE_STRIP)
        gathered_packed92=gather_packed_strip92();
#endif
    if(!gathered_raw92 && !gathered_packed92 && !gathered_direct28 && !gathered_direct90 && !gathered_compact91 && !(hardware_program && index_gather_enabled() && gather_hardware_indices()))switch (s_gpu.prim) {
    case NV_PRIM_TRIANGLES:
        for (i = 0; i + 2 < s_gpu.idx_count; i += 3)
            raster_indexed(s_gpu.idx[i], s_gpu.idx[i+1], s_gpu.idx[i+2],
                           (program_color?0:vertex_color(s_gpu.idx[i])));
        break;
    case NV_PRIM_TRIANGLE_STRIP:
        for (i = 0; i + 2 < s_gpu.idx_count; i++)
            raster_indexed(s_gpu.idx[i], s_gpu.idx[i+1+(i&1)], s_gpu.idx[i+2-(i&1)],
                           (program_color?0:vertex_color(s_gpu.idx[i])));
        break;
    case NV_PRIM_TRIANGLE_FAN:
        /* A fan and a quad both rasterise as a triangle fan around index 0;
         * for a quad that is exactly its two triangles. */
        for (i = 1; i + 1 < s_gpu.idx_count; i++)
            raster_indexed(s_gpu.idx[0], s_gpu.idx[i], s_gpu.idx[i+1],
                           (program_color?0:vertex_color(s_gpu.idx[0])));
        break;
    case NV_PRIM_QUADS:
        for (i = 0; i + 3 < s_gpu.idx_count; i += 4) {
            uint32_t color = (program_color?0:vertex_color(s_gpu.idx[i]));
            raster_indexed(s_gpu.idx[i], s_gpu.idx[i+1], s_gpu.idx[i+2], color);
            raster_indexed(s_gpu.idx[i], s_gpu.idx[i+2], s_gpu.idx[i+3], color);
        }
        break;
    case NV_PRIM_QUAD_STRIP:
        for (i = 0; i + 3 < s_gpu.idx_count; i += 2) {
            uint32_t color = (program_color?0:vertex_color(s_gpu.idx[i]));
            raster_indexed(s_gpu.idx[i], s_gpu.idx[i+1], s_gpu.idx[i+3], color);
            raster_indexed(s_gpu.idx[i], s_gpu.idx[i+3], s_gpu.idx[i+2], color);
        }
        break;
    default:
        break;                             /* points and lines: not yet */
    }

    if (verbose_enabled() && s_gpu.tris_drawn && (s_gpu.tris_drawn % 500000) == 0)
        fprintf(stderr, "  [GPU] %u triangles rasterised\n", s_gpu.tris_drawn);
    if(hardware_batch && hardware_count){
#ifdef NIGHTFIRE_VERTEX_BATCH92
        if(gathered_raw92){
            const void *source=(const uint8_t*)xbox_GetMemoryOffset()+raw92_plan.span.address;
            unsigned vertices=raw92_plan.compact?hardware_input_count:raw92_plan.span.vertices;
            int drawn=raw92_plan.compact?
                nf_hw_draw_raw_compact_indexed(source,raw92_plan.span.vertices,raw92_plan.stride,compact92_sources,vertices,raw92_plan.indices,raw92_plan.index_count,raw92_plan.topology):
                nf_hw_draw_raw_indexed(source,vertices,raw92_plan.stride,raw92_plan.indices,raw92_plan.index_count,raw92_plan.topology);
            if(!drawn){
                fprintf(stderr,"[HW-GPU] batch92 draw failed\n");exit(4);
            }
            batch92_draws++;batch92_compact_draws+=raw92_plan.compact;
            batch92_strip_draws+=raw92_plan.topology==NF_HW_TRIANGLE_STRIP;
            batch92_vertices+=vertices;batch92_bytes+=(uint64_t)vertices*raw92_plan.stride;
            batch92_indices+=raw92_plan.index_count;
        }else
        if(gathered_packed92){
            if(!nf_hw_draw_packed_topology(hardware_inputs,hardware_input_count,hardware_input_mask,hardware_indices,s_gpu.idx_count,NF_HW_TRIANGLE_STRIP)){
                fprintf(stderr,"[HW-GPU] packed92 strip draw failed\n");exit(4);
            }
            packed92_draws++;packed92_vertices+=hardware_input_count;
            packed92_bytes+=(uint64_t)hardware_input_count*hardware_input_stride*4;
            packed92_indices+=s_gpu.idx_count;packed92_triangles+=hardware_count/3;
        }else
#endif
#ifdef NIGHTFIRE_COMPACT_VERTEX91
        if(gathered_compact91){
            if(!nf_hw_draw_direct28(compact28_vertices,hardware_input_count,hardware_indices,hardware_count)){fprintf(stderr,"[HW-GPU] compact91 draw failed\n");exit(4);}
            compact91_draws++;compact91_vertices+=hardware_input_count;compact91_bytes+=(uint64_t)hardware_input_count*28;
        }else
#endif
#ifdef NIGHTFIRE_DIRECT_DEFAULT90
        if(gathered_direct90){
            if(!nf_hw_draw_direct90((const uint8_t*)xbox_GetMemoryOffset()+direct90_plan.span.address,direct90_plan.span.vertices,hardware_indices,hardware_count)){fprintf(stderr,"[HW-GPU] direct90 draw failed\n");exit(4);}
        }else
#endif
#ifdef NIGHTFIRE_DIRECT_VERTEX88
        if(gathered_direct28){
            if(!nf_hw_draw_direct28((const uint8_t*)xbox_GetMemoryOffset()+direct28_plan.address,direct28_plan.vertices,hardware_indices,hardware_count)){fprintf(stderr,"[HW-GPU] direct28 draw failed\n");exit(4);}
        }else
#endif
        if(!(hardware_program?nf_hw_draw_packed_input(hardware_inputs,hardware_input_count,hardware_input_mask,hardware_indices,hardware_count):nf_hw_draw(hardware_vertices,hardware_count))){fprintf(stderr,"[HW-GPU] draw failed\n");exit(4);}
#ifdef NIGHTFIRE_DIRECT_VERTEX_SURVEY
        if(hardware_program)survey_direct_vertex_completed();
#endif
#ifdef NIGHTFIRE_DIRECT_VERTEX_SURVEY89
        if(hardware_program)direct_vertex89_completed(gathered_direct28 || gathered_raw92);
#endif
        last_draw_address=dma_resolve(s_gpu.color_offset);last_draw_pitch=s_gpu.pitch;
    }
#ifdef NIGHTFIRE_SHADOW_LAYOUT131_DIAGNOSTIC
    if(s_gpu.tris_drawn!=before){
        shadow131_state(0,hardware_batch && hardware_count,s_gpu.tris_drawn-before);
        material132_state(hardware_batch && hardware_count,s_gpu.tris_drawn-before);
        geometry133_capture(hardware_batch && hardware_count,s_gpu.tris_drawn-before);
    }
#endif
    capture_world_batch(hardware_batch);
    hardware_batch=0;

    /* Capture the surface while the geometry is still on it.
     *
     * The periodic report dumps too, but a title clears every frame and draws
     * in only some of them, so a report almost always lands on a surface that
     * was wiped a moment ago -- which reads as "nothing was drawn" when the
     * triangles went down correctly just before it. A few frames that actually
     * contain geometry are worth more than any number of clears. */
    if (s_gpu.tris_drawn != before && s_drawn_dumps < FB_DUMP_AFTER_DRAW) {
        s_drawn_dumps++;
        dump_surface_bmp();
    }
}

/* What a batch actually contains. Before anything can be rasterised, the
 * question is what space attribute 0 arrives in: a title running a vertex
 * program hands over object-space positions that mean nothing without running
 * it, while pre-transformed screen-space coordinates can be drawn directly. */
static void draw_primitive(void)
{
    driving_diag510_route(P510_GENERIC,6,s_gpu.prim,s_gpu.idx_count);
    float v[4];
    uint32_t i;

    if (!s_gpu.prim || !s_gpu.idx_count)
        return;
    s_gpu.draws++;
    if (verbose_enabled() && (s_gpu.draws % 20000) == 0)
        fprintf(stderr, "  [GPU] draw #%u\n", s_gpu.draws);
    s_gpu.verts += s_gpu.idx_count;

    /* How many batches carry coordinates at all, and what range they span.
     * A pipeline that decodes perfectly and draws nothing is indistinguishable
     * from one that never ran, unless the vertices themselves are measured. */
    {
        float p[4];
        if (fetch_attr(&s_gpu.attr[0], s_gpu.idx[0], p)) {
            if (p[0] != 0.0f || p[1] != 0.0f || p[2] != 0.0f) {
                s_gpu.nonzero_draws++;
                if (p[0] < s_gpu.min_x) s_gpu.min_x = p[0];
                if (p[0] > s_gpu.max_x) s_gpu.max_x = p[0];
                if (p[1] < s_gpu.min_y) s_gpu.min_y = p[1];
                if (p[1] > s_gpu.max_y) s_gpu.max_y = p[1];
            }
        }
    }

    raster_batch();

    if (verbose_enabled()) {
        static int shown;
        if (shown++ < 6) {
            fprintf(stderr, "  [GPU] prim %u, %u indices, pos attr:"
                            " off 0x%08X type %u size %u stride %u\n",
                    s_gpu.prim, s_gpu.idx_count, s_gpu.attr[0].offset,
                    s_gpu.attr[0].type, s_gpu.attr[0].size, s_gpu.attr[0].stride);
            /* The texture stage, for either kind of batch. This used to print
             * only for inline batches, which meant a title drawing through
             * vertex arrays -- Half-Life 2's menu, for one -- showed no
             * texture state at all, and the reason a quad sampled flat was
             * invisible. */
            fprintf(stderr, "  [GPU]   tex: off 0x%08X %ux%u pitch %u"
                            " colour 0x%02X swizzled %d valid %d\n",
                    s_gpu.tex.offset, s_gpu.tex.width, s_gpu.tex.height,
                    s_gpu.tex.pitch, s_gpu.tex.color,
                    d3d8_format_is_swizzled(s_gpu.tex.color), s_gpu.tex.valid);
            {
                uint32_t k;
                for (k = 0; k < s_gpu.idx_count && k < 3; k++) {
                    float t[2];
                    if (fetch_texcoord(s_gpu.idx[k], t))
                        fprintf(stderr, "  [GPU]   uv[%u] = %.3f %.3f\n",
                                k, t[0], t[1]);
                }
            }
            /* An inline batch has no guest buffer to go and look at -- the
             * vertices are the payload -- so print the payload too. */
            if (s_gpu.inline_active) {
                uint32_t k;
                fprintf(stderr, "  [GPU]   inline %u dwords:", s_gpu.inline_count);
                for (k = 0; k < s_gpu.inline_count && k < 16; k++)
                    fprintf(stderr, " %08X", s_gpu.inline_buf[k]);
                fprintf(stderr, "\n");
                for (k = 0; k < s_gpu.idx_count && k < 4; k++) {
                    float t[2];
                    if (fetch_texcoord(s_gpu.idx[k], t))
                        fprintf(stderr, "  [GPU]   uv[%u] = %.3f %.3f\n",
                                k, t[0], t[1]);
                }
                fprintf(stderr, "  [GPU]   tex: off 0x%08X %ux%u pitch %u"
                                " colour 0x%02X valid %d\n",
                        s_gpu.tex.offset, s_gpu.tex.width, s_gpu.tex.height,
                        s_gpu.tex.pitch, s_gpu.tex.color, s_gpu.tex.valid);
            }
            {
                /* Every attribute the batch has, not just position. If the
                 * other streams carry data and position does not, the problem
                 * is one buffer rather than the whole vertex path. */
                const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
                uint32_t a, k;
                for (a = 0; a < NV_VERTEX_ATTRS; a++) {
                    const VertexAttr *at = &s_gpu.attr[a];
                    uint32_t nz = 0;
                    if (!at->offset || !at->size)
                        continue;
                    if(nf_hw_clear_pending)nightfire_gpu_read_guard(at->offset,64);
                    for (k = 0; k < 64; k++)
                        if (mem[at->offset + k]) nz++;
                    fprintf(stderr, "  [GPU]   attr%-2u off 0x%08X type %u"
                                    " size %u stride %-3u  %u/64 bytes set\n",
                            a, at->offset, at->type, at->size, at->stride, nz);
                }
            }
            {
                /* Raw bytes at the array, in case the values read as zero:
                 * that looks the same whether the offset is wrong or the
                 * buffer genuinely has not been filled yet. */
                const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
                uint32_t k;
                fprintf(stderr, "  [GPU]   bytes @0x%08X:", s_gpu.attr[0].offset);
                if(nf_hw_clear_pending)nightfire_gpu_read_guard(s_gpu.attr[0].offset,32);
                for (k = 0; k < 32; k++)
                    fprintf(stderr, " %02X", mem[s_gpu.attr[0].offset + k]);
                fprintf(stderr, "\n");
                fprintf(stderr, "  [GPU]   indices:");
                for (k = 0; k < s_gpu.idx_count && k < 8; k++)
                    fprintf(stderr, " %u", s_gpu.idx[k]);
                fprintf(stderr, "\n");
            }
            for (i = 0; i < s_gpu.idx_count && i < 3; i++) {
                if (fetch_attr(&s_gpu.attr[0], s_gpu.idx[i], v))
                    fprintf(stderr, "  [GPU]   v[%u] = %.3f %.3f %.3f %.3f\n",
                            s_gpu.idx[i], v[0], v[1], v[2], v[3]);
            }
        }
    }
}

/* Draw the vertices the title wrote straight into the pushbuffer.
 *
 * INLINE_ARRAY carries no offsets and no indices: the dwords between BEGIN and
 * END *are* the vertex buffer, packed in attribute order using the same
 * SET_VERTEX_DATA_ARRAY_FORMAT registers an ordinary array would use. So the
 * whole batch is describable as a vertex array whose base happens to be that
 * payload, which means synthesising the layout and handing it to the existing
 * path -- rather than a second copy of the topology and rasterisation code.
 *
 * The title's own attribute table is saved and put back: these offsets and
 * strides are ours, and it has not stopped using its.
 *
 * ponytail: each attribute is padded to a whole dword. That is exact for the
 * float and D3DCOLOR formats every inline batch actually uses; a packed
 * sub-dword attribute would need the unpadded layout.
 */
static void draw_inline_array(void)
{
    VertexAttr saved[NV_VERTEX_ATTRS];
    uint32_t off = 0, a, i, vsize, count;

    memcpy(saved, s_gpu.attr, sizeof saved);

    for (a = 0; a < NV_VERTEX_ATTRS; a++) {
        uint32_t bytes;
        if (!s_gpu.attr[a].size)
            continue;
        switch (s_gpu.attr[a].type) {
        case 0:  bytes = 4;                        break;  /* D3DCOLOR   */
        case 2:  bytes = 4 * s_gpu.attr[a].size;   break;  /* float      */
        case 4:  bytes = s_gpu.attr[a].size;       break;  /* ubyte norm */
        default: bytes = 4 * s_gpu.attr[a].size;   break;
        }
        s_gpu.attr[a].offset = off;
        off += (bytes + 3u) & ~3u;
    }
    vsize = off;
    if (!vsize)
        goto out;

    count = (s_gpu.inline_count * 4) / vsize;
    if (count < 3 || count > NV_MAX_INDICES)
        goto out;
    for (a = 0; a < NV_VERTEX_ATTRS; a++)
        if (s_gpu.attr[a].size)
            s_gpu.attr[a].stride = vsize;

    for (i = 0; i < count; i++)
        s_gpu.idx[i] = (uint16_t)i;
    s_gpu.idx_count = count;

    s_gpu.inline_active = 1;
    draw_primitive();
    s_gpu.inline_active = 0;

out:
    memcpy(s_gpu.attr, saved, sizeof saved);
    s_gpu.idx_count = 0;
}

static void diagnostic_draw_state(void)
{
    if(gpu_diagnostics() && s_gpu.attr[0].size>=3){
        static uint32_t offsets[128];static unsigned used;
        unsigned found=0;for(unsigned j=0;j<used;j++)if(offsets[j]==s_gpu.tex.offset)found=1;
        if(!found && used<128){offsets[used++]=s_gpu.tex.offset;
            fprintf(stderr,"[WORLD-TEXTURE] offset=%08X size=%ux%u format=%08X filter=%08X control=%08X address=%08X\n",
                s_gpu.tex.offset,s_gpu.tex.width,s_gpu.tex.height,s_tex_reg[1],s_tex_reg[5],s_tex_reg[3],s_tex_reg[2]);}
    }
    static unsigned seen[32],count,lines;
    if(!gpu_diagnostics() || (diagnostic_presents<2450 && s_gpu.attr[0].size<3) || lines>=48) return;
    unsigned signature=diagnostic_vp_mode ^ (diagnostic_vp_start<<8) ^ (s_gpu.attr[0].type<<24) ^ (s_gpu.attr[0].size<<28);
    unsigned i;for(i=0;i<count;i++)if(seen[i]==signature)return;
    if(count<32)seen[count++]=signature;lines++;
    char trace_path[128];snprintf(trace_path,sizeof trace_path,"analysis/checkpoint-34/transform-%u.txt",lines);
    FILE *trace=fopen(trace_path,"w");
    if(trace){unsigned begin=transform_trace_count>3072?transform_trace_count-3072:0;
        for(unsigned n=begin;n<transform_trace_count;n++)fprintf(trace,"%08X %08X\n",transform_trace[n%3072].method,transform_trace[n%3072].value);
        fclose(trace);}
    fprintf(stderr,"[WORLD-DRAW] present=%u mode=%08X start=%08X prim=%u count=%u attr0 type=%u size=%u stride=%u offset=%08X screen_guess=%d\n",
        diagnostic_presents,diagnostic_vp_mode,diagnostic_vp_start,s_gpu.prim,s_gpu.idx_count,
        s_gpu.attr[0].type,s_gpu.attr[0].size,s_gpu.attr[0].stride,s_gpu.attr[0].offset,batch_is_screen_space());
    fprintf(stderr,"[WORLD-DEPTH] enable=%u func=%X mask=%u zeta=%08X pitch=%u format=%08X clear=%08X rect=%08X,%08X control0=%08X\n",s_gpu.depth_enable,s_gpu.depth_func,s_gpu.depth_mask,s_gpu.zeta_offset,s_gpu.zeta_pitch,s_gpu.format,s_gpu.zclear,s_gpu.clear_x,s_gpu.clear_y,s_gpu.control0);
    fprintf(stderr,"[WORLD-CULL] enable=%u face=%X front=%X culled=%u\n",s_gpu.cull_enable,s_gpu.cull_face,s_gpu.front_face,s_gpu.culled);
    for(i=0;i<s_gpu.idx_count && i<3;i++){float p[4];if(fetch_attr(&s_gpu.attr[0],s_gpu.idx[i],p))fprintf(stderr,"[WORLD-VERTEX] %.9g %.9g %.9g %.9g\n",p[0],p[1],p[2],p[3]);}
}
/* CP120: keep the common small batch allocation-free. Larger BEGIN/END
 * streams are retained in host storage until END, just like small streams;
 * do not draw while collecting commands, since that changes guest visibility
 * and the state used by the existing executor. Hardware staging stays 4096.
 * The safety ceiling bounds malformed command streams, not vertex values. */
#define NF_INDEX_STREAM_LIMIT120 (1024u * 1024u)
static uint16_t *index_stream120;
static unsigned index_count120, index_capacity120;
static int index_sources120_safe(void)
{
    NFIndexGuard120 guard = {0};
    uint64_t rows = (uint64_t)s_gpu.clip_y + s_gpu.clip_h;
    guard.color.address = dma_resolve(s_gpu.color_offset);
    guard.color.bytes = rows * s_gpu.pitch;
    uint32_t depth;
    if (depth_surface(&depth)) {
        guard.depth.address = depth;
        guard.depth.bytes = rows * s_gpu.zeta_pitch;
    }
    unsigned max_index = 0;
    for (unsigned i=0;i<index_count120;i++) if(index_stream120[i]>max_index) max_index=index_stream120[i];
    for (unsigned i=0;i<NV_VERTEX_ATTRS;i++) {
        const VertexAttr *a = &s_gpu.attr[i];
        if (a->offset && a->size &&
            !nf_index_guard120_vertex_range(a->offset,a->stride,max_index,&guard.vertices[guard.vertex_count++])) return 0;
    }
    guard.texture.enabled = s_gpu.tex.valid;
    guard.texture.address = s_gpu.tex.offset;
    guard.texture.format = s_tex_reg[1];
    guard.texture.width = s_gpu.tex.width;
    guard.texture.height = s_gpu.tex.height;
    guard.texture.pitch = s_gpu.tex.pitch;
    return nf_index_guard120_safe(&guard);
}
static void index_stream120_append(const uint16_t *indices, unsigned count)
{
    unsigned used = index_count120 ? index_count120 : s_gpu.idx_count;
    if (count > NF_INDEX_STREAM_LIMIT120 - used) {
        fprintf(stderr,"[GPU] index stream exceeds bounded host limit: %u + %u\n",used,count);
        exit(4);
    }
    if (!index_count120 && used + count <= NV_MAX_INDICES) {
        memcpy(s_gpu.idx + used, indices, (size_t)count * 2);
        s_gpu.idx_count += count;
        return;
    }
    if (used + count > index_capacity120) {
        unsigned capacity = index_capacity120 ? index_capacity120 : NV_MAX_INDICES * 2;
        while (capacity < used + count) capacity *= 2;
        uint16_t *grown = realloc(index_stream120, (size_t)capacity * 2);
        if (!grown) { fprintf(stderr,"[GPU] index stream allocation failed\n"); exit(4); }
        index_stream120 = grown;
        index_capacity120 = capacity;
    }
    if (!index_count120) memcpy(index_stream120, s_gpu.idx, (size_t)used * 2);
    memcpy(index_stream120 + used, indices, (size_t)count * 2);
    index_count120 = used + count;
}
static void draw_index_stream120(void)
{
    if (!index_count120) { draw_primitive(); return; }
    /* Re-reading an output alias between chunks would change this draw's
     * source data. Keep that uncommon, unimplemented case explicit. */
    if (!index_sources120_safe()) {
        fprintf(stderr,"[GPU] oversized index stream has unsupported source/target alias or range\n");
        exit(4);
    }
    unsigned start = 0, total = index_count120;
    if (s_gpu.prim != NV_PRIM_TRIANGLES && s_gpu.prim != NV_PRIM_TRIANGLE_STRIP &&
        s_gpu.prim != NV_PRIM_TRIANGLE_FAN && s_gpu.prim != NV_PRIM_QUADS &&
        s_gpu.prim != NV_PRIM_QUAD_STRIP) {
        fprintf(stderr,"[GPU] oversized unsupported primitive: %u indices=%u\n",s_gpu.prim,total);
        exit(4);
    }
    static unsigned shown;
    if (shown++ < 8) fprintf(stderr,"[GPU-INDEX120] splitting prim=%u indices=%u at END\n",s_gpu.prim,total);
    if (s_gpu.prim == NV_PRIM_TRIANGLE_FAN) start = 1;
    while (start < total) {
        unsigned take = total - start;
        unsigned capacity = s_gpu.prim == NV_PRIM_TRIANGLES ? NV_MAX_INDICES / 3 * 3 :
                            s_gpu.prim == NV_PRIM_TRIANGLE_FAN ? NV_MAX_INDICES - 1 : NV_MAX_INDICES;
        if (take > capacity) take = capacity;
        unsigned anchor = s_gpu.prim == NV_PRIM_TRIANGLE_FAN;
        if (anchor) s_gpu.idx[0] = index_stream120[0];
        memcpy(s_gpu.idx + anchor, index_stream120 + start, (size_t)take * 2);
        s_gpu.idx_count = take + anchor;
        draw_primitive();
        if (start + take == total) break;
        /* Even-sized strip chunks preserve winding, including degenerates.
         * Fans retain the original anchor and the last edge vertex. */
        if (anchor) start += take - 1;
        else if (s_gpu.prim == NV_PRIM_TRIANGLE_STRIP || s_gpu.prim == NV_PRIM_QUAD_STRIP)
            start += take - 2;
        else start += take;
    }
    index_count120 = 0;
    s_gpu.idx_count = 0;
}
void nv2a_pb_exec_method(uint32_t subch, uint32_t method, uint32_t param);
/* Exact bulk equivalent for the two plain repeated-data methods. The first
 * word follows the normal path, including one-time initialization. No vertex
 * program, combiner or trace state is affected by subsequent data words. */
unsigned nv2a_pb_exec_repeat(uint32_t subch,uint32_t method,const uint32_t *values,unsigned count)
{
    static int enabled=-1;
    if(enabled<0){const char *v=getenv("NIGHTFIRE_GPU_BULK");enabled=!v || strcmp(v,"0");}
    if(!enabled || !count || (method!=NV097_ARRAY_ELEMENT16 && method!=NV097_INLINE_ARRAY))return 0;
    nv2a_pb_exec_method(subch,method,values[0]);
    unsigned copied=count-1;
    if(s_gpu.prim && copied){
        if(method==NV097_ARRAY_ELEMENT16){
            index_stream120_append((const uint16_t *)(values+1),copied*2);
        }else{
            unsigned room=s_gpu.inline_count<=NV_MAX_INLINE?NV_MAX_INLINE-s_gpu.inline_count:0;if(copied>room)copied=room;
            if(copied)memcpy(s_gpu.inline_buf+s_gpu.inline_count,values+1,copied*4);s_gpu.inline_count+=copied;
        }
    }
    return count; /* Every index word is retained; inline handling is unchanged. */
}
void nv2a_pb_exec_method(uint32_t subch, uint32_t method, uint32_t param)
{
    if(vertex_program_enabled())nf_vp_method(&vertex_program,method,param);
    if(method==0x1e94)diagnostic_vp_mode=param;
    if(method==0x1ea0)diagnostic_vp_start=param;
    if(gpu_diagnostics() && ((method>=0xb00 && method<0xc00) || (method>=0x1e94 && method<=0x1ea4) ||
       (method>=0xa20 && method<0xa30) || (method>=0xaf0 && method<0xb00))) {
        transform_trace[transform_trace_count%3072].method=method;
        transform_trace[transform_trace_count%3072].value=param;transform_trace_count++;
    }
    switch(method) {
    case 0x29c:fog_mode=param;break;case 0x2a0:fog_gen=param;break;
    case 0x2a4:fog_enable=param;break;case 0x2a8:fog_color=param;break;
    case 0x9c0:case 0x9c4:case 0x9c8:fog_params[(method-0x9c0)/4]=param;break;
    case 0x260:combiner_alpha=param;break;case 0xac0:combiner_rgb=param;break;
    case 0xaa0:combiner_alpha_out=param;break;case 0x1e40:combiner_rgb_out=param;break;
    case 0x1e60:combiner_count=param;break;case 0x288:final0=param;break;case 0x28c:final1=param;break;
    }
    if(method==0x260 || method==0x288 || method==0x28c || method==0xaa0 || method==0xac0 || method==0x1e40 || method==0x1e60) {
        static unsigned count;
        if(count++<96) fprintf(stderr,"[COMBINER] method=%04X value=%08X\n",method,param);
    }
    static int inited;
    if (!inited) {
        inited = 1;
        s_gpu.min_x = s_gpu.min_y = 1e30f;
        s_gpu.max_x = s_gpu.max_y = -1e30f;
        s_gpu.alpha_func=0x207; s_gpu.blend_src=1; s_gpu.blend_equation=0x8006;
    }
    /* Pinned nv2a_regs.h register values; alpha is not an opaque RGB texel. */
    uint32_t *state=NULL;
    switch(method) {
    case 0x37c:state=&shade_mode;break;
    case 0x308:state=&s_gpu.cull_enable;break;
    case 0x39c:state=&s_gpu.cull_face;break;
    case 0x3a0:state=&s_gpu.front_face;break;
    case 0x290:state=&s_gpu.control0;break;
    case 0x30c:state=&s_gpu.depth_enable;break;
    case 0x354:state=&s_gpu.depth_func;break;
    case 0x35c:state=&s_gpu.depth_mask;break;
    case 0x214:state=&s_gpu.zeta_offset;break;
    case 0x1d8c:state=&s_gpu.zclear;break;
    case 0x1d98:state=&s_gpu.clear_x;break;
    case 0x1d9c:state=&s_gpu.clear_y;break;
    case 0x300:state=&s_gpu.alpha_test;break;
    case 0x304:state=&s_gpu.blend_enable;break;
    case 0x33c:state=&s_gpu.alpha_func;break;
    case 0x340:state=&s_gpu.alpha_ref;break;
    case 0x344:state=&s_gpu.blend_src;break;
    case 0x348:state=&s_gpu.blend_dst;break;
    case 0x34c:state=&s_gpu.blend_color;break;
    case 0x350:state=&s_gpu.blend_equation;break;
    }
    if(state) {
        static unsigned logs;
        *state=param;
        if(logs++<32) fprintf(stderr,"[GPU-BLEND] method=%04X value=%08X\n",method,param);
        return;
    }
    /* Bring-up: the first parameters each surface method carries. A wrong
     * pitch or clip is indistinguishable from a method never arriving unless
     * the values are visible. */
    if (verbose_enabled()) {
        static int shown[8];
        int slot = -1;
        switch (method) {
        case NV097_SET_SURFACE_CLIP_HORIZONTAL: slot = 0; break;
        case NV097_SET_SURFACE_CLIP_VERTICAL:   slot = 1; break;
        case NV097_SET_SURFACE_FORMAT:          slot = 2; break;
        case NV097_SET_SURFACE_PITCH:           slot = 3; break;
        case NV097_SET_SURFACE_COLOR_OFFSET:    slot = 4; break;
        case NV097_SET_COLOR_CLEAR_VALUE:       slot = 5; break;
        case NV097_CLEAR_SURFACE:               slot = 6; break;
        default: break;
        }
        if (slot >= 0 && shown[slot]++ < 4)
            fprintf(stderr, "  [GPU] subch %u method 0x%04X param 0x%08X\n",
                    subch, method, param);
    }

    if (subch != 0) {                      /* 3D class lives on subchannel 0 */
        note_unhandled(method);
        return;
    }
#ifdef NIGHTFIRE_SHADOW_LAYOUT131_DIAGNOSTIC
    /* Observe ignored polygon-offset registers without changing their handling. */
    /* Pinned nv2a_regs.h: FILL_ENABLE, SCALE_FACTOR, BIAS respectively. */
    if(method==0x338)material132_offset_enable=param;
    if(method==0x384)material132_offset_scale=param;
    if(method==0x388)material132_offset_bias=param;
#endif
    switch (method) {
    case NV097_SET_SURFACE_CLIP_HORIZONTAL:
        s_gpu.clip_x = param & 0xFFFF;
        s_gpu.clip_w = (param >> 16) & 0xFFFF;
        break;
    case NV097_SET_SURFACE_CLIP_VERTICAL:
        s_gpu.clip_y = param & 0xFFFF;
        s_gpu.clip_h = (param >> 16) & 0xFFFF;
        break;
    case NV097_SET_SURFACE_FORMAT:
        s_gpu.format = param;
        break;
    case NV097_SET_SURFACE_PITCH:
        s_gpu.pitch = param & 0xFFFF;      /* colour pitch; zeta is the top half */
        s_gpu.zeta_pitch=param>>16;
        break;
    case NV097_SET_SURFACE_COLOR_OFFSET:
        s_gpu.color_offset = param;
        break;
    case NV097_SET_COLOR_CLEAR_VALUE:
        s_gpu.clear_color = param;
        break;
    case NV097_CLEAR_SURFACE:
        clear_surface(param);
        break;

    case NV097_SET_BEGIN_END:
        if (param) {
            s_gpu.prim = param;
            s_gpu.idx_count = 0;
            index_count120 = 0;
            s_gpu.inline_count = 0;
        } else {
            driving_diag510_route(P510_GENERIC,7,s_gpu.prim,s_gpu.inline_count);
            if (s_gpu.inline_count)
                draw_inline_array();
            else
                draw_index_stream120();
            s_gpu.prim = 0;
            s_gpu.inline_count = 0;
        }
        break;

    case NV097_INLINE_ARRAY:
        /* Vertex data, not a pointer to it. Buffered rather than decoded here
         * because the format is only fully known at END. */
        if (s_gpu.prim && s_gpu.inline_count < NV_MAX_INLINE)
            s_gpu.inline_buf[s_gpu.inline_count++] = param;
        break;

    case NV097_ARRAY_ELEMENT16:
        /* Two 16-bit indices per parameter word. */
        if (s_gpu.prim) {
            uint16_t pair[2] = {(uint16_t)param, (uint16_t)(param >> 16)};
            index_stream120_append(pair, 2);
        }
        break;
    case NV097_ARRAY_ELEMENT32:
        /* The PAL DrawIndexedVertices path emits an odd trailing index with
         * this method (00104C80). Dropping it removes the final triangle. */
        if(s_gpu.prim){
            if(param>0xffff){fprintf(stderr,"[GPU] unsupported index32 value: %08X\n",param);exit(4);}
            uint16_t index = (uint16_t)param;
            index_stream120_append(&index, 1);
        }
        break;
    case NV097_SET_TEXTURE_OFFSET:
        /* A texture offset is a DMA-object offset, exactly like a surface or a
         * vertex array offset -- physical, and reachable only through the
         * contiguous window when it names contiguous memory. */
        s_gpu.tex.offset = texture_resolve(param);
        {
            static unsigned shown;
            if (shown++ < 8)
                fprintf(stderr, "[GPU-TEXADDR] submitted=%08X resolved=%08X contiguous_bytes=%08X\n",
                        param, s_gpu.tex.offset, xbox_ContiguousAllocatedBytes());
        }
        record_tex_reg(method, param);
        break;

    case NV097_SET_FLIP_READ:
        s_gpu.flip_read = param;
        return;

    case NV097_SET_FLIP_WRITE:
        s_gpu.flip_write = param;
        return;

    case NV097_SET_FLIP_MODULO:
        s_gpu.flip_modulo = param;
        return;

    case NV097_FLIP_INCREMENT_WRITE:
        s_gpu.flip_write = s_gpu.flip_modulo
                         ? (s_gpu.flip_write + 1) % s_gpu.flip_modulo
                         : s_gpu.flip_write + 1;
        s_gpu.flips++;
        return;

    case NV097_FLIP_STALL:
        /* The stall ends when the buffer being read is the one just finished.
         * There is no scanout here to wait for, so that is now. */
        s_gpu.flip_read = s_gpu.flip_write;
        if (verbose_enabled()) {
            static unsigned n;
            if (n++ < 8) {
                fprintf(stderr, "  [GPU] flip %u: read=%u write=%u\n",
                        s_gpu.flips, s_gpu.flip_read, s_gpu.flip_write);
                fflush(stderr);
            }
        }
        return;

    case NV097_SET_TEXTURE_FORMAT:
        s_gpu.tex.color = (param >> 8) & 0xFF;
        /* A swizzled texture carries its own dimensions here, as log2 in
         * BASE_SIZE_U/V (nv2a_regs.h: 0x00F00000 / 0x0F000000). It has to:
         * SET_TEXTURE_IMAGE_RECT describes a linear image, and a title that
         * only uses swizzled textures never sends one -- this title sends it
         * once and sets a format 3,176 times. Without this the width and
         * height stayed zero and nothing was ever sampled. */
        if (tex_size_from_format((param >> 8) & 0xFF)) {
            s_gpu.tex.width  = 1u << ((param >> 20) & 0xF);
            s_gpu.tex.height = 1u << ((param >> 24) & 0xF);
        }
        record_tex_reg(method, param);
        break;

    case NV097_SET_TEXTURE_ADDRESS:
        /* Four bits per axis. 1 is wrap, 3 is clamp-to-edge; the rest (mirror,
         * border) fall back to clamp, which is wrong at an edge rather than
         * wrong everywhere. */
        s_gpu.tex.addr_u =  param        & 0xF;
        s_gpu.tex.addr_v = (param >>  8) & 0xF;
        record_tex_reg(method, param);
        break;

    case NV097_SET_TEXTURE_CONTROL1:
        /* Pitch lives in the top half. Only meaningful for a linear format; a
         * swizzled texture has no pitch because it has no rows. */
        s_gpu.tex.pitch = param >> 16;
        record_tex_reg(method, param);
        break;

    case NV097_SET_TEXTURE_IMAGE_RECT:
        s_gpu.tex.width  = param >> 16;
        s_gpu.tex.height = param & 0xFFFF;
        record_tex_reg(method, param);
        break;

    default:
        if (method >= NV_TEX_FIRST && method <= NV_TEX_LAST)
            record_tex_reg(method, param);
        if (method >= NV097_SET_VERTEX_DATA_ARRAY_OFFSET
                && method < NV097_SET_VERTEX_DATA_ARRAY_OFFSET + NV_VERTEX_ATTRS * 4) {
            /* Resolved here, once, so every consumer -- the rasteriser's
             * attribute reads and the diagnostics alike -- sees the same
             * address. A vertex array offset is a DMA-object offset exactly
             * like a surface offset: physical, and addressable only through
             * the window when it names contiguous memory. */
            s_gpu.attr[(method - NV097_SET_VERTEX_DATA_ARRAY_OFFSET) / 4].offset =
                dma_resolve(param);
        } else if (method >= NV097_SET_VERTEX_DATA_ARRAY_FORMAT
                && method < NV097_SET_VERTEX_DATA_ARRAY_FORMAT + NV_VERTEX_ATTRS * 4) {
            VertexAttr *a = &s_gpu.attr[(method - NV097_SET_VERTEX_DATA_ARRAY_FORMAT) / 4];
            a->type   =  param        & 0x0F;
            a->size   = (param >> 4)  & 0x0F;
            a->stride = (param >> 8)  & 0xFF;
        } else {
            note_unhandled(method);
        }
        break;
    }
}

/* Find where the title actually wrote its quad.
 *
 * The GPU is pointed at a buffer that stays zero, which says the data went
 * somewhere else -- and the only way to find somewhere else is to look for the
 * data. A screen-space quad for a 640x480 target contains 640.0f and 480.0f as
 * floats, which is a distinctive enough pair to search guest RAM for. Whatever
 * address that turns up is where the title's writes are landing, and the
 * difference from the programmed offset is the bug. */
static void find_quad_vertices(void)
{
    const uint32_t W = 0x44200000u;   /* 640.0f */
    const uint32_t H = 0x43F00000u;   /* 480.0f */
    const uint32_t *ram = (const uint32_t *)xbox_GetMemoryOffset();
    uint32_t i, hits = 0;

    fprintf(stderr, "[GPU] searching guest RAM for 640.0f/480.0f pairs...\n");
    for (i = 0x1000 / 4; i < (0x04000000u / 4) - 8 && hits < 12; i++) {
        if (ram[i] != W && ram[i] != H)
            continue;
        /* Both values within a few words of each other: a lone 640.0f is
         * common, the pair much less so. */
        {
            int has_w = 0, has_h = 0;
            uint32_t k;
            for (k = 0; k < 8; k++) {
                if (ram[i + k] == W) has_w = 1;
                if (ram[i + k] == H) has_h = 1;
            }
            if (!has_w || !has_h)
                continue;
        }
        hits++;
        fprintf(stderr, "  [GPU]   0x%08X:", i * 4);
        {
            uint32_t k;
            for (k = 0; k < 8; k++)
                fprintf(stderr, " %08X", ram[i + k]);
        }
        fprintf(stderr, "\n");
        i += 8;
    }
    if (!hits)
        fprintf(stderr, "  [GPU]   none found -- the quad is not in RAM in"
                        " that form\n");
    fflush(stderr);
}

/* Locate NaN-filled transform matrices in guest RAM.
 *
 * A matrix arriving as NaN says the maths went wrong somewhere upstream, and
 * the only way to find where is to find the matrix and watch who writes it.
 * Three consecutive real-indefinite values is a distinctive enough signature:
 * ordinary data does not contain runs of 0xFFC00000. */
static void find_nan_matrices(void)
{
    const uint32_t NAN_NEG = 0xFFC00000u;
    const uint32_t *ram = (const uint32_t *)xbox_GetMemoryOffset();
    uint32_t i, hits = 0;

    fprintf(stderr, "[GPU] searching guest RAM for NaN matrices...\n");
    for (i = 0x1000 / 4; i < (0x04000000u / 4) - 20 && hits < 10; i++) {
        if (ram[i] != NAN_NEG || ram[i + 1] != NAN_NEG || ram[i + 2] != NAN_NEG)
            continue;
        hits++;
        fprintf(stderr, "  [GPU]   0x%08X:", i * 4);
        {
            uint32_t k;
            for (k = 0; k < 16; k++)
                fprintf(stderr, " %08X", ram[i + k]);
        }
        fprintf(stderr, "\n");
        i += 16;
    }
    if (!hits)
        fprintf(stderr, "  [GPU]   none in RAM -- the NaNs are computed into"
                        " registers, not stored\n");
    fflush(stderr);
}

/* Print guest dwords named by RECOMP_PEEK, as hex and as float.
 *
 * Chasing a value backwards means reading it, and a value that is only wrong
 * for one frame in a thousand cannot be caught by stopping. Both
 * interpretations are printed because the question is usually "is this a
 * pointer or a number", and guessing wrong costs a run. */
static void peek_addresses(void)
{
    const char *spec = getenv("RECOMP_PEEK");
    const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
    char buf[256];
    char *tok, *ctx = NULL;

    if (!spec)
        return;
    strncpy(buf, spec, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    for (tok = strtok_s(buf, ",", &ctx); tok; tok = strtok_s(NULL, ",", &ctx)) {
        uint32_t va = (uint32_t)strtoul(tok, NULL, 0);
        uint32_t v;
        float f;
        if (va < 0x1000u || va >= 0x04000000u)
            continue;
        v = *(const uint32_t *)(mem + va);
        memcpy(&f, &v, 4);
        fprintf(stderr, "  [PEEK] 0x%08X = %08X  (%g)\n", va, v, f);
    }
    fflush(stderr);
}

/* Walk a pointer chain and print every step.
 *
 * RECOMP_PEEK_CHAIN="0x1315A8,8,0x10,0" starts at that address, and for each
 * offset dereferences the current pointer and adds it. Following a chain by
 * hand costs one run per level; this costs one run for the whole chain, and
 * prints where it goes wrong when a level is null.
 */
static void peek_chain(void)
{
    const char *spec = getenv("RECOMP_PEEK_CHAIN");
    const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
    char buf[256], *tok, *ctx = NULL;
    uint32_t cur = 0;
    int step = 0;

    if (!spec)
        return;
    strncpy(buf, spec, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    for (tok = strtok_s(buf, ",", &ctx); tok; tok = strtok_s(NULL, ",", &ctx)) {
        uint32_t off = (uint32_t)strtoul(tok, NULL, 0);
        if (step == 0) {
            cur = off;
            fprintf(stderr, "  [CHAIN] start 0x%08X\n", cur);
        } else {
            if (cur < 0x1000u || cur + 4 >= 0x04000000u) {
                fprintf(stderr, "  [CHAIN] step %d: 0x%08X is not a guest"
                                " pointer -- chain ends\n", step, cur);
                return;
            }
            cur = *(const uint32_t *)(mem + cur) + off;
            fprintf(stderr, "  [CHAIN] step %d: deref +0x%X -> 0x%08X\n",
                    step, off, cur);
        }
        step++;
    }
    if (cur >= 0x1000u && cur + 4 < 0x04000000u)
        fprintf(stderr, "  [CHAIN] final value at 0x%08X = 0x%08X\n",
                cur, *(const uint32_t *)(mem + cur));
    fflush(stderr);
}

void nv2a_pb_exec_report(void)
{
    peek_addresses();
    peek_chain();
    if (getenv("RECOMP_FIND_NAN")) {
        /* Every report, not once: the matrix is fine early on and only turns
         * to NaN later, so a single scan at startup finds nothing and says
         * nothing. */
        find_nan_matrices();
    }
    if (getenv("RECOMP_FIND_QUAD")) {
        static int done;
        if (!done) { done = 1; find_quad_vertices(); }
    }
    int i, j;

    fprintf(stderr, "[GPU] surface 0x%08X pitch %u clip %ux%u+%u+%u"
                    " clears %u | %u unhandled methods (%d distinct)\n",
            s_gpu.color_offset, s_gpu.pitch, s_gpu.clip_w, s_gpu.clip_h,
            s_gpu.clip_x, s_gpu.clip_y, s_gpu.clears,
            s_gpu.unhandled_total, s_unhandled_count);
    fprintf(stderr, "[GPU] draws %u (%u with coordinates), %u indices;"
                    " x %.1f..%.1f  y %.1f..%.1f\n",
            s_gpu.draws, s_gpu.nonzero_draws, s_gpu.verts,
            s_gpu.min_x, s_gpu.max_x, s_gpu.min_y, s_gpu.max_y);
    /* One picture per report rather than per clear: a title clears hundreds of
     * times a second and nobody wants that many files. */
    dump_surface_bmp();

    /* Drawn and skipped separately: "nothing appeared" and "every batch needed
     * a vertex program we do not run" look identical on screen, and only one
     * of them means the rasteriser is broken. */
    fprintf(stderr, "[GPU] rasterised %u triangles; %u batches skipped as not"
                    " screen-space, %u triangles fully off-surface\n",
            s_gpu.tris_drawn, s_gpu.batches_untransformed,
            s_gpu.tris_skipped_offscreen);

    /* And of the batches that did rasterise, how many sampled anything. A menu
     * that draws its background from one texture and its text from another
     * shows both as flat colour if either half is missing, so the split is
     * what says which half. */
    fprintf(stderr, "[GPU] batches: %u textured, %u with no texcoords,"
                    " %u with texcoords but no usable stage\n",
            s_gpu.batches_textured, s_gpu.batches_no_uv, s_gpu.batches_no_tex);
    for (i = 0; i < s_tex_use_count; i++)
        fprintf(stderr, "  [TEXUSE] 0x%08X %ux%u fmt 0x%02X%s: %u batches\n",
                s_tex_use[i].offset, s_tex_use[i].width, s_tex_use[i].height,
                s_tex_use[i].color,
                d3d8_format_dxt_block_bytes(s_tex_use[i].color) ? " dxt"
                    : d3d8_format_is_swizzled(s_tex_use[i].color) ? " swz" : " lin",
                s_tex_use[i].batches);

    if (getenv("RECOMP_TEX_STATE")) {
        uint32_t k;
        for (k = 0; k < sizeof s_tex_set / sizeof s_tex_set[0]; k++)
            if (s_tex_set[k])
                fprintf(stderr, "  [TEX] 0x%04X = 0x%08X\n",
                        (unsigned)(NV_TEX_FIRST + k * 4), s_tex_reg[k]);
    }

    /* Top ten by frequency: selection sort over a small table, once every few
     * seconds, is not worth a better algorithm.
     *
     * RECOMP_PB_UNHANDLED_ALL lists every one instead. Ten is the right
     * default -- the tail is a long list of state registers nobody needs to
     * read -- but when a title stops and the question is which method it
     * stopped on, the answer is as likely to be the one seen twice as the
     * one seen a thousand times, and ten hides it. */
    {
        int shown = getenv("RECOMP_PB_UNHANDLED_ALL") ? s_unhandled_count : 10;
    for (i = 0; i < shown && i < s_unhandled_count; i++) {
        int best = i;
        for (j = i + 1; j < s_unhandled_count; j++)
            if (s_unhandled[j].count > s_unhandled[best].count)
                best = j;
        if (best != i) {
            PbUnhandled t = s_unhandled[i];
            s_unhandled[i] = s_unhandled[best];
            s_unhandled[best] = t;
        }
        fprintf(stderr, "  [GPU]   0x%04X x%u\n",
                s_unhandled[i].method, s_unhandled[i].count);
    }
    }    fflush(stderr);
}


