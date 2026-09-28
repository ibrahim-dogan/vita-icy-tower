/* The playing scene: runs the engine, records the replay and draws the
 * tower with all of the original's effects. Presentation logic follows
 * icytower-ng's reconstruction of Icy Tower 1.3.1. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "config.h"
#include "core.h"
#include "replay.h"
#include "rnd.h"

GameResult last_result;
Replay last_replay, file_replay;
int last_replay_valid;

enum { EDGE_NONE, EDGE_LEFT, EDGE_RIGHT };

typedef struct {
    int timer;
    int color;
    double x, y, dx, dy;
    double px, py; /* position one tick ago, for interpolation */
} Star;

#define MAX_STARS 512

static struct {
    GameMode mode;
    Core core;
    ReplayCursor cursor;
    const Replay *playing;
    int alive;

    int ticks;
    int anim_ticks, anim_frame;
    int spinning;
    double rot, prev_rot;
    int edge, edge_ticks;
    int death, death_ticks, death_voice, gameover_sfx;
    int quit, pause, escape, wait_resume;
    int shake;
    int hurry_y, prev_hurry_y;
    int clock_ticks;
    int reward, reward_timer, reward_size;
    int combo_last;
    int wide_level;
    int prev_screen_y;
    double prev_x, prev_y;
    Star stars[MAX_STARS];
    int watch_speed, watch_paused;
    int end_wait;
} G;

/* ---- floor graphics ---- */
static const char *const FLOOR_GFX[NUM_FLOOR_TYPES][4] = {
    {"FLOOR01", "FLOOR02", "FLOOR03", "SIGN01"},    {"FLOOR04", "FLOOR05", "FLOOR06", "SIGN02"},
    {"FLOOR07", "FLOOR08", "FLOOR09", "SIGN03"},    {"FLOOR10", "FLOOR11", "FLOOR12", "SIGN04"},
    {"FLOOR12a", "FLOOR12b", "FLOOR12c", "SIGN04a"}, {"FLOOR13", "FLOOR14", "FLOOR15", "SIGN05"},
    {"FLOOR16", "FLOOR17", "FLOOR18", "SIGN06"},    {"FLOOR18a", "FLOOR18b", "FLOOR18c", "SIGN06a"},
    {"FLOOR19", "FLOOR20", "FLOOR21", "SIGN07"},    {"FLOOR22", "FLOOR23", "FLOOR24", "SIGN08"},
    {"FLOOR25", "FLOOR26", "FLOOR27", "SIGN09"},
};

const char *floor_gfx(int type, int part) { return FLOOR_GFX[type][part]; }

static const char *const REWARD_GFX[10] = {"REWARD000", "REWARD001", "REWARD002", "REWARD003", "REWARD004",
                                           "REWARD005", "REWARD006", "REWARD007", "REWARD008", "REWARD009"};
static const SfxId REWARD_SFX[10] = {SFX_GOOD,    SFX_SWEET,   SFX_GREAT,     SFX_SUPER,    SFX_WOW,
                                     SFX_AMAZING, SFX_EXTREME, SFX_FANTASTIC, SFX_SPLENDID, SFX_UNBELIEVABLE};

static int reward_of(int total)
{
    static const int limits[9] = {7, 15, 25, 35, 50, 70, 100, 140, 200};
    int i = 0;
    while (i < 9 && total >= limits[i]) i++;
    return i;
}

static const char *const STAR_GFX[8] = {"STAR01", "STAR02", "STAR03", "STAR04",
                                        "STAR05", "STAR06", "STAR07", "STAR08"};

static Star *create_star(double x, double y)
{
    int i;
    for (i = 0; i < MAX_STARS; i++)
        if (G.stars[i].timer == 0) break;
    if (i == MAX_STARS) return &G.stars[0];
    Star *s = &G.stars[i];
    s->timer = 255;
    s->x = s->px = x;
    s->y = s->py = y;
    s->dx = (rnd_custom() % 50 - 25) / 10.0;
    s->dy = (rnd_custom() % 50 - 25) / 50.0;
    s->color = rnd_custom() % 8;
    return s;
}

static void update_stars(void)
{
    for (int i = 0; i < MAX_STARS; i++) {
        Star *s = &G.stars[i];
        if (!s->timer) continue;
        s->px = s->x;
        s->py = s->y;
        s->timer--;
        s->x += s->dx;
        s->y += s->dy;
        s->dy += 0.3;
        if (rnd_custom() % 5 == 1) s->color = rnd_custom() % 8;
    }
}

static Character *hero(void) { return &R.chars[R.cur_char]; }

#ifndef __vita__
/* Tests: ICYTOWER_AUTOPLAY=file.itr plays a normal game with that replay's
 * inputs, so the screens after a good game can be checked headless. */
static Replay g_autoplay;
static ReplayCursor g_autocur;
static int g_autoplay_on;
#endif

/* ---- sounds ---- */
static double g_sound_x = 320;

void play_sound(Sound *s, int stereo, int jitter)
{
    if (!s) return;
    float pan = stereo ? (float)(1.5 * (g_sound_x / 640.0) - 0.75) : 0.0f;
    float speed = jitter ? 0.9f + (rnd_custom() % 40000) / 100000.0f : 1.0f;
    audio_play(s, cfg.sound_vol / 10.0f, pan, speed, 0);
}

void play_sfx(SfxId id, int stereo, int jitter) { play_sound(R.sfx[id], stereo, jitter); }

/* ---- scene ---- */

void game_start(GameMode mode)
{
    G.mode = mode;
    app_goto(SC_GAME, 16);
}

static void enter(void)
{
    GameMode mode = G.mode;
    memset(&G, 0, sizeof(G));
    G.mode = mode;
    music_stop();
    unsigned int seed;
    int rejump;
    if (mode == GAME_PLAY) {
        rnd_seed_custom(rnd_msvc() % 102392);
        seed = (unsigned int)rnd_msvc();
        rejump = cfg.rejump;
#ifndef __vita__
        const char *ap = getenv("ICYTOWER_AUTOPLAY");
        replay_free(&g_autoplay);
        g_autoplay_on = ap && replay_load(&g_autoplay, ap) == 0;
        if (g_autoplay_on) {
            seed = g_autoplay.seed;
            rejump = g_autoplay.rejump;
            replay_cursor_init(&g_autocur, &g_autoplay);
        }
#endif
        replay_init(&last_replay, rejump, seed);
        last_replay_valid = 0;
    } else {
        G.playing = mode == GAME_WATCH_LAST ? &last_replay : &file_replay;
        seed = G.playing->seed;
        rejump = G.playing->rejump;
        replay_cursor_init(&G.cursor, G.playing);
        G.watch_speed = 1;
    }
    core_init(&G.core, rejump, seed);
    G.alive = 1;
    G.death_voice = -1;
    G.hurry_y = G.prev_hurry_y = 480;
    G.wide_level = 50;
    G.prev_x = G.core.x;
    G.prev_y = G.core.y;
    app.offset_y = 0;
}

static void entered(void)
{
    Character *c = hero();
    music_play(c->snd[CS_BGMUSIC] ? c->snd[CS_BGMUSIC] : R.sfx[SFX_BG_BEAT]);
    g_sound_x = G.core.x;
    play_sound(c->snd[CS_GREETING], 0, 0);
}

static int input_keys(const Input *in)
{
    int k = 0;
    if (in->held & BTN_LEFT) k |= CORE_KEY_LEFT;
    if (in->held & BTN_RIGHT) k |= CORE_KEY_RIGHT;
    if (in->held & BTN_JUMP) k |= CORE_KEY_JUMP;
    return k;
}

static void on_death(void)
{
    Core *c = &G.core;
    c->frozen = 1;
    if (G.mode == GAME_PLAY) {
        int score = c->score + c->floor * 10;
        replay_finish(&last_replay, score, c->floor, c->combo);
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        static const char *const MON[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        if (tm) snprintf(last_replay.date, sizeof(last_replay.date), "%2d %s %d", tm->tm_mday, MON[tm->tm_mon],
                         tm->tm_year + 1900);
        last_replay_valid = 1;
        last_result.score = score;
        last_result.floor = c->floor;
        last_result.combo = c->combo;
    }
}

static void tick(const Input *in)
{
    Core *c = &G.core;
    Character *h = hero();
    G.prev_screen_y = c->screen_y;
    G.prev_x = c->x;
    G.prev_y = c->y;
    G.prev_rot = G.rot;
    G.prev_hurry_y = G.hurry_y;
    G.ticks++;

    int keys;
    if (G.playing) {
        keys = G.alive ? replay_cursor_next(&G.cursor) : 0;
        if (keys < 0) keys = 0;
    } else {
        keys = input_keys(in);
#ifndef __vita__
        if (g_autoplay_on) {
            keys = replay_cursor_next(&g_autocur);
            if (keys < 0) keys = 0;
        }
#endif
        if (G.alive) replay_record(&last_replay, keys);
    }
    int was_alive = G.alive;
    int still = core_frame(c, keys);
    if (was_alive && !still) {
        G.alive = 0;
        on_death();
    }
    g_sound_x = c->x;

    if (c->events & EV_JUMP) {
        if (c->jump_dy < -22.0) {
            G.spinning = 1;
            G.rot = G.prev_rot = 0;
            play_sound(h->snd[CS_JUMPHI], 1, 1);
        } else if (c->jump_dy < -15.0) {
            play_sound(h->snd[CS_JUMPMED], 1, 1);
        } else {
            play_sound(h->snd[CS_JUMPLO], 1, 1);
        }
    }
    if (c->events & EV_LAND) {
        play_sfx(SFX_STEP, 1, 1);
        G.spinning = 0;
    }
    if (G.spinning && c->combo_timer && cfg.eye_candy == 2) create_star(c->x, c->y - 16);
    update_stars();

    /* clock and hurry up */
    if (!G.death && c->screen_y > 100) G.clock_ticks = c->speed < 5 ? c->speed_counter : G.clock_ticks - 44;
    if (G.hurry_y < 480 && G.hurry_y > -100) G.hurry_y -= 2;
    if (c->events & EV_SPEEDUP) {
        G.hurry_y = 479;
        play_sfx(SFX_HURRYUP, 0, 0);
        play_sfx(SFX_RING, 0, 0);
    }

    if (c->events & EV_COMBO_END) {
        G.combo_last = c->last_combo;
        G.reward = reward_of(c->last_combo);
        G.reward_timer = 80;
        G.reward_size = 0;
        if (cfg.eye_candy == 2 && G.reward >= 3) {
            int count = 16 * (G.reward - 2);
            while (count--) {
                Star *s = create_star(320, 360);
                s->dy = -(rnd_custom() % 500 + 500) / 100.0;
                s->dx = (G.reward - 2) * (rnd_custom() % 1000 - 500) / 100.0;
            }
        }
        play_sfx(REWARD_SFX[G.reward], 0, 0);
#ifndef __vita__
        if (getenv("ICYTOWER_EVENTS")) fprintf(stderr, "event tick=%d combo=%d reward=%d floor=%d\n", G.ticks,
                                               c->last_combo, G.reward, c->floor);
#endif
    }

    /* death */
    if (c->y > 540.0 && !G.death) {
        G.death = 1;
        c->combo_timer = 0;
        g_sound_x = c->x;
        G.death_voice = h->snd[CS_DEATH] ? audio_play(h->snd[CS_DEATH], cfg.sound_vol / 10.0f,
                                                      (float)(1.5 * (c->x / 640.0) - 0.75), 1, 0)
                                         : -1;
        G.death_ticks = 1;
    }
    if (c->y > 900.0 && !G.gameover_sfx) {
        G.gameover_sfx = 1;
        play_sfx(SFX_GAMEOVER, 0, 0);
    }
    if (G.death_ticks) G.death_ticks++;
    if (G.death_ticks > 250 || (G.death_ticks && G.death_ticks > 5 * c->floor)) {
        G.death_ticks = 0;
        audio_stop(G.death_voice);
        play_sfx(SFX_SPLAT, 1, 0);
        G.shake = 24;
    }

    /* every 50 floors: cheer and a burst of stars */
    if (c->floor >= G.wide_level) {
        play_sfx(SFX_AIGHT, 0, 0);
        if (cfg.eye_candy == 2) {
            int count = G.wide_level / 2;
            while (count--) {
                Star *s = create_star(rnd_custom() % 600 + 20, 480);
                s->dy = -(rnd_custom() % 200) / 10.0;
            }
        }
        G.wide_level += G.wide_level < 1000 ? 50 : 500;
    }

    /* balancing on an edge */
    int feet = core_feet_on_floor(c);
    G.edge = feet == 1 ? EDGE_RIGHT : feet == 2 ? EDGE_LEFT : EDGE_NONE;
    if (G.edge == EDGE_NONE) G.edge_ticks = 0;
    if (c->status == ST_IDLE && G.edge_ticks == 11) play_sound(h->snd[CS_EDGE], 1, 1);
    if (G.edge != EDGE_NONE && ++G.edge_ticks == 50) G.edge_ticks = 0;

    if (G.shake) {
        G.shake--;
        app.offset_y = rnd_custom() % 8;
    } else {
        app.offset_y = 0;
    }

    if (++G.anim_ticks == 50) G.anim_ticks = 0;
    if (G.anim_ticks % 10 == 0 && ++G.anim_frame == 4) G.anim_frame = 0;
    if (G.spinning) G.rot += 8;
    if (G.death && G.death < 300) G.death += 8;
    if (G.reward_timer) {
        if (G.reward_timer > 60) G.reward_size += 3277;
        else if (G.reward_timer <= 9) G.reward_size -= 6554;
        G.reward_timer--;
    }
}

static void finish(void)
{
    app.offset_y = 0;
    audio_stop(G.death_voice);
    if (G.mode == GAME_PLAY) {
        flow_after_game();
    } else if (G.mode == GAME_WATCH_LAST) {
        app_goto(SC_GAMEOVER, 0);
    } else {
        music_stop();
        app_goto(SC_REPLAYS, 16);
    }
}

static void update(Input *in)
{
    if (G.quit) return;
    if (G.playing) {
        /* VCR: circle stops, cross pauses, right plays fast, up faster */
        if (in->pressed & BTN_CIRCLE) {
            G.quit = 1;
            finish();
            return;
        }
        if (in->pressed & (BTN_CROSS | BTN_START)) G.watch_paused = !G.watch_paused;
        G.watch_speed = (in->held & BTN_UP) ? 4 : (in->held & (BTN_RIGHT | BTN_R)) ? 2 : 1;
        if (G.watch_paused) {
            G.prev_screen_y = G.core.screen_y;
            G.prev_x = G.core.x;
            G.prev_y = G.core.y;
            return;
        }
        for (int i = 0; i < G.watch_speed && !G.quit; i++) {
            tick(in);
            if (G.death > 250 && ++G.end_wait > 50) {
                G.quit = 1;
                finish();
            }
        }
        return;
    }

    if (G.pause) {
        if (G.wait_resume) {
            if (in->released & BTN_START) G.pause = G.escape = G.wait_resume = 0;
        } else if (in->pressed & BTN_START) {
            G.wait_resume = 1;
        } else if (G.escape && (in->pressed & BTN_CIRCLE)) {
            music_stop();
            play_sfx(SFX_TRYAGAIN, 0, 0);
            G.quit = 1;
            app_goto(SC_MENU, 16);
        } else if (in->pressed) {
            G.pause = G.escape = 0;
        }
        return;
    }

    tick(in);

    if (G.death > 250 && (in->pressed & (BTN_JUMP | BTN_START))) {
        G.quit = 1;
        finish();
        return;
    }
    if (in->pressed & BTN_CIRCLE) {
        if (G.death) {
            if (G.death > 8) {
                G.quit = 1;
                finish();
            }
        } else {
            G.pause = G.escape = 1;
            play_sound(hero()->snd[CS_PAUSE], 1, 0);
        }
    } else if ((in->pressed & BTN_START) && !G.death) {
        G.pause = 1;
        G.escape = 0;
        play_sound(hero()->snd[CS_PAUSE], 1, 0);
    }
}

/* ---- drawing ---- */

static uint8_t c_white, c_shadow;

static void draw_floors(Bitmap *dst, double off)
{
    const Core *c = &G.core;
    int top = core_top_level(c);
    for (int level = top; level >= 0 && level > top - CORE_FLOOR_RING + 1; level--) {
        int y = core_floor_surface(c, level) - 6 + (int)floor(off);
        if (y > SCREEN_H) break;
        if (y < -64) continue;
        const CoreFloor *f = core_floor(c, level);
        int type = cfg.start_floor + level / 100;
        if (type >= NUM_FLOOR_TYPES - 1) type = NUM_FLOOR_TYPES - 2;
        if (level >= (NUM_FLOOR_TYPES - 1) * 100) type = NUM_FLOOR_TYPES - 1;
        Bitmap *l = res_bmp(FLOOR_GFX[type][0]), *m = res_bmp(FLOOR_GFX[type][1]), *r = res_bmp(FLOOR_GFX[type][2]);
        int x = 16 * f->start - 5;
        gfx_sprite(dst, l, x, y);
        x += 21;
        while (x < f->end * 16) {
            gfx_sprite(dst, m, x, y);
            x += 16;
        }
        gfx_sprite(dst, r, x, y);
        if (level != 0 && level % 10 == 0) {
            Bitmap *sign = res_bmp(FLOOR_GFX[type][3]);
            char num[16];
            snprintf(num, sizeof(num), "%d", level);
            int sx = 16 * (f->start + (f->end - f->start) / 2), sy = y + 16;
            gfx_sprite(dst, sign, sx, sy);
            sx += sign->w / 2;
            text_draw(dst, R.font3, num, sx + 1, sy + 7, c_shadow, ALIGN_CENTER);
            text_draw(dst, R.font3, num, sx + 2, sy + 6, c_shadow, ALIGN_CENTER);
            text_draw(dst, R.font3, num, sx, sy + 6, c_shadow, ALIGN_CENTER);
            text_draw(dst, R.font3, num, sx + 1, sy + 5, c_shadow, ALIGN_CENTER);
            text_draw(dst, R.font3, num, sx + 1, sy + 6, c_white, ALIGN_CENTER);
        }
    }
}

static void draw_character(Bitmap *dst, double off, float a)
{
    const Core *c = &G.core;
    Bitmap **fr = hero()->frame;
    double fx = G.prev_x + (c->x - G.prev_x) * a, fy = G.prev_y + (c->y - G.prev_y) * a;
    (void)off;
    int x = (int)fx, y = (int)fy;
    double dx = c->dx, dy = c->dy;
    Bitmap *b = NULL;
    if (c->status == ST_IDLE) {
        if (fabs(dx) < 0.02) {
            if (G.edge == EDGE_NONE) {
                if (c->screen_y <= 200 || c->y <= 400.0) {
                    int t = G.anim_ticks;
                    b = t <= 11 ? fr[CF_IDLE2] : t <= 24 ? fr[CF_IDLE1] : t <= 36 ? fr[CF_IDLE3] : fr[CF_IDLE1];
                } else {
                    b = fr[CF_CHOCK];
                }
                if (dx > 0) gfx_sprite(dst, b, x - 14, y - 51);
                else gfx_sprite_hflip(dst, b, x - 14, y - 51);
            } else {
                b = (G.anim_ticks & 8) == 0 ? fr[CF_EDGE1] : fr[CF_EDGE2];
                if (G.edge == EDGE_LEFT) gfx_sprite_hflip(dst, b, x - 26, y - 50);
                else gfx_sprite(dst, b, x - 11, y - 50);
            }
        } else {
            int f = fabs(dx) < 0.2 ? 0 : G.anim_frame;
            b = fr[CF_WALK1 + f];
            if (dx > 0) gfx_sprite(dst, b, x - 14, y - 51);
            else gfx_sprite_hflip(dst, b, x - 14, y - 51);
        }
    } else if (G.spinning) {
        b = fr[CF_ROTATE];
        double rot = G.prev_rot + (G.rot - G.prev_rot) * a;
        gfx_rotate_scaled(dst, b, x - 14 + b->w / 2.0, y - 51 + b->h / 2.0, rot, 1.0);
    } else {
        if (fabs(dx) < 0.01) b = fr[CF_JUMP];
        else if (c->status == ST_FLY_UP && dy < -3.0) b = fr[CF_JUMP1];
        else if ((c->status == ST_FLY_IDLE || c->status == ST_FLY_DOWN) && dy > 3.0) b = fr[CF_JUMP3];
        else b = fr[CF_JUMP2];
        if (dx > 0) gfx_sprite(dst, b, x - 14, y - 51);
        else gfx_sprite_hflip(dst, b, x - 14, y - 51);
    }
}

static void draw_hud(Bitmap *dst, float a)
{
    const Core *c = &G.core;
    /* combo meter */
    Bitmap *count = res_bmp("COMBO_COUNT");
    gfx_sprite(dst, res_bmp("COMBO_METER"), 20, 100);
    char num[32];
    if (c->combo_timer) {
        gfx_sprite_region(dst, res_bmp("COMBO_LIQUID"), 0, 100 - c->combo_timer, 16, c->combo_timer, 31,
                          219 - c->combo_timer);
        gfx_sprite(dst, count, -10, 210);
        snprintf(num, sizeof(num), "%d", c->combo_floor);
        text_draw(dst, R.font1, num, 40, 214, -1, ALIGN_CENTER);
    } else if (G.reward_timer) {
        gfx_sprite(dst, count, -10, 210);
        snprintf(num, sizeof(num), "%d", G.combo_last);
        text_draw(dst, R.font1, num, 40, 214, -1, ALIGN_CENTER);
    }

    /* clock */
    Bitmap *clock = res_bmp("CLOCK"), *hand = res_bmp("CLOCK_HAND");
    int t = G.anim_ticks, hy = G.hurry_y;
    int cx = 4, cy = 10, hx = 32, hyy = 28;
    if (hy < 480 && hy > 250) {
        cx += t % 3 - 1;
        cy += (t + 1) % 3 - 1;
    }
    gfx_sprite(dst, clock, cx, cy);
    if (hy < 480 && hy > 200) {
        hx += (t + 2) % 3 - 1;
        hyy += (t + 3) % 3 - 1;
    }
    gfx_rotate_scaled(dst, hand, hx + hand->w / 2.0, hyy + hand->h / 2.0, (G.clock_ticks % 1500) * 0.1706666, 1.0);

    /* reward */
    if (G.reward_timer && cfg.eye_candy) {
        Bitmap *r = res_bmp(REWARD_GFX[G.reward]);
        double scale = G.reward_size / 65536.0;
        if (scale > 0) {
            if (cfg.eye_candy == 1)
                gfx_stretch_sprite(dst, r, (int)(320 - (scale / 2) * r->w), (int)(360 - scale * r->h),
                                   (int)(scale * r->w), (int)(scale * r->h));
            else
                gfx_rotate_scaled(dst, r, 320, 360 + scale * (r->h - 120.0), scale * 256.0, scale);
        }
    }
    (void)a;

    /* game over */
    if (G.death) {
        Bitmap *go = res_bmp("GAMEOVER");
        int d = G.death;
        gfx_sprite(dst, go, 320 - go->w / 2, 480 - d);
        text_draw(dst, R.font1, "Score:", 140, 580 - d, -1, ALIGN_LEFT);
        text_draw(dst, R.font1, "Level:", 140, 620 - d, -1, ALIGN_LEFT);
        text_draw(dst, R.font1, "Best combo:", 140, 660 - d, -1, ALIGN_LEFT);
        snprintf(num, sizeof(num), "%d", c->score + 10 * c->floor);
        text_draw(dst, R.font1, num, 500, 580 - d, -1, ALIGN_RIGHT);
        snprintf(num, sizeof(num), "%d", c->floor);
        text_draw(dst, R.font1, num, 500, 620 - d, -1, ALIGN_RIGHT);
        snprintf(num, sizeof(num), "%d", c->combo);
        text_draw(dst, R.font1, num, 500, 660 - d, -1, ALIGN_RIGHT);
    }

    snprintf(num, sizeof(num), "score: %d", c->score + 10 * c->floor);
    text_draw(dst, R.font1, num, 8, 440, -1, ALIGN_LEFT);
}

static void draw_world(Bitmap *dst, float a)
{
    const Core *c = &G.core;
    c_white = (uint8_t)pal_nearest(R.pal, 255, 255, 255);
    c_shadow = (uint8_t)pal_nearest(R.pal, 16, 16, 16);
    double sy = G.prev_screen_y + (c->screen_y - G.prev_screen_y) * a;
    double off = sy - c->screen_y;

    /* background wall, scrolling at half speed */
    int bg = (int)floor(fmod(sy, 128.0) / 2.0);
    for (int i = 0; i < 9; i++) gfx_blit(R.background, dst, 60, 0, 60, bg + (i - 1) * 64, 520, 64);

    /* hurry up */
    Bitmap *hu = res_bmp("HURRYUP");
    double hy = G.prev_hurry_y + (G.hurry_y - G.prev_hurry_y) * a;
    if (G.hurry_y < 480 && G.hurry_y > -100) gfx_sprite(dst, hu, 320 - hu->w / 2, (int)hy);

    draw_floors(dst, off);

    for (int i = 0; i < MAX_STARS; i++) {
        const Star *s = &G.stars[i];
        if (!s->timer) continue;
        double x = s->px + (s->x - s->px) * a, y = s->py + (s->y - s->py) * a;
        gfx_sprite(dst, res_bmp(STAR_GFX[s->color]), (int)(x + 0.5), (int)(y + 0.5));
    }

    draw_character(dst, off, a);

    /* side walls, scrolling faster than the floors */
    Bitmap *side = res_bmp("SIDEBLOCK");
    int wy = (int)(fmod(sy, 84.0) * 1.476);
    for (int i = 0; i < 5; i++) {
        gfx_sprite(dst, side, 565, wy + (i - 1) * 124);
        gfx_sprite_hflip(dst, side, -57, wy + (i - 1) * 124);
    }
}

static void draw_pause(Bitmap *dst)
{
    gfx_grid(dst, (uint8_t)pal_nearest(R.pal, 0, 0, 0));
    if (G.escape) {
        text_draw(dst, R.font1, "DO YOU REALLY WANT TO EXIT?", 320, 160, -1, ALIGN_CENTER);
        text_draw(dst, R.font2, "Press any key to resume", 320, 210, -1, ALIGN_CENTER);
        text_draw(dst, R.font2, "Press CIRCLE to exit", 320, 240, -1, ALIGN_CENTER);
    } else {
        text_draw(dst, R.font1, "Game Paused", 320, 160, -1, ALIGN_CENTER);
        text_draw(dst, R.font2, "Press any key to resume", 320, 210, -1, ALIGN_CENTER);
    }
}

static void draw_vcr(Bitmap *dst)
{
    Bitmap *vcr = res_bmp("VCR");
    int x = 640 - vcr->w - 70, y = 480 - vcr->h - 4;
    gfx_sprite(dst, vcr, x, y);
    /* light up the arrow of the current speed */
    Bitmap *lamp = G.watch_paused ? res_bmp("VCR_LEFT") : G.watch_speed == 4 ? res_bmp("VCR_UP") : res_bmp("VCR_RIGHT");
    gfx_sprite(dst, lamp, x + vcr->w - 34, y + 5);
    const Replay *r = G.playing;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", r->name);
    text_draw(dst, R.font2, buf, 632, 8, -1, ALIGN_RIGHT);
}

static void draw(Bitmap *dst, float a)
{
    if (G.pause || (G.playing && G.watch_paused)) a = 1;
    draw_world(dst, a);
    draw_hud(dst, a);
    if (G.playing) draw_vcr(dst);
    if (G.pause) draw_pause(dst);
}

void game_draw_frozen(Bitmap *dst)
{
    draw_world(dst, 1);
    draw_hud(dst, 1);
}

const Scene scene_game = {enter, entered, update, draw};
