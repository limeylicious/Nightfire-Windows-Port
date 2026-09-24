/* Private, opt-in geometry evidence at an explicit user/replay-held view.
 * Included by the executor after transform_vertex and hardware gathering exist.
 * One presentation frame/token, <=512 batches, <=65536 unique vertex records,
 * <=16MiB text. No guest writes. Existing attribute read guards remain active;
 * diagnostic frames are never used for timing comparisons. A private program
 * copy avoids changing the renderer's shader decode/error cache.
 */
#ifndef NIGHTFIRE_GEOMETRY133_H
#define NIGHTFIRE_GEOMETRY133_H
static void geometry133_capture(unsigned hardware,unsigned triangles){
    static int enabled=-1;static const char *trigger,*directory;
    static FILE *file;static unsigned checked=~0u,token,first,batches,vertices;
    static unsigned seen[65536],generation;
    if(enabled<0){const char *v=getenv("NIGHTFIRE_GEOMETRY133");enabled=v && !strcmp(v,"1");
        trigger=getenv("NIGHTFIRE_SHADOW_LAYOUT131_TRIGGER");directory=getenv("NIGHTFIRE_SHADOW_LAYOUT131_DIR");}
    if(!enabled || !trigger || !directory)return;
    if(checked!=diagnostic_presents){
        checked=diagnostic_presents;
        if(file && checked!=first){fprintf(file,"END batches=%u vertices=%u\n",batches,vertices);fclose(file);file=NULL;}
        unsigned next=0;FILE *t=fopen(trigger,"r");if(t){fscanf(t,"%u",&next);fclose(t);}
        if(next && next!=token){
            token=next;first=checked;batches=vertices=0;
            char path[1024];int n=snprintf(path,sizeof path,"%s/geometry133-token%u.txt",directory,token);
            if(n>0 && n<sizeof path)file=fopen(path,"w");
            if(file)fprintf(file,"GEOMETRY133 frame=%u token=%u; CPU shader observation, not GPU output; guarded input reads\n",first,token);
        }
    }
    if(!file)return;
    if(batches>=512 || vertices>=65536 || ftell(file)>=16*1024*1024){
        fprintf(file,"LIMIT batches=%u vertices=%u\n",batches,vertices);fclose(file);file=NULL;return;
    }
    ++batches;
    fprintf(file,"B %u frame=%u target=%08X surface=%08X prim=%u count=%u triangles=%u hw=%u program=%u inputs=%X tex=%08X fmt=%08X depth=%u/%X/%u alpha=%u/%X/%u cull=%u/%X/%X blend=%u/%X/%X shade=%X\n",
        batches,first,dma_resolve(s_gpu.color_offset),s_gpu.format,s_gpu.prim,s_gpu.idx_count,triangles,hardware,
        hardware_program,hardware_input_mask,s_gpu.tex.valid?s_gpu.tex.offset:0,s_tex_reg[1],
        s_gpu.depth_enable,s_gpu.depth_func,s_gpu.depth_mask,s_gpu.alpha_test,s_gpu.alpha_func,s_gpu.alpha_ref,
        s_gpu.cull_enable,s_gpu.cull_face,s_gpu.front_face,s_gpu.blend_enable,s_gpu.blend_src,s_gpu.blend_dst,shade_mode);
    fprintf(file,"I");for(unsigned i=0;i<s_gpu.idx_count;i++)fprintf(file," %u",s_gpu.idx[i]);fprintf(file,"\n");
    if(!hardware || !hardware_program){fprintf(file,"UNOBSERVED non-program backend\n");return;}
    NFVertexProgram copy=vertex_program;
    fprintf(file,"PROGRAM start=%u mode=%u\n",copy.start,copy.mode);
    for(unsigned pc=copy.start;pc<136 && copy.valid[pc]==15;pc++){
        fprintf(file,"P %u %08X %08X %08X %08X\n",pc,copy.code[pc][0],copy.code[pc][1],copy.code[pc][2],copy.code[pc][3]);
        if(copy.code[pc][3]&1)break;
    }
    for(unsigned c=0;c<192;c++)if(copy.constant_valid[c])fprintf(file,"C %u %X %08X %08X %08X %08X\n",c,copy.constant_valid[c],copy.constant_words[c][0],copy.constant_words[c][1],copy.constant_words[c][2],copy.constant_words[c][3]);
    if(++generation==0){memset(seen,0,sizeof seen);generation=1;}
    for(unsigned i=0;i<s_gpu.idx_count;i++){
        unsigned index=s_gpu.idx[i];if(index>=65536 || seen[index]==generation)continue;
        seen[index]=generation;
        if(vertices>=65536 || ftell(file)>=16*1024*1024){fprintf(file,"LIMIT vertices=%u\n",vertices);fclose(file);file=NULL;return;}
        ++vertices;float in[16][4]={{0}},out[16][4];unsigned fetched=0;
        for(unsigned a=0;a<16;a++){
            in[a][3]=1;
            if(hardware_input_mask&(1u<<a)){
                if(fetch_attr(&s_gpu.attr[a],index,in[a]))fetched|=1u<<a;
                uint32_t bits[4];memcpy(bits,in[a],sizeof bits);
                fprintf(file,"A %u %u %08X %08X %08X %08X\n",index,a,bits[0],bits[1],bits[2],bits[3]);
            }
        }
        int ok=nf_vp_run(&copy,(const float (*)[4])in,out);
        fprintf(file,"V %u ok=%d fetched=%X",index,ok,fetched);
        if(ok){uint32_t bits[16][4];memcpy(bits,out,sizeof bits);
            for(unsigned a=0;a<16;a++)for(unsigned c=0;c<4;c++)fprintf(file," %08X",bits[a][c]);}
        fprintf(file,"\n");
    }
    fflush(file);
}
#endif
