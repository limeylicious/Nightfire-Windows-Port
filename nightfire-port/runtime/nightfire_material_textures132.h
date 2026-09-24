/* Included privately by the enabled shadow diagnostic implementation.
 * Caller has validated the entire texture and completed overlapping GPU work.
 * Optional natural source snapshots only; no GPU calls or guest writes. */
static struct {
    unsigned initialized,on,token,count;
    size_t total;
    struct {const void *p;unsigned format;size_t length;} seen[64];
} material_textures132;
void nf_material132_texture(const void *texture,unsigned format,size_t length){
    if(!material_textures132.initialized){
        material_textures132.initialized=1;
        const char *v=getenv("NIGHTFIRE_MATERIAL_TEXTURES132");material_textures132.on=v && !strcmp(v,"1");
    }
    if(!material_textures132.on || !shadow131.active || shadow131.frame-shadow131.first>=2 || !texture || !length || length>4*1024*1024)return;
    if(material_textures132.token!=shadow131.token){
        material_textures132.token=shadow131.token;material_textures132.count=0;material_textures132.total=0;
    }
    if(material_textures132.count>=64 || length>16*1024*1024-material_textures132.total)return;
    for(unsigned i=0;i<material_textures132.count;i++)
        if(material_textures132.seen[i].p==texture && material_textures132.seen[i].format==format && material_textures132.seen[i].length==length)return;
    unsigned index=material_textures132.count++;material_textures132.total+=length;
    material_textures132.seen[index].p=texture;material_textures132.seen[index].format=format;material_textures132.seen[index].length=length;
    char name[128],path[1024];snprintf(name,sizeof name,"material132-token%u-texture%u-%08X.bin",shadow131.token,index,format);
    int n=snprintf(path,sizeof path,"%s/%s",shadow131.directory,name);if(n<0 || n>=sizeof path)return;
    FILE *f=fopen(path,"wb");if(!f)return;size_t written=fwrite(texture,1,length,f);fclose(f);
    if(shadow131_ready())fprintf(shadow131.log,"{\"event\":\"texture_source132\",\"frame\":%u,\"host\":\"%p\",\"format\":\"%08X\",\"bytes\":%llu,\"written\":%llu,\"file\":\"%s\"}\n",
        shadow131.frame,texture,format,(unsigned long long)length,(unsigned long long)written,name);
}
