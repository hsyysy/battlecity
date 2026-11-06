#include "sound.h"
#include <gme/gme.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned char ids[3],count,channels,priority;
    int frames;
} Definition;
// Sound request flags $0300..$031b and stream pointers $ecfe come from this ROM.
// Lower numbered requests have priority on a shared NES audio channel.  The
// channel masks are 1=pulse 1, 2=pulse 2, 4=triangle, 8=noise, 16=DMC and were
// checked by muting each Game_Music_Emu voice for every request.
static const Definition definitions[SOUND_COUNT]={
    {{1,2,3},3,7,1,265}, {{15},1,1,15,16},
    {{12},1,4,12,4}, {{13},1,2,13,5},
    {{10},1,8,10,15}, {{7},1,8,7,27},
    {{7,11},2,10,7,28}, {{0},1,2,0,37},
    {{17},1,2,17,0}, {{18},1,2,18,0},
    {{9},1,2,9,33},
    {{21,22,23},3,3,21,0},   // stage-clear/record music (pulse 1 + 2)
    {{24,25,26},3,7,24,114}, // GAME OVER jingle (pulse 1 + 2 + triangle)
    {{19,20},2,10,19,6}      // tally count-up tick (pulse 2 + noise)
};
typedef struct { Music_Emu *emu;int remaining;bool active; } Voice;
struct Sound { Voice voices[SOUND_COUNT];int engine; };

static void word(unsigned char *p,unsigned value) {p[0]=(unsigned char)value;p[1]=(unsigned char)(value>>8);}

Sound *sound_create(const Rom *rom,char *error,size_t error_size) {
    // Address-specific entry points must never be used for an unrelated NROM.
    static const unsigned char signature[]={0xa9,0x0f,0x8d,0x15,0x40,0xa9,0xc0,0x8d,0x17,0x40};
    if (memcmp(rom->prg+0x2a51,signature,sizeof(signature))) {
        snprintf(error,error_size,"ROM sound driver at $ea51 is not supported");return NULL;
    }
    Sound *sound=calloc(1,sizeof(*sound));
    unsigned char *nsf=calloc(1,128+32768);
    if (!sound || !nsf) {free(sound);free(nsf);snprintf(error,error_size,"Sound allocation failed");return NULL;}
    sound->engine=-1;
    memcpy(nsf,"NESM\032",5);nsf[5]=1;nsf[6]=SOUND_COUNT;nsf[7]=1;
    word(nsf+8,0x8000);word(nsf+10,0x8000);word(nsf+12,0x8020);
    memcpy(nsf+14,"Battle City ROM sound driver",28);
    word(nsf+110,16639);word(nsf+120,19997);
    memcpy(nsf+128+16384,rom->prg,16384);
    // INIT(A=track): save track, call the original init, then set its request flags.
    // $8100 contains eight-byte records: (count, request ids...).
    static const unsigned char init[]={
        0x48,0x20,0x51,0xea,0x68,0x0a,0x0a,0x0a,0xa8,
        0xb9,0x00,0x81,0x85,0x00,0xc8,0xb9,0x00,0x81,
        0xaa,0xa9,0x01,0x9d,0x00,0x03,0xc8,0xc6,0x00,0xd0,0xf2,0x60
    };
    memcpy(nsf+128,init,sizeof(init));
    static const unsigned char play[]={0x20,0x7e,0xea,0x60};
    memcpy(nsf+128+32,play,sizeof(play));
    for (int i=0;i<SOUND_COUNT;++i) {
        nsf[128+256+i*8]=definitions[i].count;
        memcpy(nsf+128+257+i*8,definitions[i].ids,definitions[i].count);
    }
    for (int i=0;i<SOUND_COUNT;++i) {
        gme_err_t err=gme_open_data(nsf,128+32768,&sound->voices[i].emu,SOUND_RATE);
        if (err) {
            snprintf(error,error_size,"ROM audio: %s",err);free(nsf);sound_destroy(sound);return NULL;
        }
        gme_ignore_silence(sound->voices[i].emu,1);
        gme_set_stereo_depth(sound->voices[i].emu,0);
        gme_enable_accuracy(sound->voices[i].emu,1);
    }
    free(nsf);return sound;
}

void sound_destroy(Sound *sound) {
    if (!sound) return;
    for (int i=0;i<SOUND_COUNT;++i) if (sound->voices[i].emu) gme_delete(sound->voices[i].emu);
    free(sound);
}

void sound_trigger(Sound *sound,SoundEvent event) {
    if (!sound || event<0 || event>=SOUND_COUNT) return;
    Voice *v=&sound->voices[event];
    if (gme_start_track(v->emu,event)) {v->active=false;return;}
    v->active=true;
    v->remaining=definitions[event].frames*SOUND_FRAME_SAMPLES;
}

void sound_engine(Sound *sound,bool enabled,bool moving) {
    if (!sound) return;
    int next=enabled?(moving?SOUND_MOVE:SOUND_IDLE):-1;
    if (next==sound->engine) return;
    sound->voices[SOUND_MOVE].active=false;sound->voices[SOUND_IDLE].active=false;
    sound->engine=next;
    if (next>=0) sound_trigger(sound,(SoundEvent)next);
}

void sound_stop(Sound *sound) {
    if (!sound) return;
    for (int i=0;i<SOUND_COUNT;++i) sound->voices[i].active=false;
    sound->engine=-1;
}

// Long-running music/engine beds must not silence short effects when both use
// the same NES channel.  Effects temporarily take over the shared channel and
// the bed resumes when they finish; effect-versus-effect conflicts keep the
// original lower-slot priority.
static bool background_event(int event) {
    return event==SOUND_START || event==SOUND_MOVE || event==SOUND_IDLE ||
           event==SOUND_STAGE_CLEAR || event==SOUND_GAMEOVER;
}

bool sound_render(Sound *sound,int16_t *samples,int count) {
    if (count<0 || !samples) return false;
    memset(samples,0,(size_t)count*sizeof(*samples));
    if (!sound) return true;
    for (int start=0;start<count;start+=SOUND_FRAME_SAMPLES) {
        int n=count-start;if (n>SOUND_FRAME_SAMPLES) n=SOUND_FRAME_SAMPLES;
        int mix[SOUND_FRAME_SAMPLES]={0};
        for (int i=0;i<SOUND_COUNT;++i) {
            Voice *v=&sound->voices[i];if (!v->active) continue;
            unsigned allowed=definitions[i].channels;
            bool background_i=background_event(i);
            for (int j=0;j<SOUND_COUNT;++j) {
                if (!sound->voices[j].active || j==i) continue;
                bool background_j=background_event(j);
                if (background_i && !background_j)
                    allowed &= (unsigned)~definitions[j].channels;
                else if (!background_i && !background_j &&
                         definitions[j].priority<definitions[i].priority)
                    allowed &= (unsigned)~definitions[j].channels;
            }
            gme_mute_voices(v->emu,(int)(~allowed&31u));
            short stereo[SOUND_FRAME_SAMPLES*2];
            if (gme_play(v->emu,n*2,stereo)) return false;
            for (int k=0;k<n;++k) mix[k]+=(stereo[k*2]+stereo[k*2+1])/2;
            if (definitions[i].frames && (v->remaining-=n)<=0) v->active=false;
        }
        for (int k=0;k<n;++k) {
            int value=mix[k]*3/4;
            if (value>INT16_MAX) value=INT16_MAX;
            if (value<INT16_MIN) value=INT16_MIN;
            samples[start+k]=(int16_t)value;
        }
    }
    return true;
}

static void dword(unsigned char *p,uint32_t value) {word(p,value&65535);word(p+2,value>>16);}

FILE *sound_wav_open(const char *path) {
    FILE *f=fopen(path,"wb");if (!f) return NULL;
    unsigned char header[44]={0};
    memcpy(header,"RIFF",4);memcpy(header+8,"WAVEfmt ",8);dword(header+16,16);
    word(header+20,1);word(header+22,1);dword(header+24,SOUND_RATE);
    dword(header+28,SOUND_RATE*2);word(header+32,2);word(header+34,16);memcpy(header+36,"data",4);
    if (fwrite(header,1,44,f)!=44) {fclose(f);return NULL;}
    return f;
}

bool sound_wav_write(FILE *file,const int16_t *samples,int count) {
    if (!file || !samples || count<0) return false;
    for (int i=0;i<count;++i) {
        unsigned char bytes[2];word(bytes,(uint16_t)samples[i]);
        if (fwrite(bytes,1,2,file)!=2) return false;
    }
    return true;
}

bool sound_wav_close(FILE *file) {
    if (!file) return true;
    long size=ftell(file);
    bool ok=size>=44 && (unsigned long)size<=UINT32_MAX;
    unsigned char bytes[4];
    if (ok) {
        dword(bytes,(uint32_t)size-8);
        ok=fseek(file,4,SEEK_SET)==0 && fwrite(bytes,1,4,file)==4;
        dword(bytes,(uint32_t)size-44);
        ok=ok && fseek(file,40,SEEK_SET)==0 && fwrite(bytes,1,4,file)==4;
    }
    return fclose(file)==0 && ok;
}
