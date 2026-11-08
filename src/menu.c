#include "menu.h"
#include <string.h>

static void begin_stage_select(Game *game,int players,int stage,bool fresh) {
    int previous_high=game->high_score;
    // A map built in Construction mode must survive game_start_mode's reset
    // and stay in place while the stage-select screen is open.
    bool custom=game->custom_map_ready;
    Cell saved_map[MAP_SIZE][MAP_SIZE];
    if (custom) memcpy(saved_map,game->custom_map,sizeof(saved_map));
    if (fresh) game_start_mode(game,players,stage);
    else if (game->stage>=35) game_start_mode(game,players,1);
    else game_next_stage(game);
    if (previous_high>game->high_score) game->high_score=previous_high;
    if (custom) {
        memcpy(game->custom_map,saved_map,sizeof(saved_map));
        game->custom_map_ready=true;
        game->use_custom_map=true;
    }
    game->phase=STAGE_SELECT;
    if (custom) game_set_stage(game,game->stage);
    game->sound_events=0;
}

// Keyboard and gamepad menu actions share this handler.  Gamepad Start/Back/
// d-pad are translated to the equivalent key codes below.
void menu_handle_key(SDL_Keycode key,Game *game,int *players,int stage,
                     bool *quit,Sound *sound,SDL_AudioStream *audio) {
    if (key==SDLK_ESCAPE) *quit=true;
    if (key==SDLK_R &&
        (game->phase==PLAYING || game->phase==PAUSED)) {
        sound_stop(sound);if (audio) SDL_ClearAudioStream(audio);
        game_restart_stage(game);
    }
    if (game->phase==STAGE_SELECT) {
        if (key==SDLK_LEFT || key==SDLK_DOWN) {
            game_set_stage(game,game->stage-1);
        } else if (key==SDLK_RIGHT || key==SDLK_UP) {
            game_set_stage(game,game->stage+1);
        } else if (key==SDLK_RETURN || key==SDLK_SPACE) {
            sound_stop(sound);if (audio) SDL_ClearAudioStream(audio);
            game_begin_play(game);
            game->sound_events |= 1u<<SOUND_START;
        }
    }
    else if (game->phase==CONSTRUCTION && key==SDLK_RETURN) {
        sound_stop(sound);if (audio) SDL_ClearAudioStream(audio);
        game_exit_construction(game);
    }
    else if (game->phase==TITLE &&
        (key==SDLK_UP || key==SDLK_LEFT || key==SDLK_DOWN ||
         key==SDLK_RIGHT || key==SDLK_TAB)) {
        int step=(key==SDLK_UP || key==SDLK_LEFT)?2:1;
        game->title_cursor=(game->title_cursor+step)%3;
        if (game->title_cursor<2) *players=game->title_cursor+1;
        game->players=*players;
    }
    else if (game->phase==TITLE &&
        (key==SDLK_1 || key==SDLK_2)) {
        *players=(key==SDLK_2)?2:1;
        game->title_cursor=*players-1;
        sound_stop(sound);if (audio) SDL_ClearAudioStream(audio);
        begin_stage_select(game,*players,stage,true);
    }
    else if (key==SDLK_RETURN && game->phase==TITLE) {
        sound_stop(sound);if (audio) SDL_ClearAudioStream(audio);
        if (game->title_cursor==2) {
            game_enter_construction(game);
        } else {
            *players=game->title_cursor+1;
            game->players=*players;
            begin_stage_select(game,*players,stage,true);
        }
    }
    else if (key==SDLK_RETURN && game->phase==WON) {
        sound_stop(sound);if (audio) SDL_ClearAudioStream(audio);
        begin_stage_select(game,*players,stage,false);
    }
    else if ((key==SDLK_RETURN || key==SDLK_SPACE) && game->phase==LOST_FIELD) {
        sound_stop(sound);if (audio) SDL_ClearAudioStream(audio);
        game->phase=LOST;
        game->phase_timer=0;
    }
    else if ((key==SDLK_RETURN || key==SDLK_SPACE) && game->phase==LOST) {
        sound_stop(sound);if (audio) SDL_ClearAudioStream(audio);
        game->phase=GAME_OVER;
        game->phase_timer=0;
        game->sound_events |= 1u<<SOUND_GAMEOVER;
    }
    else if ((key==SDLK_RETURN || key==SDLK_SPACE) && game->phase==GAME_OVER) {
        sound_stop(sound);if (audio) SDL_ClearAudioStream(audio);
        game->phase=TITLE;
        game->phase_timer=0;
        game->title_anim=0;
    }
    else if (key==SDLK_P || key==SDLK_RETURN) {
        if (game->phase==PLAYING) {
            game->phase=PAUSED;sound_stop(sound);game->sound_events|=1u<<SOUND_PAUSE;
            if (audio) SDL_ClearAudioStream(audio);
        }
        else if (game->phase==PAUSED) game->phase=PLAYING;
    }
}

