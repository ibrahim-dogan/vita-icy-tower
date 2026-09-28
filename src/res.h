/* Game resources, all read from the user's copy of Icy Tower 1.3.1. */
#pragma once

#include "audio.h"
#include "font.h"
#include "gfx.h"

enum {
    CF_IDLE1,
    CF_WALK1,
    CF_WALK2,
    CF_WALK3,
    CF_WALK4,
    CF_JUMP1,
    CF_JUMP2,
    CF_JUMP3,
    CF_JUMP,
    CF_IDLE2,
    CF_IDLE3,
    CF_CHOCK,
    CF_ROTATE,
    CF_EDGE2,
    CF_EDGE1,
    CF_COUNT
};

/* Character sounds, in the order they are stored in character datafiles. */
enum { CS_JUMPLO, CS_JUMPMED, CS_JUMPHI, CS_GREETING, CS_PAUSE, CS_DEATH, CS_EDGE, CS_BGMUSIC, CS_COUNT };

typedef enum {
    SFX_AIGHT,
    SFX_AMAZING,
    SFX_BG_BEAT,
    SFX_BG_MENU,
    SFX_CHEER,
    SFX_EXTREME,
    SFX_FANTASTIC,
    SFX_GAMEOVER,
    SFX_GOOD,
    SFX_GREAT,
    SFX_HURRYUP,
    SFX_MENU_CHANGE,
    SFX_MENU_CHOOSE,
    SFX_RING,
    SFX_SPLAT,
    SFX_SPLENDID,
    SFX_STEP,
    SFX_SUPER,
    SFX_SWEET,
    SFX_TRYAGAIN,
    SFX_UNBELIEVABLE,
    SFX_WOW,
    SFX_COUNT
} SfxId;

typedef struct {
    char dir[64];
    Bitmap *frame[CF_COUNT];
    Palette pal;
    int has_pal;
    Sound *snd[CS_COUNT];
} Character;

#define MAX_CHARACTERS 64

typedef struct {
    Font *font1, *font2, *font3, *font8;
    Sound *sfx[SFX_COUNT];
    Palette base_pal; /* AAAPAL from data.dat */
    Palette pal;      /* current palette (from the selected character) */
    Character chars[MAX_CHARACTERS];
    int nchars;
    int cur_char;
    Bitmap *background; /* 640x64 generated tower wall */
    uint8_t map_white[256]; /* colour font -> white text with dark outline */
    uint8_t map_dim[256];   /* darkened screen (unlock screen, pause) */
    uint8_t white;          /* palette index closest to white */
    uint8_t map_band[3][256]; /* title screen bottom band, rows 448, 449, 450+ */
} Res;

extern Res R;

/* The 8x8 font and a basic palette (indices 1 white, 2 red, 3 blue,
 * 4 grey) for screens shown before the game data is there. */
void res_init_basic(void);
/* Loads everything. Returns 0 or fills err with a message. */
int res_load(char *err, int errlen);
Bitmap *res_bmp(const char *name);
/* Selects the character whose palette the game uses. */
void res_select_character(int i);
int res_find_character(const char *dir);
