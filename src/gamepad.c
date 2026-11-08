#include "gamepad.h"
#include <stdlib.h>

// The first two connected gamepads control P1 and P2.  Slots compact on
// removal, then any still-connected but previously ignored pad fills them.
static SDL_Gamepad *gamepads[2];
static SDL_JoystickID gamepad_ids[2];

static void open_gamepad(SDL_JoystickID id) {
    for (int i=0;i<2;++i) if (gamepads[i] && gamepad_ids[i]==id) return;
    for (int i=0;i<2;++i) if (!gamepads[i]) {
        gamepads[i]=SDL_OpenGamepad(id);
        if (gamepads[i]) gamepad_ids[i]=id;
        return;
    }
}

void gamepads_open_existing(void) {
    int count=0;
    SDL_JoystickID *ids=SDL_GetGamepads(&count);
    if (!ids) return;
    for (int i=0;i<count;++i) open_gamepad(ids[i]);
    SDL_free(ids);
}

void gamepads_added(SDL_JoystickID id) {
    open_gamepad(id);
}

void gamepads_removed(SDL_JoystickID id) {
    for (int i=0;i<2;++i) if (gamepads[i] && gamepad_ids[i]==id) {
        SDL_CloseGamepad(gamepads[i]);
        for (int j=i;j<1;++j) {
            gamepads[j]=gamepads[j+1];
            gamepad_ids[j]=gamepad_ids[j+1];
        }
        gamepads[1]=NULL;
        gamepad_ids[1]=0;
        // A third pad may have been connected while both slots were full and
        // never produced another ADDED event.  Fill the freed slot now.
        gamepads_open_existing();
        return;
    }
}

void gamepads_shutdown(void) {
    for (int i=0;i<2;++i) {
        if (gamepads[i]) SDL_CloseGamepad(gamepads[i]);
        gamepads[i]=NULL;
        gamepad_ids[i]=0;
    }
}

Input gamepad_input(int player) {
    Input in={.dir=UP};
    if (player<0 || player>1 || !gamepads[player]) return in;
    SDL_Gamepad *pad=gamepads[player];
    if (SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_UP)) {in.dir=UP;in.moving=true;}
    else if (SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {in.dir=RIGHT;in.moving=true;}
    else if (SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {in.dir=DOWN;in.moving=true;}
    else if (SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {in.dir=LEFT;in.moving=true;}
    else {
        int x=SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFTX);
        int y=SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFTY);
        const int deadzone=12000;
        if (abs(x)>abs(y) && abs(x)>deadzone) {
            in.dir=x>0?RIGHT:LEFT;in.moving=true;
        } else if (abs(y)>deadzone) {
            in.dir=y>0?DOWN:UP;in.moving=true;
        }
    }
    in.fire=SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_SOUTH);
    in.fire2=SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_EAST);
    return in;
}

SDL_JoystickID gamepad_player_id(int player) {
    if (player<0 || player>1 || !gamepads[player]) return 0;
    return gamepad_ids[player];
}
