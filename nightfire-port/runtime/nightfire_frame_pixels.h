#ifndef NIGHTFIRE_FRAME_PIXELS_H
#define NIGHTFIRE_FRAME_PIXELS_H
#include <stdint.h>
#include <stdio.h>
/* This checkpoint observes the logged 640x480, 2560-byte pitch, 32-bit mode.
 * Alpha is ignored by scanout. Nonblack pixels alone do not prove rendering. */
#define NF_FRAME_WIDTH 640u
#define NF_FRAME_HEIGHT 480u
#define NF_FRAME_BYTES (NF_FRAME_WIDTH * NF_FRAME_HEIGHT * 4u)
static unsigned nf_frame_nonblack(const uint32_t *pixels)
{
    unsigned count=0;
    for (unsigned i=0;i<NF_FRAME_WIDTH*NF_FRAME_HEIGHT;i++)
        count += (pixels[i]&0x00ffffffu)!=0;
    return count;
}
static void nf_bmp_u32(uint8_t *p,uint32_t v)
{
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static int nf_frame_write_bmp(const char *path,const uint32_t *pixels)
{
    uint8_t hdr[54]={0},row[NF_FRAME_WIDTH*3];
    FILE *f=fopen(path,"wb"); if(!f)return -1;
    hdr[0]='B';hdr[1]='M';nf_bmp_u32(hdr+2,54+sizeof(row)*NF_FRAME_HEIGHT);
    hdr[10]=54;hdr[14]=40;nf_bmp_u32(hdr+18,NF_FRAME_WIDTH);nf_bmp_u32(hdr+22,NF_FRAME_HEIGHT);
    hdr[26]=1;hdr[28]=24;nf_bmp_u32(hdr+34,sizeof(row)*NF_FRAME_HEIGHT);
    int ok=fwrite(hdr,1,sizeof(hdr),f)==sizeof(hdr);
    for(unsigned y=NF_FRAME_HEIGHT;y>0 && ok;y--) {
        for(unsigned x=0;x<NF_FRAME_WIDTH;x++) {
            uint32_t v=pixels[(y-1)*NF_FRAME_WIDTH+x];
            row[x*3]=(uint8_t)v;row[x*3+1]=(uint8_t)(v>>8);row[x*3+2]=(uint8_t)(v>>16);
        }
        ok=fwrite(row,1,sizeof(row),f)==sizeof(row);
    }
    if(fclose(f)!=0)ok=0;
    return ok?0:-1;
}
#endif
