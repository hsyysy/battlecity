#ifndef BATTLECITY_GAMEPAD_H
#define BATTLECITY_GAMEPAD_H

#include <SDL3/SDL.h>
#include "game.h"

// Enumeration hook so tests can isolate virtual devices from physical pads
// already connected to the machine.  Passing NULL restores SDL enumeration.
typedef int (*GamepadEnumerator)(SDL_JoystickID **ids);
void gamepads_set_enumerator(GamepadEnumerator enumerator);
void gamepads_open_existing(void);
void gamepads_added(SDL_JoystickID id);
void gamepads_removed(SDL_JoystickID id);
void gamepads_shutdown(void);
Input gamepad_input(int player);
SDL_JoystickID gamepad_player_id(int player);

#endif
