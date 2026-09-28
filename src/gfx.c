#include "gfx.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

Bitmap *bmp_create(int w, int h)
{
    Bitmap *b = calloc(1, sizeof(Bitmap));
    if (!b) return NULL;
    b->w = w;
    b->h = h;
    b->px = calloc((size_t)(w > 0 ? w : 1) * (size_t)(h > 0 ? h : 1), 1);
    if (!b->px) {
        free(b);
        return NULL;
    }
    bmp_reset_clip(b);
    return b;
}

void bmp_free(Bitmap *b)
{
    if (!b) return;
    free(b->px);
    free(b);
}

void bmp_clear(Bitmap *b, uint8_t c) { memset(b->px, c, (size_t)b->w * (size_t)b->h); }

void bmp_reset_clip(Bitmap *b)
{
    b->cl = b->ct = 0;
    b->cr = b->w;
    b->cb = b->h;
}

void bmp_set_clip(Bitmap *b, int x, int y, int w, int h)
{
    b->cl = x < 0 ? 0 : x;
    b->ct = y < 0 ? 0 : y;
    b->cr = x + w > b->w ? b->w : x + w;
    b->cb = y + h > b->h ? b->h : y + h;
}

/* Clips a w*h rectangle drawn at (*x, *y) against dst, adjusting the source
 * offset. Returns 0 when nothing is left. */
static int clip(const Bitmap *dst, int *sx, int *sy, int *x, int *y, int *w, int *h)
{
    if (*x < dst->cl) {
        *sx += dst->cl - *x;
        *w -= dst->cl - *x;
        *x = dst->cl;
    }
    if (*y < dst->ct) {
        *sy += dst->ct - *y;
        *h -= dst->ct - *y;
        *y = dst->ct;
    }
    if (*x + *w > dst->cr) *w = dst->cr - *x;
    if (*y + *h > dst->cb) *h = dst->cb - *y;
    return *w > 0 && *h > 0;
}

void gfx_blit(const Bitmap *src, Bitmap *dst, int sx, int sy, int dx, int dy, int w, int h)
{
    /* clip against the source first */
    if (sx < 0) {
        dx -= sx;
        w += sx;
        sx = 0;
    }
    if (sy < 0) {
        dy -= sy;
        h += sy;
        sy = 0;
    }
    if (sx + w > src->w) w = src->w - sx;
    if (sy + h > src->h) h = src->h - sy;
    if (w <= 0 || h <= 0 || !clip(dst, &sx, &sy, &dx, &dy, &w, &h)) return;
    for (int j = 0; j < h; j++)
        memcpy(dst->px + (size_t)(dy + j) * dst->w + dx, src->px + (size_t)(sy + j) * src->w + sx, (size_t)w);
}

void gfx_sprite_region(Bitmap *dst, const Bitmap *src, int sx, int sy, int w, int h, int x, int y)
{
    if (sx < 0) {
        x -= sx;
        w += sx;
        sx = 0;
    }
    if (sy < 0) {
        y -= sy;
        h += sy;
        sy = 0;
    }
    if (sx + w > src->w) w = src->w - sx;
    if (sy + h > src->h) h = src->h - sy;
    if (w <= 0 || h <= 0 || !clip(dst, &sx, &sy, &x, &y, &w, &h)) return;
    for (int j = 0; j < h; j++) {
        const uint8_t *s = src->px + (size_t)(sy + j) * src->w + sx;
        uint8_t *d = dst->px + (size_t)(y + j) * dst->w + x;
        for (int i = 0; i < w; i++)
            if (s[i]) d[i] = s[i];
    }
}

void gfx_sprite(Bitmap *dst, const Bitmap *src, int x, int y)
{
    gfx_sprite_region(dst, src, 0, 0, src->w, src->h, x, y);
}

void gfx_sprite_hflip(Bitmap *dst, const Bitmap *src, int x, int y)
{
    int w = src->w, h = src->h;
    for (int j = 0; j < h; j++) {
        int yy = y + j;
        if (yy < dst->ct || yy >= dst->cb) continue;
        const uint8_t *s = src->px + (size_t)j * src->w;
        uint8_t *d = dst->px + (size_t)yy * dst->w;
        for (int i = 0; i < w; i++) {
            int xx = x + w - 1 - i;
            if (xx < dst->cl || xx >= dst->cr) continue;
            if (s[i]) d[xx] = s[i];
        }
    }
}

void gfx_sprite_mapped(Bitmap *dst, const Bitmap *src, int x, int y, const uint8_t *map)
{
    int sx = 0, sy = 0, w = src->w, h = src->h;
    if (!clip(dst, &sx, &sy, &x, &y, &w, &h)) return;
    for (int j = 0; j < h; j++) {
        const uint8_t *s = src->px + (size_t)(sy + j) * src->w + sx;
        uint8_t *d = dst->px + (size_t)(y + j) * dst->w + x;
        for (int i = 0; i < w; i++)
            if (s[i]) d[i] = map[s[i]];
    }
}

void gfx_sprite_color(Bitmap *dst, const Bitmap *src, int x, int y, uint8_t color)
{
    int sx = 0, sy = 0, w = src->w, h = src->h;
    if (!clip(dst, &sx, &sy, &x, &y, &w, &h)) return;
    for (int j = 0; j < h; j++) {
        const uint8_t *s = src->px + (size_t)(sy + j) * src->w + sx;
        uint8_t *d = dst->px + (size_t)(y + j) * dst->w + x;
        for (int i = 0; i < w; i++)
            if (s[i]) d[i] = color;
    }
}

static void stretch(const Bitmap *src, Bitmap *dst, int x, int y, int w, int h, int masked)
{
    if (w <= 0 || h <= 0) return;
    int x0 = x < dst->cl ? dst->cl : x, x1 = x + w > dst->cr ? dst->cr : x + w;
    int y0 = y < dst->ct ? dst->ct : y, y1 = y + h > dst->cb ? dst->cb : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t xs = (uint32_t)(((uint64_t)src->w << 16) / (uint32_t)w);
    uint32_t ys = (uint32_t)(((uint64_t)src->h << 16) / (uint32_t)h);
    for (int yy = y0; yy < y1; yy++) {
        int sy = (int)(((uint32_t)(yy - y) * ys) >> 16);
        const uint8_t *s = src->px + (size_t)sy * src->w;
        uint8_t *d = dst->px + (size_t)yy * dst->w;
        uint32_t u = (uint32_t)(x0 - x) * xs;
        for (int xx = x0; xx < x1; xx++, u += xs) {
            uint8_t c = s[u >> 16];
            if (c || !masked) d[xx] = c;
        }
    }
}

void gfx_stretch_sprite(Bitmap *dst, const Bitmap *src, int x, int y, int w, int h)
{
    stretch(src, dst, x, y, w, h, 1);
}

void gfx_stretch_blit(const Bitmap *src, Bitmap *dst, int x, int y, int w, int h)
{
    stretch(src, dst, x, y, w, h, 0);
}

void gfx_rotate_scaled(Bitmap *dst, const Bitmap *src, double cx, double cy, double angle, double scale)
{
    if (scale <= 0.0001) return;
    double a = angle * (2.0 * M_PI / 256.0);
    double c = cos(a), s = sin(a);
    double hw = src->w * scale * 0.5, hh = src->h * scale * 0.5;
    double ex = fabs(hw * c) + fabs(hh * s), ey = fabs(hw * s) + fabs(hh * c);
    int x0 = (int)floor(cx - ex), x1 = (int)ceil(cx + ex);
    int y0 = (int)floor(cy - ey), y1 = (int)ceil(cy + ey);
    if (x0 < dst->cl) x0 = dst->cl;
    if (y0 < dst->ct) y0 = dst->ct;
    if (x1 > dst->cr) x1 = dst->cr;
    if (y1 > dst->cb) y1 = dst->cb;
    if (x0 >= x1 || y0 >= y1) return;

    /* Inverse mapping in 16.16 fixed point: for each destination pixel
     * centre find the source texel. */
    double inv = 1.0 / scale;
    int32_t du = (int32_t)(c * inv * 65536.0), dv = (int32_t)(-s * inv * 65536.0);
    int32_t sw = src->w << 16, sh = src->h << 16;
    for (int y = y0; y < y1; y++) {
        double rx = x0 + 0.5 - cx, ry = y + 0.5 - cy;
        int32_t u = (int32_t)(((rx * c + ry * s) * inv + src->w * 0.5) * 65536.0);
        int32_t v = (int32_t)(((-rx * s + ry * c) * inv + src->h * 0.5) * 65536.0);
        uint8_t *d = dst->px + (size_t)y * dst->w;
        for (int x = x0; x < x1; x++, u += du, v += dv) {
            if (u < 0 || v < 0 || u >= sw || v >= sh) continue;
            uint8_t p = src->px[(size_t)(v >> 16) * src->w + (u >> 16)];
            if (p) d[x] = p;
        }
    }
}

void gfx_fill(Bitmap *dst, int x, int y, int w, int h, uint8_t c)
{
    int sx = 0, sy = 0;
    if (!clip(dst, &sx, &sy, &x, &y, &w, &h)) return;
    for (int j = 0; j < h; j++) memset(dst->px + (size_t)(y + j) * dst->w + x, c, (size_t)w);
}

void gfx_remap_rect(Bitmap *dst, int x, int y, int w, int h, const uint8_t *map)
{
    int sx = 0, sy = 0;
    if (!clip(dst, &sx, &sy, &x, &y, &w, &h)) return;
    for (int j = 0; j < h; j++) {
        uint8_t *d = dst->px + (size_t)(y + j) * dst->w + x;
        for (int i = 0; i < w; i++) d[i] = map[d[i]];
    }
}

void gfx_grid(Bitmap *dst, uint8_t c)
{
    for (int y = 0; y < dst->h; y++) {
        uint8_t *d = dst->px + (size_t)y * dst->w;
        if ((y & 1) == 0) {
            memset(d, c, (size_t)dst->w);
        } else {
            for (int x = 0; x < dst->w; x += 2) d[x] = c;
        }
    }
}

int pal_nearest(const Palette pal, int r, int g, int b)
{
    int best = 0, bestd = 1 << 30;
    for (int i = 1; i < 256; i++) {
        int dr = pal[i].r - r, dg = pal[i].g - g, db = pal[i].b - b;
        int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
        if (d < bestd) {
            bestd = d;
            best = i;
        }
    }
    return best;
}

void pal_make_darken(const Palette pal, double f, uint8_t *map)
{
    for (int i = 0; i < 256; i++)
        map[i] = (uint8_t)pal_nearest(pal, (int)(pal[i].r * f), (int)(pal[i].g * f), (int)(pal[i].b * f));
    map[0] = 0;
}

/* Allegro 4 bestfit_color on the 6-bit palette. */
static int bestfit6(const int (*p6)[3], int r, int g, int b)
{
    int best = 0, lowest = 1 << 30;
    for (int i = 1; i < 256; i++) {
        int dg = p6[i][1] - g, dr = p6[i][0] - r, db = p6[i][2] - b;
        int d = dg * dg * 59 * 59 + dr * dr * 30 * 30 + db * db * 11 * 11;
        if (d < lowest) {
            lowest = d;
            best = i;
            if (d == 0) break;
        }
    }
    return best;
}

void pal_make_light(const Palette pal, int r, int g, int b, int level, uint8_t *map)
{
    int p6[256][3];
    for (int i = 0; i < 256; i++) {
        p6[i][0] = (pal[i].r * 63 + 127) / 255;
        p6[i][1] = (pal[i].g * 63 + 127) / 255;
        p6[i][2] = (pal[i].b * 63 + 127) / 255;
    }
    if (level >= 255) {
        for (int i = 0; i < 256; i++) map[i] = (uint8_t)i;
        return;
    }
    /* create_light_table() without an rgb_map */
    unsigned t1 = (unsigned)level * 0x010101u, t2 = 0xFFFFFFu - t1;
    unsigned r1 = (1u << 23) + (unsigned)r * t2, g1 = (1u << 23) + (unsigned)g * t2, b1 = (1u << 23) + (unsigned)b * t2;
    for (int i = 0; i < 256; i++) {
        int r2 = (int)((r1 + (unsigned)p6[i][0] * t1) >> 24);
        int g2 = (int)((g1 + (unsigned)p6[i][1] * t1) >> 24);
        int b2 = (int)((b1 + (unsigned)p6[i][2] * t1) >> 24);
        map[i] = (uint8_t)bestfit6((const int(*)[3])p6, r2, g2, b2);
    }
}
