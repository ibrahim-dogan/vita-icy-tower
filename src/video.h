/* Puts the 8-bit frame on the display.
 *
 * Vita: the frame is copied as-is into a paletted (P8) GXM texture and the
 * GPU does the palette lookup and scaling (vita2d).
 * Host: converted to RGBA and drawn with SDL (window or headless). */
#pragma once

#include "gfx.h"

int video_init(int headless);
/* overlay 0..255 darkens the palette (fades); shake moves the picture down. */
void video_present(const Bitmap *frame, const Palette pal, int overlay, int shake, int mode);
void video_shutdown(void);
#ifndef __vita__
/* Headless runs: the last presented 960x544 frame as RGB24 (960*544*3). */
const uint8_t *video_display_rgb(void);
#endif
/* Converts the frame to RGBA (host screenshots and benchmarks). */
void video_convert(const Bitmap *frame, const Palette pal, int overlay, int shake, uint32_t *out, int pitch_px);
