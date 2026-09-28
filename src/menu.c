/* Title screen with the option pages, instructions, credits and the
 * "game data missing" screen. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "config.h"
#include "rnd.h"

enum { PG_MAIN, PG_OPTIONS, PG_GAME, PG_GFX, PG_SOUND, PG_CONTROLS };

static const char *const MARQUEE =
    "Welcome to Icy Tower!   Another game from Free Lunch Design.    Help Harold the Homeboy to climb as high as he "
    "can!       Use the d-pad to move and cross to jump.      Good luck!";

static struct {
    int page, item;
    unsigned ticks;
    int face;
    int marquee_x;
} M = {PG_MAIN, 0, 0, 0, 640};

static int page_items(int p)
{
    switch (p) {
    case PG_MAIN:
    case PG_OPTIONS: return 5;
    case PG_CONTROLS: return 6;
    default: return 3;
    }
}

static void menu_enter(void)
{
    music_play(R.sfx[SFX_BG_MENU]);
}

static void change_page(int page, int item)
{
    M.page = page;
    M.item = item;
}

static void menu_update(Input *in)
{
    M.ticks++;
    if (rnd_custom() % 198 == 1 && ++M.face == 3) M.face = 0;
    M.marquee_x -= 2;
    if (M.marquee_x < -text_width(R.font2, MARQUEE)) M.marquee_x = 640;
    if (app_in_transition()) return;

    int select = in->pressed & BTN_CONFIRM, esc = in->pressed & BTN_CANCEL;
    int up = in->repeat & BTN_UP, down = in->repeat & BTN_DOWN;
    int left = in->repeat & BTN_LEFT, right = in->repeat & BTN_RIGHT;
    int count = page_items(M.page);

    if (down) {
        play_sfx(SFX_MENU_CHOOSE, 0, 0);
        M.item = (M.item + 1) % count;
    } else if (up) {
        play_sfx(SFX_MENU_CHOOSE, 0, 0);
        M.item = (M.item - 1 + count) % count;
    } else if (esc && M.item != count - 1) {
        play_sfx(SFX_MENU_CHOOSE, 0, 0);
        M.item = count - 1;
    } else if ((esc || select) && M.item == count - 1) {
        play_sfx(SFX_MENU_CHANGE, 0, 0);
        switch (M.page) {
        case PG_MAIN: app_goto(SC_CREDITS, 32); break;
        case PG_OPTIONS: change_page(PG_MAIN, 3); break;
        case PG_GAME: change_page(PG_OPTIONS, 0); config_save(); break;
        case PG_GFX: change_page(PG_OPTIONS, 1); config_save(); break;
        case PG_SOUND: change_page(PG_OPTIONS, 2); config_save(); break;
        case PG_CONTROLS: change_page(PG_OPTIONS, 3); config_save(); break;
        }
    } else if (select) {
        if (M.page == PG_MAIN) {
            play_sfx(SFX_MENU_CHANGE, 0, 0);
            if (M.item == 0) game_start(GAME_PLAY);
            else if (M.item == 1) app_goto(SC_REPLAYS, 16);
            else if (M.item == 2) app_goto(SC_INSTRUCTIONS, 16);
            else change_page(PG_OPTIONS, 0);
        } else if (M.page == PG_OPTIONS) {
            play_sfx(SFX_MENU_CHANGE, 0, 0);
            change_page(PG_GAME + M.item, 0);
        } else if (M.page == PG_GFX && M.item == 1) {
            play_sfx(SFX_MENU_CHANGE, 0, 0);
            cfg.screen = (cfg.screen + 1) % SCREEN_MODES;
        } else if (M.page == PG_CONTROLS && M.item == 4) {
            play_sfx(SFX_MENU_CHANGE, 0, 0);
            cfg.rejump = !cfg.rejump;
        }
    } else if (left || right) {
        int d = right ? 1 : -1;
        if (M.page == PG_GAME && M.item == 0 && R.nchars > 1) {
            int c = R.cur_char + d;
            if (c >= 0 && c < R.nchars) {
                play_sfx(SFX_MENU_CHANGE, 0, 0);
                res_select_character(c);
                snprintf(cfg.character, sizeof(cfg.character), "%s", R.chars[c].dir);
            }
        } else if (M.page == PG_GAME && M.item == 1) {
            int f = cfg.start_floor + d;
            if (f >= 0 && f <= cfg.best_floor) {
                play_sfx(SFX_MENU_CHANGE, 0, 0);
                cfg.start_floor = f;
            }
        } else if (M.page == PG_GFX && M.item == 0) {
            int e = cfg.eye_candy + d;
            if (e >= 0 && e <= 2) {
                play_sfx(SFX_MENU_CHANGE, 0, 0);
                cfg.eye_candy = e;
            }
        } else if (M.page == PG_GFX && M.item == 1) {
            play_sfx(SFX_MENU_CHANGE, 0, 0);
            cfg.screen = (cfg.screen + d + SCREEN_MODES) % SCREEN_MODES;
        } else if (M.page == PG_SOUND && M.item < 2) {
            int *v = M.item == 0 ? &cfg.sound_vol : &cfg.music_vol;
            if (*v + d >= 0 && *v + d <= 10) {
                *v += d;
                play_sfx(SFX_MENU_CHANGE, 0, 0);
                music_apply_volume();
            }
        } else if (M.page == PG_CONTROLS && M.item == 4) {
            play_sfx(SFX_MENU_CHANGE, 0, 0);
            cfg.rejump = !cfg.rejump;
        }
    }
}

/* ---- drawing ---- */

static void draw_face(Bitmap *dst)
{
    static const char *const FACES[3] = {"HEROFACE000", "HEROFACE001", "HEROFACE002"};
    Bitmap *f = res_bmp(FACES[M.face]);
    double t = M.ticks;
    double cx = f->w / 2.0 + 40;
    /* Allegro fixsin takes 256 units per turn and returns 16.16 values;
     * fixtoi rounds. */
    double cy = f->h / 2.0 + 10 + floor(10 * sin(5 * t * 2 * M_PI / 256) + 0.5);
    double angle = 5 * sin((3 * t + 10.0 / 65536.0) * 2 * M_PI / 256);
    gfx_rotate_scaled(dst, f, cx, cy, angle, 1.0);
}

static const char *const HS_TITLE[HS_TABLES] = {"BEST SCORES", "BEST FLOORS", "BEST COMBOS"};

#define HS_ROW 21
#define HS_HEAD_GAP 31
#define HS_TAIL_GAP 44
#define HS_PERIOD (HS_HEAD_GAP + (HS_ENTRIES - 1) * HS_ROW + HS_TAIL_GAP)

static void draw_hs_row(Bitmap *dst, const HsEntry *e, int y)
{
    char b[16];
    text_draw(dst, R.font2, e->name, 370, y, -1, ALIGN_LEFT);
    snprintf(b, sizeof(b), "%d", e->floor);
    text_draw(dst, R.font2, b, 480, y, -1, ALIGN_RIGHT);
    snprintf(b, sizeof(b), "%d", e->combo);
    text_draw(dst, R.font2, b, 540, y, -1, ALIGN_RIGHT);
    snprintf(b, sizeof(b), "%d", e->score);
    text_draw(dst, R.font2, b, 630, y, -1, ALIGN_RIGHT);
}

static void draw_highscores(Bitmap *dst)
{
    text_draw(dst, R.font2, "HIGHSCORES", 510, 240, -1, ALIGN_CENTER);
    gfx_sprite(dst, res_bmp("HISCTOP"), 370, 264);
    bmp_set_clip(dst, 360, 280, 280, 168);
    int total = HS_PERIOD * HS_TABLES;
    int scroll = (int)((M.ticks / 2) % (unsigned)total);
    for (int rep = 0; rep < 2; rep++)
        for (int t = 0; t < HS_TABLES; t++) {
            int y = 412 - scroll + rep * total + t * HS_PERIOD;
            if (y > 460 || y + HS_PERIOD < 270) continue;
            text_draw(dst, R.font2, HS_TITLE[t], 510, y, -1, ALIGN_CENTER);
            for (int i = 0; i < HS_ENTRIES; i++) draw_hs_row(dst, &cfg.hs[t][i], y + HS_HEAD_GAP + i * HS_ROW);
        }
    bmp_reset_clip(dst);
}

static void item(Bitmap *dst, int i, const char *s)
{
    text_draw(dst, R.font1, s, 40, 270 + 28 * i, -1, ALIGN_LEFT);
}

static void draw_page(Bitmap *dst)
{
    char b[64];
    switch (M.page) {
    case PG_MAIN:
        item(dst, 0, "Start Game");
        item(dst, 1, "Load Replay");
        item(dst, 2, "Instructions");
        item(dst, 3, "Options");
        item(dst, 4, "Exit");
        break;
    case PG_OPTIONS:
        item(dst, 0, "Game Options");
        item(dst, 1, "GFX OPTIONS");
        item(dst, 2, "Sound Options");
        item(dst, 3, "Controls");
        item(dst, 4, "Back");
        break;
    case PG_GAME: {
        item(dst, 0, "Character:");
        Bitmap *idle = R.chars[R.cur_char].frame[CF_IDLE1];
        gfx_sprite(dst, idle, 330, 308 - idle->h);
        item(dst, 1, "Start floor:");
        gfx_sprite(dst, res_bmp(floor_gfx(cfg.start_floor, 0)), 315, 308);
        gfx_sprite(dst, res_bmp(floor_gfx(cfg.start_floor, 1)), 336, 308);
        gfx_sprite(dst, res_bmp(floor_gfx(cfg.start_floor, 2)), 352, 308);
        item(dst, 2, "Back");
        break;
    }
    case PG_GFX: {
        static const char *const EC[3] = {"None", "Some", "Lots"};
        static const char *const SM[SCREEN_MODES] = {"Sharp", "Smooth", "Wide", "1:1"};
        snprintf(b, sizeof(b), "Eye Candy: %s", EC[cfg.eye_candy]);
        item(dst, 0, b);
        snprintf(b, sizeof(b), "Screen: %s", SM[cfg.screen]);
        item(dst, 1, b);
        item(dst, 2, "Back");
        break;
    }
    case PG_SOUND: {
        char bar[11];
        for (int i = 0; i < 10; i++) bar[i] = i < cfg.sound_vol ? '}' : '{';
        bar[10] = 0;
        snprintf(b, sizeof(b), "Sound:%s", bar);
        item(dst, 0, b);
        for (int i = 0; i < 10; i++) bar[i] = i < cfg.music_vol ? '}' : '{';
        snprintf(b, sizeof(b), "Music:%s", bar);
        item(dst, 1, b);
        item(dst, 2, "Back");
        break;
    }
    case PG_CONTROLS: {
        static const char *const NAMES[4] = {"LEFT", "RIGHT", "JUMP", "PAUSE"};
        static const char *const KEYS[4] = {"(Left)", "(Right)", "(Cross)", "(Start)"};
        for (int i = 0; i < 4; i++) {
            item(dst, i, NAMES[i]);
            text_draw(dst, R.font1, KEYS[i], 180, 270 + 28 * i, -1, ALIGN_LEFT);
        }
        snprintf(b, sizeof(b), "ReJump: %s", cfg.rejump ? "Yes" : "No");
        item(dst, 4, b);
        item(dst, 5, "Back");
        break;
    }
    }
    Bitmap *bullet = res_bmp("MENU_BULLET");
    gfx_sprite(dst, bullet, 40 - bullet->w, 262 + 28 * M.item);
}

static void draw_title(Bitmap *dst)
{
    gfx_blit(res_bmp("TITLE_BG"), dst, 0, 0, 0, 0, 640, 480);
    gfx_sprite(dst, res_bmp("TITLE"), 250, 20);
}

static void menu_draw(Bitmap *dst, float a)
{
    (void)a;
    draw_title(dst);
    draw_face(dst);
    draw_highscores(dst);
    text_draw(dst, R.font3, VERSION_STRING, 606, 3, pal_nearest(R.pal, 0, 0, 0), ALIGN_LEFT);
    text_draw(dst, R.font3, VERSION_STRING, 605, 2, pal_nearest(R.pal, 109, 81, 85), ALIGN_LEFT);
    /* darkened strip behind the scrolling text */
    gfx_remap_rect(dst, 0, 448, 640, 1, R.map_band[0]);
    gfx_remap_rect(dst, 0, 449, 640, 1, R.map_band[1]);
    gfx_remap_rect(dst, 0, 450, 640, 30, R.map_band[2]);
    text_draw(dst, R.font2, MARQUEE, M.marquee_x, 450, -1, ALIGN_LEFT);
    draw_page(dst);
}

const Scene scene_menu = {menu_enter, NULL, menu_update, menu_draw};

/* ---- instructions ---- */

static void instr_update(Input *in)
{
    if (!app_in_transition() && (in->pressed & (BTN_CONFIRM | BTN_CANCEL | BTN_START))) app_goto(SC_MENU, 16);
}

static void instr_draw(Bitmap *dst, float a)
{
    (void)a;
    gfx_blit(res_bmp("TITLE_BG"), dst, 0, 0, 0, 0, 640, 480);
    gfx_sprite(dst, res_bmp("INSTRUCTIONS"), 0, 0);
}

const Scene scene_instructions = {NULL, NULL, instr_update, instr_draw};

/* ---- credits (on exit) ---- */

static int g_credit_ticks;

static void credits_enter(void) { g_credit_ticks = 0; }

static void credits_update(Input *in)
{
    if (app_in_transition()) return;
    if (++g_credit_ticks >= 150 || (in->pressed & BTN_CANCEL)) app.quit = 1;
}

static void credits_draw(Bitmap *dst, float a)
{
    (void)a;
    draw_title(dst);
    text_draw(dst, R.font1, "Thanks for playing!", 320, 300, -1, ALIGN_CENTER);
    text_draw(dst, R.font2, "CODING & GFX: Johan Peitz", 320, 350, -1, ALIGN_CENTER);
    text_draw(dst, R.font2, "MUSIC & SFX: Anders Svensson", 320, 376, -1, ALIGN_CENTER);
    text_draw(dst, R.font2, "PS VITA PORT: Ibrahim Dogan", 320, 402, -1, ALIGN_CENTER);
    text_draw(dst, R.font3, "Icy Tower is (c) Free Lunch Design. Unofficial PS Vita port.", 320, 440,
              pal_nearest(R.pal, 200, 200, 200), ALIGN_CENTER);
}

const Scene scene_credits = {credits_enter, NULL, credits_update, credits_draw};

/* ---- missing data ---- */

static void error_update(Input *in)
{
    if (in->pressed & (BTN_START | BTN_CANCEL | BTN_CONFIRM)) app.quit = 1;
}

static void error_draw(Bitmap *dst, float a)
{
    (void)a;
    static const char *const LINES[] = {
        "ICY TOWER for PS Vita",
        "",
        "The original game files were not found.",
        "",
        "This port does not include Icy Tower's",
        "graphics or sounds. Copy the original,",
        "free installer of Icy Tower 1.3.1",
        "",
        "  icytower13_install.exe",
        "",
        "into ux0:data/icytower/ and start the",
        "game again. It unpacks the files itself.",
        "",
        "(Or copy data/ and characters/ from an",
        "installed PC copy to ux0:data/icytower/.)",
        "",
        "Details: see the README on GitHub.",
        "Press any button to exit.",
    };
    bmp_clear(dst, 0);
    int n = (int)(sizeof(LINES) / sizeof(LINES[0]));
    for (int i = 0; i < n; i++) text_draw(dst, R.font8, LINES[i], 64, 60 + i * 18, 1, ALIGN_LEFT);
    text_draw(dst, R.font8, app.error, 64, 60 + n * 18 + 10, 2, ALIGN_LEFT);
}

const Scene scene_error = {NULL, NULL, error_update, error_draw};
