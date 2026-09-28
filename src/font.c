#include "font.h"

#include <stdlib.h>
#include <string.h>

#include "datafile.h"

void font_free(Font *f)
{
    if (!f) return;
    for (int i = 0; i < f->count; i++) bmp_free(f->glyphs[i]);
    free(f->glyphs);
    free(f);
}

static int add_glyph(Font *f, Bitmap *g)
{
    Bitmap **grown = realloc(f->glyphs, (size_t)(f->count + 1) * sizeof(Bitmap *));
    if (!grown) return -1;
    f->glyphs = grown;
    f->glyphs[f->count++] = g;
    if (g->h > f->height) f->height = g->h;
    return 0;
}

Font *font_from_dat(const uint8_t *d, size_t size)
{
    Font *f = calloc(1, sizeof(Font));
    if (!f) return NULL;
    size_t p = 0;
#define NEED(n)                                                                                                        \
    do {                                                                                                               \
        if (p + (n) > size) goto fail;                                                                                 \
    } while (0)
    NEED(4);
    /* A leading height of 0 means the "new" unicode format with ranges. */
    if (dat_be16(d) != 0) goto fail;
    int ranges = dat_be16(d + 2);
    p = 4;
    for (int r = 0; r < ranges; r++) {
        NEED(9);
        int depth = d[p];
        int begin = dat_be32(d + p + 1), end = dat_be32(d + p + 5);
        p += 9;
        if (r == 0) f->first = begin;
        else if (begin != f->first + f->count) goto fail; /* only contiguous ranges */
        for (int c = begin; c <= end; c++) {
            NEED(4);
            int w = dat_be16(d + p), h = dat_be16(d + p + 2);
            p += 4;
            if (w < 0 || h < 0) goto fail;
            Bitmap *g = bmp_create(w, h);
            if (!g) goto fail;
            if (depth == 1 || depth == 255) {
                f->mono = 1;
                int stride = (w + 7) / 8;
                NEED((size_t)stride * h);
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < w; x++)
                        g->px[y * w + x] = (d[p + y * stride + x / 8] & (0x80 >> (x & 7))) ? 1 : 0;
                p += (size_t)stride * h;
            } else {
                NEED((size_t)w * h);
                memcpy(g->px, d + p, (size_t)w * h);
                p += (size_t)w * h;
            }
            if (add_glyph(f, g) != 0) {
                bmp_free(g);
                goto fail;
            }
        }
    }
#undef NEED
    return f;
fail:
    font_free(f);
    return NULL;
}

Font *font_from_8x8(const unsigned char table[128][8])
{
    Font *f = calloc(1, sizeof(Font));
    if (!f) return NULL;
    f->first = 32;
    f->mono = 1;
    for (int c = 32; c < 128; c++) {
        Bitmap *g = bmp_create(8, 8);
        if (!g) break;
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) g->px[y * 8 + x] = (table[c][y] >> x) & 1;
        add_glyph(f, g);
    }
    return f;
}

static const Bitmap *glyph(const Font *f, unsigned char c)
{
    int i = (int)c - f->first;
    if (i < 0 || i >= f->count) i = '^' - f->first;
    if (i < 0 || i >= f->count) return NULL;
    return f->glyphs[i];
}

int text_width(const Font *f, const char *s)
{
    int w = 0;
    for (; *s; s++) {
        const Bitmap *g = glyph(f, (unsigned char)*s);
        if (g) w += g->w;
    }
    return w;
}

static int aligned_x(const Font *f, const char *s, int x, int align)
{
    if (align == ALIGN_CENTER) return x - text_width(f, s) / 2;
    if (align == ALIGN_RIGHT) return x - text_width(f, s);
    return x;
}

void text_draw(Bitmap *dst, const Font *f, const char *s, int x, int y, int color, int align)
{
    x = aligned_x(f, s, x, align);
    for (; *s; s++) {
        const Bitmap *g = glyph(f, (unsigned char)*s);
        if (!g) continue;
        if (color >= 0 || f->mono)
            gfx_sprite_color(dst, g, x, y, (uint8_t)(color >= 0 ? color : 255));
        else
            gfx_sprite(dst, g, x, y);
        x += g->w;
    }
}

void text_draw_mapped(Bitmap *dst, const Font *f, const char *s, int x, int y, const uint8_t *map, int align)
{
    x = aligned_x(f, s, x, align);
    for (; *s; s++) {
        const Bitmap *g = glyph(f, (unsigned char)*s);
        if (!g) continue;
        gfx_sprite_mapped(dst, g, x, y, map);
        x += g->w;
    }
}
