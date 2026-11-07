#include "ui/bmp.h"
#include "fs/vfs.h"
#include "memory.h"

static unsigned char disk[256 * 512];
static int read_sector(void *ctx, uint64_t s, void *out) { (void)ctx; if (s >= 256) return -1; unsigned char *d = out; for (unsigned i=0;i<512;i++) d[i]=disk[s*512+i]; return 0; }
static int write_sector(void *ctx, uint64_t s, const void *in) { (void)ctx; if (s >= 256) return -1; const unsigned char *p=in; for(unsigned i=0;i<512;i++) disk[s*512+i]=p[i]; return 0; }
static void put16(unsigned char *p, unsigned v) { p[0]=v; p[1]=v>>8; }
static void put32(unsigned char *p, unsigned v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }

int main(void)
{
    struct block_device dev = { 0, 256, "test", "ram", read_sector, write_sector, 0 };
    unsigned char bmp[70] = {0};
    memory_init();
    if (vfs_attach_block_device(&dev) || vfs_format()) return 1;
    bmp[0]='B'; bmp[1]='M'; put32(bmp+2,70); put32(bmp+10,54); put32(bmp+14,40);
    put32(bmp+18,2); put32(bmp+22,2); put16(bmp+26,1); put16(bmp+28,24); put32(bmp+34,16);
    /* BMP rows are bottom up, and each two pixel row has two padding bytes. */
    bmp[54]=255; bmp[55]=0; bmp[56]=0; bmp[57]=255; bmp[58]=255; bmp[59]=255;
    bmp[62]=0; bmp[63]=0; bmp[64]=255; bmp[65]=0; bmp[66]=255; bmp[67]=0;
    if (vfs_create("/image.bmp", bmp, sizeof(bmp))) return 2;
    image_t *image = bmp_load("/image.bmp");
    if (!image || image->width != 2 || image->height != 2 || image->pixels[0] != 0xff0000 ||
        image->pixels[1] != 0x00ff00 || image->pixels[2] != 0x0000ff || image->pixels[3] != 0xffffff) return 3;
    bmp_free(image);
    if (bmp_load("/missing.bmp") != 0) return 4;
    bmp[28]=8; bmp[29]=0;
    if (vfs_create("/bad.bmp", bmp, sizeof(bmp))) return 5;
    if (bmp_load("/bad.bmp") != 0) return 6;
    bmp[28]=24; bmp[29]=0; bmp[0]='X';
    if (vfs_create("/invalid.bmp", bmp, sizeof(bmp)) || bmp_load("/invalid.bmp") != 0) return 7;
    unsigned char bmp32[58] = {0};
    bmp32[0]='B'; bmp32[1]='M'; put32(bmp32+2,58); put32(bmp32+10,54); put32(bmp32+14,40);
    put32(bmp32+18,1); put32(bmp32+22,0xffffffffu); put16(bmp32+26,1); put16(bmp32+28,32); put32(bmp32+34,4);
    bmp32[54]=0x33; bmp32[55]=0x22; bmp32[56]=0x11; bmp32[57]=0xff;
    if (vfs_create("/topdown.bmp", bmp32, sizeof(bmp32))) return 8;
    image = bmp_load("/topdown.bmp");
    if (!image || image->width != 1 || image->height != 1 || image->pixels[0] != 0x112233) return 9;
    bmp_free(image);
    return 0;
}
