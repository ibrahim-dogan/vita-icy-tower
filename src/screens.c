/* The screens around a finished game: highscore initials, start floor
 * unlock, the replay menu, saving a replay and the replay browser. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "app.h"
#include "config.h"
#include "platform.h"
#include "replay.h"

static void bullet(Bitmap *dst, int x, int y)
{
    Bitmap *b = res_bmp("MENU_BULLET");
    gfx_sprite(dst, b, x - b->w, y);
}

static void white(Bitmap *dst, const char *s, int x, int y, int align)
{
    text_draw(dst, R.font2, s, x, y, -1, align);
}

static void dark_grid(Bitmap *dst) { gfx_grid(dst, (uint8_t)pal_nearest(R.pal, 0, 0, 0)); }

/* ---- what happens after a game ---- */

static int g_hs_pending, g_unlock_pending;

void flow_after_game(void)
{
    const GameResult *r = &last_result;
    g_hs_pending = 0;
    for (int t = 0; t < HS_TABLES; t++)
        if (hs_rank(t, r->floor, r->combo, r->score) >= 0) g_hs_pending = 1;
    int last_floor = r->floor / 100;
    if (last_floor >= NUM_FLOOR_TYPES - 1) last_floor = NUM_FLOOR_TYPES - 2;
    g_unlock_pending = last_floor > cfg.best_floor;
    if (g_unlock_pending) {
        cfg.best_floor = last_floor;
        config_save();
    }
    if (g_hs_pending) {
        app_goto(SC_HISCORE, 16);
    } else if (g_unlock_pending) {
        app_goto(SC_UNLOCK, 16);
    } else {
        music_stop();
        app_goto(SC_GAMEOVER, 0);
    }
}

static void after_hiscore(void)
{
    if (g_unlock_pending) {
        app_goto(SC_UNLOCK, 16);
    } else {
        music_stop();
        app_goto(SC_GAMEOVER, 16);
    }
}

/* ---- replay menu after a game ---- */

static int g_go_item;

static void go_enter(void) { g_go_item = 0; }

static void go_update(Input *in)
{
    if (app_in_transition()) return;
    if (in->repeat & BTN_DOWN) {
        play_sfx(SFX_MENU_CHOOSE, 0, 0);
        g_go_item = (g_go_item + 1) % 4;
    } else if (in->repeat & BTN_UP) {
        play_sfx(SFX_MENU_CHOOSE, 0, 0);
        g_go_item = (g_go_item + 3) % 4;
    } else if ((in->pressed & BTN_CANCEL) && g_go_item != 3) {
        play_sfx(SFX_MENU_CHOOSE, 0, 0);
        g_go_item = 3;
    } else if (in->pressed & (BTN_CONFIRM | BTN_CANCEL)) {
        switch (g_go_item) {
        case 0:
            play_sfx(SFX_MENU_CHANGE, 0, 0);
            play_sfx(SFX_TRYAGAIN, 0, 0);
            game_start(GAME_PLAY);
            break;
        case 1:
            if (!last_replay_valid) break;
            play_sfx(SFX_MENU_CHANGE, 0, 0);
            game_start(GAME_WATCH_LAST);
            break;
        case 2:
            if (!last_replay_valid) break;
            play_sfx(SFX_MENU_CHANGE, 0, 0);
            app_goto(SC_SAVE_REPLAY, 0);
            break;
        default:
            play_sfx(SFX_MENU_CHANGE, 0, 0);
            play_sfx(SFX_TRYAGAIN, 0, 0);
            app_goto(SC_MENU, 16);
            break;
        }
    }
}

static void go_panel(Bitmap *dst)
{
    game_draw_frozen(dst);
    dark_grid(dst);
    gfx_stretch_sprite(dst, res_bmp("REPLAY_BG"), 120, 140, 380, 170);
}

static void go_draw(Bitmap *dst, float a)
{
    (void)a;
    go_panel(dst);
    static const char *const ITEMS[4] = {"Play Again", "Watch Replay", "Save Replay", "Main Menu"};
    for (int i = 0; i < 4; i++) text_draw(dst, R.font1, ITEMS[i], 180, 160 + 28 * i, -1, ALIGN_LEFT);
    bullet(dst, 180, 152 + 28 * g_go_item);
}

const Scene scene_gameover = {go_enter, NULL, go_update, go_draw};

/* ---- arcade style text entry ---- */

typedef struct {
    char buf[32];
    int len, max, cur;
    const char *charset;
} Field;

static void field_set(Field *f, const char *s, int max, const char *charset)
{
    memset(f, 0, sizeof(*f));
    f->max = max;
    f->charset = charset;
    for (int i = 0; s[i] && f->len < max; i++)
        if (strchr(charset, s[i])) f->buf[f->len++] = s[i];
    if (f->len == 0) f->buf[f->len++] = charset[0];
    f->cur = f->len - 1;
}

/* Up/down change the letter under the cursor, left/right move, square
 * deletes. Returns 1 when the field is accepted. */
static int field_update(Field *f, const Input *in)
{
    int n = (int)strlen(f->charset);
    if (in->repeat & (BTN_UP | BTN_DOWN)) {
        const char *p = strchr(f->charset, f->buf[f->cur]);
        int i = p ? (int)(p - f->charset) : 0;
        i = (i + ((in->repeat & BTN_UP) ? n - 1 : 1)) % n;
        f->buf[f->cur] = f->charset[i];
        play_sfx(SFX_MENU_CHOOSE, 0, 0);
    }
    if (in->repeat & BTN_RIGHT) {
        if (f->cur + 1 < f->len) {
            f->cur++;
        } else if (f->len < f->max) {
            f->buf[f->len] = f->buf[f->cur];
            f->cur = f->len++;
        }
    }
    if ((in->repeat & BTN_LEFT) && f->cur > 0) f->cur--;
    if ((in->pressed & BTN_SQUARE) && f->len > 1) {
        memmove(f->buf + f->cur, f->buf + f->cur + 1, (size_t)(f->len - f->cur));
        f->len--;
        if (f->cur >= f->len) f->cur = f->len - 1;
    }
    f->buf[f->len] = 0;
    return (in->pressed & (BTN_CONFIRM | BTN_START)) != 0;
}

static void field_draw(Bitmap *dst, const Field *f, const Font *font, int x, int y, int active, int blink)
{
    int cx = x;
    for (int i = 0; i < f->len; i++) {
        char s[2] = {f->buf[i], 0};
        int w = text_width(font, s);
        if (active && i == f->cur && blink) gfx_fill(dst, cx, y + font->height - 3, w > 0 ? w : 8, 3, R.white);
        if (font == R.font1) text_draw(dst, font, s, cx, y, -1, ALIGN_LEFT);
        else text_draw(dst, font, s, cx, y, -1, ALIGN_LEFT);
        cx += w;
    }
}

/* ---- highscore initials ---- */

static const char INITIALS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ .#!";
static Field g_initials;
static char g_last_initials[4] = "AAA";
static int g_blink;

static void hs_enter(void)
{
    field_set(&g_initials, g_last_initials, 3, INITIALS);
    g_initials.cur = 0;
}

static void hs_update(Input *in)
{
    g_blink++;
    if (app_in_transition()) return;
    if (field_update(&g_initials, in)) {
        while (g_initials.len < 3) g_initials.buf[g_initials.len++] = ' ';
        g_initials.buf[3] = 0;
        snprintf(g_last_initials, sizeof(g_last_initials), "%s", g_initials.buf);
        HsEntry e;
        snprintf(e.name, sizeof(e.name), "%s", g_initials.buf);
        e.floor = last_result.floor;
        e.combo = last_result.combo;
        e.score = last_result.score;
        for (int t = 0; t < HS_TABLES; t++) hs_insert(t, &e);
        config_save();
        play_sfx(SFX_CHEER, 0, 0);
        after_hiscore();
    }
}

static void hs_draw(Bitmap *dst, float a)
{
    (void)a;
    game_draw_frozen(dst);
    gfx_remap_rect(dst, 0, 0, SCREEN_W, SCREEN_H, R.map_dim);
    gfx_remap_rect(dst, 0, 0, SCREEN_W, SCREEN_H, R.map_dim);
    dark_grid(dst);
    Bitmap *title = res_bmp("HIGHSCORE");
    gfx_sprite(dst, title, 320 - title->w / 2, 40);
    white(dst, "Enter your initials", 320, 150, ALIGN_CENTER);
    int w = 0;
    for (int i = 0; i < g_initials.len; i++) {
        char s[2] = {g_initials.buf[i], 0};
        w += text_width(R.font1, s);
    }
    field_draw(dst, &g_initials, R.font1, 320 - w / 2, 190, 1, (g_blink / 12) % 2);
    gfx_sprite(dst, res_bmp("HISCTOP"), 191, 270);
    char b[16];
    white(dst, g_initials.buf, 192, 290, ALIGN_LEFT);
    snprintf(b, sizeof(b), "%d", last_result.floor);
    white(dst, b, 300, 290, ALIGN_RIGHT);
    snprintf(b, sizeof(b), "%d", last_result.combo);
    white(dst, b, 359, 290, ALIGN_RIGHT);
    snprintf(b, sizeof(b), "%d", last_result.score);
    white(dst, b, 451, 290, ALIGN_RIGHT);
    text_draw(dst, R.font3, "UP/DOWN: letter   LEFT/RIGHT: move   CROSS: done", 320, 440, R.white,
              ALIGN_CENTER);
}

const Scene scene_hiscore = {hs_enter, NULL, hs_update, hs_draw};

/* ---- start floor unlocked ---- */

static void unlock_entered(void) { play_sfx(SFX_AIGHT, 0, 0); }

static void unlock_update(Input *in)
{
    if (!app_in_transition() && (in->pressed & (BTN_CONFIRM | BTN_CANCEL | BTN_START))) {
        music_stop();
        app_goto(SC_GAMEOVER, 64);
    }
}

static void unlock_draw(Bitmap *dst, float a)
{
    (void)a;
    gfx_blit(res_bmp("TITLE_BG"), dst, 0, 0, 0, 0, 640, 480);
    gfx_remap_rect(dst, 0, 0, 640, 480, R.map_dim);
    Bitmap *face = res_bmp("HEROFACE000");
    gfx_sprite(dst, face, 320 - face->w / 2, 20);
    text_draw(dst, R.font1, "A new start floor", 320, 300, -1, ALIGN_CENTER);
    text_draw(dst, R.font1, "has been unlocked!", 320, 350, -1, ALIGN_CENTER);
    text_draw(dst, R.font2, "(Get it in the options menu)", 320, 440, -1, ALIGN_CENTER);
}

const Scene scene_unlock = {NULL, unlock_entered, unlock_update, unlock_draw};

/* ---- save replay ---- */

static const char NAME_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .-!#@\"'";
static const char FILE_CHARS[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
static Field g_name, g_file;
static int g_sr_field; /* 0 name, 1 filename, 2 confirm overwrite, 3 saved */
static int g_sr_yes;
static char g_sr_msg[64];

static void default_filename(char *out, size_t n)
{
    char name[32];
    snprintf(name, sizeof(name), "%s", g_name.buf);
    for (char *p = name; *p; p++)
        if (!strchr(FILE_CHARS, *p)) *p = '_';
    snprintf(out, n, "%s_%d_%d_%d", name, last_result.score, last_result.floor, last_result.combo);
}

static void sr_enter(void)
{
    field_set(&g_name, cfg.replay_name, 20, NAME_CHARS);
    char f[64];
    default_filename(f, sizeof(f));
    field_set(&g_file, f, 30, FILE_CHARS);
    g_sr_field = 0;
    g_sr_msg[0] = 0;
}

static const char *replay_file_path(void)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/replays/%s.itr", game_root(), g_file.buf);
    return p;
}

static void do_save(void)
{
    snprintf(last_replay.name, sizeof(last_replay.name), "%s", g_name.buf);
    platform_mkdir(game_path("replays"));
    if (replay_save(&last_replay, replay_file_path()) == 0) {
        snprintf(g_sr_msg, sizeof(g_sr_msg), "Replay saved.");
        snprintf(cfg.replay_name, sizeof(cfg.replay_name), "%s", g_name.buf);
        config_save();
    } else {
        snprintf(g_sr_msg, sizeof(g_sr_msg), "Failed to save replay.");
    }
    g_sr_field = 3;
}

static void sr_update(Input *in)
{
    g_blink++;
    if (g_sr_field == 3) {
        if (in->pressed) app_goto(SC_GAMEOVER, 0);
        return;
    }
    if (g_sr_field == 2) {
        if (in->repeat & (BTN_LEFT | BTN_RIGHT)) g_sr_yes = !g_sr_yes;
        if (in->pressed & BTN_CONFIRM) {
            if (g_sr_yes) do_save();
            else g_sr_field = 1;
        } else if (in->pressed & BTN_CANCEL) {
            g_sr_field = 1;
        }
        return;
    }
    if (in->pressed & BTN_CANCEL) {
        if (g_sr_field == 1) g_sr_field = 0;
        else app_goto(SC_GAMEOVER, 0);
        return;
    }
    Field *f = g_sr_field == 0 ? &g_name : &g_file;
    if (field_update(f, in)) {
        play_sfx(SFX_MENU_CHANGE, 0, 0);
        if (g_sr_field == 0) {
            char fn[64];
            default_filename(fn, sizeof(fn));
            field_set(&g_file, fn, 30, FILE_CHARS);
            g_sr_field = 1;
        } else {
            struct stat st;
            if (stat(replay_file_path(), &st) == 0) {
                g_sr_field = 2;
                g_sr_yes = 0;
            } else {
                do_save();
            }
        }
    }
}

static void sr_draw(Bitmap *dst, float a)
{
    (void)a;
    go_panel(dst);
    white(dst, "SAVE REPLAY", 310, 150, ALIGN_CENTER);
    white(dst, "Your name:", 140, 190, ALIGN_LEFT);
    field_draw(dst, &g_name, R.font2, 140, 214, g_sr_field == 0, (g_blink / 12) % 2);
    white(dst, "Filename:", 140, 244, ALIGN_LEFT);
    field_draw(dst, &g_file, R.font2, 140, 268, g_sr_field == 1, (g_blink / 12) % 2);
    if (g_sr_field == 2) {
        dark_grid(dst);
        white(dst, "The file exists.", 320, 190, ALIGN_CENTER);
        white(dst, "Do you want to overwrite it?", 320, 216, ALIGN_CENTER);
        Bitmap *y = res_bmp("BT_YES"), *n = res_bmp("BT_NO");
        gfx_sprite(dst, y, 270, 250);
        gfx_sprite(dst, n, 335, 250);
        gfx_fill(dst, g_sr_yes ? 270 : 335, 288, 35, 3, R.white);
    } else if (g_sr_field == 3) {
        dark_grid(dst);
        white(dst, g_sr_msg, 320, 220, ALIGN_CENTER);
    }
    text_draw(dst, R.font3, "UP/DOWN: letter  LEFT/RIGHT: move  SQUARE: delete  CROSS: ok  CIRCLE: back", 320, 440,
              R.white, ALIGN_CENTER);
}

const Scene scene_save_replay = {sr_enter, NULL, sr_update, sr_draw};

/* ---- replay browser ---- */

typedef struct {
    char name[128]; /* file or directory name */
    int is_dir;
    char who[32];
    int score, floor, combo;
} Entry;

#define MAX_ENTRIES 512
static Entry g_entries[MAX_ENTRIES];
static int g_nentries, g_sel, g_top, g_sort;
static char g_dir[256] = "replays";
static int g_confirm_delete;
static char g_msg[96];
static int g_msg_ticks;

static int cmp_entry(const void *pa, const void *pb)
{
    const Entry *a = pa, *b = pb;
    if (a->is_dir != b->is_dir) return b->is_dir - a->is_dir;
    if (a->is_dir || g_sort == 0) return strcmp(a->name, b->name);
    int ka = g_sort == 1 ? a->score : g_sort == 2 ? a->floor : a->combo;
    int kb = g_sort == 1 ? b->score : g_sort == 2 ? b->floor : b->combo;
    return kb - ka;
}

static void scan(void)
{
    g_nentries = 0;
    if (strcmp(g_dir, "replays") != 0) {
        snprintf(g_entries[0].name, sizeof(g_entries[0].name), "..");
        g_entries[0].is_dir = 1;
        g_nentries = 1;
    }
    DIR *d = opendir(game_path(g_dir));
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && g_nentries < MAX_ENTRIES) {
            if (e->d_name[0] == '.') continue;
            Entry *en = &g_entries[g_nentries];
            memset(en, 0, sizeof(*en));
            snprintf(en->name, sizeof(en->name), "%s", e->d_name);
            char rel[512];
            snprintf(rel, sizeof(rel), "%s/%s", g_dir, e->d_name);
            struct stat st;
            if (stat(game_path(rel), &st) == 0 && S_ISDIR(st.st_mode)) {
                en->is_dir = 1;
            } else {
                size_t n = strlen(e->d_name);
                if (n < 4 || strcmp(e->d_name + n - 4, ".itr") != 0) continue;
                Replay r;
                if (replay_load(&r, game_path(rel)) != 0) continue;
                snprintf(en->who, sizeof(en->who), "%s", r.name);
                en->score = r.score;
                en->floor = r.floor;
                en->combo = r.combo;
                replay_free(&r);
            }
            g_nentries++;
        }
        closedir(d);
    }
    qsort(g_entries, (size_t)g_nentries, sizeof(Entry), cmp_entry);
    if (g_sel >= g_nentries) g_sel = g_nentries - 1;
    if (g_sel < 0) g_sel = 0;
}

static void rp_enter(void)
{
    g_confirm_delete = 0;
    g_msg_ticks = 0;
    scan();
}

static void rp_entered(void) { music_play(R.sfx[SFX_BG_MENU]); }

static void rp_update(Input *in)
{
    if (g_msg_ticks) g_msg_ticks--;
    if (app_in_transition()) return;
    Entry *e = g_nentries ? &g_entries[g_sel] : NULL;
    char rel[512];
    if (g_confirm_delete) {
        if (in->pressed & BTN_CONFIRM && e) {
            snprintf(rel, sizeof(rel), "%s/%s", g_dir, e->name);
            remove(game_path(rel));
            scan();
        }
        if (in->pressed & (BTN_CONFIRM | BTN_CANCEL)) g_confirm_delete = 0;
        return;
    }
    if (in->repeat & BTN_DOWN && g_sel + 1 < g_nentries) {
        g_sel++;
        play_sfx(SFX_MENU_CHOOSE, 0, 0);
    }
    if (in->repeat & BTN_UP && g_sel > 0) {
        g_sel--;
        play_sfx(SFX_MENU_CHOOSE, 0, 0);
    }
    if (in->repeat & BTN_RIGHT) g_sel = g_sel + 12 < g_nentries ? g_sel + 12 : g_nentries - 1;
    if (in->repeat & BTN_LEFT) g_sel = g_sel > 12 ? g_sel - 12 : 0;
    if (in->pressed & BTN_SQUARE) {
        g_sort = (g_sort + 1) % 4;
        scan();
    }
    if ((in->pressed & BTN_TRIANGLE) && e && !e->is_dir) g_confirm_delete = 1;
    if (in->pressed & BTN_CANCEL) {
        play_sfx(SFX_MENU_CHANGE, 0, 0);
        app_goto(SC_MENU, 16);
        return;
    }
    if ((in->pressed & BTN_CONFIRM) && e) {
        play_sfx(SFX_MENU_CHANGE, 0, 0);
        if (e->is_dir) {
            if (!strcmp(e->name, "..")) {
                char *slash = strrchr(g_dir, '/');
                if (slash) *slash = 0;
            } else {
                size_t n = strlen(g_dir);
                snprintf(g_dir + n, sizeof(g_dir) - n, "/%s", e->name);
            }
            g_sel = g_top = 0;
            scan();
        } else {
            snprintf(rel, sizeof(rel), "%s/%s", g_dir, e->name);
            replay_free(&file_replay);
            if (replay_load(&file_replay, game_path(rel)) == 0) {
                game_start(GAME_WATCH_FILE);
            } else {
                snprintf(g_msg, sizeof(g_msg), "Not a valid Icy Tower replay.");
                g_msg_ticks = 100;
            }
        }
    }
    if (g_sel < g_top) g_top = g_sel;
    if (g_sel >= g_top + 12) g_top = g_sel - 11;
}

static void rp_draw(Bitmap *dst, float a)
{
    (void)a;
    gfx_blit(res_bmp("TITLE_BG"), dst, 0, 0, 0, 0, 640, 480);
    Bitmap *bg = res_bmp("REPLAY_BG");
    int px = 320 - bg->w / 2, py = 18;
    gfx_sprite(dst, bg, px, py);
    white(dst, "SELECT REPLAY", 320, py + 16, ALIGN_CENTER);
    uint8_t wc = R.white, hl = (uint8_t)pal_nearest(R.pal, 255, 220, 0);
    char b[160];
    snprintf(b, sizeof(b), "%s/", g_dir);
    text_draw(dst, R.font3, b, px + 30, py + 50, wc, ALIGN_LEFT);
    static const char *const SORT[4] = {"name", "score", "floor", "combo"};
    snprintf(b, sizeof(b), "sort by %s", SORT[g_sort]);
    text_draw(dst, R.font3, b, px + bg->w - 30, py + 50, wc, ALIGN_RIGHT);
    text_draw(dst, R.font3, "DUDE", px + 30, py + 72, wc, ALIGN_LEFT);
    text_draw(dst, R.font3, "SCORE", px + 290, py + 72, wc, ALIGN_RIGHT);
    text_draw(dst, R.font3, "FLOOR", px + 350, py + 72, wc, ALIGN_RIGHT);
    text_draw(dst, R.font3, "COMBO", px + 420, py + 72, wc, ALIGN_RIGHT);
    if (!g_nentries) text_draw(dst, R.font3, "(no replays)", 320, py + 110, wc, ALIGN_CENTER);
    for (int i = 0; i < 12 && g_top + i < g_nentries; i++) {
        const Entry *e = &g_entries[g_top + i];
        int y = py + 96 + i * 22;
        uint8_t col = g_top + i == g_sel ? hl : wc;
        if (g_top + i == g_sel) gfx_remap_rect(dst, px + 22, y - 3, bg->w - 44, 20, R.map_dim);
        if (e->is_dir) {
            snprintf(b, sizeof(b), "[%s]", e->name);
            text_draw(dst, R.font3, b, px + 30, y, col, ALIGN_LEFT);
            continue;
        }
        text_draw(dst, R.font3, e->who[0] ? e->who : e->name, px + 30, y, col, ALIGN_LEFT);
        snprintf(b, sizeof(b), "%d", e->score);
        text_draw(dst, R.font3, b, px + 290, y, col, ALIGN_RIGHT);
        snprintf(b, sizeof(b), "%d", e->floor);
        text_draw(dst, R.font3, b, px + 350, y, col, ALIGN_RIGHT);
        snprintf(b, sizeof(b), "%d", e->combo);
        text_draw(dst, R.font3, b, px + 420, y, col, ALIGN_RIGHT);
    }
    text_draw(dst, R.font3, "CROSS: watch   SQUARE: sort   TRIANGLE: delete   CIRCLE: back", 320, py + 400, wc,
              ALIGN_CENTER);
    if (g_confirm_delete && g_nentries) {
        dark_grid(dst);
        snprintf(b, sizeof(b), "Do you really want to delete '%s'?", g_entries[g_sel].name);
        text_draw(dst, R.font3, b, 320, 200, wc, ALIGN_CENTER);
        text_draw(dst, R.font3, "WARNING: It will be gone forever.", 320, 222, wc, ALIGN_CENTER);
        text_draw(dst, R.font3, "CROSS: delete   CIRCLE: keep", 320, 260, wc, ALIGN_CENTER);
    }
    if (g_msg_ticks) {
        gfx_fill(dst, 150, 220, 340, 30, (uint8_t)pal_nearest(R.pal, 0, 0, 0));
        text_draw(dst, R.font3, g_msg, 320, 228, wc, ALIGN_CENTER);
    }
}

const Scene scene_replays = {rp_enter, rp_entered, rp_update, rp_draw};
