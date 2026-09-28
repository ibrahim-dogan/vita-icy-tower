/* Icy Tower 1.3 replay files (.itr, "ITR130"), compatible with the PC game. */
#pragma once

#include <stdint.h>

typedef struct {
    int frames;         /* extra repeats after the first frame */
    unsigned char keys; /* CORE_KEY_* bits, 0x80 marks the end */
} ReplayMacro;

typedef struct Replay {
    char name[32];
    char date[32];
    int score, floor, combo;
    int rejump;
    unsigned int seed;
    ReplayMacro *macros;
    int count, cap;
} Replay;

#define REPLAY_END 0x80

void replay_init(Replay *r, int rejump, unsigned int seed);
void replay_free(Replay *r);
/* Appends one frame of input (run length encoded). */
void replay_record(Replay *r, int keys);
/* Marks the end of the game (the frame Harold fell off). */
void replay_finish(Replay *r, int score, int floor, int combo);

int replay_load(Replay *r, const char *path);
int replay_save(const Replay *r, const char *path);

/* Plays the replay through the engine. Returns 1 if Harold dies exactly at
 * the end marker, like the official checker. */
int replay_validate(const Replay *r, int *score, int *floor, int *combo);

/* Frame-by-frame playback helper. */
typedef struct {
    const Replay *r;
    int index, repeat;
    int last; /* macro that produced the last returned frame */
} ReplayCursor;

void replay_cursor_init(ReplayCursor *c, const Replay *r);
/* Returns the keys for the next frame, or -1 when the replay is over. */
int replay_cursor_next(ReplayCursor *c);
