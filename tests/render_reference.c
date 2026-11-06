#include "game.h"
#include "render.h"
#include "rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Deterministic Construction editor input timeline shared with
// tools/capture_construction.lua.  Each interval is five frames, matching the
// press/release windows in that capture script.
static void construction_edit_scene(Game *g) {
    enum { BTN_RIGHT, BTN_DOWN, BTN_A, BTN_B };
    static const struct { int first, last, button; } schedule[] = {
        {610,614,BTN_RIGHT},{620,624,BTN_RIGHT},
        {630,634,BTN_DOWN},{640,644,BTN_DOWN},
        {650,654,BTN_A},{660,664,BTN_A},{670,674,BTN_A},
        {680,684,BTN_A},{690,694,BTN_A},{700,704,BTN_A},
        {710,714,BTN_RIGHT},{720,724,BTN_RIGHT},{720,724,BTN_A},
        {730,734,BTN_DOWN},{730,734,BTN_A},
        {740,744,BTN_A},{750,754,BTN_A},{760,764,BTN_A},
        {770,774,BTN_A},{780,784,BTN_A},{790,794,BTN_A},
        {800,804,BTN_RIGHT},{810,814,BTN_RIGHT},{810,814,BTN_A},
        {820,824,BTN_DOWN},{830,834,BTN_DOWN},
        {840,844,BTN_A},{850,854,BTN_A},
        {860,864,BTN_RIGHT},{870,874,BTN_A},{880,884,BTN_A},
        {890,894,BTN_RIGHT},{900,904,BTN_B},{910,914,BTN_B},
        {920,924,BTN_B},{930,934,BTN_DOWN},
        {940,944,BTN_A},{950,954,BTN_A},{960,964,BTN_A},
    };
    game_enter_construction(g);
    // Match the absolute emulator frame counter of the capture timeline so the
    // 16-on/16-off editor cursor blink lands on the same phase.
    g->frame_counter=499;
    for (int frame=500; frame<=980; ++frame) {
        Input in={.dir=UP};
        for (unsigned i=0;i<sizeof(schedule)/sizeof(schedule[0]);++i) {
            if (frame<schedule[i].first || frame>schedule[i].last) continue;
            if (schedule[i].button==BTN_RIGHT) {in.dir=RIGHT;in.moving=true;}
            else if (schedule[i].button==BTN_DOWN) {in.dir=DOWN;in.moving=true;}
            else if (schedule[i].button==BTN_A) in.fire=true;
            else if (schedule[i].button==BTN_B) in.fire2=true;
        }
        const Input inputs[2]={in,{.dir=UP}};
        game_step_inputs(g,inputs);
    }
}

// Render controlled states, without advancing AI to an unrelated emulator frame.
int main(int argc,char **argv) {
    if (argc!=3) return 1;
    Rom rom;char error[256];
    if (!rom_load(&rom,TEST_ROM_PATH,error,sizeof(error))) {
        fprintf(stderr,"%s\n",error);return 1;
    }
    Game g;game_init(&g);
    unsigned frame=0;
    if (!strcmp(argv[1],"stage_select")) {
        g.phase=STAGE_SELECT;
        g.stage=1;
    } else if (!strcmp(argv[1],"result")) {
        g.phase=WON;
        g.stage=1;
    } else if (!strcmp(argv[1],"result2p")) {
        game_start_mode(&g,2,1);
        g.phase=WON;
        g.score[0]=12345;g.score[1]=6789;
        for(int t=0;t<4;++t) {g.kills[0][t]=t+1;g.kills[1][t]=t+5;}
    } else if (!strcmp(argv[1],"gameover")) {
        g.phase=GAME_OVER;
    } else if (!strcmp(argv[1],"fieldoverlay")) {
        game_start_mode(&g,1,1);
        g.phase=LOST_FIELD;
        g.phase_timer=LOST_FIELD_DELAY;
    } else if (!strcmp(argv[1],"pause")) {
        game_start_mode(&g,1,1);
        g.phase=PAUSED;
    } else if (!strncmp(argv[1],"tally2p",7)) {
        game_start_mode(&g,2,1);
        g.phase=LOST;
        g.phase_timer=atoi(argv[1]+7);
        g.high_score=20000;
        g.score[0]=123456;g.score[1]=678900;
        g.kills[0][0]=1;g.kills[0][1]=2;g.kills[0][2]=3;g.kills[0][3]=4;
        g.kills[1][0]=5;g.kills[1][1]=6;g.kills[1][2]=7;g.kills[1][3]=8;
    } else if (!strncmp(argv[1],"tally",5)) {
        game_start_mode(&g,1,1);
        g.phase=LOST;
        g.phase_timer=atoi(argv[1]+5);
        g.high_score=20000;
        g.score[0]=123456;
        g.kills[0][0]=1;g.kills[0][1]=2;
        g.kills[0][2]=3;g.kills[0][3]=4;
    } else if (!strcmp(argv[1],"construction")) {
        game_enter_construction(&g);
        g.frame_counter=600;
    } else if (!strcmp(argv[1],"construction_blink")) {
        game_enter_construction(&g);
        g.frame_counter=540;
    } else if (!strcmp(argv[1],"construction_edit")) {
        construction_edit_scene(&g);
    } else if (!strcmp(argv[1],"titlec")) {
        g.title_cursor=2;
    } else if (!strncmp(argv[1],"titlescroll",11)) {
        g.title_anim=atoi(argv[1]+11);
    } else if (strcmp(argv[1],"title")) {
        if (!strcmp(argv[1],"2p")) game_start_mode(&g,2,1);
        else game_start(&g);
        memset(g.tanks,0,sizeof(g.tanks));
        g.remaining=18; // Captured frame 1300: two enemies have spawned.
        Tank *t=&g.tanks[0];
        *t=(Tank){.alive=true,.hp=1,.dir=UP};
        if (!strcmp(argv[1],"2p")) {
            t->x=64;t->y=192;
            g.tanks[1]=(Tank){.alive=true,.hp=1,.dir=UP,.x=128,.y=192};
        }
        else if (!strcmp(argv[1],"stage1")) t->alive=false;
        else if (!strcmp(argv[1],"right")) {t->x=72;t->y=192;t->dir=RIGHT;}
        else if (!strcmp(argv[1],"up")) {t->x=72;t->y=170;}
        else if (!strcmp(argv[1],"left")) {t->x=64;t->y=168;t->dir=LEFT;}
        else if (!strcmp(argv[1],"down")) {t->x=64;t->y=190;t->dir=DOWN;}
        else if (!strcmp(argv[1],"spawn")) {t->x=64;t->y=192;t->spawn=8;}
        else if (!strcmp(argv[1],"shield")) {t->x=64;t->y=192;t->shield=100;frame=8;}
        else if (!strcmp(argv[1],"enemy")) {
            t->alive=false;g.tanks[1]=(Tank){.alive=true,.x=96,.y=23,.dir=DOWN,.animation=8};
        } else if (!strcmp(argv[1],"bullet")) {
            t->alive=false;g.bullets[0]=(Bullet){true,71,115,UP,0,2,0};
        } else if (!strcmp(argv[1],"powerup")) {
            t->alive=false;
            g.powerup_active=true;
            g.powerup_x=32;g.powerup_y=32;g.powerup_type=5;g.tick=8;
        } else return 1;
    }
    uint32_t pixels[SCREEN_W*SCREEN_H];
    render_game(pixels,&g,&rom,frame);
    FILE *f=fopen(argv[2],"wb");
    if (!f) return 1;
    fprintf(f,"P6\n%d %d\n255\n",SCREEN_W,SCREEN_H);
    for (int i=0;i<SCREEN_W*SCREEN_H;++i) {
        unsigned char rgb[3]={(unsigned char)(pixels[i]>>16),
                              (unsigned char)(pixels[i]>>8),(unsigned char)pixels[i]};
        if (fwrite(rgb,1,3,f)!=3) {fclose(f);return 1;}
    }
    return fclose(f)!=0;
}
