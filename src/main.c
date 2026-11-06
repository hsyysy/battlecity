#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "game.h"
#include "render.h"
#include "rom.h"
#include "sound.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DEFAULT_ROM_PATH
#define DEFAULT_ROM_PATH "BattleCity.nes"
#endif

// The first two connected gamepads control P1 and P2.  Removals compact the
// array, so a lone second pad still becomes pad 0.
static SDL_Gamepad *gamepads[2];
static SDL_JoystickID gamepad_ids[2];

static void open_gamepad(SDL_JoystickID id) {
    for (int i=0;i<2;++i) if (gamepad_ids[i]==id) return;
    for (int i=0;i<2;++i) if (!gamepads[i]) {
        gamepads[i]=SDL_OpenGamepad(id);
        if (gamepads[i]) gamepad_ids[i]=id;
        return;
    }
}

static void close_gamepad(SDL_JoystickID id) {
    for (int i=0;i<2;++i) if (gamepads[i] && gamepad_ids[i]==id) {
        SDL_CloseGamepad(gamepads[i]);
        for (int j=i;j<1;++j) {
            gamepads[j]=gamepads[j+1];
            gamepad_ids[j]=gamepad_ids[j+1];
        }
        gamepads[1]=NULL;
        gamepad_ids[1]=0;
        return;
    }
}

static void open_existing_gamepads(void) {
    int count=0;
    SDL_JoystickID *ids=SDL_GetGamepads(&count);
    if (!ids) return;
    for (int i=0;i<count;++i) open_gamepad(ids[i]);
    SDL_free(ids);
}

static Input merge_input(Input in,const Input *pad) {
    if (!in.moving && pad->moving) {in.dir=pad->dir;in.moving=true;}
    in.fire=in.fire || pad->fire;
    in.fire2=in.fire2 || pad->fire2;
    return in;
}

static Input gamepad_input(int player) {
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

static bool screenshot(const char *path, uint32_t *pixels) {
    SDL_Surface *surface=SDL_CreateSurfaceFrom(SCREEN_W,SCREEN_H,SDL_PIXELFORMAT_ARGB8888,
                                               pixels,SCREEN_W*(int)sizeof(*pixels));
    if (!surface) return false;
    bool ok=SDL_SaveBMP(surface,path);
    SDL_DestroySurface(surface);
    return ok;
}

static Input one_player_input(const bool *keys,int player) {
    Input in={.dir=UP};
    if (player==0) {
        if (keys[SDL_SCANCODE_W]) {in.dir=UP;in.moving=true;}
        else if (keys[SDL_SCANCODE_D]) {in.dir=RIGHT;in.moving=true;}
        else if (keys[SDL_SCANCODE_S]) {in.dir=DOWN;in.moving=true;}
        else if (keys[SDL_SCANCODE_A]) {in.dir=LEFT;in.moving=true;}
        in.fire=keys[SDL_SCANCODE_SPACE]||keys[SDL_SCANCODE_J]||keys[SDL_SCANCODE_Z];
        in.fire2=keys[SDL_SCANCODE_X]||keys[SDL_SCANCODE_BACKSPACE];
    } else {
        if (keys[SDL_SCANCODE_UP]) {in.dir=UP;in.moving=true;}
        else if (keys[SDL_SCANCODE_RIGHT]) {in.dir=RIGHT;in.moving=true;}
        else if (keys[SDL_SCANCODE_DOWN]) {in.dir=DOWN;in.moving=true;}
        else if (keys[SDL_SCANCODE_LEFT]) {in.dir=LEFT;in.moving=true;}
        in.fire=keys[SDL_SCANCODE_K]||keys[SDL_SCANCODE_RSHIFT]||
                 keys[SDL_SCANCODE_KP_0];
        in.fire2=keys[SDL_SCANCODE_COMMA]||keys[SDL_SCANCODE_KP_DECIMAL];
    }
    return in;
}

static void read_inputs(bool demo,int tick,int players,bool editor,Input inputs[2]) {
    inputs[0]=(Input){.dir=UP};
    inputs[1]=(Input){.dir=UP};
    if (demo) {
        Direction directions[]={UP,RIGHT,UP,LEFT,DOWN,LEFT,UP,RIGHT};
        inputs[0]=(Input){directions[(tick/180)%8],true,true,false};
        return;
    }
    const bool *keys=SDL_GetKeyboardState(NULL);
    Input pad0=gamepad_input(0);
    if (editor || players==1) {
        // In one-player mode and in the Construction editor the arrows remain
        // an alternative to WASD, regardless of the selected game mode.
        Input in=one_player_input(keys,0);
        if (!in.moving) {
            if (keys[SDL_SCANCODE_UP]) {in.dir=UP;in.moving=true;}
            else if (keys[SDL_SCANCODE_RIGHT]) {in.dir=RIGHT;in.moving=true;}
            else if (keys[SDL_SCANCODE_DOWN]) {in.dir=DOWN;in.moving=true;}
            else if (keys[SDL_SCANCODE_LEFT]) {in.dir=LEFT;in.moving=true;}
        }
        inputs[0]=merge_input(in,&pad0);
    } else {
        Input pad1=gamepad_input(1);
        inputs[0]=merge_input(one_player_input(keys,0),&pad0);
        inputs[1]=merge_input(one_player_input(keys,1),&pad1);
    }
}

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

static bool audio_step(Sound *sound,SDL_AudioStream *stream,FILE *recording,Game *game,
                       const Input inputs[2]) {
    if (!sound) {game->sound_events=0;return true;}
    for (int i=0;i<SOUND_COUNT;++i)
        if (i!=SOUND_MOVE && i!=SOUND_IDLE &&
            (game->sound_events&(1u<<i))) sound_trigger(sound,(SoundEvent)i);
    game->sound_events=0;
    bool engine=false;
    for (int p=0;p<game->players;++p)
        if (game->tanks[p].alive && game->tanks[p].spawn==0) engine=true;
    bool moving=inputs[0].moving || (game->players>1 && inputs[1].moving);
    sound_engine(sound,game->phase==PLAYING && engine,moving);
    int16_t pcm[SOUND_FRAME_SAMPLES];
    if (!sound_render(sound,pcm,SOUND_FRAME_SAMPLES)) return false;
    if (stream && !SDL_PutAudioStreamData(stream,pcm,sizeof(pcm))) return false;
    return !recording || sound_wav_write(recording,pcm,SOUND_FRAME_SAMPLES);
}

// Keyboard and gamepad menu actions share this handler.  Gamepad Start/Back/
// d-pad are translated to the equivalent key codes below.
static void handle_key(SDL_Keycode key,Game *game,int *players,int stage,
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
    else if (game->phase==CONSTRUCTION &&
        (key==SDLK_RETURN || key==SDLK_SPACE)) {
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
        if (game->title_cursor==2)
            game_enter_construction(game);
        else
            begin_stage_select(game,game->title_cursor+1,stage,true);
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

int main(int argc,char **argv) {
    const char *rom_path=DEFAULT_ROM_PATH,*shot=NULL,*audio_path=NULL;
    bool headless=false,demo=false,start=false,mute=false;
    int limit=0,stage=1,players=1;
    for (int i=1;i<argc;++i) {
        if (!strcmp(argv[i],"--headless")) headless=true;
        else if (!strcmp(argv[i],"--demo")) demo=true;
        else if (!strcmp(argv[i],"--start")) start=true;
        else if (!strcmp(argv[i],"--mute")) mute=true;
        else if (!strcmp(argv[i],"--stage") && i+1<argc) {
            char *end=NULL;errno=0;long n=strtol(argv[++i],&end,10);
            if (errno || !*argv[i] || *end || n<1 || n>35) {
                fprintf(stderr,"Invalid --stage value (expected 1..35)\n");return 1;
            }
            stage=(int)n;
        }
        else if (!strcmp(argv[i],"--players") && i+1<argc) {
            char *end=NULL;errno=0;long n=strtol(argv[++i],&end,10);
            if (errno || !*argv[i] || *end || n<1 || n>2) {
                fprintf(stderr,"Invalid --players value (expected 1 or 2)\n");return 1;
            }
            players=(int)n;
        }
        else if ((!strcmp(argv[i],"--rom") || !strcmp(argv[i],"--screenshot") || !strcmp(argv[i],"--frames") || !strcmp(argv[i],"--audio-out")) && i+1<argc) {
            const char *option=argv[i],*value=argv[++i];
            if (!strcmp(option,"--rom")) rom_path=value;
            else if (!strcmp(option,"--screenshot")) shot=value;
            else if (!strcmp(option,"--audio-out")) audio_path=value;
            else {
                char *end=NULL;errno=0;long n=strtol(value,&end,10);
                if (errno || !*value || *end || n<1 || n>INT_MAX) {fprintf(stderr,"Invalid --frames value\n");return 1;}
                limit=(int)n;
            }
        } else if (!strcmp(argv[i],"--help")) {
            puts("Battle City — C11 / SDL3\n"
                 "Usage: battlecity [--rom PATH] [--start] [--stage N] [--players 1|2]\n"
                 "                  [--demo] [--headless] [--frames N] [--screenshot PATH.bmp]\n"
                 "                  [--mute] [--audio-out PATH.wav]\n"
                 "Title: Up/Down/Tab moves 1P/2P/CONSTRUCTION, Enter selects, 1/2 starts\n"
                 "P1: WASD + Space/J/Z | P2: arrows + K/RShift\n"
                 "Gamepad: first pad=P1, second=P2 | dpad/left stick move, A fire, B editor prev\n"
                 "          Start=start/pause, Back=title Select; hot-plug supported\n"
                 "Construction: arrows/WASD move, Space/J/Z paints+next, X/Backspace paints+prev\n"
                 "Enter: pause/next stage/continue/leave editor | P: pause | R: restart stage | F12: screenshot\n"
                 "--stage starts a new game at stage N (1..35).\n"
                 "--players chooses one or two players (default 1).\n"
                 "--frames runs N deterministic updates without real-time pacing.\n"
                 "--audio-out records ROM-driver mono PCM, also in headless mode.\n"
                 "--headless selects SDL's dummy/software drivers (default 600 frames).");
            return 0;
        } else {fprintf(stderr,"Unknown or incomplete option: %s\n",argv[i]);return 1;}
    }
    Rom rom;char error[512];
    if (!rom_load(&rom,rom_path,error,sizeof(error))) {fprintf(stderr,"%s\n",error);return 1;}
    if (headless) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
        if (!limit) limit=600;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());return 1;}
    // Gamepads are optional; keyboard remains fully functional if the
    // subsystem or physical controller is unavailable.
    if (SDL_InitSubSystem(SDL_INIT_GAMEPAD)) open_existing_gamepads();
    SDL_Window *window=SDL_CreateWindow("Battle City | C + SDL3 | Enter: start",768,720,
                                       headless?SDL_WINDOW_HIDDEN:SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer=window?SDL_CreateRenderer(window,headless?"software":NULL):NULL;
    SDL_Texture *texture=renderer?SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
                                                   SDL_TEXTUREACCESS_STREAMING,SCREEN_W,SCREEN_H):NULL;
    int result=1;
    Sound *sound=NULL;
    SDL_AudioStream *audio=NULL;
    FILE *recording=NULL;
    if (!texture) {fprintf(stderr,"SDL video: %s\n",SDL_GetError());goto cleanup;}
    if (!SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST) ||
        !SDL_SetRenderLogicalPresentation(renderer,SCREEN_W,SCREEN_H,SDL_LOGICAL_PRESENTATION_INTEGER_SCALE)) {
        fprintf(stderr,"SDL presentation: %s\n",SDL_GetError());goto cleanup;
    }
    if (!limit) SDL_SetRenderVSync(renderer,1);
    Game game;game_init(&game);
    game.players=players;
    game.title_cursor=players>1?1:0;
    game.stage=stage;
    if (start || demo || headless) game_start_mode(&game,players,stage);
    if (audio_path || (!mute && !headless && !limit)) {
        sound=sound_create(&rom,error,sizeof(error));
        if (!sound) {fprintf(stderr,"%s\n",error);goto cleanup;}
        if (audio_path) {
            recording=sound_wav_open(audio_path);
            if (!recording) {fprintf(stderr,"Cannot write audio: %s\n",audio_path);goto cleanup;}
        }
        if (!mute && !headless && !limit && SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            const SDL_AudioSpec spec={.format=SDL_AUDIO_S16,.channels=1,.freq=SOUND_RATE};
            audio=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,NULL,NULL);
            if (audio && !SDL_ResumeAudioStreamDevice(audio)) {SDL_DestroyAudioStream(audio);audio=NULL;}
        }
        if (!mute && !headless && !limit && !audio)
            fprintf(stderr,"Audio device unavailable: %s\n",SDL_GetError());
    }
    uint32_t pixels[SCREEN_W*SCREEN_H];
    bool quit=false;unsigned frames=0;
    Uint64 previous=SDL_GetPerformanceCounter();
    double accumulator=0,frequency=(double)SDL_GetPerformanceFrequency();
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type==SDL_EVENT_QUIT) quit=true;
            if (event.type==SDL_EVENT_WINDOW_FOCUS_LOST && game.phase==PLAYING && !limit) {
                game.phase=PAUSED;sound_stop(sound);game.sound_events=0;
                if (audio) SDL_ClearAudioStream(audio);
            }
            if (event.type==SDL_EVENT_GAMEPAD_ADDED) {
                open_gamepad(event.gdevice.which);
            } else if (event.type==SDL_EVENT_GAMEPAD_REMOVED) {
                close_gamepad(event.gdevice.which);
            } else if (event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key==SDLK_F12) {
                    render_game(pixels,&game,&rom,frames);
                    if (!screenshot("battlecity.bmp",pixels)) fprintf(stderr,"Screenshot: %s\n",SDL_GetError());
                } else {
                    handle_key(event.key.key,&game,&players,stage,&quit,sound,audio);
                }
            } else if (event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                SDL_Keycode key=SDLK_UNKNOWN;
                switch (event.gbutton.button) {
                case SDL_GAMEPAD_BUTTON_START: key=SDLK_RETURN; break;
                case SDL_GAMEPAD_BUTTON_BACK: key=SDLK_TAB; break;
                case SDL_GAMEPAD_BUTTON_DPAD_UP: key=SDLK_UP; break;
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN: key=SDLK_DOWN; break;
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT: key=SDLK_LEFT; break;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: key=SDLK_RIGHT; break;
                default: break;
                }
                if (key!=SDLK_UNKNOWN)
                    handle_key(key,&game,&players,stage,&quit,sound,audio);
            }
        }
        Uint64 now=SDL_GetPerformanceCounter();
        double elapsed=(double)(now-previous)/frequency;previous=now;
        if (elapsed>0.25) elapsed=0.25;
        accumulator+=elapsed;
        if (limit) {
            Input inputs[2];read_inputs(demo,game.tick,game.players,game.phase==CONSTRUCTION,inputs);
            game_step_inputs(&game,inputs);++frames;
            if (!audio_step(sound,audio,recording,&game,inputs)) {fprintf(stderr,"Audio rendering failed\n");goto cleanup;}
        }
        else while (accumulator>=1.0/60.0) {
            Input inputs[2];read_inputs(demo,game.tick,game.players,game.phase==CONSTRUCTION,inputs);
            game_step_inputs(&game,inputs);
            if (!audio_step(sound,audio,recording,&game,inputs)) {fprintf(stderr,"Audio rendering failed\n");goto cleanup;}
            accumulator-=1.0/60.0;++frames;
        }
        render_game(pixels,&game,&rom,frames);
        if (!SDL_UpdateTexture(texture,NULL,pixels,SCREEN_W*(int)sizeof(*pixels)) ||
            !SDL_RenderClear(renderer) || !SDL_RenderTexture(renderer,texture,NULL,NULL) || !SDL_RenderPresent(renderer)) {
            fprintf(stderr,"SDL render: %s\n",SDL_GetError());goto cleanup;
        }
        if (limit && frames>=(unsigned)limit) break;
        if (!limit) SDL_Delay(1);
    }
    if (shot && !screenshot(shot,pixels)) {fprintf(stderr,"Screenshot: %s\n",SDL_GetError());goto cleanup;}
    printf("frames=%u ticks=%d players=%d score=%d/%d lives=%d/%d queued=%d killed=%d phase=%d\n",
           frames,game.tick,game.players,game.score[0],game.score[1],
           game.lives[0],game.lives[1],game.remaining,game.killed,game.phase);
    result=0;
cleanup:
    if (recording && !sound_wav_close(recording)) {fprintf(stderr,"Audio recording failed\n");result=1;}
    for (int i=0;i<2;++i) if (gamepads[i]) {
        SDL_CloseGamepad(gamepads[i]);
        gamepads[i]=NULL;
        gamepad_ids[i]=0;
    }
    if (audio) SDL_DestroyAudioStream(audio);
    sound_destroy(sound);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
