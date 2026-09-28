/* Settings, unlocked start floors and the three highscore tables. */
#pragma once

#define HS_ENTRIES 5
#define NUM_FLOOR_TYPES 11

enum { HS_SCORES, HS_FLOORS, HS_COMBOS, HS_TABLES };

enum { SCREEN_SHARP, SCREEN_SMOOTH, SCREEN_STRETCH, SCREEN_PIXEL, SCREEN_MODES };

typedef struct {
    char name[4];
    int floor, combo, score;
} HsEntry;

typedef struct {
    char character[64];
    int start_floor, best_floor;
    int eye_candy;  /* 0 none, 1 some, 2 lots */
    int sound_vol;  /* 0..10 */
    int music_vol;  /* 0..10 */
    int rejump;
    int screen;     /* SCREEN_* */
    char replay_name[32];
    HsEntry hs[HS_TABLES][HS_ENTRIES];
} Config;

extern Config cfg;

void config_defaults(void);
void config_load(void);
void config_save(void);

/* Position the entry would take in table t, or -1. */
int hs_rank(int t, int floor, int combo, int score);
void hs_insert(int t, const HsEntry *e);
