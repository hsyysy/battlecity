#ifndef BATTLECITY_GAME_H
#define BATTLECITY_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "sound_events.h"

enum { MAP_SIZE = 26, FIELD_SIZE = 208, MAX_TANKS = 8, MAX_BULLETS = 16, MAX_EFFECTS = 24 };
// Tally timing measured from FCEUX frames and request-write traces.  Each
// enemy kill is added to the per-type rows every 9 frames; the next type group
// starts 30 frames after the previous group's last entry.  For the captured
// 1/2/3/4 kills this gives type starts 33/72/120/177 and TOTAL at 251.
enum { TALLY_START_1P = 33, TALLY_START_2P = 34,
       TALLY_ENTRY_DELAY = 9,
       TALLY_GROUP_DELAY_1P = 30, TALLY_GROUP_DELAY_2P = 30,
       TALLY_TOTAL_DELAY = 8, TALLY_END_DELAY = 142,
       TITLE_SCROLL_DELAY = 20,
       LOST_FIELD_DELAY = 127, LOST_FIELD_TEXT_FRAMES = 130,
       GAME_OVER_FRAMES = 114, TITLE_DRAW_FRAMES = 260 };
typedef enum { UP, RIGHT, DOWN, LEFT } Direction;
typedef enum { EMPTY, BRICK, STEEL, WATER, FOREST, ICE, BASE } Terrain;
typedef enum { TITLE, STAGE_SELECT, CONSTRUCTION, PLAYING, PAUSED, WON, LOST_FIELD, LOST, GAME_OVER } Phase;
typedef enum {
    POWER_HELMET, POWER_CLOCK, POWER_SHOVEL, POWER_STAR,
    POWER_GRENADE, POWER_LIFE, POWER_COUNT
} PowerupType;
typedef struct { Terrain type; uint8_t bricks; uint8_t tile; } Cell;
typedef struct {
    bool alive;
    float x, y;
    Direction dir;
    int cooldown, shield, spawn, think, animation, hp, kind, rank;
    bool bonus;
} Tank;
typedef struct {
    bool alive;
    float x, y;
    Direction dir;
    int owner, speed, power;
} Bullet;
typedef struct { float x, y; int life, duration; bool big; } Effect;
typedef struct { Direction dir; bool moving, fire, fire2; } Input;
typedef struct {
    Cell map[MAP_SIZE][MAP_SIZE];
    Tank tanks[MAX_TANKS];
    Bullet bullets[MAX_BULLETS];
    Effect effects[MAX_EFFECTS];
    Phase phase;
    uint32_t rng;
    uint32_t sound_events;
    int tick, remaining, killed, spawn_timer, high_score;
    int phase_timer, title_anim;
    int lives[2], score[2], player_rank[2], respawn_timer[2];
    int kills[2][4];
    bool bonus_awarded[2];
    uint32_t frame_counter;
    int stage, spawn_point, enemy_type_index, players;
    int powerup_x, powerup_y, powerup_type, powerup_flash;
    int freeze_timer, shovel_timer;
    bool powerup_active;
    unsigned char enemy_counts[4];
    bool base_alive;

    // Title menu and Construction editor.  The editor stores the built map
    // separately so it survives the trip through the stage-select screen.
    int title_cursor;
    int editor_x, editor_y, editor_block, editor_flag, editor_delay;
    bool editor_prev_moving, editor_prev_fire, editor_prev_fire2;
    Direction editor_prev_dir;
    bool custom_map_ready, use_custom_map;
    Cell custom_map[MAP_SIZE][MAP_SIZE];
} Game;

void game_init(Game *g);
void game_start(Game *g);
void game_start_stage(Game *g, int stage);
void game_start_mode(Game *g, int players, int stage);
void game_set_stage(Game *g, int stage);
void game_next_stage(Game *g);
void game_restart_stage(Game *g);
void game_begin_play(Game *g);
void game_enter_construction(Game *g);
void game_exit_construction(Game *g);
void game_step(Game *g, Input input);
void game_step_inputs(Game *g, const Input inputs[2]);
bool game_can_move(const Game *g, int tank, float x, float y);
bool game_fire(Game *g, int tank);
void game_explode(Game *g, float x, float y, bool big);
int game_tally_entries(const Game *g, int type, int timer);
int game_tally_total_frame(const Game *g);
int game_tally_end_frame(const Game *g);

#endif
