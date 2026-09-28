/* 8-bit BMP and PCX loading (custom character frame sheets). */
#pragma once

#include "gfx.h"

/* Loads a 256 colour image. Returns NULL for other formats. */
Bitmap *img_load_8bit(const char *path, Palette pal);
