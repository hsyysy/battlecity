#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "gamepad.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#c); exit(1); } } while (0)

static SDL_JoystickID attach(const char *name) {
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes=4;
    desc.nbuttons=16;
    desc.nhats=1;
    desc.button_mask=0xffff;
    desc.axis_mask=0xf;
    desc.name=name;
    SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);
    CHECK(id!=0);
    return id;
}

int main(void) {
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_JoystickID a=attach("pad-a");
    SDL_JoystickID b=attach("pad-b");
    SDL_JoystickID c=attach("pad-c");
    SDL_PumpEvents();

    // Only two pads can be assigned; the third waits while both slots are full.
    gamepads_open_existing();
    CHECK(gamepad_player_id(0)==a && gamepad_player_id(1)==b);

    // Removing P1's pad must promote the second pad and then fill the free
    // slot from the already-connected third pad.
    SDL_DetachVirtualJoystick(a);
    SDL_PumpEvents();
    gamepads_removed(a);
    CHECK(gamepad_player_id(0)==b && gamepad_player_id(1)==c);

    // The promoted third pad now controls P2.
    SDL_Joystick *joystick=SDL_GetJoystickFromID(c);
    CHECK(joystick!=NULL);
    CHECK(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_SOUTH,true));
    SDL_PumpEvents();
    Input p2=gamepad_input(1);
    Input p1=gamepad_input(0);
    CHECK(p2.fire && !p1.fire);

    gamepads_shutdown();
    SDL_DetachVirtualJoystick(b);
    SDL_DetachVirtualJoystick(c);
    SDL_Quit();
    puts("PASS: third gamepad fills the slot freed by a disconnected pad");
    return 0;
}
