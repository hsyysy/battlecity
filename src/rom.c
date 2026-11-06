#include "rom.h"
#include <stdio.h>
#include <string.h>

bool rom_load(Rom *rom, const char *path, char *error, size_t error_size) {
    FILE *file=fopen(path,"rb");
    if (!file) { snprintf(error,error_size,"Cannot open ROM: %s",path); return false; }
    uint8_t h[16];
    bool valid=fread(h,1,16,file)==16;
    if (!valid || memcmp(h,"NES\032",4)!=0 || h[4]!=1 || h[5]!=1 ||
        (h[6]>>4)!=0 || (h[7]&0xfc)!=0) {
        snprintf(error,error_size,"Expected iNES mapper 0, 16 KiB PRG + 8 KiB CHR");
        fclose(file); return false;
    }
    long offset=16+((h[6]&4)?512:0);
    valid=fseek(file,offset,SEEK_SET)==0 && fread(rom->prg,1,16384,file)==16384 &&
          fread(rom->chr,1,8192,file)==8192;
    fclose(file);
    if (!valid) snprintf(error,error_size,"ROM is truncated");
    return valid;
}

unsigned rom_pixel(const Rom *rom, int bank, int tile, int x, int y) {
    size_t at=(size_t)(bank*4096+tile*16+y);
    return ((rom->chr[at]>>(7-x))&1u) | (((rom->chr[at+8]>>(7-x))&1u)<<1);
}
