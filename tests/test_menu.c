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

    // Starting 1P from the title must also update the menu's player count.
    // Otherwise a 2P game that returns to the title and starts 1P would turn
    // back into 2P after clearing stage 35.
    Game modes;
    game_start_mode(&modes,2,35);
    modes.phase=TITLE;
    modes.title_cursor=0;
    players=2;
    stage=1;
    menu_handle_key(SDLK_RETURN,&modes,&players,stage,&quit,NULL,NULL);
    CHECK(modes.phase==STAGE_SELECT && modes.players==1 && players==1);
    modes.stage=35;
    modes.phase=WON;
    menu_handle_key(SDLK_RETURN,&modes,&players,stage,&quit,NULL,NULL);
    CHECK(modes.phase==STAGE_SELECT && modes.players==1 && modes.stage==1);

    puts("PASS: Construction Space/Enter and title player-count sync");
    return 0;
}
