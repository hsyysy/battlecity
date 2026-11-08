#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "gamepad.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#c); exit(1); } } while (0)

static SDL_JoystickID test_ids[3];
static int test_id_count;

static int test_enumerate(SDL_JoystickID **ids) {
    *ids=SDL_malloc(sizeof(**ids)*test_id_count);
    if (!*ids) return 0;
    for (int i=0;i<test_id_count;++i) (*ids)[i]=test_ids[i];
    return test_id_count;
}

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
    // Simulate a machine that already has an unrelated controller connected.
    SDL_JoystickID pre=attach("pre-existing-pad");
    SDL_JoystickID a=attach("pad-a");
    SDL_JoystickID b=attach("pad-b");
    SDL_JoystickID c=attach("pad-c");
    test_ids[0]=a;test_ids[1]=b;test_ids[2]=c;test_id_count=3;
    SDL_PumpEvents();

    // Only two pads can be assigned; the third waits while both slots are full.
    // The injected enumeration isolates the test from devices that were
    // already present and gives a deterministic device order.
    gamepads_set_enumerator(test_enumerate);
    gamepads_open_existing();
    CHECK(gamepad_player_id(0)==a && gamepad_player_id(1)==b);

    // Removing P1's pad must promote the second pad and then fill the free
    // slot from the already-connected third pad.
    SDL_DetachVirtualJoystick(a);
    SDL_PumpEvents();
    test_ids[0]=b;test_ids[1]=c;test_id_count=2;
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

    // When a pre-existing pad is part of the enumeration, it takes part in
    // normal slot assignment like any other connected device.
    gamepads_shutdown();
    test_ids[0]=pre;test_ids[1]=b;test_ids[2]=c;test_id_count=3;
    gamepads_open_existing();
    CHECK(gamepad_player_id(0)==pre && gamepad_player_id(1)==b);

    gamepads_set_enumerator(NULL);
    gamepads_shutdown();
    SDL_DetachVirtualJoystick(pre);
    SDL_DetachVirtualJoystick(b);
    SDL_DetachVirtualJoystick(c);
    SDL_Quit();
    puts("PASS: third gamepad fills the slot freed by a disconnected pad");
    return 0;
}
