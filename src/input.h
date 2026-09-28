/* Buttons, sampled once per 50 Hz game tick. */
#pragma once

#include <stdint.h>

/* CONFIRM/CANCEL follow the console's enter-button setting. */
enum {
    BTN_UP = 1 << 0,
    BTN_DOWN = 1 << 1,
    BTN_LEFT = 1 << 2,
    BTN_RIGHT = 1 << 3,
    BTN_CONFIRM = 1 << 4,
    BTN_CANCEL = 1 << 5,
    BTN_SQUARE = 1 << 6,
    BTN_TRIANGLE = 1 << 7,
    BTN_L = 1 << 8,
    BTN_R = 1 << 9,
    BTN_START = 1 << 10,
    BTN_SELECT = 1 << 11,
    /* the physical buttons, whatever the region's confirm button is */
    BTN_CROSS = 1 << 12,
    BTN_CIRCLE = 1 << 13,
};
#define BTN_COUNT 14
#define BTN_DIRS (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)
/* In play cross, square and triangle jump and circle is the escape key. */
#define BTN_JUMP (BTN_CROSS | BTN_SQUARE | BTN_TRIANGLE)

typedef struct {
    uint32_t held;
    uint32_t pressed;  /* went down this tick */
    uint32_t released; /* went up this tick */
    uint32_t repeat;   /* pressed, plus auto-repeat for the d-pad */
    int hold_ticks[BTN_COUNT];
    int quit;
    char shot[256]; /* scripted runs: save a screenshot after this tick */
#ifndef __vita__
    char dshot[256];  /* scripted runs: save the 960x544 display (as on the Vita) */
    char record[256]; /* scripted runs: append display frames to this raw file, "off" stops */
#endif
} Input;

void input_init(void);
/* Pumps OS events. Call once per displayed frame. */
void input_pump(Input *in);
/* Samples the buttons for one game tick. */
void input_tick(Input *in);
int input_headless(void);
/* Swallows the current presses (used when a screen changes). */
void input_flush(Input *in);
