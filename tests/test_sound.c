#include "sound.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#c); exit(1); } } while (0)

static long long energy(const int16_t *pcm,int count) {
    long long result=0;
    for (int i=0;i<count;++i) result+=(long long)pcm[i]*pcm[i];
    return result;
}

int main(int argc,char **argv) {
    Rom rom;char error[256];CHECK(rom_load(&rom,TEST_ROM_PATH,error,sizeof(error)));
    Sound *a=sound_create(&rom,error,sizeof(error)),*b=sound_create(&rom,error,sizeof(error));
    CHECK(a && b);
    // Exercise the same SDL3 device-stream API as normal gameplay, without
    // requiring speakers or a desktop in CI.
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");CHECK(SDL_Init(SDL_INIT_AUDIO));
    const SDL_AudioSpec spec={.format=SDL_AUDIO_S16,.channels=1,.freq=SOUND_RATE};
    SDL_AudioStream *stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,NULL,NULL);
    CHECK(stream);CHECK(SDL_ResumeAudioStreamDevice(stream));
    int16_t left[SOUND_FRAME_SAMPLES],right[SOUND_FRAME_SAMPLES];
    CHECK(sound_render(a,left,SOUND_FRAME_SAMPLES));CHECK(energy(left,SOUND_FRAME_SAMPLES)==0);
    // Same original driver request must produce the same PCM, including the tail.
    sound_trigger(a,SOUND_FIRE);sound_trigger(b,SOUND_FIRE);
    long long fire_energy=0;
    for (int i=0;i<30;++i) {
        CHECK(sound_render(a,left,SOUND_FRAME_SAMPLES));CHECK(sound_render(b,right,SOUND_FRAME_SAMPLES));
        CHECK(memcmp(left,right,sizeof(left))==0);fire_energy+=energy(left,SOUND_FRAME_SAMPLES);
        if (i>=16) CHECK(energy(left,SOUND_FRAME_SAMPLES)==0);
    }
    CHECK(fire_energy>1000000);
    sound_destroy(b);
    // Render a reviewable sequence: opening, idle, driving, fire, brick, steel,
    // enemy, player, base, pause, powerup, tally, stage clear, game over.
    FILE *wav=argc>1?sound_wav_open(argv[1]):NULL;CHECK(argc<=1 || wav);
    const SoundEvent events[]={SOUND_START,SOUND_IDLE,SOUND_MOVE,SOUND_FIRE,SOUND_BRICK,
                               SOUND_STEEL,SOUND_ENEMY,SOUND_PLAYER,SOUND_BASE,SOUND_PAUSE,
                               SOUND_POWERUP,SOUND_TALLY,SOUND_STAGE_CLEAR,SOUND_GAMEOVER};
    const int frames[]={300,60,60,30,15,15,30,45,45,60,45,10,120,120};
    for (unsigned i=0;i<sizeof(events)/sizeof(events[0]);++i) {
        sound_stop(a);sound_trigger(a,events[i]);long long sum=0;
        for (int frame=0;frame<frames[i];++frame) {
            CHECK(sound_render(a,left,SOUND_FRAME_SAMPLES));sum+=energy(left,SOUND_FRAME_SAMPLES);
            if (wav) CHECK(sound_wav_write(wav,left,SOUND_FRAME_SAMPLES));
            CHECK(SDL_PutAudioStreamData(stream,left,sizeof(left)));
        }
        if (sum<=1000000) { fprintf(stderr,"event %d (%d frames) produced no audio\n",
                                    (int)events[i],frames[i]); exit(1); }
    }
    CHECK(sound_wav_close(wav));
    sound_engine(a,true,true);CHECK(sound_render(a,left,SOUND_FRAME_SAMPLES));
    sound_stop(a);CHECK(sound_render(a,left,SOUND_FRAME_SAMPLES));CHECK(energy(left,SOUND_FRAME_SAMPLES)==0);
    sound_destroy(a);

    // A short effect must remain audible while the battle-music bed is
    // playing.  The background voice loses the shared channel only while the
    // effect is active, so the mixed PCM gains the fire pulse.
    Sound *mixA=sound_create(&rom,error,sizeof(error));
    Sound *mixB=sound_create(&rom,error,sizeof(error));
    CHECK(mixA && mixB);
    sound_trigger(mixA,SOUND_START);sound_trigger(mixB,SOUND_START);
    for (int i=0;i<30;++i) {
        CHECK(sound_render(mixA,left,SOUND_FRAME_SAMPLES));
        CHECK(sound_render(mixB,right,SOUND_FRAME_SAMPLES));
    }
    sound_trigger(mixB,SOUND_FIRE);
    long long music_energy=0,mixed_energy=0;bool changed=false;
    for (int i=0;i<20;++i) {
        CHECK(sound_render(mixA,left,SOUND_FRAME_SAMPLES));
        CHECK(sound_render(mixB,right,SOUND_FRAME_SAMPLES));
        music_energy+=energy(left,SOUND_FRAME_SAMPLES);
        mixed_energy+=energy(right,SOUND_FRAME_SAMPLES);
        if (memcmp(left,right,sizeof(left))!=0) changed=true;
    }
    CHECK(changed && mixed_energy>music_energy);
    sound_destroy(mixA);sound_destroy(mixB);
    CHECK(SDL_ClearAudioStream(stream));CHECK(SDL_GetAudioStreamQueued(stream)==0);
    SDL_DestroyAudioStream(stream);SDL_Quit();
    // Unsupported ROMs cannot jump into address-specific sound code.
    rom.prg[0x2a51]^=1;CHECK(!sound_create(&rom,error,sizeof(error)));
    puts("PASS: original ROM audio requests, tally tick, stage-clear/game-over music, mixed music/effects, deterministic PCM, silence, stop, WAV and ROM guard");
    return 0;
}
