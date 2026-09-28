#include "img.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = n > 0 ? malloc((size_t)n) : NULL;
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) {
        free(b);
        b = NULL;
    }
    fclose(f);
    *len = (size_t)n;
    return b;
}

static uint32_t le32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static int le16(const uint8_t *p) { return p[0] | (p[1] << 8); }

static Bitmap *load_bmp(const uint8_t *d, size_t n, Palette pal)
{
    if (n < 54) return NULL;
    uint32_t off = le32(d + 10), hsize = le32(d + 14);
    int w = (int)le32(d + 18), h = (int)le32(d + 22);
    int bpp = le16(d + 28), comp = (int)le32(d + 30);
    uint32_t ncol = hsize >= 40 ? le32(d + 46) : 0;
    if (bpp != 8 || w <= 0 || w > 8192) return NULL;
    int flip = h > 0;
    if (h < 0) h = -h;
    if (!ncol) ncol = 256;
    const uint8_t *pd = d + 14 + hsize;
    memset(pal, 0, sizeof(Palette));
    for (uint32_t i = 0; i < ncol && i < 256 && pd + i * 4 + 3 < d + n; i++) {
        pal[i].b = pd[i * 4];
        pal[i].g = pd[i * 4 + 1];
        pal[i].r = pd[i * 4 + 2];
    }
    Bitmap *b = bmp_create(w, h);
    if (!b) return NULL;
    if (comp == 0) {
        int stride = (w + 3) & ~3;
        if (off + (size_t)stride * h > n) {
            bmp_free(b);
            return NULL;
        }
        for (int y = 0; y < h; y++)
            memcpy(b->px + (size_t)(flip ? h - 1 - y : y) * w, d + off + (size_t)y * stride, (size_t)w);
    } else if (comp == 1) { /* RLE8 */
        size_t p = off;
        int x = 0, y = 0;
        while (p + 1 < n && y < h) {
            int c = d[p], v = d[p + 1];
            p += 2;
            if (c) {
                for (int i = 0; i < c && x < w; i++, x++) b->px[(size_t)(flip ? h - 1 - y : y) * w + x] = (uint8_t)v;
            } else if (v == 0) {
                x = 0;
                y++;
            } else if (v == 1) {
                break;
            } else if (v == 2) {
                if (p + 1 >= n) break;
                x += d[p];
                y += d[p + 1];
                p += 2;
            } else {
                for (int i = 0; i < v && p < n; i++, p++, x++)
                    if (x < w) b->px[(size_t)(flip ? h - 1 - y : y) * w + x] = d[p];
                if (v & 1) p++;
            }
        }
    } else {
        bmp_free(b);
        return NULL;
    }
    return b;
}

static Bitmap *load_pcx(const uint8_t *d, size_t n, Palette pal)
{
    if (n < 128 + 769 || d[0] != 10 || d[3] != 8 || d[65] != 1) return NULL;
    int w = le16(d + 8) - le16(d + 4) + 1, h = le16(d + 10) - le16(d + 6) + 1;
    int bpl = le16(d + 66);
    if (w <= 0 || h <= 0 || bpl < w) return NULL;
    const uint8_t *pd = d + n - 768;
    if (d[n - 769] != 12) return NULL;
    for (int i = 0; i < 256; i++) {
        pal[i].r = pd[i * 3];
        pal[i].g = pd[i * 3 + 1];
        pal[i].b = pd[i * 3 + 2];
    }
    Bitmap *b = bmp_create(w, h);
    if (!b) return NULL;
    size_t p = 128, end = n - 769;
    for (int y = 0; y < h; y++) {
        int x = 0;
        while (x < bpl && p < end) {
            int c = d[p++], cnt = 1;
            if ((c & 0xC0) == 0xC0) {
                cnt = c & 0x3F;
                if (p >= end) break;
                c = d[p++];
            }
            for (int i = 0; i < cnt && x < bpl; i++, x++)
                if (x < w) b->px[(size_t)y * w + x] = (uint8_t)c;
        }
    }
    return b;
}

Bitmap *img_load_8bit(const char *path, Palette pal)
{
    size_t n;
    uint8_t *d = slurp(path, &n);
    if (!d) return NULL;
    Bitmap *b = NULL;
    if (n > 2 && d[0] == 'B' && d[1] == 'M') b = load_bmp(d, n, pal);
    else b = load_pcx(d, n, pal);
    free(d);
    return b;
}
