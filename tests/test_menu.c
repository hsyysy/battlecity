#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "game.h"
#include "menu.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#c); exit(1); } } while (0)

int main(void) {
    Game g;
    game_init(&g);
    game_enter_construction(&g);
    int players = 1, stage = 1;
    bool quit = false;

    // Space is the editor's paint/next-block key and must not leave the
    // Construction screen.  Only Enter/Start exits and stores the map.
    menu_handle_key(SDLK_SPACE, &g, &players, stage, &quit, NULL, NULL);
    CHECK(g.phase == CONSTRUCTION && !quit);
    CHECK(g.map[0][1].type == EMPTY);
    const Input fire[2] = {{.dir=UP, .fire=true}, {.dir=UP}};
    game_step_inputs(&g, fire);
    CHECK(g.phase == CONSTRUCTION);
    CHECK(g.map[0][1].type == BRICK && g.map[1][1].type == BRICK);
    const Input idle[2] = {{.dir=UP}, {.dir=UP}};
    game_step_inputs(&g, idle);

    menu_handle_key(SDLK_RETURN, &g, &players, stage, &quit, NULL, NULL);
    CHECK(g.phase == TITLE && !quit && g.custom_map_ready);
    CHECK(g.custom_map[0][1].type == BRICK && g.custom_map[1][1].type == BRICK);

    puts("PASS: Construction Space paints without exiting; Enter stores and exits");
    return 0;
}
