#include "replay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"

#define HEADER_SIZE 98

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void replay_init(Replay *r, int rejump, unsigned int seed)
{
    free(r->macros);
    memset(r, 0, sizeof(*r));
    r->rejump = rejump;
    r->seed = seed;
}

void replay_free(Replay *r)
{
    free(r->macros);
    memset(r, 0, sizeof(*r));
}

static void push(Replay *r, int frames, int keys)
{
    if (r->count == r->cap) {
        int cap = r->cap ? r->cap * 2 : 1024;
        ReplayMacro *m = realloc(r->macros, (size_t)cap * sizeof(ReplayMacro));
        if (!m) return;
        r->macros = m;
        r->cap = cap;
    }
    r->macros[r->count].frames = frames;
    r->macros[r->count].keys = (unsigned char)keys;
    r->count++;
}

void replay_record(Replay *r, int keys)
{
    if (r->count > 0 && r->macros[r->count - 1].keys == keys)
        r->macros[r->count - 1].frames++;
    else
        push(r, 0, keys);
}

void replay_finish(Replay *r, int score, int floor, int combo)
{
    push(r, 0, REPLAY_END);
    r->score = score;
    r->floor = floor;
    r->combo = combo;
}

/* Hash from RaMMicHaeL's replay_checker; names use signed chars like the
 * x86 original. */
static uint32_t calc_hash(const uint8_t *hdr, const ReplayMacro *m, int count)
{
    int score = (int)get32(hdr + 74), floor = (int)get32(hdr + 78), combo = (int)get32(hdr + 82);
    int rejump = (int)get32(hdr + 86);
    uint32_t seed = get32(hdr + 90);
    const signed char *name = (const signed char *)hdr + 10;
    const signed char *date = (const signed char *)hdr + 42;
    uint32_t hash = (uint32_t)(score * 7 + floor * 13 + combo * 23 + rejump * 26) + seed * 17 + 43;
    int i;
    for (i = 0; i < 31; i++) hash += (uint32_t)((name[i] + i) * (date[i] + i) * 117 * (i + 1));
    hash += (uint32_t)((name[31] + i) * (date[31] + i) * 117 * (i + 1));
    for (i = 1; i < count; i++) hash += (uint32_t)((m[i].frames * 3 + m[i].keys * 5) * i);
    return hash;
}

static void build_header(const Replay *r, uint8_t *hdr)
{
    memset(hdr, 0, HEADER_SIZE);
    memcpy(hdr, "ITR130", 6);
    put32(hdr + 6, (uint32_t)r->count);
    memcpy(hdr + 10, r->name, strnlen(r->name, 31));
    memcpy(hdr + 42, r->date, strnlen(r->date, 31));
    put32(hdr + 74, (uint32_t)r->score);
    put32(hdr + 78, (uint32_t)r->floor);
    put32(hdr + 82, (uint32_t)r->combo);
    put32(hdr + 86, (uint32_t)(r->rejump ? 1 : 0));
    put32(hdr + 90, r->seed);
    put32(hdr + 94, calc_hash(hdr, r->macros, r->count));
}

int replay_save(const Replay *r, const char *path)
{
    uint8_t hdr[HEADER_SIZE];
    build_header(r, hdr);
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    int ok = fwrite(hdr, 1, HEADER_SIZE, f) == HEADER_SIZE;
    for (int i = 0; ok && i < r->count; i++) {
        uint8_t m[5];
        put32(m, (uint32_t)r->macros[i].frames);
        m[4] = r->macros[i].keys;
        ok = fwrite(m, 1, 5, f) == 5;
    }
    if (fclose(f) != 0) ok = 0;
    return ok ? 0 : -1;
}

int replay_load(Replay *r, const char *path)
{
    memset(r, 0, sizeof(*r));
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t hdr[HEADER_SIZE];
    if (size < HEADER_SIZE || fread(hdr, 1, HEADER_SIZE, f) != HEADER_SIZE || memcmp(hdr, "ITR130", 6) != 0) {
        fclose(f);
        return -1;
    }
    int count = (int)get32(hdr + 6);
    if (count < 0 || (long)count * 5 > size - HEADER_SIZE) {
        fclose(f);
        return -1;
    }
    r->macros = malloc((size_t)(count ? count : 1) * sizeof(ReplayMacro));
    if (!r->macros) {
        fclose(f);
        return -1;
    }
    for (int i = 0; i < count; i++) {
        uint8_t m[5];
        if (fread(m, 1, 5, f) != 5) {
            fclose(f);
            replay_free(r);
            return -1;
        }
        r->macros[i].frames = (int)get32(m);
        r->macros[i].keys = m[4];
    }
    fclose(f);
    r->count = r->cap = count;
    if (calc_hash(hdr, r->macros, count) != get32(hdr + 94)) {
        replay_free(r);
        return -1;
    }
    memcpy(r->name, hdr + 10, 31);
    r->name[31] = 0;
    memcpy(r->date, hdr + 42, 31);
    r->date[31] = 0;
    r->score = (int)get32(hdr + 74);
    r->floor = (int)get32(hdr + 78);
    r->combo = (int)get32(hdr + 82);
    r->rejump = get32(hdr + 86) != 0;
    r->seed = get32(hdr + 90);
    return 0;
}

void replay_cursor_init(ReplayCursor *c, const Replay *r)
{
    c->r = r;
    c->index = 0;
    c->repeat = 0;
    c->last = -1;
}

int replay_cursor_next(ReplayCursor *c)
{
    const Replay *r = c->r;
    if (c->index >= r->count) return -1;
    const ReplayMacro *m = &r->macros[c->index];
    if (m->keys & REPLAY_END) return -1;
    int keys = m->keys;
    c->last = c->index;
    if (++c->repeat > m->frames) {
        c->index++;
        c->repeat = 0;
    }
    return keys;
}

int replay_validate(const Replay *r, int *score, int *floor, int *combo)
{
    Core core;
    ReplayCursor cur;
    core_init(&core, r->rejump, r->seed);
    replay_cursor_init(&cur, r);
    int alive = 1, keys;
    while (alive && (keys = replay_cursor_next(&cur)) >= 0) alive = core_frame(&core, keys);
    if (score) *score = core.score + core.floor * 10;
    if (floor) *floor = core.floor;
    if (combo) *combo = core.combo;
    if (alive) return 0;
    /* Harold must fall on the very last recorded frame. */
    int next = cur.last + 1;
    return next < r->count && (r->macros[next].keys & REPLAY_END);
}
