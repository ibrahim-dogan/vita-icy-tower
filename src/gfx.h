/* 8-bit paletted software drawing, modelled on Allegro 4 in 256 colour mode.
 *
 * The whole game renders into a 640x480 index buffer like the original, and
 * only the finished frame is converted to RGB and handed to the GPU. Index 0
 * is the transparent colour for sprites, as in Allegro. */
#pragma once

#include <stdint.h>

#define SCREEN_W 640
#define SCREEN_H 480

typedef struct {
    int w, h;
    uint8_t *px;
    int cl, ct, cr, cb; /* clip rectangle, right/bottom exclusive */
} Bitmap;

typedef struct {
    uint8_t r, g, b;
} Rgb;

typedef Rgb Palette[256];

Bitmap *bmp_create(int w, int h);
void bmp_free(Bitmap *b);
void bmp_clear(Bitmap *b, uint8_t c);
void bmp_set_clip(Bitmap *b, int x, int y, int w, int h);
void bmp_reset_clip(Bitmap *b);

/* Opaque copy of a region. */
void gfx_blit(const Bitmap *src, Bitmap *dst, int sx, int sy, int dx, int dy, int w, int h);
/* Masked (index 0 transparent) draws. */
void gfx_sprite(Bitmap *dst, const Bitmap *src, int x, int y);
void gfx_sprite_hflip(Bitmap *dst, const Bitmap *src, int x, int y);
void gfx_sprite_region(Bitmap *dst, const Bitmap *src, int sx, int sy, int w, int h, int x, int y);
void gfx_stretch_sprite(Bitmap *dst, const Bitmap *src, int x, int y, int w, int h);
void gfx_stretch_blit(const Bitmap *src, Bitmap *dst, int x, int y, int w, int h);
/* Rotates `src` around its centre and draws that centre at (cx, cy).
 * angle uses Allegro units (256 = full turn, clockwise). */
void gfx_rotate_scaled(Bitmap *dst, const Bitmap *src, double cx, double cy, double angle, double scale);
/* Masked draw where every source index goes through `map` first. */
void gfx_sprite_mapped(Bitmap *dst, const Bitmap *src, int x, int y, const uint8_t *map);
/* Masked draw of every non-zero pixel in a single colour. */
void gfx_sprite_color(Bitmap *dst, const Bitmap *src, int x, int y, uint8_t color);

void gfx_fill(Bitmap *dst, int x, int y, int w, int h, uint8_t c);
/* Replaces every pixel of the rectangle by map[pixel]. */
void gfx_remap_rect(Bitmap *dst, int x, int y, int w, int h, const uint8_t *map);
/* Blacks out every other row and column, like the original pause screen. */
void gfx_grid(Bitmap *dst, uint8_t c);

/* Palette helpers. */
int pal_nearest(const Palette pal, int r, int g, int b);
/* map[i] = nearest palette entry to pal[i] scaled by f (0..1). */
void pal_make_darken(const Palette pal, double f, uint8_t *map);
/* Allegro create_light_table(): map[i] = palette colour i lit at `level`
 * (255 = unchanged) towards the 6-bit colour r,g,b. */
void pal_make_light(const Palette pal, int r, int g, int b, int level, uint8_t *map);
