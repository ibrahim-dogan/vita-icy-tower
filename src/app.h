/* Scenes and the glue between them. */
#pragma once

#include "audio.h"
#include "gfx.h"
#include "input.h"
#include "res.h"

#define GAME_HZ 50
#define VERSION_STRING "v1.3.1"
/* Version of this port (release tags are v<PORT_VERSION>). */
#define PORT_VERSION "1.0.1"

typedef enum {
    SC_ERROR,
    SC_MENU,
    SC_GAME,
    SC_GAMEOVER,
    SC_HISCORE,
    SC_UNLOCK,
    SC_INSTRUCTIONS,
    SC_CREDITS,
    SC_SAVE_REPLAY,
    SC_REPLAYS,
    SC_COUNT
} SceneId;

typedef struct {
    void (*enter)(void);   /* screen is black, scene becomes current */
    void (*entered)(void); /* fade-in finished */
    void (*update)(Input *in);
    void (*draw)(Bitmap *dst, float alpha);
} Scene;

extern const Scene scene_error, scene_menu, scene_game, scene_gameover, scene_hiscore, scene_unlock,
    scene_instructions, scene_credits, scene_save_replay, scene_replays;

typedef struct {
    SceneId scene;
    int overlay; /* 0 = clear, 255 = black */
    int quit;
    int offset_y; /* screen shake */
    char error[256];
} App;

extern App app;

/* speed 0 switches immediately; otherwise fades out and in by `speed` per
 * tick like the original. */
void app_goto(SceneId s, int speed);
int app_in_transition(void);

/* Sound helpers following the original's play_sample calls: `stereo` pans
 * by Harold's position, `jitter` randomises the pitch a little. */
void play_sound(Sound *s, int stereo, int jitter);
void play_sfx(SfxId id, int stereo, int jitter);
void music_play(Sound *s);
void music_stop(void);
void music_apply_volume(void);

/* ---- shared between game and the screens that follow it ---- */
typedef enum { GAME_PLAY, GAME_WATCH_LAST, GAME_WATCH_FILE } GameMode;

void game_start(GameMode mode);
void game_draw_frozen(Bitmap *dst);
/* Results of the last game played (not watched). */
typedef struct {
    int score, floor, combo;
    int new_best_floor;
} GameResult;
extern GameResult last_result;
/* The replay of the last game and the one being watched. */
struct Replay;
extern struct Replay last_replay, file_replay;
extern int last_replay_valid;
/* Called by the game when the player leaves after dying. */
void flow_after_game(void);

/* Floor graphics of each floor type: part 0 left, 1 middle, 2 right, 3 sign. */
const char *floor_gfx(int type, int part);
