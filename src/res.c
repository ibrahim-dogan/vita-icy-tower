#include "res.h"

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "datafile.h"
#include "font8x8_basic.h"
#include "img.h"
#include "log.h"
#include "platform.h"
#include "rnd.h"

Res R;

#define GFX_PASSWORD "gostflor"

typedef struct {
    char name[32];
    Bitmap *bmp;
} NamedBitmap;

static NamedBitmap g_bmps[128];
static int g_nbmps;

/* sfx13.dat object names, and the file names the original looks for in
 * sfx/ to let players replace them. */
static const char *const SFX_OBJ[SFX_COUNT] = {
    "S_AIGHT", "S_AMAZING", "S_BG_BEAT", "S_BG_MENU", "S_CHEER", "S_EXTREME", "S_FANTASTIC", "S_GAMEOVER",
    "S_GOOD", "S_GREAT", "S_HURRYUP", "S_MENU_CHANGE", "S_MENU_CHOOSE", "S_RING", "S_SPLAT", "S_SPLENDID",
    "S_STEP", "S_SUPER", "S_SWEET", "S_TRYAGAIN", "S_UNBELIEVABLE", "S_WOW",
};
static const char *const SFX_WAV[SFX_COUNT] = {
    "aight", "amazing", "bg_beat", "bg_meny", "cheer", "extreme", "fantastic", "gameover", "good", "great", "hurryup",
    "menu_change", "menu_choose", "ring", "splat", "splendid", "step", "super", "sweet", "tryagain", "unbelievable",
    "wow",
};

Bitmap *res_bmp(const char *name)
{
    for (int i = 0; i < g_nbmps; i++)
        if (strcmp(g_bmps[i].name, name) == 0) return g_bmps[i].bmp;
    /* Missing art must not crash the game; hand out an empty bitmap. */
    static Bitmap *empty;
    if (!empty) empty = bmp_create(1, 1);
    return empty;
}

static Bitmap *bitmap_object(const DatObject *o)
{
    if (o->size < 6 || dat_be16(o->data) != 8) return NULL;
    int w = dat_be16(o->data + 2), h = dat_be16(o->data + 4);
    if (w <= 0 || h <= 0 || o->size < 6 + (size_t)w * h) return NULL;
    Bitmap *b = bmp_create(w, h);
    if (b) memcpy(b->px, o->data + 6, (size_t)w * h);
    return b;
}

static void palette_object(const DatObject *o, Palette pal)
{
    for (int i = 0; i < 256 && (size_t)(i * 4 + 2) < o->size; i++) {
        int r = o->data[i * 4] & 63, g = o->data[i * 4 + 1] & 63, b = o->data[i * 4 + 2] & 63;
        pal[i].r = (uint8_t)(r * 255 / 63);
        pal[i].g = (uint8_t)(g * 255 / 63);
        pal[i].b = (uint8_t)(b * 255 / 63);
    }
}

/* ---- the tower wall ------------------------------------------------------
 * The original builds its background at start-up from the small BGTILE
 * stone: rows of tiles that get narrower towards the sides, shaded with a
 * cosine so the wall looks round, a little noise, then a soft blur. This is
 * the algorithm recovered by icytower-ng, working on the 16 shades of the
 * tile instead of RGB. */
static void build_background(void)
{
    Bitmap *tile = res_bmp("BGTILE");
    Bitmap *tmp = bmp_create(640, 128);
    R.background = bmp_create(640, 64);
    if (!tmp || !R.background || tile->w < 2) {
        bmp_free(tmp);
        return;
    }
    /* The tile uses a ramp of 16 blue-grey shades; find each shade's index by
     * sorting the tile's colours by brightness. */
    int used[256] = {0}, shades[16], nshades = 0;
    for (int i = 0; i < tile->w * tile->h; i++) used[tile->px[i]] = 1;
    static const uint8_t ramp[16][3] = {
        {0x00, 0x00, 0x00}, {0x0c, 0x0c, 0x14}, {0x18, 0x18, 0x2c}, {0x24, 0x24, 0x41},
        {0x34, 0x34, 0x55}, {0x45, 0x45, 0x65}, {0x55, 0x55, 0x75}, {0x65, 0x65, 0x86},
        {0x75, 0x75, 0x9a}, {0x86, 0x86, 0xaa}, {0x9a, 0x9a, 0xba}, {0xaa, 0xaa, 0xcb},
        {0xba, 0xba, 0xdf}, {0xcb, 0xcb, 0xef}, {0xdb, 0xdb, 0xff}, {0xeb, 0xeb, 0xff},
    };
    for (int s = 0; s < 16; s++) shades[s] = pal_nearest(R.base_pal, ramp[s][0], ramp[s][1], ramp[s][2]);
    (void)used;
    (void)nshades;
    uint8_t level_of[256];
    for (int i = 0; i < 256; i++) {
        level_of[i] = 4;
        for (int s = 0; s < 16; s++)
            if (shades[s] == i) level_of[i] = (uint8_t)s;
    }

    for (int i = 0; i < 4; i++) {
        int x = (i % 2 == 0) ? 32 : 0, y = 32 * i;
        for (int j = 0; j < 15; j++) {
            int w = 64 - x / 4;
            gfx_stretch_blit(tile, tmp, x - (w / 2) + 320, y, w, 32);
            gfx_stretch_blit(tile, tmp, -x - (w / 2) + 320, y, w, 32);
            x += w;
        }
    }
    static uint8_t lv[128][640];
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 640; x++) {
            double a = (double)(abs(x - 320) / 6) * (2.0 * M_PI / 256.0);
            int wave = (int)lround(cos(a) * 8.0);
            int noise = rnd_custom() % 3 - 1;
            int p = level_of[tmp->px[y * 640 + x]] + noise - wave;
            lv[y][x] = (uint8_t)(p < 0 ? 0 : p > 15 ? 15 : p);
        }
    for (int y = 32; y < 96; y++)
        for (int x = 0; x < 640; x++) {
            double sum;
            if (x == 0 || x == 639) {
                sum = lv[y][x];
            } else {
                sum = 0;
                for (int k = -1; k <= 1; k++)
                    for (int l = -1; l <= 1; l++) sum += lv[y + k][x + l] * ((k == 0 && l == 0) ? 0.2 : 0.1);
            }
            int p = (int)lround(sum);
            R.background->px[(y - 32) * 640 + x] = (uint8_t)shades[p < 0 ? 0 : p > 15 ? 15 : p];
        }
    bmp_free(tmp);
}

/* ---- characters ---------------------------------------------------------- */

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

/* Cuts the frames out of a character sheet: boxes of non-255 pixels along
 * the first row that has any. */
static int slice_frames(Character *c, const Bitmap *sheet)
{
    int y0 = -1;
    for (int y = 0; y < sheet->h && y0 < 0; y++)
        for (int x = 0; x < sheet->w; x++)
            if (sheet->px[y * sheet->w + x] != 255) {
                y0 = y;
                break;
            }
    if (y0 < 0) return -1;
    int n = 0;
    for (int x = 0; x < sheet->w && n < CF_COUNT;) {
        if (sheet->px[y0 * sheet->w + x] == 255) {
            x++;
            continue;
        }
        int x1 = x, y1 = y0;
        while (x1 < sheet->w && sheet->px[y0 * sheet->w + x1] != 255) x1++;
        while (y1 < sheet->h && sheet->px[y1 * sheet->w + x] != 255) y1++;
        Bitmap *f = bmp_create(x1 - x, y1 - y0);
        if (!f) return -1;
        gfx_blit(sheet, f, x, y0, 0, 0, f->w, f->h);
        c->frame[n++] = f;
        x = x1;
    }
    return n == CF_COUNT ? 0 : -1;
}

static const char *const SND_KEYS[CS_COUNT] = {
    "[jumplo]", "[jumpmed]", "[jumphi]", "[greeting]", "[pause]", "[death]", "[edge]", "[bgmusic]",
};

static void free_character(Character *c)
{
    for (int i = 0; i < CF_COUNT; i++) bmp_free(c->frame[i]);
    for (int i = 0; i < CS_COUNT; i++) sound_free(c->snd[i]);
    memset(c, 0, sizeof(*c));
}

static int load_character(Character *c, const char *dir)
{
    char path[512], line[512];
    memset(c, 0, sizeof(*c));
    snprintf(c->dir, sizeof(c->dir), "%s", dir);
    snprintf(path, sizeof(path), "%s/characters/%s/%s.txt", game_root(), dir, dir);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char datafile[256] = "", frames[256] = "", sounds[CS_COUNT][256];
    memset(sounds, 0, sizeof(sounds));
    while (fgets(line, sizeof(line), f)) {
        char key[64], val[256];
        trim(line);
        if (sscanf(line, " %63s %255s", key, val) != 2) continue;
        if (!strcmp(key, "[datafile]")) snprintf(datafile, sizeof(datafile), "%s", val);
        else if (!strcmp(key, "[frames]")) snprintf(frames, sizeof(frames), "%s", val);
        for (int i = 0; i < CS_COUNT; i++)
            if (!strcmp(key, SND_KEYS[i])) snprintf(sounds[i], sizeof(sounds[i]), "%s", val);
    }
    fclose(f);

    if (datafile[0]) {
        Datafile df;
        snprintf(path, sizeof(path), "%s/characters/%s/%s", game_root(), dir, datafile);
        if (dat_load(&df, path, NULL) != 0) return -1;
        char name[16];
        for (int i = 0; i < CF_COUNT; i++) {
            snprintf(name, sizeof(name), "%03d_BMP", i + 1);
            const DatObject *o = dat_find(&df, name);
            if (o) c->frame[i] = bitmap_object(o);
        }
        const DatObject *p = dat_find(&df, "000_PAL");
        if (p) {
            palette_object(p, c->pal);
            c->has_pal = 1;
        }
        for (int i = 0; i < CS_COUNT; i++) {
            snprintf(name, sizeof(name), "%03d", 16 + i);
            const DatObject *o = dat_find(&df, name);
            if (o) c->snd[i] = sound_from_memory(o->data, o->size, i == CS_BGMUSIC);
        }
        dat_free(&df);
    } else if (frames[0]) {
        snprintf(path, sizeof(path), "%s/characters/%s/%s", game_root(), dir, frames);
        Bitmap *sheet = img_load_8bit(path, c->pal);
        if (!sheet) return -1;
        c->has_pal = 1;
        int rc = slice_frames(c, sheet);
        bmp_free(sheet);
        if (rc != 0) {
            free_character(c);
            return -1;
        }
        for (int i = 0; i < CS_COUNT; i++) {
            if (!sounds[i][0]) continue;
            snprintf(path, sizeof(path), "%s/characters/%s/%s", game_root(), dir, sounds[i]);
            c->snd[i] = sound_from_file(path, i == CS_BGMUSIC);
        }
    } else {
        return -1;
    }
    for (int i = 0; i < CF_COUNT; i++)
        if (!c->frame[i]) {
            free_character(c);
            return -1;
        }
    return 0;
}

static int cmp_str(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

static void load_characters(void)
{
    char names[MAX_CHARACTERS][64];
    int n = 0;
    DIR *d = opendir(game_path("characters"));
    if (!d) {
        log_printf("*** no characters folder: %s", game_path("characters"));
        return;
    }
    struct dirent *e;
    while ((e = readdir(d)) && n < MAX_CHARACTERS) {
        if (e->d_name[0] == '.') continue;
        char rel[128];
        struct stat st;
        snprintf(rel, sizeof(rel), "characters/%s", e->d_name);
        if (stat(game_path(rel), &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        snprintf(names[n++], 64, "%s", e->d_name);
    }
    closedir(d);
    qsort(names, (size_t)n, 64, cmp_str);
    for (int i = 0; i < n; i++) {
        if (load_character(&R.chars[R.nchars], names[i]) == 0) {
            int sounds = 0;
            for (int k = 0; k < CS_COUNT; k++) sounds += R.chars[R.nchars].snd[k] != NULL;
            log_printf("character %s: ok (%d sounds%s)", names[i], sounds,
                       R.chars[R.nchars].has_pal ? ", own palette" : "");
            R.nchars++;
        } else {
            log_printf("character %s: skipped (missing %s.txt, or its [datafile]/[frames] could not be read or "
                       "does not hold 15 frames)",
                       names[i], names[i]);
        }
    }
}

int res_find_character(const char *dir)
{
    for (int i = 0; i < R.nchars; i++)
        if (!strcmp(R.chars[i].dir, dir)) return i;
    return -1;
}

static void build_maps(void)
{
    int white = pal_nearest(R.pal, 255, 255, 255);
    R.white = (uint8_t)white;
    for (int i = 0; i < 256; i++) {
        int lum = (R.pal[i].r * 30 + R.pal[i].g * 59 + R.pal[i].b * 11) / 100;
        R.map_white[i] = (uint8_t)(lum < 72 ? i : white);
    }
    pal_make_darken(R.pal, 0.6, R.map_dim);
    pal_make_light(R.pal, 0, 2, 2, 220, R.map_band[0]);
    pal_make_light(R.pal, 0, 2, 2, 148, R.map_band[1]);
    pal_make_light(R.pal, 0, 2, 2, 92, R.map_band[2]);
}

void res_select_character(int i)
{
    if (i < 0 || i >= R.nchars) i = 0;
    R.cur_char = i;
    if (R.nchars && R.chars[i].has_pal) memcpy(R.pal, R.chars[i].pal, sizeof(Palette));
    else memcpy(R.pal, R.base_pal, sizeof(Palette));
    build_maps();
}

void res_init_basic(void)
{
    if (!R.font8) R.font8 = font_from_8x8((const unsigned char(*)[8])font8x8_basic);
    /* a tiny palette for the screens shown before the game data is loaded */
    memset(R.pal, 0, sizeof(R.pal));
    R.pal[1].r = R.pal[1].g = R.pal[1].b = 255;
    R.pal[2].r = 255;
    R.pal[2].g = R.pal[2].b = 90;
    R.pal[3].r = 70;
    R.pal[3].g = 140;
    R.pal[3].b = 255;
    R.pal[4].r = R.pal[4].g = R.pal[4].b = 60;
}

int res_load(char *err, int errlen)
{
    res_init_basic();

    Datafile gfx;
    if (dat_load(&gfx, game_path("data/data.dat"), GFX_PASSWORD) != 0) {
        snprintf(err, (size_t)errlen, "Could not read %s", game_path("data/data.dat"));
        return -1;
    }
    for (int i = 0; i < gfx.count; i++) {
        const DatObject *o = &gfx.objs[i];
        if (o->type == DAT_BMP && g_nbmps < (int)(sizeof(g_bmps) / sizeof(g_bmps[0]))) {
            Bitmap *b = bitmap_object(o);
            if (!b) continue;
            snprintf(g_bmps[g_nbmps].name, sizeof(g_bmps[0].name), "%s", o->name);
            g_bmps[g_nbmps++].bmp = b;
        } else if (o->type == DAT_PAL && !strcmp(o->name, "AAAPAL")) {
            palette_object(o, R.base_pal);
        } else if (o->type == DAT_FONT) {
            Font *f = font_from_dat(o->data, o->size);
            if (!strcmp(o->name, "FONT1")) R.font1 = f;
            else if (!strcmp(o->name, "FONT2")) R.font2 = f;
            else if (!strcmp(o->name, "FONT3")) R.font3 = f;
            else font_free(f);
        }
    }
    dat_free(&gfx);
    log_printf("data.dat: %d bitmaps", g_nbmps);
    if (!R.font1 || !R.font2 || !R.font3) {
        snprintf(err, (size_t)errlen, "data.dat is damaged (fonts missing)");
        return -1;
    }

    Datafile sfx;
    if (dat_load(&sfx, game_path("data/sfx13.dat"), GFX_PASSWORD) != 0) {
        snprintf(err, (size_t)errlen, "Could not read %s", game_path("data/sfx13.dat"));
        return -1;
    }
    for (int i = 0; i < SFX_COUNT; i++) {
        int music = i == SFX_BG_BEAT || i == SFX_BG_MENU;
        char rel[64];
        snprintf(rel, sizeof(rel), "sfx/%s.wav", SFX_WAV[i]);
        R.sfx[i] = sound_from_file(game_path(rel), music);
        if (R.sfx[i]) {
            log_printf("custom sound: %s", rel);
        } else {
            const DatObject *o = dat_find(&sfx, SFX_OBJ[i]);
            if (o) R.sfx[i] = sound_from_memory(o->data, o->size, music);
            if (!R.sfx[i]) log_printf("*** sound %s missing", SFX_OBJ[i]);
        }
    }
    dat_free(&sfx);

    memcpy(R.pal, R.base_pal, sizeof(Palette));
    build_background();
    load_characters();
    if (R.nchars == 0) {
        snprintf(err, (size_t)errlen, "No characters found in %s", game_path("characters"));
        return -1;
    }
    res_select_character(0);
    return 0;
}
