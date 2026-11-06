#include "render.h"
#include "stage1.h"
#include "result_screen.h"
#include "gameover_screen.h"

#define colors reference_rgb

static void pixel(uint32_t *p,int x,int y,uint32_t color) {
    if (x>=0 && y>=0 && x<SCREEN_W && y<SCREEN_H) p[y*SCREEN_W+x]=0xff000000u|color;
}
static void box(uint32_t *p,int x,int y,int w,int h,uint32_t color) {
    for (int j=0;j<h;++j) for (int i=0;i<w;++i) pixel(p,x+i,y+j,color);
}
static void tile(uint32_t *p,const Rom *rom,int bank,int index,int x,int y,
                 const unsigned char pal[4],bool transparent) {
    for (int j=0;j<8;++j) for (int i=0;i<8;++i) {
        unsigned c=rom_pixel(rom,bank,index,i,j);
        if (!transparent || c) pixel(p,x+i,y+j,colors[pal[c]&63]);
    }
}
static void background(uint32_t *p,const Rom *rom,const unsigned char tiles[30][32],
                       const unsigned char attributes[8][8],const unsigned char palettes[8][4]) {
    for (int y=0;y<30;++y) for (int x=0;x<32;++x) {
        unsigned a=attributes[y/4][x/4];
        unsigned shift=(unsigned)(((y%4)/2)*4+((x%4)/2)*2);
        tile(p,rom,1,tiles[y][x],x*8,y*8,palettes[(a>>shift)&3],false);
    }
}
// NES 8x16 sprite: the low tile bit selects the pattern table.
static void sprite(uint32_t *p,const Rom *rom,int index,int x,int y,unsigned attributes,
                   const unsigned char palettes[8][4]) {
    for (int j=0;j<16;++j) for (int i=0;i<8;++i) {
        int sx=(attributes&0x40)?7-i:i,sy=(attributes&0x80)?15-j:j;
        unsigned c=rom_pixel(rom,index&1,(index&0xfe)+sy/8,sx,sy%8);
        if (c) pixel(p,x+i,y+j,colors[palettes[4+(attributes&3)][c]]);
    }
}

static void tank(uint32_t *p,const Rom *rom,const Tank *t,int player,unsigned frame) {
    int x=(int)t->x+16,y=(int)t->y+17;
    if (t->spawn>0) {
        // The spawn star ping-pongs a1 -> a5 -> a9 -> a5.
        int phase=(t->spawn/4)%4;
        if (phase==3) phase=1;
        int at=0xa1+phase*4;
        sprite(p,rom,at,x,y,3,reference_palettes);
        sprite(p,rom,at+2,x+8,y,3,reference_palettes);
        return;
    }
    static const int direction_tiles[]={0,24,16,8};
    int base=player>=0?(t->rank&3)*0x20:0x80+(t->kind&3)*0x20;
    int at=base+direction_tiles[t->dir]+((t->animation/8)%2)*4;
    unsigned attr=player==1?1:(player==0?0:2);
    // Powerup carriers flash between the normal and alternate enemy palette
    // every 8 frames, matching the ROM's $A8 bit2 path.
    if (player<0 && t->bonus) attr=2+((frame/8)&1);
    sprite(p,rom,at,x,y,attr,reference_palettes);
    sprite(p,rom,at+2,x+8,y,attr,reference_palettes);
    if (t->shield>0) {
        int shield=0x29+((frame/8)%2)*4;
        sprite(p,rom,shield,x,y,2,reference_palettes);
        sprite(p,rom,shield+2,x+8,y,2,reference_palettes);
    }
}

// Water changes between palette sets 1 and 2 every 32 frames in the ROM.
static const unsigned char water_palette_b[4]={15,18,60,18};

static void terrain_cell(uint32_t *p,const Rom *rom,const Cell *c,int px,int py,
                         unsigned frame,bool base_alive) {
    switch (c->type) {
    case BRICK: {
        tile(p,rom,1,c->tile?c->tile:15,px,py,reference_palettes[0],false);
        for (int q=0;q<4;++q) if (!(c->bricks&(1<<q)))
            box(p,px+(q%2)*4,py+(q/2)*4,4,4,0);
        break;
    }
    case STEEL:
        tile(p,rom,1,c->tile?c->tile:16,px,py,reference_palettes[3],false);
        break;
    case WATER: {
        const unsigned char *pal=((frame/32)&1)?water_palette_b:reference_palettes[1];
        tile(p,rom,1,c->tile?c->tile:0x12,px,py,pal,false);
        break;
    }
    case ICE:
        tile(p,rom,1,c->tile?c->tile:0x21,px,py,reference_palettes[3],false);
        break;
    case BASE: {
        int index=0xc8+(px/8-14)*2+(py/8-26);
        if (!base_alive) index+=4;
        tile(p,rom,1,index,px,py,reference_palettes[0],false);
        break;
    }
    default: break;
    }
}

static void powerup(uint32_t *p,const Rom *rom,const Game *g) {
    if (!g->powerup_active && g->powerup_flash==0) return;
    int x=16+g->powerup_x-8,y=17+g->powerup_y-8;
    if (g->powerup_flash>0) {
        // After pickup the ROM leaves a 50-frame sparkle at the item's
        // position (tile $3B and its right half) before clearing it.
        sprite(p,rom,0x3b,x,y,2,reference_palettes);
        sprite(p,rom,0x3d,x+8,y,2,reference_palettes);
        return;
    }
    if (!(g->tick&8)) return;
    int at=0x81+(g->powerup_type&7)*4;
    sprite(p,rom,at,x,y,2,reference_palettes);
    sprite(p,rom,at+2,x+8,y,2,reference_palettes);
}

// Keep the captured HUD and replace only the stage-number glyph.
static void hud(uint32_t *p,const Rom *rom,const Game *g) {
    for (int i=0;i<20;++i)
        tile(p,rom,1,i<g->remaining?0x6a:0x11,232+(i%2)*8,24+(i/2)*8,
             reference_palettes[0],false);
    int reserves=g->lives[0]>0?g->lives[0]-1:0;
    if (reserves>9) reserves=9;
    tile(p,rom,1,0x6e + reserves,240,144,reference_palettes[0],false);
    if (g->players==2) {
        // The 2P HUD adds "IIP", a tank icon and the second player's lives.
        tile(p,rom,1,0x5a,232,160,reference_palettes[0],false);
        tile(p,rom,1,0x13,240,160,reference_palettes[0],false);
        tile(p,rom,1,0x14,232,168,reference_palettes[0],false);
        int reserves2=g->lives[1]>0?g->lives[1]-1:0;
        if (reserves2>9) reserves2=9;
        tile(p,rom,1,0x6e + reserves2,240,168,reference_palettes[0],false);
    }
    if (g->stage>1) {
        int tens=g->stage/10, ones=g->stage%10;
        if (tens) tile(p,rom,1,0x6e + tens,232,200,reference_palettes[0],false);
        tile(p,rom,1,0x6e + ones,240,200,reference_palettes[0],false);
    }
}

// The interstage screen uses BG palette set 4 and tile $11 for the curtain.
static const unsigned char interstage_palette[4]={0x0f,0x17,0x06,0x00};

static void stage_select_screen(uint32_t *p,const Rom *rom,const Game *g) {
    for (int y=0;y<30;++y) for (int x=0;x<32;++x)
        tile(p,rom,1,0x11,x*8,y*8,interstage_palette,false);
    // DrawStageInter writes tiles $23-$27 followed by the stage number.
    static const unsigned char label[5]={0x23,0x24,0x25,0x26,0x27};
    for (int i=0;i<5;++i)
        tile(p,rom,1,label[i],96+i*8,112,interstage_palette,false);
    if (g->stage>=10)
        tile(p,rom,1,0x6e + g->stage/10,144,112,interstage_palette,false);
    tile(p,rom,1,0x6e + g->stage%10,152,112,interstage_palette,false);
}

static const unsigned char *result_cell_palette(int col,int row) {
    unsigned a=result_attributes[row/4][col/4];
    unsigned shift=(unsigned)(((row%4)/2)*4+((col%4)/2)*2);
    return result_palettes[(a>>shift)&3];
}

static void result_put(uint32_t *p,const Rom *rom,int index,int col,int row) {
    tile(p,rom,1,index,col*8,row*8,result_cell_palette(col,row),false);
}

static void result_number(uint32_t *p,const Rom *rom,unsigned value,
                          int end_col,int width,int row,int min_digits) {
    for (int i=0;i<width;++i) result_put(p,rom,0,end_col-i,row);
    if (value==0 && min_digits==0) return;
    unsigned digits=1;
    for (unsigned v=value;v>=10;v/=10) ++digits;
    if ((int)digits<min_digits) digits=(unsigned)min_digits;
    if ((int)digits>width) digits=(unsigned)width;
    for (unsigned i=0;i<digits;++i) {
        result_put(p,rom,0x30+(int)(value%10),end_col-(int)i,row);
        value/=10;
    }
}

// The ROM's two-player score line is a 14-digit combined field: the P1 score
// at columns 5-10, then an eight-digit P2 field at 11-18.  When P1 is zero the
// combined field right-aligns the P2 value; the II-PLAYER column repeats it at
// 23-28.  This reproduces the original screen, including its duplicate P2 copy.
static void result_scores_2p(uint32_t *p,const Rom *rom,const Game *g) {
    unsigned p1=(unsigned)g->score[0],p2=(unsigned)g->score[1];
    if (p1!=0 || p2==0)
        result_number(p,rom,p1,10,6,9,1);
    else
        result_number(p,rom,0,10,6,9,0);
    if (p1!=0 && p2!=0) {
        result_put(p,rom,0x30,11,9);
        result_put(p,rom,0x30,12,9);
        result_number(p,rom,p2,18,6,9,6); // zero-padded second half
    } else {
        result_put(p,rom,0,11,9);
        result_put(p,rom,0,12,9);
        if (p2!=0) result_number(p,rom,p2,18,6,9,1);
        else result_number(p,rom,0,18,6,9,0);
    }
    result_number(p,rom,p2,28,6,9,1);
}

static void result_icons(uint32_t *p,const Rom *rom) {
    for (int type=0;type<4;++type) {
        int at=0x80+type*0x20;
        int y=93+type*24;
        sprite(p,rom,at,121,y,2,reference_palettes);
        sprite(p,rom,at+2,129,y,2,reference_palettes);
    }
}

static void render_result(uint32_t *p,const Rom *rom,const Game *g) {
    background(p,rom,result_tiles,result_attributes,result_palettes);
    result_number(p,rom,(unsigned)g->high_score,23,5,3,5);
    result_number(p,rom,(unsigned)g->stage,19,2,5,1);
    int tally_start=g->players==2?TALLY_START_2P:TALLY_START_1P;
    bool tally_started=g->phase_timer>=tally_start;
    if (!tally_started) {
        // TOTAL caption and separator belong to the ROM's second draw step.
        for (int i=6;i<=10;++i) result_put(p,rom,0,i,23);
        for (int i=12;i<=19;++i) result_put(p,rom,0,i,22);
    }
    if (g->players==1)
        result_number(p,rom,(unsigned)g->score[0],10,7,9,1);
    else
        result_scores_2p(p,rom,g);
    for (int player=0;player<g->players;++player) {
        if (player==1) {
            static const unsigned char label[8]={0x5f,0x6b,0x50,0x4c,0x41,0x59,0x45,0x52};
            for (int i=0;i<8;++i) result_put(p,rom,label[i],21+i,7);
            for (int type=0;type<4;++type) {
                int row=12+type*3;
                result_put(p,rom,0x5d,17,row);
                if (tally_started) {
                    result_put(p,rom,0x50,26,row);
                    result_put(p,rom,0x54,27,row);
                    result_put(p,rom,0x53,28,row);
                }
            }
        }
        int total=0;
        bool total_visible=g->phase_timer>=game_tally_total_frame(g);
        for (int type=0;type<4;++type) {
            int row=12+type*3;
            int count=g->kills[player][type];
            int shown=game_tally_entries(g,type,g->phase_timer);
            if (shown>count) shown=count;
            int points=shown*100*(type+1);
            total+=count;
            if (player==0) {
                result_number(p,rom,(unsigned)points,6,6,row,shown?1:0);
                result_number(p,rom,(unsigned)shown,13,3,row,shown?1:0);
            } else {
                result_number(p,rom,(unsigned)shown,19,2,row,shown?1:0);
                result_number(p,rom,(unsigned)points,24,4,row,shown?1:0);
            }
        }
        if (total_visible)
            result_number(p,rom,(unsigned)total,player?19:13,2,23,1);
        else
            result_number(p,rom,0,player?19:13,2,23,0);
    }
    if (tally_started) result_icons(p,rom);
}

static void render_game_over(uint32_t *p,const Rom *rom) {
    background(p,rom,gameover_tiles,gameover_attributes,gameover_palettes);
}

// After GAME OVER the ROM waits briefly, then scrolls the completed title
// nametable upward at one pixel per frame. title_anim < 0 means static.
static void title_number(uint32_t *p,const Rom *rom,unsigned value,
                         int end_col,int width,int row,int offset) {
    for (int i=0;i<width;++i)
        tile(p,rom,1,0,(end_col-i)*8,row*8+offset,title_palettes[0],false);
    unsigned digits=1;
    for (unsigned v=value;v>=10;v/=10) ++digits;
    if ((int)digits<width) digits=(unsigned)width;
    for (unsigned i=0;i<digits && (int)i<width;++i) {
        tile(p,rom,1,0x30+(int)(value%10),(end_col-(int)i)*8,row*8+offset,
             title_palettes[0],false);
        value/=10;
    }
}

static void render_title(uint32_t *p,const Rom *rom,const Game *g) {
    box(p,0,0,SCREEN_W,SCREEN_H,colors[0x0f]);
    int offset=0;
    if (g->title_anim>=0) {
        int scroll=g->title_anim-TITLE_SCROLL_DELAY;
        if (scroll<0) scroll=0;
        if (scroll>SCREEN_H) scroll=SCREEN_H;
        offset=SCREEN_H-scroll;
    }
    for (int row=0;row<30;++row) {
        int y=row*8+offset;
        if (y>=SCREEN_H) continue;
        for (int col=0;col<32;++col)
            tile(p,rom,1,title_tiles[row][col],col*8,y,
                 title_palettes[0],false);
    }
    // The top line is "I- 00 HI- nnnnn"; keep the captured HI digits in
    // sync with the score earned in this session.
    title_number(p,rom,(unsigned)g->high_score,19,5,3,offset);
    int row=g->title_cursor;
    if (row<0) row=0;
    if (row>2) row=2;
    int cursor=132+row*16+offset;
    if (g->title_anim<0 && cursor<SCREEN_H) {
        sprite(p,rom,0x1c,64,cursor,0,title_palettes);
        sprite(p,rom,0x1e,72,cursor,0,title_palettes);
    }
}

// Construction mode: Make_GrayFrame's plain gray surround, a black 26x26
// playfield, the brick-walled eagle (or the custom map), and the 16x16 tank
// cursor.  The ROM writes the frame with background tile $11 and palette 0.
static void render_construction(uint32_t *p,const Rom *rom,const Game *g) {
    // Make_GrayFrame keeps water on the construction palette ($3C/$10/$12)
    // and does not run Swap_Pal_Colors, so the tiles stay static.
    static const unsigned char construction_water[4]={15,60,16,18};
    for (int y=0;y<30;++y) for (int x=0;x<32;++x)
        tile(p,rom,1,0x11,x*8,y*8,reference_palettes[0],false);
    box(p,16,16,FIELD_SIZE,FIELD_SIZE,0);
    for (int y=0;y<MAP_SIZE;++y) for (int x=0;x<MAP_SIZE;++x) {
        const Cell *c=&g->map[y][x];
        if (c->type==FOREST)
            tile(p,rom,1,c->tile?c->tile:0x22,16+x*8,16+y*8,
                 reference_palettes[2],false);
        else if (c->type==WATER)
            tile(p,rom,1,c->tile?c->tile:0x12,16+x*8,16+y*8,
                 construction_water,false);
        else
            terrain_cell(p,rom,c,16+x*8,16+y*8,0,g->base_alive);
    }
    // The ROM only rebuilds the sprite buffer when Frame_Counter bit 4 is set,
    // so the editor tank blinks on for 16 frames and off for 16.
    if (g->frame_counter&0x10) {
        // OAM stores Y-1, so the 16x16 cursor starts one pixel below the cell.
        int cursor_x=16+g->editor_x*16, cursor_y=17+g->editor_y*16;
        sprite(p,rom,0x00,cursor_x,cursor_y,0,reference_palettes);
        sprite(p,rom,0x02,cursor_x+8,cursor_y,0,reference_palettes);
    }
}

void render_game(uint32_t p[SCREEN_W*SCREEN_H],const Game *g,const Rom *rom,unsigned frame) {
    box(p,0,0,SCREEN_W,SCREEN_H,colors[0]);
    if (g->phase==TITLE) {
        render_title(p,rom,g);
        return;
    }
    if (g->phase==STAGE_SELECT) {
        stage_select_screen(p,rom,g);
        return;
    }
    if (g->phase==CONSTRUCTION) {
        render_construction(p,rom,g);
        return;
    }
    if (g->phase==GAME_OVER) {
        render_game_over(p,rom);
        return;
    }
    if (g->phase==WON || g->phase==LOST) {
        render_result(p,rom,g);
        return;
    }
    background(p,rom,stage1_background,stage1_attributes,reference_palettes);
    box(p,16,16,FIELD_SIZE,FIELD_SIZE,0);
    for (int y=0;y<MAP_SIZE;++y) for (int x=0;x<MAP_SIZE;++x) {
        const Cell *c=&g->map[y][x];
        if (c->type!=FOREST) terrain_cell(p,rom,c,16+x*8,16+y*8,frame,g->base_alive);
    }
    for (int i=0;i<MAX_TANKS;++i) if (g->tanks[i].alive)
        tank(p,rom,&g->tanks[i],i<g->players?i:-1,frame);
    static const int bullet_tiles[]={0xb1,0xb7,0xb5,0xb3};
    for (int i=0;i<MAX_BULLETS;++i) if (g->bullets[i].alive) {
        const Bullet *b=&g->bullets[i];
        sprite(p,rom,bullet_tiles[b->dir],16+(int)b->x-4,17+(int)b->y-8,2,reference_palettes);
    }
    for (int i=0;i<MAX_EFFECTS;++i) {
        const Effect *e=&g->effects[i];if (!e->life) continue;
        int age=e->duration-e->life;
        if (!e->big) {
            int at=0xf1+(age<3?0:age<6?4:8);
            int x=16+(int)e->x-8,y=17+(int)e->y-8;
            sprite(p,rom,at,x,y,3,reference_palettes);
            sprite(p,rom,at+2,x+8,y,3,reference_palettes);
            continue;
        }
        int size=e->big && age>=8?32:16;
        int at=size==32?(age<16?0xd0:0xe0):0xa0;
        const unsigned char explosion_palette[4]={0x0f,0x16,0x27,0x30};
        for (int yy=0;yy<size/8;++yy) for(int xx=0;xx<size/8;++xx)
            tile(p,rom,1,at+xx*2+yy%2+(yy/2)*8,16+(int)e->x-size/2+xx*8,
                 16+(int)e->y-size/2+yy*8,explosion_palette,true);
    }
    powerup(p,rom,g);
    // The last-life / base-destroyed message is four 8x16 sprites spelling
    // GAME OVER over the frozen field; it appears 127 frames after the loss.
    if (g->phase==LOST_FIELD && g->phase_timer>=LOST_FIELD_DELAY) {
        sprite(p,rom,0x79,104,106,3,reference_palettes);
        sprite(p,rom,0x7b,112,106,3,reference_palettes);
        sprite(p,rom,0x7d,120,106,3,reference_palettes);
        sprite(p,rom,0x7f,128,106,3,reference_palettes);
    }
    // Original pause overlay: five 8x16 sprites spelling PAUSE at OAM Y=120.
    if (g->phase==PAUSED) {
        sprite(p,rom,0x17,100,121,3,reference_palettes);
        sprite(p,rom,0x19,108,121,3,reference_palettes);
        sprite(p,rom,0x1b,116,121,3,reference_palettes);
        sprite(p,rom,0x1d,124,121,3,reference_palettes);
        sprite(p,rom,0x1f,132,121,3,reference_palettes);
    }
    // Trees are background tiles drawn after the sprites, exactly like the
    // original PPU layering, so tanks disappear underneath them.
    for (int y=0;y<MAP_SIZE;++y) for (int x=0;x<MAP_SIZE;++x) {
        const Cell *c=&g->map[y][x];
        if (c->type==FOREST)
            tile(p,rom,1,c->tile?c->tile:0x22,16+x*8,16+y*8,reference_palettes[2],false);
    }
    hud(p,rom,g);
}
