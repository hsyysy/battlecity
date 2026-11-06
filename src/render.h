#ifndef BATTLECITY_RENDER_H
#define BATTLECITY_RENDER_H
#include "game.h"
#include "rom.h"
enum { SCREEN_W=256, SCREEN_H=240 };
void render_game(uint32_t pixels[SCREEN_W*SCREEN_H], const Game *g, const Rom *rom, unsigned frame);
#endif
