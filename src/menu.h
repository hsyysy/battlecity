#ifndef BATTLECITY_MENU_H
#define BATTLECITY_MENU_H

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "game.h"
#include "sound.h"

void menu_handle_key(SDL_Keycode key, Game *game, int *players, int stage,
                     bool *quit, Sound *sound, SDL_AudioStream *audio);

#endif
