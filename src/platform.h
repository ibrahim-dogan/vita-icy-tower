#pragma once

void platform_init(void);
/* Folder holding the user's copy of the original game (data/, characters/). */
const char *game_root(void);
/* Joins game_root() and a relative path into a rotating static buffer. */
const char *game_path(const char *rel);
/* Where settings and highscores are kept. */
const char *config_path(void);
/* 1 if the system confirm button is circle (Japanese consoles). */
int platform_enter_is_circle(void);
void platform_mkdir(const char *path);
