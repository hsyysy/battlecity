#include "game.h"
#include "stages.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const int dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};

// StageDataTable nibbles are 2x2 tile blocks. Values 0-3 and 5-8 are the
// four half-block brick/steel patterns used by the original map loader.
static const unsigned char block_tiles[16][4] = {
    {0x00,0x0f,0x00,0x0f}, {0x00,0x00,0x0f,0x0f},
    {0x0f,0x00,0x0f,0x00}, {0x0f,0x0f,0x00,0x00},
    {0x0f,0x0f,0x0f,0x0f}, {0x20,0x10,0x20,0x10},
    {0x20,0x20,0x10,0x10}, {0x10,0x20,0x10,0x20},
    {0x10,0x10,0x20,0x20}, {0x10,0x10,0x10,0x10},
    {0x12,0x12,0x12,0x12}, {0x22,0x22,0x22,0x22},
    {0x21,0x21,0x21,0x21}, {0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00},
};

static uint32_t random_next(Game *g) {
    uint32_t x = g->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return g->rng = x;
}

static void damage_tank(Game *g, int i, int attacker);

static void update_high_score(Game *g) {
    for (int p=0; p<g->players; ++p)
        if (g->score[p]>g->high_score) g->high_score=g->score[p];
}

// $D13A: while the eagle is alive, award one extra life per player when the
// ten-thousands score digit reaches 2 (20,000 points). The ROM sets sound
// request slots 4 and 5 for the 1UP.  Returns true when a life awarded during
// LOST_FIELD revives a player and cancels that failure.
static bool check_bonus_life(Game *g) {
    if (!g->base_alive) return false;
    bool revived=false;
    for (int p=0; p<g->players; ++p) {
        if (g->bonus_awarded[p] || g->score[p] < 20000) continue;
        g->bonus_awarded[p]=true;
        ++g->lives[p];
        // A lingering bullet can cross 20,000 in the same frame as the last
        // death, or one frame later after LOST_FIELD has already begun.  The
        // life is still awarded, so bring the player back and cancel a
        // failure caused only by exhausted lives.  A destroyed eagle cannot
        // be undone and is excluded by base_alive above.
        if (!g->tanks[p].alive && g->respawn_timer[p]==0) {
            g->respawn_timer[p]=90;
            if (g->phase==LOST_FIELD) {
                g->phase=PLAYING;
                g->phase_timer=0;
                revived=true;
            }
        }
        g->sound_events |= (1u<<SOUND_ENEMY)|(1u<<SOUND_PLAYER);
    }
    return revived;
}

static bool overlaps(float ax, float ay, float aw, float ah,
                     float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static Cell cell_from_tile(unsigned tile) {
    if (tile == 0x0f) return (Cell){BRICK, 15, 0x0f};
    if (tile == 0x10 || tile == 0x20) return (Cell){STEEL, 0, (uint8_t)tile};
    if (tile == 0x12) return (Cell){WATER, 0, 0x12};
    if (tile == 0x22) return (Cell){FOREST, 0, 0x22};
    if (tile == 0x21) return (Cell){ICE, 0, 0x21};
    return (Cell){EMPTY, 0, 0};
}

// Make_Respawn passes the tank centre to Draw_TSABlock, which snaps to an
// even tile and clears the 2x2 block under the 16x16 sprite.
static void clear_tank_block(Game *g, float field_x, float field_y) {
    int tx=(((int)field_x+8)/8)&~1;
    int ty=(((int)field_y+8)/8)&~1;
    for (int oy=0;oy<2;++oy) for (int ox=0;ox<2;++ox)
        if (ty+oy<MAP_SIZE && tx+ox<MAP_SIZE)
            g->map[ty+oy][tx+ox]=(Cell){EMPTY,0,0};
}

static void set_eagle_wall(Game *g, unsigned tile) {
    Cell cell=cell_from_tile(tile);
    for (int x = 11; x <= 14; ++x) g->map[23][x] = cell;
    g->map[24][11] = cell;
    g->map[25][11] = cell;
    g->map[24][14] = cell;
    g->map[25][14] = cell;
}

static void load_stage_map(Game *g) {
    memset(g->map, 0, sizeof(g->map));
    for (int by = 0; by < 13; ++by)
        for (int bx = 0; bx < 13; ++bx) {
            unsigned nibble = stage_blocks[g->stage - 1][by][bx];
            for (int q = 0; q < 4; ++q) {
                int tx = bx * 2 + (q & 1), ty = by * 2 + q / 2;
                g->map[ty][tx] = cell_from_tile(block_tiles[nibble][q]);
            }
        }
    // The eagle and its one-tile-thick wall are not stored in the stage
    // nibble table; the original draws them separately after the map load.
    set_eagle_wall(g,0x0f);
    for (int y = 24; y <= 25; ++y)
        for (int x = 12; x <= 13; ++x) g->map[y][x] = (Cell){BASE, 0, 0};
}

void game_init(Game *g) {
    memset(g, 0, sizeof(*g));
    g->phase = TITLE;
    g->players = 1;
    g->stage = 1;
    g->high_score = 20000;
    g->phase_timer = 0;
    g->title_anim = -1;
    g->rng = 0x19850909u;
    g->title_cursor = 0;
}

// Construction entry draws the same gray frame and brick-walled eagle as the
// ROM's DraW_Normal_HQ path.  Re-entering before a level has been played keeps
// the map already in the nametable; starting a level resets that state.
static void construction_blank_map(Game *g) {
    memset(g->map,0,sizeof(g->map));
    set_eagle_wall(g,0x0f);
    for (int y=24; y<=25; ++y)
        for (int x=12; x<=13; ++x) g->map[y][x]=(Cell){BASE,0,0};
}

static void construction_place(Game *g) {
    int bx=g->editor_x*2, by=g->editor_y*2;
    for (int q=0;q<4;++q)
        g->map[by+q/2][bx+q%2]=cell_from_tile(block_tiles[g->editor_block&15][q]);
}

static void construction_move(Game *g, Direction dir) {
    int x=g->editor_x+dx[dir], y=g->editor_y+dy[dir];
    if (x<0) x=0;
    if (y<0) y=0;
    if (x>12) x=12;
    if (y>12) y=12;
    g->editor_x=x;
    g->editor_y=y;
}

// The ROM editor moves the cursor in 16-pixel steps: an initial press moves
// immediately, held directions wait 20 frames, then repeat every 5.  A and B
// draw the current block; their edge presses step through the 16-entry TSA
// table (the last three entries are empty), while holding either paints the
// block under the cursor.  Moving resets the press counter, so the next A/B
// press paints instead of changing the selected block.
static void construction_step(Game *g, const Input *in) {
    bool moving=in->moving;
    bool new_direction=moving &&
        (!g->editor_prev_moving || in->dir!=g->editor_prev_dir);
    if (moving) {
        ++g->editor_delay;
        g->editor_flag=0;
        if (g->editor_delay==20) {
            g->editor_delay=15;
            construction_move(g,in->dir);
        } else if (new_direction) {
            construction_move(g,in->dir);
        }
        if (in->fire || in->fire2) construction_place(g);
    } else {
        g->editor_delay=0;
        if (in->fire && !g->editor_prev_fire) {
            if (!g->editor_flag) g->editor_flag=1;
            else g->editor_block=(g->editor_block+1)%14;
            construction_place(g);
        } else if (in->fire2 && !g->editor_prev_fire2) {
            if (!g->editor_flag) g->editor_flag=1;
            else g->editor_block=(g->editor_block+13)%14;
            construction_place(g);
        } else if (in->fire || in->fire2) {
            construction_place(g);
        }
    }
    g->editor_prev_moving=moving;
    g->editor_prev_dir=in->dir;
    g->editor_prev_fire=in->fire;
    g->editor_prev_fire2=in->fire2;
}

void game_enter_construction(Game *g) {
    if (!g->custom_map_ready) construction_blank_map(g);
    else memcpy(g->map,g->custom_map,sizeof(g->map));
    g->phase=CONSTRUCTION;
    g->tick=0;
    g->base_alive=true;
    g->title_anim=-1;
    g->editor_x=0;
    g->editor_y=0;
    g->editor_block=0;
    g->editor_flag=0;
    g->editor_delay=0;
    g->editor_prev_moving=false;
    g->editor_prev_fire=false;
    g->editor_prev_fire2=false;
    g->editor_prev_dir=UP;
    g->sound_events=0;
}

void game_exit_construction(Game *g) {
    memcpy(g->custom_map,g->map,sizeof(g->map));
    g->custom_map_ready=true;
    g->use_custom_map=false;
    g->phase=TITLE;
    g->title_anim=-1;
    g->title_cursor=2;
    g->sound_events=0;
}

void game_begin_play(Game *g) {
    g->phase=PLAYING;
    // SetUp_LevelVARs clears Construction_Flag in the ROM, so the built map is
    // used for exactly one level and later stages return to the ROM stage data.
    g->custom_map_ready=false;
    g->use_custom_map=false;
}

static void player_spawn(Game *g, int player) {
    // P1 centre is screen x=88, P2 centre is screen x=152; stored here as
    // field-relative sprite top-left coordinates.
    float x=player?128:64, y=192;
    // Make_Respawn clears the 16x16 TSA block under a spawning tank.  This
    // matters for Construction maps, where the player may have built there.
    clear_tank_block(g,x,y);
    g->tanks[player] = (Tank){.alive=true, .x=x, .y=y, .dir=UP,
                              .shield=192, .spawn=33, .hp=1,
                              .rank=g->player_rank[player]};
}

static void stage_setup(Game *g) {
    memset(g->tanks, 0, sizeof(g->tanks));
    memset(g->bullets, 0, sizeof(g->bullets));
    memset(g->effects, 0, sizeof(g->effects));
    g->phase = PLAYING;
    g->phase_timer = 0;
    g->title_anim = -1;
    g->base_alive = true;
    g->remaining = 20;
    g->killed = 0;
    memset(g->kills,0,sizeof(g->kills));
    g->tick = 0;
    g->spawn_timer = 0;
    g->respawn_timer[0] = g->respawn_timer[1] = 0;
    g->spawn_point = 0;
    g->enemy_type_index = 0;
    g->powerup_active = false;
    g->powerup_type = 0;
    g->powerup_flash = 0;
    g->freeze_timer = 0;
    g->shovel_timer = 0;
    memcpy(g->enemy_counts, stage_enemy_counts[g->stage - 1], sizeof(g->enemy_counts));
    if (g->use_custom_map) {
        memcpy(g->map,g->custom_map,sizeof(g->map));
        // Start_Level redraws the bare HQ over the construction map; its
        // player-built wall (if any) is left untouched.
        for (int y=24; y<=25; ++y)
            for (int x=12; x<=13; ++x) g->map[y][x]=(Cell){BASE,0,0};
    } else {
        load_stage_map(g);
    }
    g->sound_events = 1u << SOUND_START;
    for (int p=0; p<g->players; ++p)
        if (g->lives[p]>0) player_spawn(g,p);
}

void game_start(Game *g) {
    game_start_mode(g, 1, 1);
}

void game_start_stage(Game *g, int stage) {
    game_start_mode(g, 1, stage);
}

void game_start_mode(Game *g, int players, int stage) {
    game_init(g);
    if (players < 1) players = 1;
    if (players > 2) players = 2;
    if (stage < 1) stage = 1;
    if (stage > STAGE_COUNT) stage = STAGE_COUNT;
    g->players = players;
    g->stage = stage;
    g->lives[0] = 3;
    g->lives[1] = players==2?3:0;
    g->score[0] = g->score[1] = 0;
    g->player_rank[0] = g->player_rank[1] = 0;
    stage_setup(g);
}

void game_set_stage(Game *g, int stage) {
    if (stage < 1) stage = STAGE_COUNT;
    if (stage > STAGE_COUNT) stage = 1;
    g->stage = stage;
    if (g->phase==STAGE_SELECT) {
        stage_setup(g);
        g->phase=STAGE_SELECT;
        g->sound_events=0;
    }
}

void game_next_stage(Game *g) {
    if (g->stage >= STAGE_COUNT) { g->phase = WON; return; }
    ++g->stage;
    stage_setup(g);
}

void game_restart_stage(Game *g) {
    stage_setup(g);
}

bool game_can_move(const Game *g, int tank, float x, float y) {
    (void)tank;
    // One pixel of sprite margin keeps adjacent lanes navigable.
    float left=x+1, top=y+1, size=14;
    if (x<0 || y<0 || x>FIELD_SIZE-16 || y>FIELD_SIZE-16) return false;
    for (int cy=(int)top/8; cy<=(int)(top+size-0.01f)/8; ++cy)
        for (int cx=(int)left/8; cx<=(int)(left+size-0.01f)/8; ++cx) {
            Cell c=g->map[cy][cx];
            if (c.type==STEEL || c.type==WATER || c.type==BASE) return false;
            if (c.type==BRICK) for (int q=0; q<4; ++q)
                if ((c.bricks & (1<<q)) && overlaps(left,top,size,size,
                        (float)(cx*8+(q%2)*4),(float)(cy*8+(q/2)*4),4,4)) return false;
        }
    // The original does not test tank-versus-tank collision; enemies and
    // players may briefly overlap while passing each other.
    return true;
}

static bool tank_on_ice(const Game *g, const Tank *t) {
    int tx=((int)t->x+8)/8, ty=((int)t->y+8)/8;
    return tx>=0 && ty>=0 && tx<MAP_SIZE && ty<MAP_SIZE &&
           g->map[ty][tx].type==ICE;
}

static bool move_tank(Game *g, int i, Direction dir, float speed) {
    Tank *t=&g->tanks[i];
    float x=t->x, y=t->y;
    if ((t->dir%2)!=(dir%2)) {
        // Snap only the perpendicular axis, and check the entire path.
        if (dir==UP || dir==DOWN) x=roundf(x/8)*8;
        else y=roundf(y/8)*8;
        float dist=fmaxf(fabsf(x-t->x),fabsf(y-t->y));
        for (int step=1; step<=(int)ceilf(dist); ++step) {
            float k=fminf(1,step/fmaxf(dist,1));
            if (!game_can_move(g,i,t->x+(x-t->x)*k,t->y+(y-t->y)*k)) {
                t->dir=dir; return false;
            }
        }
    }
    t->dir=dir;
    x+=dx[dir]*speed; y+=dy[dir]*speed;
    if (!game_can_move(g,i,x,y)) return false;
    t->x=x; t->y=y; ++t->animation;
    return true;
}

bool game_fire(Game *g, int i) {
    Tank *t=&g->tanks[i];
    if (!t->alive || t->spawn>0 || t->cooldown>0) return false;
    // Star upgrades raise the number of simultaneous player bullets: the
    // second star allows two, while the base tank and every enemy keep one.
    int limit=1;
    if (i<2 && i<g->players && g->player_rank[i]>=2) limit=2;
    int owner_bullets=0;
    for (int b=0; b<MAX_BULLETS; ++b)
        if (g->bullets[b].alive && g->bullets[b].owner==i) ++owner_bullets;
    if (owner_bullets>=limit) return false;
    for (int b=0; b<MAX_BULLETS; ++b) if (!g->bullets[b].alive) {
        // Begin at the muzzle; bullets advance in one pixel substeps.
        // Star upgrades and power tanks use the ROM's 4px/frame bullet;
        // the 3-star player bullet can also pierce steel.
        int speed=2,power=0;
        if (i<2 && i<g->players) {
            int rank=g->player_rank[i];
            power=rank>=3?3:(rank>=1?1:0);
            if (power) speed=4;
        } else if (t->kind==2) {
            speed=4;
            power=1;
        }
        g->bullets[b]=(Bullet){true,t->x+8+dx[t->dir]*8,
                              t->y+8+dy[t->dir]*8,t->dir,i,speed,power};
        t->cooldown=0;
        if (i<2 && i<g->players) g->sound_events |= 1u << SOUND_FIRE;
        return true;
    }
    return false;
}

void game_explode(Game *g, float x, float y, bool big) {
    for (int i=0; i<MAX_EFFECTS; ++i) if (g->effects[i].life==0) {
        int duration=big?24:9;
        g->effects[i]=(Effect){x,y,duration,duration,big}; return;
    }
}

static int tally_group_count(const Game *g,int type) {
    int count=0;
    for (int p=0;p<g->players;++p)
        if (g->kills[p][type]>count) count=g->kills[p][type];
    return count;
}

int game_tally_entries(const Game *g,int type,int timer) {
    int start=g->players==2?TALLY_START_2P:TALLY_START_1P;
    start+=(g->players==2?TALLY_GROUP_DELAY_2P:TALLY_GROUP_DELAY_1P)*type;
    for (int t=0;t<type;++t) start+=TALLY_ENTRY_DELAY*tally_group_count(g,t);
    int count=tally_group_count(g,type);
    if (timer<start || count==0) return 0;
    int shown=(timer-start)/TALLY_ENTRY_DELAY+1;
    return shown<count?shown:count;
}

int game_tally_total_frame(const Game *g) {
    int frame=(g->players==2?TALLY_START_2P:TALLY_START_1P)
             +(g->players==2?TALLY_GROUP_DELAY_2P:TALLY_GROUP_DELAY_1P)*4
             +TALLY_TOTAL_DELAY;
    for (int t=0;t<4;++t) frame+=TALLY_ENTRY_DELAY*tally_group_count(g,t);
    return frame;
}

int game_tally_end_frame(const Game *g) {
    return game_tally_total_frame(g)+TALLY_END_DELAY;
}

static void powerup_spawn(Game *g) {
    // PowerupGridSnap produces screen positions 48/96/144/192. In this
    // renderer the stored position is the field-relative centre (32/80/...).
    static const unsigned char weights[8]={0,1,2,3,4,5,4,3};
    int x=0,y=0;
    for (int attempt=0; attempt<8; ++attempt) {
        x=32+48*(int)(random_next(g)&3);
        y=32+48*(int)(random_next(g)&3);
        bool near=false;
        for (int p=0; p<g->players; ++p) {
            const Tank *t=&g->tanks[p];
            if (t->alive && abs(x-((int)t->x+8))<12 &&
                abs(y-((int)t->y+8))<12) near=true;
        }
        if (!near) break;
    }
    g->powerup_x=x;
    g->powerup_y=y;
    g->powerup_type=weights[random_next(g)&7];
    g->powerup_active=true;
    g->powerup_flash=0;
    g->sound_events |= 1u << SOUND_POWERUP;
}

static void powerup_apply(Game *g, int player, int type) {
    Tank *p=&g->tanks[player];
    switch (type) {
    case POWER_HELMET:
        p->shield=640; // 10 units x 64 frames
        break;
    case POWER_CLOCK:
        g->freeze_timer=640;
        break;
    case POWER_SHOVEL:
        g->shovel_timer=1280; // 20 units x 64 frames
        set_eagle_wall(g,0x10);
        break;
    case POWER_STAR:
        if (g->player_rank[player]<3) ++g->player_rank[player];
        p->rank=g->player_rank[player];
        break;
    case POWER_GRENADE:
        for (int i=g->players; i<MAX_TANKS; ++i) {
            Tank *t=&g->tanks[i];
            if (t->alive && t->spawn==0) {
                t->hp=1;
                damage_tank(g,i,player);
            }
        }
        break;
    case POWER_LIFE:
        ++g->lives[player];
        break;
    default: break;
    }
}

static void powerup_collect(Game *g) {
    if (!g->powerup_active) return;
    for (int player=0; player<g->players; ++player) {
        const Tank *p=&g->tanks[player];
        if (!p->alive) continue;
        if (abs((int)p->x+8-g->powerup_x)<12 &&
            abs((int)p->y+8-g->powerup_y)<12) {
            int type=g->powerup_type;
            g->powerup_active=false;
            g->powerup_flash=50;
            g->score[player]+=500;
            g->sound_events |= 1u << SOUND_POWERUP;
            powerup_apply(g,player,type);
            return;
        }
    }
}

static void damage_tank(Game *g, int i, int attacker) {
    Tank *t=&g->tanks[i];
    if (t->shield>0 || t->spawn>0) return;
    if (--t->hp>0) return;
    game_explode(g,t->x+8,t->y+8,true);
    t->alive=false;
    if (i<g->players) {
        g->sound_events |= 1u << SOUND_PLAYER;
        --g->lives[i];
        // The ROM drops every star level on death; only players who survive
        // into the next stage keep their upgrades.
        g->player_rank[i]=0;
        if (g->lives[i]>0) g->respawn_timer[i]=90;
    } else {
        g->sound_events |= 1u << SOUND_ENEMY;
        ++g->killed;
        int who=(attacker>=0 && attacker<g->players)?attacker:0;
        g->score[who]+=100*(t->kind+1);
        ++g->kills[who][t->kind&3];
        if (t->bonus) powerup_spawn(g);
    }
}

static void destroy_bullet(Game *g, Bullet *b) {
    if (!b->alive) return;
    b->alive=false;
    if (b->owner>=0 && b->owner<MAX_TANKS && g->tanks[b->owner].alive)
        g->tanks[b->owner].cooldown=9;
}

static bool bullet_wall(Game *g, Bullet *b) {
    bool hit=false;
    // The four pixel footprint cuts a strip of two brick quadrants.
    int minx=(int)floorf((b->x-2)/8), maxx=(int)floorf((b->x+1)/8);
    int miny=(int)floorf((b->y-2)/8), maxy=(int)floorf((b->y+1)/8);
    for (int y=miny; y<=maxy; ++y) for (int x=minx; x<=maxx; ++x) {
        if (x<0 || y<0 || x>=MAP_SIZE || y>=MAP_SIZE) continue;
        Cell *c=&g->map[y][x];
        if (c->type==BASE) {
            hit=true;
            if (g->base_alive) {
                g->base_alive=false; g->phase=LOST_FIELD; g->phase_timer=0;
                update_high_score(g);
                g->sound_events |= 1u << SOUND_BASE;
            }
            game_explode(g,104,200,true);
        } else if (c->type==STEEL) {
            hit=true;
            if (b->power&2) {
                // A 3-star player bullet erases steel tiles.
                c->type=EMPTY;
                c->tile=0;
                if (b->owner<g->players) g->sound_events |= 1u << SOUND_BRICK;
            } else if (b->owner<g->players) {
                g->sound_events |= 1u << SOUND_STEEL;
            }
        }
        else if (c->type==BRICK) {
            unsigned mask=0;
            for (int q=0; q<4; ++q) if ((c->bricks&(1<<q)) &&
                overlaps(b->x-2,b->y-2,4,4,(float)(x*8+(q%2)*4),(float)(y*8+(q/2)*4),4,4))
                    mask|=1u<<q;
            if (mask) {
                // Remove a 4px deep row/column at the impact edge.
                mask=b->dir%2==0 ? ((mask&3?3u:0)|(mask&12?12u:0)) :
                                      ((mask&5?5u:0)|(mask&10?10u:0));
                c->bricks &= (uint8_t)~mask;
                if (!c->bricks) c->type=EMPTY;
                hit=true;
                if (b->owner<g->players) g->sound_events |= 1u << SOUND_BRICK;
            }
        }
    }
    return hit;
}

static void bullets_step(Game *g) {
    // Advance every bullet one pixel at a time so bullet-versus-bullet tests
    // always compare positions from the same instant.  The old per-bullet
    // full move mixed this frame's position with another bullet's previous
    // position, making identical trajectories depend on slot order.
    for (int step=0; step<4; ++step) {
        for (int i=0; i<MAX_BULLETS; ++i) {
            Bullet *b=&g->bullets[i];
            if (!b->alive) continue;
            // The ROM moves ordinary bullets two pixels per 60 Hz tick and
            // power-tank bullets four.
            int speed=b->speed?b->speed:2;
            if (step>=speed) continue;
            b->x+=dx[b->dir]; b->y+=dy[b->dir];
            if (b->x<0 || b->y<0 || b->x>=FIELD_SIZE || b->y>=FIELD_SIZE || bullet_wall(g,b)) {
                if (b->owner<g->players && (b->x<0 || b->y<0 || b->x>=FIELD_SIZE || b->y>=FIELD_SIZE))
                    g->sound_events |= 1u << SOUND_STEEL;
                destroy_bullet(g,b); game_explode(g,b->x,b->y,false); continue;
            }
        }
        for (int i=0; i<MAX_BULLETS; ++i) for (int j=i+1; j<MAX_BULLETS; ++j) {
            Bullet *a=&g->bullets[i], *b=&g->bullets[j];
            if (!a->alive || !b->alive) continue;
            bool a_player=a->owner<g->players;
            bool b_player=b->owner<g->players;
            if (a_player==b_player) continue;
            if (fabsf(a->x-b->x)<4 && fabsf(a->y-b->y)<4) {
                // Interception uses the normal destruction path, so both
                // shooters receive the 9-frame cooldown.
                destroy_bullet(g,a);
                destroy_bullet(g,b);
            }
        }
        // Tank hits are resolved after interception, matching the original
        // bullet-vs-bullet-before-bullet-vs-tank order.
        for (int i=0; i<MAX_BULLETS; ++i) {
            Bullet *b=&g->bullets[i];
            if (!b->alive) continue;
            for (int j=0; j<MAX_TANKS; ++j) {
                Tank *t=&g->tanks[j];
                bool bullet_player=b->owner<g->players;
                bool tank_player=j<g->players;
                if (!t->alive || t->spawn>0 || bullet_player==tank_player) continue;
                if (overlaps(b->x-2,b->y-2,4,4,t->x+1,t->y+1,14,14)) {
                    damage_tank(g,j,b->owner); destroy_bullet(g,b); break;
                }
            }
        }
    }
}

static int next_enemy_kind(Game *g) {
    while (g->enemy_type_index < 4 && g->enemy_counts[g->enemy_type_index] == 0)
        ++g->enemy_type_index;
    if (g->enemy_type_index >= 4) return 0;
    --g->enemy_counts[g->enemy_type_index];
    // Some stages repeat a type in two slots (for example B-F-P-B), so the
    // slot index is not the tank kind; the ROM's type table is.
    switch (stage_enemy_types[g->stage - 1][g->enemy_type_index]) {
    case 0x0a: return 1;
    case 0x0c: return 2;
    case 0x0e: return 3;
    default:   return 0;
    }
}

static void enemy_spawn(Game *g) {
    if (g->remaining==0 || g->spawn_timer>0) return;
    // 1P keeps four enemy slots; 2P uses six, as in the ROM's $6C=5/7.
    int max_slot=g->players==2?MAX_TANKS-1:4;
    for (int i=max_slot; i>=g->players; --i) if (!g->tanks[i].alive) {
        // The original advances the round-robin index before spawning, so
        // the first three enemies appear at center, right, then left.
        g->spawn_point=(g->spawn_point+1)%3;
        float x=(float)(g->spawn_point*96);
        clear_tank_block(g,x,0);
        int kind=next_enemy_kind(g);
        bool bonus=g->remaining==3 || g->remaining==10 || g->remaining==17;
        g->tanks[i]=(Tank){.alive=true,.x=x,.y=0,.dir=DOWN,.spawn=50,
                           .think=30+(int)(random_next(g)%100),.cooldown=0,
                           .hp=kind==3?4:1,.kind=kind,.bonus=bonus};
        --g->remaining;
        g->spawn_timer=190-g->stage*4-(g->players==2?20:0);
        return;
    }
}

// ROM $DDA2 CalcDirToTarget.  Enemies randomly choose between the two
// $E486 tables: one prefers vertical movement when the target is off-row,
// the other prefers horizontal movement when the target is off-column.
static Direction direction_to_target(Game *g, const Tank *t, int tx, int ty) {
    int delta_x=tx-((int)t->x+8);
    int delta_y=ty-((int)t->y+8);
    if (random_next(g)&1) {
        if (delta_x<0) return LEFT;
        if (delta_x>0) return RIGHT;
        if (delta_y<0) return UP;
        if (delta_y>0) return DOWN;
        return UP;
    }
    if (delta_y<0) return UP;
    if (delta_y>0) return DOWN;
    if (delta_x<0) return LEFT;
    if (delta_x>0) return RIGHT;
    return UP;
}

// ROM $DE8E selects the player target for the mid-stage AI phase.  Even enemy
// slots prefer P1; odd slots prefer P2 when it is active, otherwise P1.
static const Tank *choose_target_player(const Game *g, int enemy_slot) {
    if (g->players<2) return g->tanks[0].alive?&g->tanks[0]:NULL;
    if (!g->tanks[0].alive) return g->tanks[1].alive?&g->tanks[1]:NULL;
    if ((enemy_slot&1)==0) return &g->tanks[0];
    return g->tanks[1].alive?&g->tanks[1]:&g->tanks[0];
}

// ROM $DE72 SpeedCtrlMove.  frame_hi is $0A (one unit per 64 frames) and
// $84 is the stage spawn-delay base.  Early in the stage enemies wander,
// then they alternate between chasing P1/P2, and finally they head for the
// eagle.  The previous order chased the player from frame zero, which drove
// the centre-spawn tank straight into the sealed shaft below the eagle wall.
static void choose_enemy_direction(Game *g, Tank *t, int i) {
    int frame_hi=g->tick/64;
    int delay=190-g->stage*4-(g->players==2?20:0);
    Direction dir;
    if (frame_hi>delay/4) {
        dir=direction_to_target(g,t,104,200);
    } else if (frame_hi>delay/8) {
        const Tank *target=choose_target_player(g,i);
        if (target) dir=direction_to_target(g,t,(int)target->x+8,(int)target->y+8);
        else dir=(Direction)(random_next(g)&3);
    } else {
        dir=(Direction)(random_next(g)&3);
    }
    t->dir=dir;
    t->think=30+(int)(random_next(g)%100);
}

// Draw_Pts_Screen requests both count-up slots ($0313/$0314) each time it
// decrements a kill counter.  The reveal cadence is one entry per type group
// every 9 frames; both player columns in 2P share a single tick.
static bool tally_tick_frame(const Game *g,int timer) {
    int start=g->players==2?TALLY_START_2P:TALLY_START_1P;
    int group=g->players==2?TALLY_GROUP_DELAY_2P:TALLY_GROUP_DELAY_1P;
    for (int type=0;type<4;++type) {
        int count=tally_group_count(g,type);
        if (count && timer>=start &&
            (timer-start)%TALLY_ENTRY_DELAY==0 &&
            (timer-start)/TALLY_ENTRY_DELAY<count) return true;
        start+=group+TALLY_ENTRY_DELAY*count;
    }
    return false;
}

void game_step(Game *g, Input in) {
    const Input inputs[2]={in,{.dir=UP,.moving=false,.fire=false}};
    game_step_inputs(g,inputs);
}

void game_step_inputs(Game *g, const Input inputs[2]) {
    // Mirrors the NMI Frame_Counter, which keeps running across every screen.
    // Construction uses its bit 4 to reproduce the ROM editor's cursor blink.
    ++g->frame_counter;
    // Construction mode runs its own 16-pixel cursor editor, independent of
    // the battlefield simulation.
    if (g->phase==CONSTRUCTION) {
        construction_step(g,&inputs[0]);
        return;
    }
    // Non-playing phases still advance their presentation timers so the
    // original game-over chain (tally -> brick screen -> title) runs by itself.
    bool lost_field=g->phase==LOST_FIELD;
    if (g->phase==LOST) {
        ++g->phase_timer;
        if (tally_tick_frame(g,g->phase_timer)) g->sound_events |= 1u<<SOUND_TALLY;
        if (g->phase_timer>=game_tally_end_frame(g)) {
            g->phase=GAME_OVER;
            g->phase_timer=0;
            g->sound_events |= 1u<<SOUND_GAMEOVER;
        }
        return;
    }
    if (g->phase==WON) {
        ++g->phase_timer;
        if (tally_tick_frame(g,g->phase_timer)) g->sound_events |= 1u<<SOUND_TALLY;
        return;
    }
    if (g->phase==GAME_OVER) {
        if (++g->phase_timer>=GAME_OVER_FRAMES) {
            g->phase=TITLE;
            g->phase_timer=0;
            g->title_anim=0;
        }
        return;
    }
    if (g->phase==TITLE) {
        if (g->title_anim>=0 && ++g->title_anim>=TITLE_DRAW_FRAMES)
            g->title_anim=-1;
        return;
    }
    if (g->phase!=PLAYING && !lost_field) return;
    ++g->tick;
    if (check_bonus_life(g)) lost_field=false;
    for (int i=0; i<MAX_EFFECTS; ++i) if (g->effects[i].life>0) --g->effects[i].life;
    if (g->powerup_flash>0) --g->powerup_flash;
    if (g->freeze_timer>0) --g->freeze_timer;
    if (g->shovel_timer>0 && --g->shovel_timer==0) set_eagle_wall(g,0x0f);
    if (g->spawn_timer>0) --g->spawn_timer;
    for (int p=0; p<2; ++p)
        if (g->respawn_timer[p]>0 && --g->respawn_timer[p]==0)
            player_spawn(g,p);
    enemy_spawn(g);
    for (int i=0; i<MAX_TANKS; ++i) {
        Tank *t=&g->tanks[i];
        if (!t->alive) continue;
        if (t->cooldown>0) --t->cooldown;
        if (t->shield>0) --t->shield;
        if (t->spawn>0) { --t->spawn; continue; }
        if (i<g->players) {
            if (lost_field) continue;
            // 3 pixels every 4 frames, as measured from the ROM's player
            // movement timing. Ice keeps the tank sliding in its last
            // direction when the d-pad is released.
            const Input *in=&inputs[i];
            bool moving=in->moving;
            Direction dir=in->dir;
            if (!moving && tank_on_ice(g,t)) { moving=true; dir=t->dir; }
            if (moving && g->tick%4!=2) move_tank(g,i,dir,1.0f);
            if (in->fire) game_fire(g,i);
        } else if (g->freeze_timer==0) {
            // Fast tanks move every frame; every other enemy type moves on
            // alternating frames. AI direction changes only happen on a
            // grid-aligned frame with 1-in-16 probability.
            bool fast=t->kind==1;
            if (fast || ((g->tick+i)&1)==0) {
                bool aligned=((int)t->x&7)==0 && ((int)t->y&7)==0;
                if (aligned && (random_next(g)&15)==0) {
                    choose_enemy_direction(g,t,i);
                } else if (!move_tank(g,i,t->dir,1.0f)) {
                    // 25% of blocked enemies reverse instead of waiting for
                    // the next grid-aligned direction change.
                    if ((random_next(g)&3)==0)
                        t->dir=(Direction)((t->dir+2)%4);
                }
            }
            // Enemy fire in the ROM is an independent 1-in-32 chance per
            // slot per frame.
            if (t->cooldown==0 && (random_next(g)&31)==0) game_fire(g,i);
        }
    }
    bullets_step(g);
    powerup_collect(g);
    // Scores can still change after the eagle is hit (same-frame shots and
    // bullets left in flight during LOST_FIELD).  Keep HI-SCORE in step with
    // the final score before the tally and title screens read it.
    update_high_score(g);
    // A kill made by a dead player's last bullet is scored after the tank
    // death, so evaluate the bonus life and the final failure only once all
    // bullets for this frame have been resolved.
    if (check_bonus_life(g)) lost_field=false;
    if (g->phase==PLAYING && g->lives[0]+g->lives[1]==0) {
        g->phase=LOST_FIELD;
        g->phase_timer=0;
        update_high_score(g);
    }
    if (lost_field) {
        if (++g->phase_timer>=LOST_FIELD_DELAY+LOST_FIELD_TEXT_FRAMES) {
            g->phase=LOST;
            g->phase_timer=0;
        }
        return;
    }
    if (g->phase==PLAYING && g->remaining==0 && g->killed==20) {
        g->phase=WON;
        g->phase_timer=0;
        update_high_score(g);
        g->sound_events |= 1u<<SOUND_STAGE_CLEAR;
    }
}
