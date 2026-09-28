#include "input.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

#ifdef __vita__
#include <psp2/ctrl.h>
#endif

#define REPEAT_DELAY 18 /* ticks */
#define REPEAT_RATE 4

static int g_enter_circle;

#ifndef __vita__
/* ---- scripted input for headless test runs ---------------------------- */
/* Commands, one per line:
 *   wait N          do nothing for N ticks
 *   press BTN [N]   hold BTN for N ticks (default 1), then release
 *   down BTN / up BTN   start/stop holding BTN until told otherwise
 *   shot FILE       save a screenshot of the next frame (640x480 game picture)
 *   dshot FILE      save the next frame as shown on the Vita (960x544 PNG)
 *   record FILE     append every displayed frame to FILE (raw RGB24 960x544)
 *   record off      stop recording
 *   quit */
static FILE *g_script;
static int g_wait;
static uint32_t g_hold, g_press;
static int g_press_ticks;
static SDL_GameController *g_pad;

int input_headless(void) { return g_script != NULL; }

static uint32_t btn_by_name(const char *n)
{
    static const struct {
        const char *n;
        uint32_t b;
    } map[] = {
        {"UP", BTN_UP},       {"DOWN", BTN_DOWN},         {"LEFT", BTN_LEFT},     {"RIGHT", BTN_RIGHT},
        {"CROSS", BTN_CONFIRM}, {"CONFIRM", BTN_CONFIRM}, {"CIRCLE", BTN_CANCEL}, {"CANCEL", BTN_CANCEL},
        {"SQUARE", BTN_SQUARE}, {"TRIANGLE", BTN_TRIANGLE}, {"L", BTN_L},          {"R", BTN_R},
        {"START", BTN_START}, {"SELECT", BTN_SELECT},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (!strcmp(map[i].n, n)) return map[i].b;
    fprintf(stderr, "script: unknown button %s\n", n);
    return 0;
}

static uint32_t script_tick(Input *in)
{
    if (g_press_ticks > 0) {
        g_press_ticks--;
        return g_hold | g_press;
    }
    g_press = 0;
    if (g_wait > 0) {
        g_wait--;
        return g_hold;
    }
    char line[512];
    while (fgets(line, sizeof(line), g_script)) {
        char cmd[32] = {0}, arg[256] = {0};
        int n = 1;
        if (line[0] == '#' || sscanf(line, "%31s", cmd) != 1) continue;
        if (!strcmp(cmd, "wait") && sscanf(line, "%*s %d", &n) == 1) {
            g_wait = n - 1;
            return g_hold;
        } else if (!strcmp(cmd, "press") && sscanf(line, "%*s %255s %d", arg, &n) >= 1) {
            g_press = btn_by_name(arg);
            g_press_ticks = n - 1;
            g_wait = 1; /* one released tick so presses don't merge */
            return g_hold | g_press;
        } else if (!strcmp(cmd, "down") && sscanf(line, "%*s %255s", arg) == 1) {
            g_hold |= btn_by_name(arg);
        } else if (!strcmp(cmd, "up") && sscanf(line, "%*s %255s", arg) == 1) {
            g_hold &= ~btn_by_name(arg);
        } else if (!strcmp(cmd, "shot") && sscanf(line, "%*s %255s", arg) == 1) {
            snprintf(in->shot, sizeof(in->shot), "%s", arg);
            return g_hold;
        } else if (!strcmp(cmd, "dshot") && sscanf(line, "%*s %255s", arg) == 1) {
            snprintf(in->dshot, sizeof(in->dshot), "%s", arg);
            return g_hold;
        } else if (!strcmp(cmd, "record") && sscanf(line, "%*s %255s", arg) == 1) {
            snprintf(in->record, sizeof(in->record), "%s", arg);
        } else if (!strcmp(cmd, "quit")) {
            in->quit = 1;
            return 0;
        }
    }
    in->quit = 1;
    return 0;
}
#else
int input_headless(void) { return 0; }
#endif

void input_init(void)
{
    g_enter_circle = platform_enter_is_circle();
#ifndef __vita__
    const char *path = getenv("ICYTOWER_SCRIPT");
    if (path) {
        g_script = fopen(path, "r");
        if (!g_script) fprintf(stderr, "cannot open script %s\n", path);
    }
#endif
}

void input_pump(Input *in)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) in->quit = 1;
#ifndef __vita__
        if (ev.type == SDL_CONTROLLERDEVICEADDED && !g_pad) g_pad = SDL_GameControllerOpen(ev.cdevice.which);
#endif
    }
}

static uint32_t read_raw(Input *in)
{
    uint32_t raw = 0;
#ifdef __vita__
    (void)in;
    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    sceCtrlPeekBufferPositive(0, &pad, 1);
    uint32_t confirm = g_enter_circle ? SCE_CTRL_CIRCLE : SCE_CTRL_CROSS;
    uint32_t cancel = g_enter_circle ? SCE_CTRL_CROSS : SCE_CTRL_CIRCLE;
    if (pad.buttons & SCE_CTRL_UP) raw |= BTN_UP;
    if (pad.buttons & SCE_CTRL_DOWN) raw |= BTN_DOWN;
    if (pad.buttons & SCE_CTRL_LEFT) raw |= BTN_LEFT;
    if (pad.buttons & SCE_CTRL_RIGHT) raw |= BTN_RIGHT;
    if (pad.buttons & confirm) raw |= BTN_CONFIRM;
    if (pad.buttons & cancel) raw |= BTN_CANCEL;
    if (pad.buttons & SCE_CTRL_CROSS) raw |= BTN_CROSS;
    if (pad.buttons & SCE_CTRL_CIRCLE) raw |= BTN_CIRCLE;
    if (pad.buttons & SCE_CTRL_SQUARE) raw |= BTN_SQUARE;
    if (pad.buttons & SCE_CTRL_TRIANGLE) raw |= BTN_TRIANGLE;
    if (pad.buttons & SCE_CTRL_LTRIGGER) raw |= BTN_L;
    if (pad.buttons & SCE_CTRL_RTRIGGER) raw |= BTN_R;
    if (pad.buttons & SCE_CTRL_START) raw |= BTN_START;
    if (pad.buttons & SCE_CTRL_SELECT) raw |= BTN_SELECT;
    if (pad.lx < 128 - 56) raw |= BTN_LEFT;
    if (pad.lx > 128 + 56) raw |= BTN_RIGHT;
    if (pad.ly < 128 - 56) raw |= BTN_UP;
    if (pad.ly > 128 + 56) raw |= BTN_DOWN;
#else
    if (g_script) {
        raw = script_tick(in);
        if (raw & BTN_CONFIRM) raw |= BTN_CROSS;
        if (raw & BTN_CANCEL) raw |= BTN_CIRCLE;
        return raw;
    }
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    if (k[SDL_SCANCODE_UP]) raw |= BTN_UP;
    if (k[SDL_SCANCODE_DOWN]) raw |= BTN_DOWN;
    if (k[SDL_SCANCODE_LEFT]) raw |= BTN_LEFT;
    if (k[SDL_SCANCODE_RIGHT]) raw |= BTN_RIGHT;
    if (k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_Z]) raw |= BTN_CONFIRM | BTN_CROSS;
    if (k[SDL_SCANCODE_ESCAPE] || k[SDL_SCANCODE_X]) raw |= BTN_CANCEL | BTN_CIRCLE;
    if (k[SDL_SCANCODE_A]) raw |= BTN_SQUARE;
    if (k[SDL_SCANCODE_S]) raw |= BTN_TRIANGLE;
    if (k[SDL_SCANCODE_Q]) raw |= BTN_L;
    if (k[SDL_SCANCODE_W]) raw |= BTN_R;
    if (k[SDL_SCANCODE_P]) raw |= BTN_START;
    if (k[SDL_SCANCODE_TAB]) raw |= BTN_SELECT;
    if (g_pad) {
        static const struct {
            SDL_GameControllerButton b;
            uint32_t m;
        } map[] = {
            {SDL_CONTROLLER_BUTTON_DPAD_UP, BTN_UP},       {SDL_CONTROLLER_BUTTON_DPAD_DOWN, BTN_DOWN},
            {SDL_CONTROLLER_BUTTON_DPAD_LEFT, BTN_LEFT},   {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, BTN_RIGHT},
            {SDL_CONTROLLER_BUTTON_A, BTN_CONFIRM | BTN_CROSS}, {SDL_CONTROLLER_BUTTON_B, BTN_CANCEL | BTN_CIRCLE},
            {SDL_CONTROLLER_BUTTON_X, BTN_SQUARE},         {SDL_CONTROLLER_BUTTON_Y, BTN_TRIANGLE},
            {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, BTN_L},   {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, BTN_R},
            {SDL_CONTROLLER_BUTTON_START, BTN_START},      {SDL_CONTROLLER_BUTTON_BACK, BTN_SELECT},
        };
        for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
            if (SDL_GameControllerGetButton(g_pad, map[i].b)) raw |= map[i].m;
        int lx = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX);
        int ly = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY);
        if (lx < -14000) raw |= BTN_LEFT;
        if (lx > 14000) raw |= BTN_RIGHT;
        if (ly < -14000) raw |= BTN_UP;
        if (ly > 14000) raw |= BTN_DOWN;
    }
#endif
    return raw;
}

void input_tick(Input *in)
{
    in->shot[0] = 0;
#ifndef __vita__
    in->dshot[0] = 0;
#endif
    uint32_t raw = read_raw(in);
    in->pressed = raw & ~in->held;
    in->released = in->held & ~raw;
    in->repeat = in->pressed;
    for (int i = 0; i < BTN_COUNT; i++) {
        uint32_t b = 1u << i;
        if (!(raw & b)) {
            in->hold_ticks[i] = 0;
            continue;
        }
        int t = ++in->hold_ticks[i];
        if ((b & BTN_DIRS) && t > REPEAT_DELAY && (t - REPEAT_DELAY) % REPEAT_RATE == 0) in->repeat |= b;
    }
    in->held = raw;
}

void input_flush(Input *in)
{
    in->pressed = in->repeat = in->released = 0;
}
