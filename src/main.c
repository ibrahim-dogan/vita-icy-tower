#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "config.h"
#include "platform.h"
#include "rnd.h"
#include "video.h"
#include "log.h"
#include "installer.h"

#ifndef __vita__
#include "stb_image_write.h"
#endif

App app;

static const Scene *const SCENES[SC_COUNT] = {
    &scene_error, &scene_menu,    &scene_game,        &scene_gameover,    &scene_hiscore,
    &scene_unlock, &scene_instructions, &scene_credits, &scene_save_replay, &scene_replays,
};

/* ---- scene transitions (fade through black, like the original) ---- */

static int g_transition, g_speed;
static SceneId g_next;

static void enter_scene(SceneId s)
{
    app.scene = s;
    if (SCENES[s]->enter) SCENES[s]->enter();
}

void app_goto(SceneId s, int speed)
{
    if (g_transition) return;
    if (speed <= 0) {
        enter_scene(s);
        app.overlay = 0;
        if (SCENES[s]->entered) SCENES[s]->entered();
        return;
    }
    g_transition = 1;
    g_speed = speed;
    g_next = s;
}

int app_in_transition(void) { return g_transition; }

static void update_transition(void)
{
    if (app.scene != g_next) {
        app.overlay += g_speed;
        if (app.overlay >= 255) {
            app.overlay = 255;
            enter_scene(g_next);
        }
    } else {
        app.overlay -= g_speed;
        if (app.overlay <= 0) {
            app.overlay = 0;
            g_transition = 0;
            if (SCENES[app.scene]->entered) SCENES[app.scene]->entered();
        }
    }
}

/* ---- music ---- */

static Sound *g_music;

void music_play(Sound *s)
{
    if (g_music == s) return;
    g_music = s;
    audio_music_volume(cfg.music_vol / 10.0f);
    if (s) audio_music_play(s);
    else audio_music_stop();
}

void music_stop(void)
{
    g_music = NULL;
    audio_music_stop();
}

void music_apply_volume(void) { audio_music_volume(cfg.music_vol / 10.0f); }

/* ---- presenting ---- */

static Bitmap *g_screen;
static int g_headless;

#ifndef __vita__
static void save_shot(const char *path)
{
    uint32_t *buf = malloc((size_t)SCREEN_W * SCREEN_H * 4);
    if (!buf) return;
    video_convert(g_screen, R.pal, app.overlay, app.offset_y, buf, SCREEN_W);
    stbi_write_png(path, SCREEN_W, SCREEN_H, 4, buf, SCREEN_W * 4);
    free(buf);
}
#endif

/* ---- hidden performance overlay (hold L+R, press SELECT) ---- */

static int g_perf;
static char g_perf_text[64] = "";

static void draw_perf(void)
{
    if (!g_perf) return;
    gfx_fill(g_screen, 4, 4, 8 * (int)strlen(g_perf_text) + 4, 12, 0);
    text_draw(g_screen, R.font8, g_perf_text, 6, 6, pal_nearest(R.pal, 255, 255, 255), ALIGN_LEFT);
}

/* ---- first start: unpack the original installer ---- */

static void draw_unpack(int done, int total)
{
    res_init_basic();
    bmp_clear(g_screen, 0);
    text_draw(g_screen, R.font8, "ICY TOWER for PS Vita", 64, 120, 1, ALIGN_LEFT);
    text_draw(g_screen, R.font8, "Unpacking the Icy Tower 1.3.1 installer...", 64, 160, 1, ALIGN_LEFT);
    gfx_fill(g_screen, 64, 200, 512, 16, 4);
    gfx_fill(g_screen, 64, 200, total ? 512 * done / total : 0, 16, 3);
    video_present(g_screen, R.pal, 0, 0, cfg.screen);
}

static char g_unpack_error[256];

static void unpack_installer_if_needed(void)
{
    FILE *f = fopen(game_path("data/data.dat"), "rb");
    if (f) {
        fclose(f);
        return;
    }
    char exe[512];
    int found = installer_find(exe, sizeof(exe));
    if (found == INSTALLER_UNKNOWN) {
        log_printf("*** an .exe was found in %s, but it is not icytower13_install.exe of version 1.3.1", game_root());
        snprintf(g_unpack_error, sizeof(g_unpack_error), "The .exe found is not the 1.3.1 installer.");
        return;
    }
    if (found != INSTALLER_OK) return;
    if (installer_extract(exe, draw_unpack, g_unpack_error, sizeof(g_unpack_error)) == 0) g_unpack_error[0] = 0;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    platform_init();
    log_open();
    log_printf("Icy Tower for PS Vita %s (engine of Icy Tower 1.3.1)", PORT_VERSION);
    log_printf("game folder: %s", game_root());
    input_init();
    g_headless = input_headless();

#ifdef __vita__
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
#else
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0) {
#endif
        log_printf("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    if (video_init(g_headless) != 0) {
        log_printf("video init failed: %s", SDL_GetError());
        return 1;
    }
    g_screen = bmp_create(SCREEN_W, SCREEN_H);

    audio_init();
    config_load();

    unsigned int t0 = (unsigned int)time(NULL);
    if (g_headless) t0 = 12345;
    rnd_seed_msvc(t0);
    rnd_msvc();
    rnd_seed_custom(rnd_msvc() % 2367);

    unpack_installer_if_needed();
    if (res_load(app.error, sizeof(app.error)) != 0) {
        log_printf("*** %s", app.error);
        if (g_unpack_error[0]) snprintf(app.error, sizeof(app.error), "%s", g_unpack_error);
        res_init_basic();
        app_goto(SC_ERROR, 0);
    } else {
        int c = res_find_character(cfg.character);
        if (c < 0) c = res_find_character("harold_the_homeboy");
        res_select_character(c < 0 ? 0 : c);
        app.overlay = 255;
        app.scene = SC_MENU;
        g_transition = 1;
        g_next = SC_MENU;
        g_speed = 16;
        SCENES[SC_MENU]->enter();
    }

    Input in;
    memset(&in, 0, sizeof(in));
    const double step = 1.0 / GAME_HZ;
    double acc = 0;
    Uint64 prev = SDL_GetPerformanceCounter(), freq = SDL_GetPerformanceFrequency();
    int perf_frames = 0;
    double perf_time = 0, perf_cpu = 0;

    while (!in.quit && !app.quit) {
        input_pump(&in);
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - prev) / (double)freq;
        prev = now;
        if (dt > 0.25) dt = 0.25;
        int ticks;
        if (g_headless) {
            ticks = 1;
            acc = 0;
        } else {
            acc += dt;
            ticks = (int)(acc / step);
            acc -= ticks * step;
            if (ticks > 5) ticks = 5;
        }
        Uint64 work = SDL_GetPerformanceCounter();
        char shot[256] = "";
#ifndef __vita__
        char dshot[256] = "";
        static FILE *rec;
#endif
        for (int i = 0; i < ticks && !in.quit && !app.quit; i++) {
            input_tick(&in);
            if (in.shot[0]) snprintf(shot, sizeof(shot), "%s", in.shot);
#ifndef __vita__
            if (in.dshot[0]) snprintf(dshot, sizeof(dshot), "%s", in.dshot);
            if (in.record[0]) {
                if (rec) fclose(rec);
                rec = strcmp(in.record, "off") ? fopen(in.record, "ab") : NULL;
                in.record[0] = 0;
            }
#endif
            if ((in.held & BTN_L) && (in.held & BTN_R) && (in.pressed & BTN_SELECT)) {
                g_perf = !g_perf;
                in.pressed &= ~(uint32_t)BTN_SELECT;
            }
            if (g_transition) update_transition();
            else SCENES[app.scene]->update(&in);
        }
        float alpha = g_headless ? 1.0f : (float)(acc / step);
        if (alpha > 1) alpha = 1;
#ifndef __vita__
        static int bench = -1;
        static double bench_t, bench_max;
        static long bench_n;
        if (bench < 0) bench = getenv("ICYTOWER_BENCH") != NULL;
        Uint64 b0 = SDL_GetPerformanceCounter();
#endif
        SCENES[app.scene]->draw(g_screen, alpha);
#ifndef __vita__
        if (bench) {
            static uint32_t sink[SCREEN_W * SCREEN_H];
            video_convert(g_screen, R.pal, app.overlay, app.offset_y, sink, SCREEN_W);
            double t = (double)(SDL_GetPerformanceCounter() - b0) / (double)freq;
            bench_t += t;
            if (t > bench_max) bench_max = t;
            if (++bench_n % 500 == 0)
                fprintf(stderr, "bench: %ld frames, draw+convert avg %.3f ms, max %.3f ms\n", bench_n,
                        bench_t * 1000 / bench_n, bench_max * 1000);
        }
#endif
        bmp_reset_clip(g_screen);
        draw_perf();
        video_present(g_screen, R.pal, app.overlay, app.offset_y, cfg.screen);
        perf_cpu += (double)(SDL_GetPerformanceCounter() - work) / (double)freq;
#ifndef __vita__
        if (shot[0]) save_shot(shot);
        if (dshot[0] || rec) {
            const uint8_t *rgb = video_display_rgb();
            if (rgb && dshot[0]) stbi_write_png(dshot, 960, 544, 3, rgb, 960 * 3);
            if (rgb && rec) fwrite(rgb, 1, 960 * 544 * 3, rec);
        }
#endif
        perf_frames++;
        perf_time += dt;
        if (perf_time >= 0.5) {
            snprintf(g_perf_text, sizeof(g_perf_text), "%.0f fps  cpu %.1f ms", perf_frames / perf_time,
                     perf_cpu * 1000.0 / perf_frames);
            perf_frames = 0;
            perf_time = perf_cpu = 0;
        }
    }

    config_save();
    log_printf("exit");
    log_close();
    audio_shutdown();
    video_shutdown();
    SDL_Quit();
    return 0;
}
