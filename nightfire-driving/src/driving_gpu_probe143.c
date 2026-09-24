/* Read-only snapshots at observed original D3D boundaries. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
extern ptrdiff_t g_xbox_mem_offset;
static int read143(uint32_t va, void *out, size_t n)
{
    SIZE_T got = 0;
    if (!va || n > 0x400000 || (uint64_t)va + n > 0x100000000ull) return 0;
    return ReadProcessMemory(GetCurrentProcess(), (const void *)(g_xbox_mem_offset + (uintptr_t)va), out, n, &got) && got == n;
}
static uint32_t word143(uint32_t va) { uint32_t v = 0; read143(va,&v,4); return v; }
static void dump143(const char *root, const char *kind, unsigned index, uint32_t va, size_t bytes)
{
    unsigned char *data = malloc(bytes);
    if (!data) return;
    if (read143(va,data,bytes)) {
        char path[1024]; snprintf(path,sizeof(path),"%s/gpu143-%u-%s.bin",root,index,kind);
        FILE *f = fopen(path,"wb"); if (f) { fwrite(data,1,bytes,f); fclose(f); }
    }
    free(data);
}
void driving_gpu_probe143(uint32_t site)
{
    static unsigned count;
    if (count >= 8) return;
    const char *root = getenv("DRIVING_CAPTURE_DIR"); if (!root) return;
    unsigned index = count++;
    uint32_t dev = word143(0x175418), d[0x3000/4];
    if (!read143(dev,d,sizeof(d))) return;
    uint32_t begin=d[0x24/4], end=d[0x28/4], current=d[0], submitted=d[0x2c/4];
    uint32_t dma=d[0x23bc/4], completed=d[0x34/4];
    char path[1024]; snprintf(path,sizeof(path),"%s/gpu143-%u.txt",root,index);
    FILE *f=fopen(path,"w"); if (!f) return;
    fprintf(f,"site=%08X device=%08X begin=%08X end=%08X current=%08X submitted=%08X\n",site,dev,begin,end,current,submitted);
    fprintf(f,"fence_next=%08X completed_va=%08X completed=%08X dma=%08X put=%08X get=%08X\n",d[0x30/4],completed,word143(completed),dma,word143(dma+0x40),word143(dma+0x44));
    fprintf(f,"surface_color=%08X clip=%08X semaphores=%08X\n",d[0x21c4/4],d[0x23cc/4],d[0x2c20/4]);
    fprintf(f,"ramht=%08X\n",word143(0xFD002210));
    fclose(f);
    dump143(root,"device",index,dev,sizeof(d));
    dump143(root,"pramin",index,0xFD700000,0x100000);
    if (begin >= 0x80000000u && end > begin && end-begin <= 0x400000u)
        dump143(root,"pushbuffer",index,begin,end-begin);
    fprintf(stderr,"[GPU143] site=%08X dev=%08X put=%08X get=%08X fence_next=%u completed=%u snapshot=%u\n",site,dev,word143(dma+0x40),word143(dma+0x44),d[0x30/4],word143(completed),index);
}
