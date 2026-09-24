/* Conservative synchronous adapter for two captured 256-square draw families.
 * Separate owned linear target staging preserves original swizzled guest layout.
 * This bring-up path favors inspectable ownership over performance. */
#include "driving_plan214.h"
#include "driving_vertex212.h"
static int overlap214(uint32_t a,uint32_t b){return (uint64_t)a+262144>b && (uint64_t)b+262144>a;}
static int submit214(const uint32_t *state,const unsigned char *known,NFVertexProgram *program,
                     unsigned primitive,const uint32_t *indices,unsigned count,unsigned family)
{
    if(family>1 || !indices || count<3 || count>8192 || driving_plan214(state,known,program,primitive)!=(int)family)return 0;
    unsigned dense_count=family?(count-2)*3:count;
    if(dense_count>16384)return 0;
    uint32_t ramht,allocated=xbox_ContiguousAllocatedBytes();DrivingSpan183 c,z,t,streams[3];
    if(!read143(NULL,0xfd002210,&ramht))return 0;
    if(!nf_hw_sync())fail143("world prior GPU completion214",family,count);
    if(!driving_span183(NULL,read143,ramht,state,known,DRIVING_COLOR183,0,262144,allocated,&c) ||
       !driving_span183(NULL,read143,ramht,state,known,DRIVING_DEPTH183,0,262144,allocated,&z) ||
       !driving_span183(NULL,read143,ramht,state,known,DRIVING_TEXTURE183,family?3:0,262144,allocated,&t))return 0;
    if(overlap214(c.address,z.address)||overlap214(c.address,t.address)||overlap214(z.address,t.address))return 0;
    void *color=mapped201(c.address,262144,1),*depth=mapped201(z.address,262144,1),*texture=mapped201(t.address,262144,0);
    if(!color||!depth||!texture)return 0;
    unsigned maximum=0;for(unsigned i=0;i<count;i++)if(indices[i]>maximum)maximum=indices[i];
    const uint8_t *source[3];uint32_t length[3];
    for(unsigned slot=0;slot<3;slot++){
        uint32_t fmt=state[(0x1760+4*slot)/4],kind=fmt&15,n=(fmt>>4)&15;
        unsigned bytes=n*(kind==2?4:kind==5?2:1);
        uint64_t need=(uint64_t)maximum*(fmt>>8)+bytes;
        if(!need || need>0x08000000 || !driving_span183(NULL,read143,ramht,state,known,DRIVING_VERTEX183,slot,(uint32_t)need,allocated,&streams[slot]))return 0;
        source[slot]=mapped201(streams[slot].address,(size_t)need,0);if(!source[slot])return 0;length[slot]=(uint32_t)need;
    }
    NFHardwareVertex *dense=malloc((size_t)dense_count*sizeof *dense);
    uint8_t *owned=malloc(262144*3);if(!dense||!owned){free(dense);free(owned);return 0;}
    int accepted=0;
    for(unsigned i=0;i<dense_count;i++){
        unsigned at=i;
        if(family){unsigned tri=i/3,k=i%3;at=tri+k;if(tri&1){if(k==0)at=tri+1;else if(k==1)at=tri;}}
        float in[16][4]={{0}},out[16][4];
        for(unsigned slot=0;slot<3;slot++)if(!driving_vertex212(source[slot],length[slot],state[(0x1760+4*slot)/4],0,indices[at],in[slot]))goto done214;
        if(!nf_vp_run(program,in,out))goto done214;
        unsigned texslot=family?12:9;
        if(out[0][3]!=1 || out[texslot][3]!=1 || (!family && out[5][0]!=1))goto done214;
        for(unsigned k=0;k<4;k++)if(!isfinite(out[0][k]) || !isfinite(out[texslot][k]) || !isfinite(out[3][k]) || out[3][k]<0 || out[3][k]>1)goto done214;
        unsigned rgba[4];for(unsigned k=0;k<4;k++){
            rgba[k]=(unsigned)floorf(out[3][k]*255+0.5f);
            if((float)rgba[k]/255.0f!=out[3][k] || (family && out[3][k]!=1))goto done214;
        }
        memcpy(dense[i].position,out[0],16);memcpy(dense[i].uv,out[texslot],8);
        dense[i].color=(rgba[3]<<24)|(rgba[0]<<16)|(rgba[1]<<8)|rgba[2];dense[i].fog=1;
    }
    nf_swizzled131_import_size(owned,1024,color,256);
    nf_swizzled131_import_size(owned+262144,1024,depth,256);memcpy(owned+524288,texture,262144);
    NFHardwareState s={0};s.color=owned;s.depth=owned+262144;s.width=s.height=s.right=s.bottom=256;s.pitch=s.depth_pitch=1024;
    s.depth_enable=1;s.depth_func=0x203;s.depth_write=family;s.alpha_func=0x204;
    s.blend_enable=family;s.blend_src=0x302;s.blend_dst=0x303;s.combiner=1;s.scale=family?1:2;s.color_write_mask212=0x17;
    s.texture=owned+524288;s.texture_available=262144;s.texture_format=0x08810629;s.texture_address=0x10101;s.texture_filter=0x02062000;s.filtered=1;s.sampler_anisotropy213=family?1:8;
    /* begin may retain staging before a later D3D setup operation fails.
     * Do not release/replay after that nontransactional boundary. */
    if(!nf_hw_begin(&s))fail143("world GPU begin214",family,count);
    if(!nf_hw_draw(dense,dense_count)||!nf_hw_sync())fail143("world GPU completion214",family,count);
    nf_swizzled131_export_size(color,owned,1024,256);
    /* No depth writes in family0: preserve every original depth/stencil byte. */
    if(family)nf_swizzled131_export_size(depth,owned+262144,1024,256);
    accepted=1;
done214:
    free(dense);free(owned);return accepted;
}
