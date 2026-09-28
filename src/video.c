#include "video.h"

#include <string.h>

#include "config.h"

#define DISPLAY_W 960
#define DISPLAY_H 544
/* The 4:3 picture scaled to the full 544 lines. */
#define FIT_W (DISPLAY_H * 4 / 3)

static void build_lut(const Palette pal, int overlay, uint32_t *lut)
{
    int k = 255 - overlay;
    for (int i = 0; i < 256; i++) {
        uint32_t r = (uint32_t)(pal[i].r * k / 255), g = (uint32_t)(pal[i].g * k / 255), b = (uint32_t)(pal[i].b * k / 255);
        lut[i] = 0xFF000000u | (b << 16) | (g << 8) | r; /* ABGR: bytes R G B A */
    }
}

void video_convert(const Bitmap *frame, const Palette pal, int overlay, int shake, uint32_t *out, int pitch_px)
{
    uint32_t lut[256];
    build_lut(pal, overlay, lut);
    for (int y = 0; y < SCREEN_H; y++) {
        uint32_t *o = out + (size_t)y * pitch_px;
        int sy = y - shake;
        if (sy < 0) {
            for (int x = 0; x < SCREEN_W; x++) o[x] = 0xFF000000u;
            continue;
        }
        const uint8_t *s = frame->px + (size_t)sy * SCREEN_W;
        for (int x = 0; x < SCREEN_W; x += 4) {
            o[x] = lut[s[x]];
            o[x + 1] = lut[s[x + 1]];
            o[x + 2] = lut[s[x + 2]];
            o[x + 3] = lut[s[x + 3]];
        }
    }
}

#ifdef __vita__
/* ------------------------------------------------------------------ Vita */
#include <vita2d.h>

/* Three textures of each size used in turn, so the CPU never writes one the
 * GPU may still be reading. The 2x ones serve the Sharp and Wide modes:
 * doubling the pixels on the CPU and letting the GPU filter that down keeps
 * pixels crisp at the non-integer 544/480 scale. (A vita2d render target
 * would keep the 960x544 screen projection and draw off its edges.) */
static vita2d_texture *g_tex[3], *g_big[3];
static int g_index;

int video_init(int headless)
{
    (void)headless;
    if (vita2d_init() < 0) return -1;
    vita2d_set_clear_color(0xFF000000);
    vita2d_set_vblank_wait(1);
    for (int i = 0; i < 3; i++) {
        g_tex[i] = vita2d_create_empty_texture_format(SCREEN_W, SCREEN_H, SCE_GXM_TEXTURE_FORMAT_P8_ABGR);
        g_big[i] = vita2d_create_empty_texture_format(SCREEN_W * 2, SCREEN_H * 2, SCE_GXM_TEXTURE_FORMAT_P8_ABGR);
        if (!g_tex[i] || !g_big[i]) return -1;
    }
    return 0;
}

static int darkest(const Palette pal)
{
    int best = 1 << 30, idx = 0;
    for (int i = 1; i < 256; i++) {
        int v = pal[i].r + pal[i].g + pal[i].b;
        if (v < best) best = v, idx = i;
    }
    return idx;
}

void video_present(const Bitmap *frame, const Palette pal, int overlay, int shake, int mode)
{
    g_index = (g_index + 1) % 3;
    int big = mode == SCREEN_SHARP || mode == SCREEN_STRETCH;
    vita2d_texture *t = big ? g_big[g_index] : g_tex[g_index];
    uint8_t *d = vita2d_texture_get_datap(t);
    unsigned stride = vita2d_texture_get_stride(t);
    /* the strip uncovered by the shake gets the darkest colour */
    int black = shake ? darkest(pal) : 0;
    for (int y = 0; y < SCREEN_H; y++) {
        int sy = y - shake;
        const uint8_t *src = sy >= 0 ? frame->px + (size_t)sy * SCREEN_W : NULL;
        if (!big) {
            uint8_t *row = d + (size_t)y * stride;
            if (src) memcpy(row, src, SCREEN_W);
            else memset(row, black, SCREEN_W);
            continue;
        }
        uint8_t *row = d + (size_t)(y * 2) * stride;
        if (src) {
            uint16_t *o = (uint16_t *)row;
            for (int x = 0; x < SCREEN_W; x++) o[x] = (uint16_t)(src[x] * 0x0101u);
        } else {
            memset(row, black, SCREEN_W * 2);
        }
        memcpy(row + stride, row, SCREEN_W * 2);
    }
    build_lut(pal, overlay, vita2d_texture_get_palette(t));

    vita2d_start_drawing();
    vita2d_clear_screen();
    float fx = (DISPLAY_W - FIT_W) / 2.0f;
    float tw = (float)vita2d_texture_get_width(t), th = (float)vita2d_texture_get_height(t);
    if (mode == SCREEN_PIXEL) {
        vita2d_texture_set_filters(t, SCE_GXM_TEXTURE_FILTER_POINT, SCE_GXM_TEXTURE_FILTER_POINT);
        vita2d_draw_texture(t, (DISPLAY_W - SCREEN_W) / 2, (DISPLAY_H - SCREEN_H) / 2);
    } else {
        vita2d_texture_set_filters(t, SCE_GXM_TEXTURE_FILTER_LINEAR, SCE_GXM_TEXTURE_FILTER_LINEAR);
        if (mode == SCREEN_STRETCH) vita2d_draw_texture_scale(t, 0, 0, DISPLAY_W / tw, DISPLAY_H / th);
        else vita2d_draw_texture_scale(t, fx, 0, FIT_W / tw, DISPLAY_H / th);
    }
    vita2d_end_drawing();
    vita2d_swap_buffers();
}

void video_shutdown(void)
{
    vita2d_wait_rendering_done();
    for (int i = 0; i < 3; i++) {
        vita2d_free_texture(g_tex[i]);
        vita2d_free_texture(g_big[i]);
    }
    vita2d_fini();
}

#else
/* ------------------------------------------------------------------ Host */
#include <SDL2/SDL.h>

static SDL_Window *g_win;
static SDL_Renderer *g_ren;
static SDL_Texture *g_tex, *g_big;
static SDL_Surface *g_target;

int video_init(int headless)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) return -1;
    if (headless) {
        g_target = SDL_CreateRGBSurfaceWithFormat(0, DISPLAY_W, DISPLAY_H, 32, SDL_PIXELFORMAT_ARGB8888);
        g_ren = SDL_CreateSoftwareRenderer(g_target);
    } else {
        g_win = SDL_CreateWindow("Icy Tower", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DISPLAY_W, DISPLAY_H,
                                 SDL_WINDOW_RESIZABLE);
        g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!g_ren) g_ren = SDL_CreateRenderer(g_win, -1, 0);
    }
    if (!g_ren) return -1;
    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    g_big = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, SCREEN_W * 2, SCREEN_H * 2);
    if (g_big) SDL_SetTextureScaleMode(g_big, SDL_ScaleModeLinear);
    return g_tex ? 0 : -1;
}

void video_present(const Bitmap *frame, const Palette pal, int overlay, int shake, int mode)
{
    void *pixels;
    int pitch;
    if (SDL_LockTexture(g_tex, NULL, &pixels, &pitch) == 0) {
        video_convert(frame, pal, overlay, shake, pixels, pitch / 4);
        SDL_UnlockTexture(g_tex);
    }
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);
    int ow, oh;
    SDL_GetRendererOutputSize(g_ren, &ow, &oh);
    int fw = oh * 4 / 3;
    SDL_Rect fit = {(ow - fw) / 2, 0, fw, oh}, full = {0, 0, ow, oh};
    SDL_Rect one = {(ow - SCREEN_W) / 2, (oh - SCREEN_H) / 2, SCREEN_W, SCREEN_H};
    if (mode == SCREEN_SMOOTH) {
        SDL_SetTextureScaleMode(g_tex, SDL_ScaleModeLinear);
        SDL_RenderCopy(g_ren, g_tex, NULL, &fit);
    } else if (mode == SCREEN_PIXEL) {
        SDL_SetTextureScaleMode(g_tex, SDL_ScaleModeNearest);
        SDL_RenderCopy(g_ren, g_tex, NULL, oh >= SCREEN_H ? &one : &fit);
    } else if (g_big) {
        SDL_SetTextureScaleMode(g_tex, SDL_ScaleModeNearest);
        SDL_SetRenderTarget(g_ren, g_big);
        SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
        SDL_SetRenderTarget(g_ren, NULL);
        SDL_RenderCopy(g_ren, g_big, NULL, mode == SCREEN_STRETCH ? &full : &fit);
    } else {
        SDL_RenderCopy(g_ren, g_tex, NULL, mode == SCREEN_STRETCH ? &full : &fit);
    }
    SDL_RenderPresent(g_ren);
}

const uint8_t *video_display_rgb(void)
{
    static uint8_t rgb[DISPLAY_W * DISPLAY_H * 3];
    if (!g_target) return NULL;
    SDL_LockSurface(g_target);
    for (int y = 0; y < DISPLAY_H; y++) {
        const uint32_t *p = (const uint32_t *)((const uint8_t *)g_target->pixels + (size_t)y * g_target->pitch);
        uint8_t *o = rgb + (size_t)y * DISPLAY_W * 3;
        for (int x = 0; x < DISPLAY_W; x++) { /* ARGB8888 */
            o[x * 3] = (uint8_t)(p[x] >> 16);
            o[x * 3 + 1] = (uint8_t)(p[x] >> 8);
            o[x * 3 + 2] = (uint8_t)p[x];
        }
    }
    SDL_UnlockSurface(g_target);
    return rgb;
}

void video_shutdown(void)
{
    if (g_ren) SDL_DestroyRenderer(g_ren);
    if (g_win) SDL_DestroyWindow(g_win);
    if (g_target) SDL_FreeSurface(g_target);
}
#endif
