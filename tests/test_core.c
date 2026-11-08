#include "game.h"
#include "render.h"
#include "rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#c); exit(1); } } while (0)

static void arena(Game *g) {
    game_start(g);
    memset(g->map,0,sizeof(g->map));
    g->spawn_timer=100000;
    g->tanks[0]=(Tank){.alive=true,.x=64,.y=160,.dir=UP,.hp=1};
}

static void step(Game *g,int n) { for(int i=0;i<n;++i) game_step(g,(Input){0}); }

static void collision_test(void) {
    Game g;arena(&g);
    CHECK(!game_can_move(&g,0,-1,0));CHECK(!game_can_move(&g,0,0,193));
    CHECK(game_can_move(&g,0,0,0));
    g.map[2][2]=(Cell){BRICK,15,0};CHECK(!game_can_move(&g,0,16,16));
    g.map[2][2]=(Cell){WATER,0,0};CHECK(!game_can_move(&g,0,16,16));
    g.map[2][2]=(Cell){FOREST,0,0};CHECK(game_can_move(&g,0,16,16));
    g.map[2][2]=(Cell){BASE,0,0};CHECK(!game_can_move(&g,0,16,16));
    g.map[2][2]=(Cell){EMPTY,0,0};
    // The original has no tank-versus-tank collision.
    g.tanks[1]=(Tank){.alive=true,.x=16,.y=16};
    CHECK(game_can_move(&g,0,16,16));
}

static void wall_test(void) {
    Game g;arena(&g);
    g.map[18][8]=(Cell){BRICK,15,0};g.map[18][9]=(Cell){BRICK,15,0};
    CHECK(game_fire(&g,0));CHECK(!game_fire(&g,0));
    CHECK(g.sound_events&(1u<<SOUND_FIRE));
    step(&g,8);
    CHECK(g.map[18][8].bricks==3);CHECK(g.map[18][9].bricks==3);
    CHECK(g.sound_events&(1u<<SOUND_BRICK));
    step(&g,18);CHECK(game_fire(&g,0));step(&g,8);
    CHECK(g.map[18][8].type==EMPTY);CHECK(g.map[18][9].type==EMPTY);
    arena(&g);g.map[18][8]=(Cell){STEEL,0,0};g.map[18][9]=(Cell){STEEL,0,0};
    CHECK(game_fire(&g,0));step(&g,10);CHECK(g.map[18][8].type==STEEL);
    CHECK(g.sound_events&(1u<<SOUND_STEEL));
    for(int i=0;i<MAX_BULLETS;++i) CHECK(!g.bullets[i].alive);
}

static void combat_test(void) {
    Game g;arena(&g);
    g.tanks[1]=(Tank){.alive=true,.x=64,.y=128,.dir=DOWN,.hp=1,.cooldown=999,.think=999};
    CHECK(game_fire(&g,0));step(&g,8);
    CHECK(!g.tanks[1].alive);CHECK(g.killed==1);CHECK(g.score[0]==100);
    CHECK(g.kills[0][0]==1);
    CHECK(g.sound_events&(1u<<SOUND_ENEMY));
    arena(&g);g.map[18][8]=(Cell){BASE,0,0};
    CHECK(game_fire(&g,0));step(&g,8);CHECK(g.phase==LOST_FIELD);CHECK(!g.base_alive);
    CHECK(g.sound_events&(1u<<SOUND_BASE));
    arena(&g);g.tanks[0].shield=100;
    g.bullets[0]=(Bullet){true,72,158,DOWN,1,2,0};step(&g,1);
    CHECK(g.lives[0]==3);CHECK(g.tanks[0].alive);
    g.tanks[0].shield=0;g.bullets[0]=(Bullet){true,72,158,DOWN,1,2,0};step(&g,1);
    CHECK(g.lives[0]==2);CHECK(!g.tanks[0].alive);
    CHECK(g.sound_events&(1u<<SOUND_PLAYER));
    step(&g,90);CHECK(g.tanks[0].alive);CHECK(g.tanks[0].shield>0);
    arena(&g);g.player_rank[0]=3;g.tanks[0].rank=3;g.tanks[0].shield=0;
    g.bullets[0]=(Bullet){true,72,158,DOWN,1,2,0};step(&g,1);
    CHECK(!g.tanks[0].alive && g.lives[0]==2 && g.player_rank[0]==0);
    step(&g,123);
    CHECK(g.tanks[0].alive && g.tanks[0].rank==0 && g.player_rank[0]==0);
    CHECK(game_fire(&g,0));
    for(int i=0;i<MAX_BULLETS;++i) if(g.bullets[i].alive)
        CHECK(g.bullets[i].speed==2 && g.bullets[i].power==0);
    arena(&g);g.remaining=0;g.killed=19;g.high_score=50;
    g.tanks[1]=(Tank){.alive=true,.x=64,.y=128,.dir=DOWN,.hp=1,.cooldown=999,.think=999};
    CHECK(game_fire(&g,0));step(&g,8);CHECK(g.phase==WON);
    CHECK(g.high_score>=100);
    CHECK(g.sound_events&(1u<<SOUND_STAGE_CLEAR));
    arena(&g);g.tanks[1]=(Tank){.alive=true,.x=64,.y=128,.dir=DOWN,.hp=1,.cooldown=999,.think=999};
    g.bullets[0]=(Bullet){true,20,20,RIGHT,0,2,0};
    g.bullets[1]=(Bullet){true,24,20,LEFT,1,2,0};step(&g,1);
    CHECK(!g.bullets[0].alive && !g.bullets[1].alive);
    // Interception applies the same 9-frame cooldown as wall hits.
    CHECK(g.tanks[0].cooldown==9 && g.tanks[1].cooldown==9);
    step(&g,9);
    CHECK(g.tanks[0].cooldown==0);
    CHECK(game_fire(&g,0));

    // Same-direction, same-speed bullets keep their 4px spacing and never
    // collide, regardless of which array slot holds which bullet.
    for (int swap=0; swap<2; ++swap) {
        arena(&g);
        g.tanks[1]=(Tank){.alive=true,.x=64,.y=128,.dir=DOWN,.hp=1,
                          .cooldown=999,.think=999};
        int pslot=swap, eslot=1-swap;
        g.bullets[pslot]=(Bullet){true,20,20,RIGHT,0,2,0};
        g.bullets[eslot]=(Bullet){true,24,20,RIGHT,1,2,0};
        step(&g,1);
        CHECK(g.bullets[pslot].alive && g.bullets[eslot].alive);
        CHECK((int)g.bullets[pslot].x==22 && (int)g.bullets[eslot].x==26);
    }
    // A slower bullet behind a faster one must also stay alive.
    arena(&g);g.tanks[1]=(Tank){.alive=true,.x=64,.y=128,.dir=DOWN,.hp=1,
                                .cooldown=999,.think=999};
    g.bullets[0]=(Bullet){true,20,20,RIGHT,0,2,0};
    g.bullets[1]=(Bullet){true,24,20,RIGHT,1,4,0};
    step(&g,1);
    CHECK(g.bullets[0].alive && g.bullets[1].alive);
    CHECK((int)g.bullets[0].x==22 && (int)g.bullets[1].x==28);
}

static void progression_test(void) {
    Game a,b;game_start(&a);game_start(&b);
    for(int n=0;n<5000;++n) {
        Input input={(Direction)((n/180)%4),true,true,false};
        game_step(&a,input);game_step(&b,input);
        CHECK(memcmp(&a,&b,sizeof(a))==0);
        CHECK(a.remaining>=0 && a.remaining<=20);CHECK(a.lives[0]>=0 && a.lives[0]<=3);
        for(int i=0;i<MAX_TANKS;++i) if(a.tanks[i].alive) {
            CHECK(a.tanks[i].x>=0 && a.tanks[i].x<=192);
            CHECK(a.tanks[i].y>=0 && a.tanks[i].y<=192);
        }
    }
    a.phase=PAUSED;Game frozen=a;step(&a,100);
    // Frame_Counter is the free-running NMI presentation clock; everything
    // else must be frozen while paused.
    a.frame_counter=frozen.frame_counter;
    CHECK(memcmp(&a,&frozen,sizeof(a))==0);
}

static int map_count(const Game *g,Terrain type) {
    int count=0;
    for(int y=0;y<MAP_SIZE;++y) for(int x=0;x<MAP_SIZE;++x)
        count+=g->map[y][x].type==type;
    return count;
}

static void stage_test(void) {
    Game g;game_start_stage(&g,1);
    CHECK(g.stage==1);
    CHECK(g.enemy_counts[0]==18 && g.enemy_counts[1]==2 &&
          g.enemy_counts[2]==0 && g.enemy_counts[3]==0);
    CHECK(map_count(&g,BRICK)==220);
    CHECK(map_count(&g,STEEL)==12);
    CHECK(map_count(&g,BASE)==4);

    game_start_stage(&g,35);
    CHECK(g.stage==35);
    CHECK(g.enemy_counts[0]==4 && g.enemy_counts[1]==6 &&
          g.enemy_counts[2]==0 && g.enemy_counts[3]==10);
    CHECK(map_count(&g,WATER)>0 && map_count(&g,FOREST)>0);

    g.score[0]=500;g.lives[0]=2;
    game_next_stage(&g);
    CHECK(g.stage==35 && g.phase==WON); // stage 35 is the last playable stage

    game_start_stage(&g,2);g.score[0]=500;g.lives[0]=2;
    game_next_stage(&g);
    CHECK(g.stage==3 && g.score[0]==500 && g.lives[0]==2);
    CHECK(map_count(&g,BRICK)==130); // stage 3's decoded brick cells

    // Stage selection reloads the map but keeps the players' progression.
    Game s;game_start_mode(&s,2,1);
    s.score[0]=700;s.score[1]=300;s.lives[0]=2;s.lives[1]=1;
    s.player_rank[0]=3;s.phase=STAGE_SELECT;
    game_set_stage(&s,10);
    CHECK(s.stage==10 && s.phase==STAGE_SELECT);
    CHECK(s.score[0]==700 && s.score[1]==300);
    CHECK(s.lives[0]==2 && s.lives[1]==1 && s.player_rank[0]==3);
    game_set_stage(&s,0);CHECK(s.stage==35);
    game_set_stage(&s,36);CHECK(s.stage==1);
}

static void movement_test(void) {
    Game g;arena(&g);
    g.tanks[0]=(Tank){.alive=true,.x=64,.y=160,.dir=UP,.hp=1};
    Input right={RIGHT,true,false,false};
    step(&g,4);
    CHECK(g.tanks[0].x==64); // no input supplied by step()
    for(int i=0;i<4;++i) game_step(&g,right);
    // Three pixels in four frames, measured from the ROM player timing.
    CHECK(g.tanks[0].x==67);

    arena(&g);
    g.tanks[0]=(Tank){.alive=true,.x=64,.y=160,.dir=UP,.hp=1};
    CHECK(game_fire(&g,0));
    int y=0;
    for(int i=0;i<MAX_BULLETS;++i) if(g.bullets[i].alive) y=(int)g.bullets[i].y;
    step(&g,2);
    bool found=false;
    for(int i=0;i<MAX_BULLETS;++i) if(g.bullets[i].alive) {
        CHECK((int)g.bullets[i].y==y-4);
        found=true;
    }
    CHECK(found);

    // Power tanks fire the 4px/frame bullet used by the ROM.
    arena(&g);
    g.tanks[1]=(Tank){.alive=true,.x=64,.y=64,.dir=DOWN,.hp=1,.kind=2};
    CHECK(game_fire(&g,1));
    for(int i=0;i<MAX_BULLETS;++i) if(g.bullets[i].alive)
        CHECK(g.bullets[i].owner==1 && g.bullets[i].speed==4);

    // Ice keeps the player sliding after the d-pad is released.
    arena(&g);
    g.map[21][9]=(Cell){ICE,0,0x21};
    g.tanks[0]=(Tank){.alive=true,.x=64,.y=160,.dir=RIGHT,.hp=1};
    step(&g,4);
    CHECK(g.tanks[0].x==67);
}

static void spawn_kinds(int stage,int kinds[20],int bonus[20]) {
    Game g;game_start_stage(&g,stage);
    for(int n=0;n<20;++n) {
        for(int i=1;i<MAX_TANKS;++i) g.tanks[i].alive=false;
        int before=g.remaining;
        g.spawn_timer=0;
        for(int frame=0;frame<8 && g.remaining==before;++frame)
            game_step(&g,(Input){0});
        CHECK(g.remaining==before-1);
        for(int i=1;i<MAX_TANKS;++i) if(g.tanks[i].alive) {
            kinds[n]=g.tanks[i].kind;
            bonus[n]=g.tanks[i].bonus;
        }
    }
    CHECK(g.remaining==0);
}

static void spawn_order_test(void) {
    int kinds[20]={0},bonus[20]={0};
    spawn_kinds(1,kinds,bonus);
    for(int n=0;n<18;++n) CHECK(kinds[n]==0);
    CHECK(kinds[18]==1 && kinds[19]==1);
    CHECK(bonus[3] && bonus[10] && bonus[17]);
    CHECK(!bonus[2] && !bonus[4] && !bonus[9] && !bonus[11]);

    // Stage 7 repeats basic in slot 3, so the type table (not the slot
    // index) must decide the spawned tank kind.
    spawn_kinds(7,kinds,bonus);
    for(int n=0;n<3;++n) CHECK(kinds[n]==0);
    for(int n=3;n<7;++n) CHECK(kinds[n]==1);
    for(int n=7;n<13;++n) CHECK(kinds[n]==2);
    for(int n=13;n<20;++n) CHECK(kinds[n]==0);

    // SpawnEnemy clears the 2x2 block at the enemy's centre. The first
    // round-robin step uses the centre spawn at field x=96.
    Game s;arena(&s);
    for(int y=0;y<2;++y) for(int x=12;x<14;++x)
        s.map[y][x]=(Cell){BRICK,15,0};
    s.spawn_timer=0;
    step(&s,1);
    for(int y=0;y<2;++y) for(int x=12;x<14;++x)
        CHECK(s.map[y][x].type==EMPTY);
}

static void collect_type(Game *g,int type) {
    g->powerup_active=true;
    g->powerup_x=(int)g->tanks[0].x+8;
    g->powerup_y=(int)g->tanks[0].y+8;
    g->powerup_type=type;
    g->score[0]=0;
    step(g,1);
    CHECK(!g->powerup_active);
    CHECK(g->score[0]>=500); // 500 pickup + any kills caused by the effect
    CHECK(g->sound_events&(1u<<SOUND_POWERUP));
}

static void powerup_test(void) {
    Game g;arena(&g);
    // Killing a flashing carrier creates a powerup at one of the ROM's
    // four-by-four grid positions.
    g.tanks[1]=(Tank){.alive=true,.x=64,.y=128,.dir=DOWN,.hp=1,
                      .cooldown=999,.think=999,.bonus=true};
    CHECK(game_fire(&g,0));
    step(&g,8);
    CHECK(!g.tanks[1].alive);
    CHECK(g.powerup_active);
    CHECK(g.sound_events&(1u<<SOUND_POWERUP));
    CHECK(g.powerup_x>=32 && g.powerup_x<=176);
    CHECK(g.powerup_y>=32 && g.powerup_y<=176);

    arena(&g);
    g.tanks[0].shield=0;collect_type(&g,POWER_HELMET);
    CHECK(g.tanks[0].shield>600);

    arena(&g);collect_type(&g,POWER_CLOCK);
    CHECK(g.freeze_timer>600);
    g.tanks[1]=(Tank){.alive=true,.x=64,.y=64,.dir=RIGHT,.hp=1};
    int x=(int)g.tanks[1].x;
    step(&g,4);
    CHECK((int)g.tanks[1].x==x);

    arena(&g);collect_type(&g,POWER_SHOVEL);
    CHECK(g.shovel_timer>1200);
    CHECK(g.map[23][11].type==STEEL);
    g.shovel_timer=1;step(&g,1);
    CHECK(g.map[23][11].type==BRICK);

    arena(&g);collect_type(&g,POWER_STAR);
    CHECK(g.player_rank[0]==1 && g.tanks[0].rank==1);
    arena(&g);g.player_rank[0]=3;g.tanks[0].rank=3;
    CHECK(game_fire(&g,0));
    for(int i=0;i<MAX_BULLETS;++i) if(g.bullets[i].alive)
        CHECK(g.bullets[i].power==3 && g.bullets[i].speed==4);

    // One star keeps one bullet; two and three stars allow two at a time.
    arena(&g);g.player_rank[0]=1;g.tanks[0].rank=1;
    CHECK(game_fire(&g,0));
    CHECK(!game_fire(&g,0));
    arena(&g);g.player_rank[0]=2;g.tanks[0].rank=2;
    CHECK(game_fire(&g,0));
    CHECK(game_fire(&g,0));
    CHECK(!game_fire(&g,0));
    // Destroying one bullet starts the normal 9-frame cooldown and then the
    // free capacity is usable again.
    g.bullets[0]=(Bullet){true,20,20,RIGHT,0,2,0};
    g.map[2][2]=(Cell){BRICK,15,0};
    step(&g,1);
    CHECK(!g.bullets[0].alive && g.tanks[0].cooldown==9);
    CHECK(!game_fire(&g,0));
    step(&g,9);
    CHECK(g.tanks[0].cooldown==0);
    CHECK(game_fire(&g,0));
    arena(&g);g.player_rank[0]=3;g.tanks[0].rank=3;
    CHECK(game_fire(&g,0));
    CHECK(game_fire(&g,0));
    CHECK(!game_fire(&g,0));

    arena(&g);
    g.tanks[1]=(Tank){.alive=true,.x=32,.y=64,.dir=DOWN,.hp=1};
    g.tanks[2]=(Tank){.alive=true,.x=96,.y=64,.dir=DOWN,.hp=1};
    collect_type(&g,POWER_GRENADE);
    CHECK(!g.tanks[1].alive && !g.tanks[2].alive);
    CHECK(g.killed==2 && g.score[0]==500+200);
    CHECK(g.kills[0][0]==2);

    arena(&g);int lives=g.lives[0];collect_type(&g,POWER_LIFE);
    CHECK(g.lives[0]==lives+1);
}

static void two_player_test(void) {
    Game g;game_start_mode(&g,2,1);
    CHECK(g.players==2);
    CHECK(g.lives[0]==3 && g.lives[1]==3);
    CHECK(g.tanks[0].alive && g.tanks[1].alive);
    CHECK((int)g.tanks[0].x==64 && (int)g.tanks[1].x==128);

    // P2 occupies slot 1; enemies use slots 2..7 and spawn from the top down.
    g.tanks[7].alive=false;
    g.tanks[6].alive=false;
    g.spawn_timer=0;
    step(&g,1);
    CHECK(g.tanks[7].alive && !g.tanks[6].alive);
    CHECK(g.tanks[1].alive); // P2 was not overwritten by an enemy spawn

    // Both players move independently.
    memset(g.map,0,sizeof(g.map));
    g.spawn_timer=100000;
    g.tanks[0]=(Tank){.alive=true,.x=64,.y=160,.dir=UP,.hp=1};
    g.tanks[1]=(Tank){.alive=true,.x=128,.y=160,.dir=UP,.hp=1};
    Input inputs[2]={{RIGHT,true,false,false},{LEFT,true,false,false}};
    for(int i=0;i<4;++i) game_step_inputs(&g,inputs);
    CHECK((int)g.tanks[0].x==67 && (int)g.tanks[1].x==125);

    // P2's kill score and lives are independent from P1.
    Game p;game_start_mode(&p,2,1);
    memset(p.map,0,sizeof(p.map));
    p.spawn_timer=100000;
    p.freeze_timer=1000;
    p.tanks[0]=(Tank){.alive=true,.x=64,.y=160,.dir=UP,.hp=1};
    p.tanks[1]=(Tank){.alive=true,.x=128,.y=160,.dir=UP,.hp=1};
    p.tanks[2]=(Tank){.alive=true,.x=128,.y=128,.dir=DOWN,.hp=1};
    CHECK(game_fire(&p,1));
    step(&p,30);
    CHECK(!p.tanks[2].alive);
    CHECK(p.score[1]==100 && p.score[0]==0);
    CHECK(p.kills[1][0]==1 && p.kills[0][0]==0);

    // An enemy bullet removes one P2 life and respawns P2 after 90 frames.
    p.bullets[0]=(Bullet){true,136,p.tanks[1].y+7,DOWN,2,2,0};
    step(&p,1);
    CHECK(p.lives[1]==2 && !p.tanks[1].alive);
    CHECK(p.respawn_timer[1]==89 || p.respawn_timer[1]==90);
    CHECK(p.phase==PLAYING);
    step(&p,90);
    CHECK(p.tanks[1].alive && (int)p.tanks[1].x==128);

    // The game ends only when both players have no lives left.
    p.lives[0]=0;p.lives[1]=1;
    p.tanks[1].shield=0;p.tanks[1].spawn=0;
    p.bullets[0]=(Bullet){true,136,p.tanks[1].y+7,DOWN,2,2,0};
    step(&p,1);
    CHECK(p.lives[1]==0 && p.phase==LOST_FIELD);
}

static void high_score_after_base_test(void) {
    Game g;arena(&g);
    g.high_score=20000;g.score[0]=19900;
    g.map[10][10]=(Cell){BASE,0,0};
    g.tanks[1]=(Tank){.alive=true,.x=64,.y=0,.dir=DOWN,.hp=1,.kind=3,
                      .cooldown=999,.think=999,.spawn=0};
    // The first bullet destroys the eagle; the second kills an armored tank
    // for 400 points in the same frame.
    g.bullets[0]=(Bullet){true,78,84,RIGHT,1,2,0};
    g.bullets[1]=(Bullet){true,72,16,UP,0,2,0};
    step(&g,1);
    CHECK(!g.base_alive && g.phase==LOST_FIELD);
    CHECK(g.score[0]==20300 && g.high_score==20300);
    // The high score survives the complete failure/tally/title chain.
    step(&g,700);
    CHECK(g.phase==TITLE && g.high_score==20300);

    // A kill made by a bullet already in flight during LOST_FIELD also counts.
    Game h;arena(&h);
    h.high_score=20000;h.score[0]=19900;
    h.map[10][10]=(Cell){BASE,0,0};
    h.tanks[1]=(Tank){.alive=true,.x=64,.y=4,.dir=DOWN,.hp=1,.kind=3,
                      .cooldown=999,.think=999,.spawn=0};
    h.bullets[0]=(Bullet){true,78,84,RIGHT,1,2,0};
    step(&h,1);
    CHECK(h.phase==LOST_FIELD && h.score[0]==19900);
    h.bullets[1]=(Bullet){true,72,16,UP,0,2,0};
    step(&h,1);
    CHECK(h.score[0]==20300 && h.high_score==20300);
}

static void bonus_life_test(void) {
    Game g;arena(&g);
    g.lives[0]=3;
    g.score[0]=19900;
    step(&g,1);
    CHECK(g.lives[0]==3);
    g.score[0]=20000;
    step(&g,1);
    CHECK(g.lives[0]==4 && g.bonus_awarded[0]);
    CHECK(g.sound_events&((1u<<SOUND_ENEMY)|(1u<<SOUND_PLAYER)));
    g.score[0]=90000;
    step(&g,1);
    CHECK(g.lives[0]==4); // only once per game
    Game h;arena(&h);
    h.base_alive=false;h.score[0]=30000;
    step(&h,1);
    CHECK(h.lives[0]==3 && !h.bonus_awarded[0]);

    // A life earned after the last tank was destroyed revives the player.
    Game r;game_start_mode(&r,2,1);
    memset(r.map,0,sizeof(r.map));
    r.spawn_timer=100000;r.freeze_timer=1000;
    r.tanks[0].alive=false;r.lives[0]=0;r.respawn_timer[0]=0;
    r.score[0]=20000;
    step(&r,1);
    CHECK(r.lives[0]==1 && r.bonus_awarded[0]);
    CHECK(r.respawn_timer[0]>0 && r.phase==PLAYING);
    step(&r,90);
    CHECK(r.tanks[0].alive);

    // In 1P, a last bullet that reaches 20,000 in the same frame as the
    // player's death must award the life before the failure check.
    Game s;arena(&s);
    s.lives[0]=1;s.score[0]=19900;
    s.tanks[0].shield=0;s.tanks[0].spawn=0;
    s.tanks[1]=(Tank){.alive=true,.x=64,.y=0,.dir=DOWN,.hp=1,
                      .cooldown=999,.think=999,.spawn=0};
    s.bullets[0]=(Bullet){true,72,158,DOWN,1,2,0};
    s.bullets[1]=(Bullet){true,72,16,UP,0,2,0};
    step(&s,1);
    CHECK(!s.tanks[0].alive && !s.tanks[1].alive);
    CHECK(s.score[0]==20000 && s.lives[0]==1);
    CHECK(s.phase==PLAYING && s.respawn_timer[0]>0);
    step(&s,90);
    CHECK(s.tanks[0].alive);

    // If the last bullet reaches the threshold on the frame after the last
    // death, the already-started LOST_FIELD must be cancelled by the 1UP.
    Game t;arena(&t);
    t.lives[0]=1;t.score[0]=19900;
    t.tanks[0].shield=0;t.tanks[0].spawn=0;
    t.tanks[1]=(Tank){.alive=true,.x=64,.y=0,.dir=DOWN,.hp=1,
                      .cooldown=999,.think=999,.spawn=0};
    t.bullets[0]=(Bullet){true,72,158,DOWN,1,2,0};
    t.bullets[1]=(Bullet){true,72,20,UP,0,2,0};
    step(&t,1);
    CHECK(!t.tanks[0].alive && t.phase==LOST_FIELD && t.score[0]==19900);
    step(&t,1);
    CHECK(!t.tanks[1].alive && t.score[0]==20000);
    CHECK(t.lives[0]==1 && t.respawn_timer[0]>0 && t.phase==PLAYING);
    step(&t,90);
    CHECK(t.tanks[0].alive);

    // A destroyed eagle still forbids the bonus life and cannot be revived.
    Game u;arena(&u);
    u.base_alive=false;u.phase=LOST_FIELD;u.phase_timer=0;
    u.score[0]=20000;u.lives[0]=0;u.tanks[0].alive=false;
    u.respawn_timer[0]=0;
    step(&u,1);
    CHECK(!u.bonus_awarded[0] && u.lives[0]==0);
    CHECK(u.respawn_timer[0]==0 && u.phase==LOST_FIELD);
}

static void tally_animation_test(void) {
    Game g;arena(&g);
    g.kills[0][0]=1;g.kills[0][1]=2;g.kills[0][2]=3;g.kills[0][3]=4;
    CHECK(game_tally_entries(&g,0,32)==0);
    CHECK(game_tally_entries(&g,0,33)==1);
    CHECK(game_tally_entries(&g,1,71)==0);
    CHECK(game_tally_entries(&g,1,72)==1);
    CHECK(game_tally_entries(&g,1,81)==2);
    CHECK(game_tally_entries(&g,2,119)==0);
    CHECK(game_tally_entries(&g,2,120)==1);
    CHECK(game_tally_entries(&g,2,129)==2);
    CHECK(game_tally_entries(&g,2,138)==3);
    CHECK(game_tally_entries(&g,3,176)==0);
    CHECK(game_tally_entries(&g,3,177)==1);
    CHECK(game_tally_entries(&g,3,204)==4);
    CHECK(game_tally_total_frame(&g)==251);
    CHECK(game_tally_end_frame(&g)==393);

    // Two-player mode processes both columns together with 30-frame group
    // pauses (measured from FCEUX), so it finishes earlier than the 1P tally.
    Game two;game_start_mode(&two,2,1);
    two.kills[0][0]=1;two.kills[0][1]=2;two.kills[0][2]=3;two.kills[0][3]=4;
    two.kills[1][0]=5;two.kills[1][1]=6;two.kills[1][2]=7;two.kills[1][3]=8;
    CHECK(game_tally_entries(&two,0,33)==0);
    CHECK(game_tally_entries(&two,0,34)==1);
    CHECK(game_tally_entries(&two,0,70)==5);
    CHECK(game_tally_entries(&two,1,108)==0);
    CHECK(game_tally_entries(&two,1,109)==1);
    CHECK(game_tally_entries(&two,1,154)==6);
    CHECK(game_tally_entries(&two,2,193)==1);
    CHECK(game_tally_entries(&two,2,247)==7);
    CHECK(game_tally_entries(&two,3,286)==1);
    CHECK(game_tally_entries(&two,3,349)==8);
    CHECK(game_tally_total_frame(&two)==396);
    CHECK(game_tally_end_frame(&two)==538);
    // The score row is shown immediately; only the breakdown counts up.
    Game h;arena(&h);
    h.phase=LOST;h.phase_timer=0;h.kills[0][3]=1;
    step(&h,game_tally_total_frame(&h)-1);
    CHECK(h.phase==LOST);
    step(&h,game_tally_end_frame(&h)-game_tally_total_frame(&h)+1);
    CHECK(h.phase==GAME_OVER);

    // Draw_Pts_Screen requests sound slots 19/20 once per revealed kill.
    const unsigned tally_bit=1u<<SOUND_TALLY;
    Game tick;arena(&tick);
    tick.kills[0][0]=2;
    tick.phase=LOST;tick.phase_timer=0;tick.sound_events=0;
    step(&tick,32);
    CHECK(!(tick.sound_events&tally_bit));
    step(&tick,1);
    CHECK(tick.sound_events&tally_bit);
    tick.sound_events=0;
    step(&tick,8);
    CHECK(!(tick.sound_events&tally_bit));
    step(&tick,1);
    CHECK(tick.sound_events&tally_bit);

    Game win;arena(&win);
    win.kills[0][0]=1;
    win.phase=WON;win.phase_timer=0;win.sound_events=0;
    step(&win,32);
    CHECK(!(win.sound_events&tally_bit));
    step(&win,1);
    CHECK(win.sound_events&tally_bit);

    Game none;arena(&none);
    none.phase=LOST;none.phase_timer=0;none.sound_events=0;
    step(&none,game_tally_total_frame(&none));
    CHECK(!(none.sound_events&tally_bit));

    Game two_tick;game_start_mode(&two_tick,2,1);
    two_tick.kills[0][0]=1;two_tick.kills[1][0]=5;
    two_tick.kills[1][1]=1;
    two_tick.phase=LOST;two_tick.phase_timer=0;two_tick.sound_events=0;
    step(&two_tick,33);
    CHECK(!(two_tick.sound_events&tally_bit));
    step(&two_tick,1);
    CHECK(two_tick.sound_events&tally_bit);
    two_tick.sound_events=0;
    step(&two_tick,8);
    CHECK(!(two_tick.sound_events&tally_bit));
    step(&two_tick,1);
    CHECK(two_tick.sound_events&tally_bit);
    two_tick.sound_events=0;
    step(&two_tick,70-43);
    CHECK(two_tick.sound_events&tally_bit);
    two_tick.sound_events=0;
    step(&two_tick,108-70);
    CHECK(!(two_tick.sound_events&tally_bit));
    step(&two_tick,1);
    CHECK(two_tick.sound_events&tally_bit);
}

static void construction_test(void) {
    Game g;game_init(&g);
    g.high_score=34500;
    game_enter_construction(&g);
    CHECK(g.phase==CONSTRUCTION);
    CHECK(g.high_score==34500);
    CHECK(g.editor_x==0 && g.editor_y==0 && g.editor_block==0);
    CHECK(g.map[23][11].type==BRICK && g.map[23][14].type==BRICK);
    CHECK(g.map[24][12].type==BASE && g.map[25][13].type==BASE);
    CHECK(g.map[0][0].type==EMPTY);

    // The first direction press moves immediately; a held direction repeats
    // after 20 frames and then every 5, leaving the cursor on a 16px grid.
    Input right={.dir=RIGHT,.moving=true};
    game_step_inputs(&g,(Input[2]){right,{.dir=UP}});
    CHECK(g.editor_x==1 && g.editor_y==0);
    for (int i=0;i<18;++i) game_step_inputs(&g,(Input[2]){right,{.dir=UP}});
    CHECK(g.editor_x==1);
    game_step_inputs(&g,(Input[2]){right,{.dir=UP}});
    CHECK(g.editor_x==2);
    for (int i=0;i<5;++i) game_step_inputs(&g,(Input[2]){right,{.dir=UP}});
    CHECK(g.editor_x==3);
    game_step_inputs(&g,(Input[2]){{.dir=UP},{.dir=UP}});
    CHECK(g.editor_x==3);

    // A's first edge paints the selected block; later edges advance to the
    // next TSA entry.  Cell (3,0) maps to map columns 6-7 of rows 0-1.
    Input a={.dir=UP,.fire=true};
    game_step_inputs(&g,(Input[2]){a,{.dir=UP}});
    CHECK(g.editor_block==0 && g.editor_flag==1);
    CHECK(g.map[0][6].type==EMPTY && g.map[0][7].type==BRICK);
    CHECK(g.map[1][6].type==EMPTY && g.map[1][7].type==BRICK);
    game_step_inputs(&g,(Input[2]){{.dir=UP},{.dir=UP}});
    game_step_inputs(&g,(Input[2]){a,{.dir=UP}});
    CHECK(g.editor_block==1);
    CHECK(g.map[0][6].type==EMPTY && g.map[0][7].type==EMPTY);
    CHECK(g.map[1][6].type==BRICK && g.map[1][7].type==BRICK);

    // Moving resets the press counter: the next B paints the current entry,
    // and only the following B steps backwards.
    game_step_inputs(&g,(Input[2]){right,{.dir=UP}});
    CHECK(g.editor_x==4 && g.editor_flag==0);
    game_step_inputs(&g,(Input[2]){{.dir=UP},{.dir=UP}});
    Input b={.dir=UP,.fire2=true};
    game_step_inputs(&g,(Input[2]){b,{.dir=UP}});
    CHECK(g.editor_block==1 && g.editor_flag==1);
    CHECK(g.map[0][8].type==EMPTY && g.map[1][8].type==BRICK);
    game_step_inputs(&g,(Input[2]){{.dir=UP},{.dir=UP}});
    game_step_inputs(&g,(Input[2]){b,{.dir=UP}});
    CHECK(g.editor_block==0);
    CHECK(g.map[0][8].type==EMPTY && g.map[0][9].type==BRICK);

    // Block $0D (entry 13) clears all four cells under the cursor.
    for (int i=0;i<13;++i) {
        game_step_inputs(&g,(Input[2]){{.dir=UP},{.dir=UP}});
        game_step_inputs(&g,(Input[2]){a,{.dir=UP}});
    }
    CHECK(g.editor_block==13);
    CHECK(g.map[0][8].type==EMPTY && g.map[0][9].type==EMPTY &&
          g.map[1][8].type==EMPTY && g.map[1][9].type==EMPTY);

    // Holding A while moving paints a trail of the selected block.
    game_step_inputs(&g,(Input[2]){{.dir=UP},{.dir=UP}});
    Input paint_right={.dir=RIGHT,.moving=true,.fire=true};
    game_step_inputs(&g,(Input[2]){paint_right,{.dir=UP}});
    CHECK(g.editor_x==5);
    CHECK(g.map[0][10].type==EMPTY && g.map[0][11].type==EMPTY);
    game_step_inputs(&g,(Input[2]){{.dir=UP},{.dir=UP}});

    // Start stores the map, returns to the title with the CONSTRUCTION cursor,
    // and the next stage uses the edited map instead of the ROM stage data.
    g.map[24][8]=(Cell){BRICK,15,0};
    g.map[25][8]=(Cell){BRICK,15,0};
    game_exit_construction(&g);
    CHECK(g.phase==TITLE && g.title_cursor==2 && g.custom_map_ready);
    CHECK(g.custom_map[1][7].type==BRICK);
    CHECK(g.custom_map[24][8].type==BRICK);
    g.use_custom_map=true;
    g.lives[0]=3;
    game_restart_stage(&g);
    CHECK(g.phase==PLAYING);
    CHECK(g.use_custom_map);
    CHECK(g.map[1][7].type==BRICK);
    CHECK(g.map[0][0].type==EMPTY);
    CHECK(g.map[24][8].type==EMPTY && g.map[25][8].type==EMPTY);
    CHECK(g.map[24][12].type==BASE && g.map[25][13].type==BASE);
    game_begin_play(&g);
    CHECK(g.phase==PLAYING && !g.custom_map_ready && !g.use_custom_map);
    game_restart_stage(&g);
    CHECK(!g.use_custom_map && g.map[1][7].type==EMPTY);

    // Entering the editor again after a stage has started gives a fresh field.
    game_enter_construction(&g);
    CHECK(g.map[23][11].type==BRICK && g.map[1][7].type==EMPTY);
    // Exiting and re-entering before playing preserves the edited map.
    game_step_inputs(&g,(Input[2]){a,{.dir=UP}});
    game_exit_construction(&g);
    game_enter_construction(&g);
    CHECK(g.map[0][0].type==EMPTY && g.map[0][1].type==BRICK);
}

static void lost_field_test(void) {
    Game g;game_start_mode(&g,1,1);
    g.lives[0]=0;
    g.tanks[0].alive=false;
    g.phase=LOST_FIELD;
    g.phase_timer=0;
    step(&g,LOST_FIELD_DELAY-1);
    CHECK(g.phase==LOST_FIELD);
    step(&g,1);
    CHECK(g.phase==LOST_FIELD); // GAME OVER sprites are now visible
    step(&g,LOST_FIELD_TEXT_FRAMES-1);
    CHECK(g.phase==LOST_FIELD);
    step(&g,1);
    CHECK(g.phase==LOST && g.phase_timer==0);

    // Rendering the overlay must not leave uninitialized pixels.
    Rom rom;char error[256];
    CHECK(rom_load(&rom,TEST_ROM_PATH,error,sizeof(error)));
    g.phase=LOST_FIELD;g.phase_timer=LOST_FIELD_DELAY;
    uint32_t pixels[SCREEN_W*SCREEN_H];
    render_game(pixels,&g,&rom,0);
    for(int i=0;i<SCREEN_W*SCREEN_H;++i) CHECK((pixels[i]>>24)==255);
}

static void game_over_flow_test(void) {
    Game g;arena(&g);
    g.phase=LOST;
    g.phase_timer=0;
    g.title_anim=-1;
    g.high_score=30000;
    step(&g,game_tally_end_frame(&g)-1);
    CHECK(g.phase==LOST);
    step(&g,1);
    CHECK(g.phase==GAME_OVER);
    CHECK(g.sound_events&(1u<<SOUND_GAMEOVER));
    step(&g,GAME_OVER_FRAMES-1);
    CHECK(g.phase==GAME_OVER);
    step(&g,1);
    CHECK(g.phase==TITLE && g.title_anim==0);
    step(&g,TITLE_DRAW_FRAMES);
    CHECK(g.phase==TITLE && g.title_anim==-1);
    CHECK(g.high_score==30000);

    uint32_t pixels[SCREEN_W*SCREEN_H];
    Rom rom;char error[256];
    CHECK(rom_load(&rom,TEST_ROM_PATH,error,sizeof(error)));
    render_game(pixels,&g,&rom,0);
    for(int i=0;i<SCREEN_W*SCREEN_H;++i) CHECK((pixels[i]>>24)==255);
}

static int different_in_region(const uint32_t *a,const uint32_t *b,
                               int x0,int y0,int x1,int y1) {
    int changed=0;
    for (int y=y0;y<y1;++y)
        for (int x=x0;x<x1;++x)
            if (a[y*SCREEN_W+x]!=b[y*SCREEN_W+x]) ++changed;
    return changed;
}

static void high_score_display_test(void) {
    Rom rom;char error[256];
    CHECK(rom_load(&rom,TEST_ROM_PATH,error,sizeof(error)));
    Game g;game_start(&g);
    uint32_t *a=malloc(sizeof(uint32_t)*SCREEN_W*SCREEN_H);
    uint32_t *b=malloc(sizeof(uint32_t)*SCREEN_W*SCREEN_H);
    CHECK(a && b);
    static const unsigned pairs[][2]={{123400,23400},{100000,0},{99900,100000}};
    for (int phase=0;phase<2;++phase) {
        g.phase=phase==0?TITLE:LOST;
        g.phase_timer=0;
        for (unsigned i=0;i<sizeof(pairs)/sizeof(pairs[0]);++i) {
            g.high_score=(int)pairs[i][0];
            render_game(a,&g,&rom,0);
            g.high_score=(int)pairs[i][1];
            render_game(b,&g,&rom,0);
            CHECK(memcmp(a,b,sizeof(uint32_t)*SCREEN_W*SCREEN_H)!=0);
        }
    }
    free(a);free(b);
}

static void overlay_forest_test(void) {
    Rom rom;char error[256];
    CHECK(rom_load(&rom,TEST_ROM_PATH,error,sizeof(error)));
    Game g;game_start(&g);
    memset(g.tanks,0,sizeof(g.tanks));
    memset(g.bullets,0,sizeof(g.bullets));
    memset(g.effects,0,sizeof(g.effects));
    g.powerup_active=false;
    for (int y=0;y<MAP_SIZE;++y) for (int x=0;x<MAP_SIZE;++x)
        g.map[y][x]=(Cell){FOREST,0,0x22};
    uint32_t *a=malloc(sizeof(uint32_t)*SCREEN_W*SCREEN_H);
    uint32_t *b=malloc(sizeof(uint32_t)*SCREEN_W*SCREEN_H);
    CHECK(a && b);

    // PAUSE is five sprites at x=100..139, y=121..136.  Comparing the paused
    // and playing frame with identical forest leaves exactly the glyph pixels.
    g.phase=PAUSED;render_game(a,&g,&rom,0);
    g.phase=PLAYING;render_game(b,&g,&rom,0);
    CHECK(different_in_region(a,b,100,121,140,137)==138);

    // Battlefield GAME OVER is four sprites at x=104..135, y=106..121.
    g.phase=LOST_FIELD;g.phase_timer=LOST_FIELD_DELAY;
    render_game(a,&g,&rom,0);
    g.phase_timer=0;render_game(b,&g,&rom,0);
    CHECK(different_in_region(a,b,104,106,136,122)==235);

    free(a);free(b);
}

static void assets_test(void) {
    Rom rom;char error[256];
    CHECK(rom_load(&rom,TEST_ROM_PATH,error,sizeof(error)));
    CHECK(!rom_load(&rom,"/nonexistent/battlecity.nes",error,sizeof(error)));
    CHECK(rom_load(&rom,TEST_ROM_PATH,error,sizeof(error)));
    Game g;game_start(&g);
    int bricks=0,steel=0,base=0;
    for(int y=0;y<26;++y) for(int x=0;x<26;++x) {
        bricks+=g.map[y][x].type==BRICK;steel+=g.map[y][x].type==STEEL;base+=g.map[y][x].type==BASE;
    }
    CHECK(bricks==220 && steel==12 && base==4);
    uint32_t pixels[SCREEN_W*SCREEN_H];
    for(int phase=TITLE;phase<=GAME_OVER;++phase) {
        g.phase=(Phase)phase;render_game(pixels,&g,&rom,0);
        for(int i=0;i<SCREEN_W*SCREEN_H;++i) CHECK((pixels[i]>>24)==255);
    }
    g.phase=PLAYING;g.powerup_active=true;g.powerup_x=32;g.powerup_y=32;
    g.powerup_type=POWER_LIFE;g.tick=8;render_game(pixels,&g,&rom,0);
    for(int i=0;i<SCREEN_W*SCREEN_H;++i) CHECK((pixels[i]>>24)==255);
    Game two;game_start_mode(&two,2,1);render_game(pixels,&two,&rom,0);
    for(int i=0;i<SCREEN_W*SCREEN_H;++i) CHECK((pixels[i]>>24)==255);
}

int main(void) {
    collision_test();wall_test();combat_test();progression_test();
    stage_test();movement_test();spawn_order_test();powerup_test();
    two_player_test();bonus_life_test();high_score_after_base_test();high_score_display_test();overlay_forest_test();tally_animation_test();construction_test();
    lost_field_test();game_over_flow_test();assets_test();
    puts("PASS: terrain, brick damage, steel, combat, base, shields, respawn, victory, bullet interception, determinism, pause, 35 stages, stage select, construction editor, two-player mode, spawn order, movement timing, powerups, 1P/2P tally animation and ticks, battlefield GAME OVER, game over screen, title scroll, bonus life, high score, double bullets, six-digit high scores, forest overlays, ROM and rendering");
    return 0;
}
