#include "core.h"

#include <string.h>

/* The original was built with MinGW; the float-to-int casts truncate. */
#define FTOI(f) ((int)(long long)(f))

static void new_floor(Core *c)
{
    if (c->floor_pad < 4) {
        c->floor_pad++;
        return;
    }
    c->floor_pad = 0;

    int count = c->floor_count;
    CoreFloor *f = &c->floors[count % CORE_FLOOR_RING];
    if (count <= 1000 ? !(count % 50) : !(count % 500)) {
        f->start = 0;
        f->end = 40;
    } else {
        int length;
        if (count < 240) {
            length = 6 + core_rand(&c->seed) % (9 - count / 30);
        } else if (count < 600) {
            length = 6;
            core_rand(&c->seed);
        } else if (count < 1000) {
            length = 6;
        } else if (count < 1500) {
            length = 5;
        } else if (count < 2000) {
            length = 4;
        } else if (count < 10000) {
            length = 3;
        } else {
            length = 2;
        }
        f->start = 5 + core_rand(&c->seed) % (30 - length);
        f->end = f->start + length;
    }
    c->floor_count++;
    c->events |= EV_NEW_FLOOR;
}

void core_init(Core *c, int rejump, unsigned int seed)
{
    memset(c, 0, sizeof(*c));
    c->x = 200;
    c->y = 431;
    c->dx = 0.001;
    c->dy = 0;
    c->status = ST_IDLE;
    c->rejump = rejump;
    c->floor_pad = 2;
    c->seed = seed;
    for (int i = 0; i < 32; i++) new_floor(c);
    c->events = 0;
}

static int jump(Core *c)
{
    if (c->status != ST_IDLE) return 0;
    c->status = ST_FLY_UP;
    c->dy = c->dx * 2;
    if (c->dy > 0) c->dy = -c->dy;
    if (c->dy > -12.2) c->dy = -12.2;
    c->events |= EV_JUMP;
    c->jump_dy = c->dy;
    return 1;
}

static void handle_keys(Core *c, int keys)
{
    if (keys & CORE_KEY_LEFT) {
        if (c->dx > 0) c->dx *= 0.7;
        c->dx -= 0.3;
    } else if (keys & CORE_KEY_RIGHT) {
        if (c->dx < 0) c->dx *= 0.7;
        c->dx += 0.3;
    } else {
        c->dx *= 0.9;
    }

    if (c->rejump) {
        if (keys & CORE_KEY_JUMP) jump(c);
    } else {
        if (keys & CORE_KEY_JUMP) {
            if (!c->rejump_jumped && jump(c)) c->rejump_jumped = 1;
        } else {
            c->rejump_jumped = 0;
        }
    }
}

static void handle_pos(Core *c)
{
    if (c->dx > 12.2)
        c->dx = 12.2;
    else if (c->dx < -12.2)
        c->dx = -12.2;

    if (c->dy > 12.2)
        c->dy = 12.2;
    else if (c->dy < -100)
        c->dy = -100;

    c->x += c->dx;
    if (c->x > 555) {
        c->x = 555;
        c->dx *= -0.9;
    } else if (c->x < 85) {
        c->x = 85;
        c->dx *= -0.9;
    }

    c->y += c->dy;
    if (c->y > 1000) c->y = 1000;

    if (c->status != ST_IDLE) {
        c->dy += 0.8;
        if (c->status == ST_FLY_UP && c->dy > 0) c->status = ST_FLY_IDLE;
    }
}

static int floor_from_y(const Core *c, int y, int *floor_y, int *xl, int *xr, int *level)
{
    if (y < -33 || y > 478) return 0;
    int y_relative = y + 33 + 80 - c->floor_pad * 16;
    if (y_relative % 80 > 15) return 0;
    int f = y_relative / 80;
    int lvl = c->floor_count - f;
    CoreFloor zero = {0, 0};
    const CoreFloor *fl = lvl >= 0 ? core_floor(c, lvl) : &zero;
    *floor_y = (y + 1) - (y + 1) % 16 + c->screen_y % 16;
    *xl = fl->start * 16 - 2;
    *xr = (fl->end + 1) * 16 + 1;
    *level = lvl;
    return 1;
}

static int line_intersection(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, int *xi, int *yi)
{
    int denominator = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);
    if (denominator == 0) return 0;
    double mul = (double)1 / denominator;
    double ua = (double)((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) * mul;
    if (ua < 0 || ua > 1) return 0;
    double ub = (double)((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) * mul;
    if (ub < 0 || ub > 1) return 0;
    *xi = x1 + FTOI(ua * (x2 - x1) + 0.5);
    *yi = y1 + FTOI(ua * (y2 - y1) + 0.5);
    return 1;
}

static void handle_collision(Core *c, int prev_x, int prev_y)
{
    int floor_y, floor_xl, floor_xr, floor_level;
    int col_x1, col_y1, col_x2, col_y2;

    if (c->status == ST_FLY_UP) return;

    if (!floor_from_y(c, FTOI(c->y), &floor_y, &floor_xl, &floor_xr, &floor_level) &&
        !floor_from_y(c, prev_y, &floor_y, &floor_xl, &floor_xr, &floor_level)) {
        if (c->status == ST_FLY_IDLE || c->status == ST_IDLE) c->status = ST_FLY_DOWN;
        return;
    }

    int check_1 = line_intersection(floor_xl, floor_y, floor_xr, floor_y, FTOI(c->x) - 11, FTOI(c->y) + 1,
                                    prev_x - 11, prev_y, &col_x1, &col_y1);
    if (!check_1) {
        int check_2 = line_intersection(floor_xl, floor_y, floor_xr, floor_y, FTOI(c->x) + 11, FTOI(c->y) + 1,
                                        prev_x + 11, prev_y, &col_x2, &col_y2);
        if (!check_2) {
            if (c->status == ST_FLY_IDLE || c->status == ST_IDLE) c->status = ST_FLY_DOWN;
            return;
        }
    }

    if (c->status == ST_FLY_IDLE || c->status == ST_FLY_DOWN) {
        c->status = ST_IDLE;
        c->y = floor_y - 1;
        c->dy = 0;
        if (check_1)
            c->x = col_x1 + 11;
        else
            c->x = col_x2 - 11;
        c->events |= EV_LAND;

        if (floor_level - c->floor > 1) {
            if (c->combo_timer) {
                c->combo_floor += floor_level - c->floor;
                c->combo_count++;
            } else {
                c->combo_floor = floor_level - c->floor;
                c->combo_count = 1;
            }
            c->combo_timer = 100;
        } else if (floor_level != c->floor && c->combo_timer) {
            c->combo_timer = 1;
        }
        c->floor = floor_level;
    }
}

int core_frame(Core *c, int keys)
{
    c->events = 0;
    int prev_x = FTOI(c->x);
    int prev_y = FTOI(c->y);

    handle_keys(c, keys);
    handle_pos(c);

    int screen_move;
    if (c->y < 160) {
        if (c->y < 0)
            screen_move = 13;
        else if (c->y < 20)
            screen_move = 10;
        else if (c->y < 40)
            screen_move = 8;
        else if (c->y < 60)
            screen_move = 6;
        else if (c->y < 80)
            screen_move = 5;
        else if (c->y < 100)
            screen_move = 4;
        else if (c->y < 120)
            screen_move = 3;
        else if (c->y < 140)
            screen_move = 2;
        else
            screen_move = 1;
    } else {
        screen_move = 0;
    }
    if (c->frozen) screen_move = 0;

    if (!c->frozen && c->screen_y + screen_move > 100) {
        if (c->speed > 0)
            screen_move += c->speed;
        else if (!c->zero_speed_skip)
            screen_move++;

        if (c->speed < 5) {
            c->speed_counter++;
            if (c->speed_counter > 1500) {
                c->speed++;
                c->speed_counter -= 1500;
                c->events |= EV_SPEEDUP;
            }
        }
    }

    c->zero_speed_skip = !c->zero_speed_skip;

    if (screen_move) {
        c->screen_y += screen_move;
        c->y += screen_move;
        prev_y += screen_move;
        if (c->screen_y % 16 < (c->screen_y - screen_move) % 16 || screen_move >= 16) new_floor(c);
    }

    if (c->combo_timer) {
        c->combo_timer--;
        if (c->combo_timer == 0 && c->combo_count > 1) {
            c->score += c->combo_floor * c->combo_floor;
            if (c->combo_floor > c->combo) c->combo = c->combo_floor;
            c->last_combo = c->combo_floor;
            c->events |= EV_COMBO_END;
        }
    }

    handle_collision(c, prev_x, prev_y);

    return c->y <= 540;
}

int core_floor_surface(const Core *c, int level)
{
    int f = c->floor_count - level;
    return 80 * f - 112 + 16 * c->floor_pad + c->screen_y % 16;
}

int core_feet_on_floor(const Core *c)
{
    int floor_y, xl, xr, level, xi, yi;
    int x = FTOI(c->x), y = FTOI(c->y);
    if (c->status != ST_IDLE) return 0;
    if (!floor_from_y(c, y, &floor_y, &xl, &xr, &level)) return 0;
    int feet = 0;
    if (line_intersection(xl, floor_y, xr, floor_y, x - 11, y + 1, x - 11, y, &xi, &yi)) feet |= 1;
    if (line_intersection(xl, floor_y, xr, floor_y, x + 11, y + 1, x + 11, y, &xi, &yi)) feet |= 2;
    return feet;
}
