#ifndef BATTLECITY_GAMEPAD_H
#define BATTLECITY_GAMEPAD_H

#include <SDL3/SDL.h>
#include "game.h"

void gamepads_open_existing(void);
void gamepads_added(SDL_JoystickID id);
void gamepads_removed(SDL_JoystickID id);
void gamepads_shutdown(void);
Input gamepad_input(int player);
SDL_JoystickID gamepad_player_id(int player);

#endif
