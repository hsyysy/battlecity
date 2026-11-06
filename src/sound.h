#ifndef BATTLECITY_SOUND_H
#define BATTLECITY_SOUND_H
#include "rom.h"
#include "sound_events.h"
#include <stdio.h>

enum { SOUND_RATE=44100, SOUND_FRAME_SAMPLES=735 };
typedef struct Sound Sound;
Sound *sound_create(const Rom *rom,char *error,size_t error_size);
void sound_destroy(Sound *sound);
void sound_trigger(Sound *sound,SoundEvent event);
void sound_engine(Sound *sound,bool enabled,bool moving);
void sound_stop(Sound *sound);
// Mono signed PCM; deterministic and usable without an SDL device.
bool sound_render(Sound *sound,int16_t *samples,int count);
FILE *sound_wav_open(const char *path);
bool sound_wav_write(FILE *file,const int16_t *samples,int count);
bool sound_wav_close(FILE *file);
#endif
