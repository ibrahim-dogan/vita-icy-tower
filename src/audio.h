/* Software mixer with the parts of Allegro's play_sample the game uses:
 * volume, pan, playback speed and looping. */
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct Sound Sound;

/* Decodes an OGG Vorbis or WAV file held in memory. Short sounds are
 * decoded up front; `stream` keeps OGG data compressed and decodes while
 * playing (used for music). */
Sound *sound_from_memory(const uint8_t *data, size_t size, int stream);
Sound *sound_from_file(const char *path, int stream);
void sound_free(Sound *s);

void audio_init(void);
void audio_shutdown(void);

/* vol 0..1, pan -1..1, speed 1 = normal. Returns a voice handle or -1. */
int audio_play(Sound *s, float vol, float pan, float speed, int loop);
void audio_stop(int voice);
int audio_playing(int voice);
void audio_stop_all(void);

/* Music is one looping voice at a separate volume. */
void audio_music_play(Sound *s);
void audio_music_stop(void);
void audio_music_volume(float vol);
