#include "audio.h"

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#define VOICES 24
#define MUSIC_VOICE (VOICES - 1)
#define STREAM_FRAMES 2048

struct Sound {
    int16_t *pcm; /* interleaved, `channels` per frame */
    int frames, channels, rate;
    uint8_t *ogg; /* compressed data for streamed sounds */
    size_t ogg_size;
};

typedef struct {
    Sound *snd;
    int active, loop;
    uint64_t pos, step; /* 32.32 fixed, in source frames */
    int32_t lv, rv;     /* 16.16 gains */
    /* streaming state */
    stb_vorbis *vorb;
    int16_t buf[STREAM_FRAMES * 2];
    int buf_frames;
    int serial;
} Voice;

static Voice g_voices[VOICES];
static SDL_AudioDeviceID g_dev;
static int g_out_rate = 48000, g_out_channels = 2;
static float g_music_vol = 1.0f;
static int g_serial;

/* ---- decoding ---- */

static Sound *from_wav(const uint8_t *data, size_t size)
{
    SDL_AudioSpec spec;
    Uint8 *buf;
    Uint32 len;
    if (!SDL_LoadWAV_RW(SDL_RWFromConstMem(data, (int)size), 1, &spec, &buf, &len)) return NULL;
    int ch = spec.channels > 2 ? 2 : spec.channels;
    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, (Uint8)ch, spec.freq) < 0) {
        SDL_FreeWAV(buf);
        return NULL;
    }
    cvt.len = (int)len;
    cvt.buf = SDL_malloc((size_t)len * (size_t)(cvt.len_mult > 0 ? cvt.len_mult : 1));
    if (!cvt.buf) {
        SDL_FreeWAV(buf);
        return NULL;
    }
    memcpy(cvt.buf, buf, len);
    SDL_FreeWAV(buf);
    if (cvt.needed) SDL_ConvertAudio(&cvt);
    else cvt.len_cvt = cvt.len;
    Sound *s = calloc(1, sizeof(Sound));
    if (!s) {
        SDL_free(cvt.buf);
        return NULL;
    }
    s->channels = ch;
    s->rate = spec.freq;
    s->frames = cvt.len_cvt / 2 / ch;
    s->pcm = malloc((size_t)cvt.len_cvt);
    if (s->pcm) memcpy(s->pcm, cvt.buf, (size_t)cvt.len_cvt);
    SDL_free(cvt.buf);
    if (!s->pcm) {
        free(s);
        return NULL;
    }
    return s;
}

Sound *sound_from_memory(const uint8_t *data, size_t size, int stream)
{
    if (size < 4) return NULL;
    if (memcmp(data, "RIFF", 4) == 0) return from_wav(data, size);
    if (memcmp(data, "OggS", 4) != 0) return NULL;
    Sound *s = calloc(1, sizeof(Sound));
    if (!s) return NULL;
    int err;
    stb_vorbis *v = stb_vorbis_open_memory(data, (int)size, &err, NULL);
    if (!v) {
        free(s);
        return NULL;
    }
    stb_vorbis_info info = stb_vorbis_get_info(v);
    s->channels = info.channels > 2 ? 2 : info.channels;
    s->rate = (int)info.sample_rate;
    if (stream) {
        stb_vorbis_close(v);
        s->ogg = malloc(size);
        if (!s->ogg) {
            free(s);
            return NULL;
        }
        memcpy(s->ogg, data, size);
        s->ogg_size = size;
        return s;
    }
    int total = (int)stb_vorbis_stream_length_in_samples(v);
    s->pcm = malloc((size_t)(total > 0 ? total : 1) * (size_t)s->channels * sizeof(int16_t));
    if (s->pcm)
        s->frames = stb_vorbis_get_samples_short_interleaved(v, s->channels, s->pcm, total * s->channels);
    stb_vorbis_close(v);
    if (!s->pcm) {
        free(s);
        return NULL;
    }
    return s;
}

Sound *sound_from_file(const char *path, int stream)
{
    SDL_RWops *rw = SDL_RWFromFile(path, "rb");
    if (!rw) return NULL;
    Sint64 size = SDL_RWsize(rw);
    uint8_t *buf = size > 0 ? malloc((size_t)size) : NULL;
    Sound *s = NULL;
    if (buf && SDL_RWread(rw, buf, 1, (size_t)size) == (size_t)size) s = sound_from_memory(buf, (size_t)size, stream);
    SDL_RWclose(rw);
    free(buf);
    return s;
}

void sound_free(Sound *s)
{
    if (!s) return;
    if (g_dev) {
        SDL_LockAudioDevice(g_dev);
        for (int i = 0; i < VOICES; i++)
            if (g_voices[i].snd == s) {
                if (g_voices[i].vorb) stb_vorbis_close(g_voices[i].vorb);
                g_voices[i].vorb = NULL;
                g_voices[i].active = 0;
                g_voices[i].snd = NULL;
            }
        SDL_UnlockAudioDevice(g_dev);
    }
    free(s->pcm);
    free(s->ogg);
    free(s);
}

/* ---- mixing ---- */

static int stream_refill(Voice *v)
{
    for (int tries = 0; tries < 2; tries++) {
        int n = stb_vorbis_get_samples_short_interleaved(v->vorb, v->snd->channels, v->buf,
                                                          STREAM_FRAMES * v->snd->channels);
        if (n > 0) {
            v->buf_frames = n;
            return 1;
        }
        if (!v->loop) return 0;
        stb_vorbis_seek_start(v->vorb);
    }
    return 0;
}

static void mix_voice(Voice *v, int32_t *acc, int frames)
{
    Sound *s = v->snd;
    int ch = s->channels;
    for (int i = 0; i < frames; i++) {
        uint32_t idx = (uint32_t)(v->pos >> 32);
        const int16_t *src;
        int avail;
        if (v->vorb) {
            while (idx >= (uint32_t)v->buf_frames) {
                v->pos -= (uint64_t)v->buf_frames << 32;
                idx -= (uint32_t)v->buf_frames;
                if (!stream_refill(v)) {
                    v->active = 0;
                    return;
                }
            }
            src = v->buf;
            avail = v->buf_frames;
        } else {
            if (idx >= (uint32_t)s->frames) {
                if (!v->loop || s->frames == 0) {
                    v->active = 0;
                    return;
                }
                v->pos -= (uint64_t)s->frames << 32;
                idx -= (uint32_t)s->frames;
                if (idx >= (uint32_t)s->frames) idx = 0, v->pos = 0;
            }
            src = s->pcm;
            avail = s->frames;
        }
        /* linear interpolation between neighbouring frames */
        uint32_t frac = (uint32_t)(v->pos >> 16) & 0xFFFF;
        uint32_t nxt = idx + 1 < (uint32_t)avail ? idx + 1 : idx;
        int32_t l0 = src[idx * ch], l1 = src[nxt * ch];
        int32_t r0 = ch > 1 ? src[idx * ch + 1] : l0, r1 = ch > 1 ? src[nxt * ch + 1] : l1;
        int32_t l = l0 + (int32_t)(((int64_t)(l1 - l0) * frac) >> 16);
        int32_t r = r0 + (int32_t)(((int64_t)(r1 - r0) * frac) >> 16);
        acc[i * 2] += (l * v->lv) >> 16;
        acc[i * 2 + 1] += (r * v->rv) >> 16;
        v->pos += v->step;
    }
}

static void mix_cb(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    enum { MAXF = 4096 };
    static int32_t acc[MAXF * 2];
    int16_t *out = (int16_t *)stream;
    int frames = len / 2 / g_out_channels;
    while (frames > 0) {
        int n = frames > MAXF ? MAXF : frames;
        memset(acc, 0, sizeof(int32_t) * 2 * (size_t)n);
        for (int v = 0; v < VOICES; v++)
            if (g_voices[v].active && g_voices[v].snd) mix_voice(&g_voices[v], acc, n);
        for (int i = 0; i < n; i++) {
            int32_t l = acc[i * 2], r = acc[i * 2 + 1];
            l = l > 32767 ? 32767 : l < -32768 ? -32768 : l;
            r = r > 32767 ? 32767 : r < -32768 ? -32768 : r;
            if (g_out_channels == 2) {
                out[0] = (int16_t)l;
                out[1] = (int16_t)r;
                out += 2;
            } else {
                *out++ = (int16_t)((l + r) / 2);
            }
        }
        frames -= n;
    }
}

void audio_init(void)
{
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = mix_cb;
    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (!g_dev) return;
    g_out_rate = have.freq;
    g_out_channels = have.channels >= 2 ? 2 : 1;
    SDL_PauseAudioDevice(g_dev, 0);
}

void audio_shutdown(void)
{
    if (!g_dev) return;
    SDL_CloseAudioDevice(g_dev);
    g_dev = 0;
    for (int i = 0; i < VOICES; i++)
        if (g_voices[i].vorb) stb_vorbis_close(g_voices[i].vorb);
    memset(g_voices, 0, sizeof(g_voices));
}

static void set_gains(Voice *v, float vol, float pan)
{
    if (pan < -1) pan = -1;
    if (pan > 1) pan = 1;
    /* Centre plays at full volume on both sides, the far side fades out. */
    float l = pan > 0 ? 1.0f - pan : 1.0f, r = pan < 0 ? 1.0f + pan : 1.0f;
    v->lv = (int32_t)(vol * l * 65536.0f);
    v->rv = (int32_t)(vol * r * 65536.0f);
}

static int start_voice(int slot, Sound *s, float vol, float pan, float speed, int loop)
{
    Voice *v = &g_voices[slot];
    if (v->vorb) stb_vorbis_close(v->vorb);
    memset(v, 0, sizeof(*v));
    v->snd = s;
    v->loop = loop;
    v->step = (uint64_t)((double)speed * s->rate / g_out_rate * 4294967296.0);
    set_gains(v, vol, pan);
    if (s->ogg) {
        int err;
        v->vorb = stb_vorbis_open_memory(s->ogg, (int)s->ogg_size, &err, NULL);
        if (!v->vorb || !stream_refill(v)) {
            if (v->vorb) stb_vorbis_close(v->vorb);
            v->vorb = NULL;
            return -1;
        }
    }
    v->serial = ++g_serial;
    v->active = 1;
    return slot;
}

int audio_play(Sound *s, float vol, float pan, float speed, int loop)
{
    if (!g_dev || !s || vol <= 0) return -1;
    SDL_LockAudioDevice(g_dev);
    /* Take a free voice, or steal the oldest one. */
    int slot = -1, oldest = -1;
    for (int i = 0; i < MUSIC_VOICE; i++) {
        if (!g_voices[i].active) {
            slot = i;
            break;
        }
        if (oldest < 0 || g_voices[i].serial < g_voices[oldest].serial) oldest = i;
    }
    if (slot < 0) slot = oldest;
    int r = start_voice(slot, s, vol, pan, speed, loop);
    SDL_UnlockAudioDevice(g_dev);
    return r < 0 ? -1 : (slot | ((g_voices[slot].serial & 0x7FFFFF) << 8));
}

static Voice *voice_of(int handle)
{
    if (handle < 0) return NULL;
    Voice *v = &g_voices[handle & 0xFF];
    return (handle & 0xFF) < VOICES && (v->serial & 0x7FFFFF) == (handle >> 8) ? v : NULL;
}

void audio_stop(int handle)
{
    if (!g_dev) return;
    SDL_LockAudioDevice(g_dev);
    Voice *v = voice_of(handle);
    if (v) v->active = 0;
    SDL_UnlockAudioDevice(g_dev);
}

int audio_playing(int handle)
{
    if (!g_dev) return 0;
    SDL_LockAudioDevice(g_dev);
    Voice *v = voice_of(handle);
    int r = v && v->active;
    SDL_UnlockAudioDevice(g_dev);
    return r;
}

void audio_stop_all(void)
{
    if (!g_dev) return;
    SDL_LockAudioDevice(g_dev);
    for (int i = 0; i < MUSIC_VOICE; i++) g_voices[i].active = 0;
    SDL_UnlockAudioDevice(g_dev);
}

void audio_music_play(Sound *s)
{
    if (!g_dev || !s) return;
    SDL_LockAudioDevice(g_dev);
    start_voice(MUSIC_VOICE, s, g_music_vol, 0, 1, 1);
    SDL_UnlockAudioDevice(g_dev);
}

void audio_music_stop(void)
{
    if (!g_dev) return;
    SDL_LockAudioDevice(g_dev);
    g_voices[MUSIC_VOICE].active = 0;
    SDL_UnlockAudioDevice(g_dev);
}

void audio_music_volume(float vol)
{
    g_music_vol = vol;
    if (!g_dev) return;
    SDL_LockAudioDevice(g_dev);
    set_gains(&g_voices[MUSIC_VOICE], vol, 0);
    SDL_UnlockAudioDevice(g_dev);
}
