#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

Config cfg;

void config_defaults(void)
{
    memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.character, sizeof(cfg.character), "harold_the_homeboy");
    cfg.eye_candy = 2;
    cfg.sound_vol = 8;
    cfg.music_vol = 7;
    cfg.rejump = 1;
    cfg.screen = SCREEN_SHARP;
    snprintf(cfg.replay_name, sizeof(cfg.replay_name), "Harold");
    /* The tables a fresh install of the original starts with. */
    static const HsEntry def[HS_ENTRIES] = {
        {"FLD", 58, 18, 904}, {"FLD", 47, 15, 695}, {"FLD", 35, 11, 471}, {"FLD", 23, 7, 279}, {"FLD", 11, 3, 119},
    };
    for (int t = 0; t < HS_TABLES; t++) memcpy(cfg.hs[t], def, sizeof(def));
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void config_load(void)
{
    config_defaults();
    FILE *f = fopen(config_path(), "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64], val[128];
        if (sscanf(line, "%63[^=]=%127[^\n]", key, val) != 2) continue;
        int t, i;
        if (!strcmp(key, "character")) snprintf(cfg.character, sizeof(cfg.character), "%s", val);
        else if (!strcmp(key, "start_floor")) cfg.start_floor = atoi(val);
        else if (!strcmp(key, "best_floor")) cfg.best_floor = atoi(val);
        else if (!strcmp(key, "eye_candy")) cfg.eye_candy = atoi(val);
        else if (!strcmp(key, "sound_vol")) cfg.sound_vol = atoi(val);
        else if (!strcmp(key, "music_vol")) cfg.music_vol = atoi(val);
        else if (!strcmp(key, "rejump")) cfg.rejump = atoi(val) != 0;
        else if (!strcmp(key, "screen")) cfg.screen = atoi(val);
        else if (!strcmp(key, "replay_name")) snprintf(cfg.replay_name, sizeof(cfg.replay_name), "%s", val);
        else if (sscanf(key, "hs%d_%d", &t, &i) == 2 && t >= 0 && t < HS_TABLES && i >= 0 && i < HS_ENTRIES) {
            HsEntry *e = &cfg.hs[t][i];
            char name[8] = "";
            if (sscanf(val, "%3[^,],%d,%d,%d", name, &e->floor, &e->combo, &e->score) == 4)
                snprintf(e->name, sizeof(e->name), "%s", name);
        }
    }
    fclose(f);
    cfg.best_floor = clampi(cfg.best_floor, 0, NUM_FLOOR_TYPES - 2);
    cfg.start_floor = clampi(cfg.start_floor, 0, cfg.best_floor);
    cfg.eye_candy = clampi(cfg.eye_candy, 0, 2);
    cfg.sound_vol = clampi(cfg.sound_vol, 0, 10);
    cfg.music_vol = clampi(cfg.music_vol, 0, 10);
    cfg.screen = clampi(cfg.screen, 0, SCREEN_MODES - 1);
}

void config_save(void)
{
    FILE *f = fopen(config_path(), "w");
    if (!f) return;
    fprintf(f, "character=%s\nstart_floor=%d\nbest_floor=%d\neye_candy=%d\nsound_vol=%d\nmusic_vol=%d\n"
               "rejump=%d\nscreen=%d\nreplay_name=%s\n",
            cfg.character, cfg.start_floor, cfg.best_floor, cfg.eye_candy, cfg.sound_vol, cfg.music_vol, cfg.rejump,
            cfg.screen, cfg.replay_name);
    for (int t = 0; t < HS_TABLES; t++)
        for (int i = 0; i < HS_ENTRIES; i++) {
            const HsEntry *e = &cfg.hs[t][i];
            fprintf(f, "hs%d_%d=%s,%d,%d,%d\n", t, i, e->name, e->floor, e->combo, e->score);
        }
    fclose(f);
}

static int key_of(int t, int floor, int combo, int score)
{
    return t == HS_SCORES ? score : t == HS_FLOORS ? floor : combo;
}

int hs_rank(int t, int floor, int combo, int score)
{
    int k = key_of(t, floor, combo, score);
    for (int i = 0; i < HS_ENTRIES; i++) {
        const HsEntry *e = &cfg.hs[t][i];
        if (k > key_of(t, e->floor, e->combo, e->score)) return i;
    }
    return -1;
}

void hs_insert(int t, const HsEntry *e)
{
    int r = hs_rank(t, e->floor, e->combo, e->score);
    if (r < 0) return;
    memmove(&cfg.hs[t][r + 1], &cfg.hs[t][r], sizeof(HsEntry) * (size_t)(HS_ENTRIES - 1 - r));
    cfg.hs[t][r] = *e;
}
