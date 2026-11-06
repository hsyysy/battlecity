#ifndef BATTLECITY_ROM_H
#define BATTLECITY_ROM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { uint8_t prg[16384],chr[8192]; } Rom;
bool rom_load(Rom *rom, const char *path, char *error, size_t error_size);
unsigned rom_pixel(const Rom *rom, int bank, int tile, int x, int y);
#endif
