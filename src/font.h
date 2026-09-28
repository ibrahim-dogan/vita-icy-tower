/* Allegro 4 FONT objects (colour and mono) and text drawing. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "gfx.h"

typedef struct {
    int first, count;
    Bitmap **glyphs; /* mono glyphs are stored as 0/1 index bitmaps */
    int height;
    int mono;
} Font;

enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

/* Parses a FONT datafile object. */
Font *font_from_dat(const uint8_t *data, size_t size);
/* Builds a font from an 8x8 1bpp table (for messages before the game data
 * is available). */
Font *font_from_8x8(const unsigned char table[128][8]);
void font_free(Font *f);

int text_width(const Font *f, const char *s);
/* color -1 draws colour glyphs with their own colours; any other value
 * paints every glyph pixel in that colour. */
void text_draw(Bitmap *dst, const Font *f, const char *s, int x, int y, int color, int align);
/* Draws glyphs through a palette remap table. */
void text_draw_mapped(Bitmap *dst, const Font *f, const char *s, int x, int y, const uint8_t *map, int align);
