/* Deterministic Icy Tower 1.3.1 engine.
 *
 * This is the part of the game that decides where Harold is, which floors
 * exist and how many points a combo is worth. It follows the reverse
 * engineered engine from RaMMicHaeL's replay_checker (ramensoftware.com)
 * operation for operation, so that replays recorded with the original game
 * play back identically here and vice versa. Everything visual (sounds,
 * stars, animation) lives in the gameplay scene and only reads this state. */
#pragma once

#include <stdint.h>

#define CORE_KEY_LEFT 0x01
#define CORE_KEY_RIGHT 0x02
#define CORE_KEY_JUMP 0x10

enum {
    ST_IDLE = 0,     /* standing on a floor */
    ST_FLY_UP = 1,   /* rising after a jump */
    ST_FLY_IDLE = 2, /* past the top of the jump */
    ST_FLY_DOWN = 3, /* falling with no floor under him */
};

/* Things that happened during one core frame, for sounds and effects. */
enum {
    EV_JUMP = 1 << 0,       /* core.jump_dy holds the launch speed */
    EV_LAND = 1 << 1,
    EV_COMBO_END = 1 << 2,  /* core.last_combo holds the combo size */
    EV_SPEEDUP = 1 << 3,
    EV_NEW_FLOOR = 1 << 4,
};

#define CORE_FLOOR_RING 16

typedef struct {
    int start, end; /* in 16 px tiles */
} CoreFloor;

typedef struct {
    double x, y, dx, dy;
    int status;
    int screen_y;
    int speed, speed_counter, zero_speed_skip;
    int rejump, rejump_jumped;
    int combo_timer, combo_count, combo_floor;
    int score, floor, combo;

    /* floor generator */
    CoreFloor floors[CORE_FLOOR_RING]; /* indexed by level % CORE_FLOOR_RING */
    int floor_count;                   /* floors generated so far (levels 0..count-1) */
    int floor_pad;
    unsigned int seed;

    /* when set the screen stops scrolling (after death) */
    int frozen;

    /* per frame outputs */
    int events;
    double jump_dy;
    int last_combo;
} Core;

void core_init(Core *c, int rejump, unsigned int seed);
/* Advances one 50 Hz frame. Returns 1 while Harold is still on screen. */
int core_frame(Core *c, int keys);

/* Screen y of the walking surface of floor `level`. */
int core_floor_surface(const Core *c, int level);
/* Highest level that has been generated. */
static inline int core_top_level(const Core *c) { return c->floor_count - 1; }
static inline const CoreFloor *core_floor(const Core *c, int level)
{
    return &c->floors[level % CORE_FLOOR_RING];
}

/* Left/right intersection tests against the floor under Harold, used for
 * the edge balancing animation. Returns bit 1 for left foot, 2 for right. */
int core_feet_on_floor(const Core *c);

/* The MSVC rand() used for the floor layout. */
static inline int core_rand(unsigned int *seed)
{
    return (int)(((*seed = *seed * 214013u + 2531011u) >> 16) & 0x7FFF);
}
