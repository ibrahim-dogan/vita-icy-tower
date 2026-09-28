#include "installer.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "LzmaDec.h"
#include "log.h"
#include "platform.h"

/* The installer is Inno Setup 5.1.2. Every file is its own chunk: "zlb\x1a",
 * five bytes of LZMA properties and a raw LZMA stream. Only this one
 * installer (identified by size and SHA-1) is supported, so the chunk
 * positions are listed here instead of parsing Inno's setup header.
 * Offsets point at the LZMA properties. The PC executable, gamepad.txt and
 * the licence texts are not needed on the Vita. */
#define INSTALLER_SIZE 2647172L
static const uint8_t INSTALLER_SHA1[20] = {0xa0, 0x0a, 0xa6, 0xeb, 0xc4, 0xc3, 0x7f, 0xac, 0x7c, 0x44,
                                           0xde, 0x91, 0x67, 0x14, 0x77, 0xef, 0x7e, 0x32, 0x38, 0x9a};

typedef struct {
    uint32_t offset, packed, size;
    const char *path;
} Chunk;

static const Chunk CHUNKS[] = {
    {0x0534e9, 308110, 331154, "data/data.dat"},
    {0x09e87b, 1178719, 1231096, "data/sfx13.dat"},
    {0x1c322c, 3365, 8997, "characters/characters.txt"},
    {0x1c3f55, 90390, 139728, "characters/harold_the_homeboy/harold.dat"},
    {0x1da06f, 193, 247, "characters/harold_the_homeboy/harold_the_homeboy.txt"},
    {0x1da134, 453348, 503458, "characters/disco_dave/dave.dat"},
    {0x248c1c, 175, 212, "characters/disco_dave/disco_dave.txt"},
    {0x248ccf, 2813, 44278, "characters/template/template.bmp"},
    {0x2497d0, 263, 385, "characters/template/template.txt"},
    {0x051d9a, 5963, 14593, "readme.txt"},
    {0x1be4de, 121, 173, "replays/last_game.itr"},
    {0x1be55b, 84, 84, "replays/replays.txt"},
    {0x1be5b3, 1359, 5418, "replays/examples/Arnon_Yaari_12648_582_56.itr"},
    {0x1beb06, 1062, 4048, "replays/examples/Arnon_Yaari_61719_480_237.itr"},
    {0x1bef30, 1339, 5388, "replays/examples/Bartek__Tajek__Kruk_167294_657_400.itr"},
    {0x1bf46f, 1332, 5393, "replays/examples/Bartek__Tajek__Kruk_98394_602_300.itr"},
    {0x1bf9a7, 950, 3693, "replays/examples/David__Davo__Haines_175546_415_414.itr"},
    {0x1bfd61, 1770, 7500, "replays/examples/David__Davo__Haines_43751_923_153.itr"},
    {0x1c044f, 1125, 4138, "replays/examples/Honza__John_Beak__Snabl_288065_562_531.itr"},
    {0x1c08b8, 1800, 7238, "replays/examples/Honza__John_Beak__Snabl_532156_970_712.itr"},
    {0x1c0fc4, 1737, 7113, "replays/examples/Hubertus__Strack__Struck_36936_941_119.itr"},
    {0x1c1691, 1455, 6023, "replays/examples/Hubertus__Strack__Struck_529697_774_721.itr"},
    {0x1c1c44, 916, 3303, "replays/examples/Johan_Peitz_5625_317_38.itr"},
    {0x1c1fdc, 946, 3353, "replays/examples/Johan_Peitz_5760_287_39.itr"},
    {0x1c2392, 1625, 6888, "replays/examples/Sebastian__Syo__Zyrkowski_57833_735_222.itr"},
    {0x1c29ef, 1113, 4303, "replays/examples/Sebastian__Syo__Zyrkowski_75902_498_265.itr"},
    {0x1c2e4c, 988, 3633, "replays/examples/_nders_5265_352_16.itr"},
};
#define NCHUNKS ((int)(sizeof(CHUNKS) / sizeof(CHUNKS[0])))

static const char *const DIRS[] = {
    "data", "characters", "characters/harold_the_homeboy", "characters/disco_dave", "characters/template",
    "replays", "replays/examples",
};

/* ---- SHA-1 ---- */

typedef struct {
    uint32_t h[5];
    uint64_t len;
    uint8_t buf[64];
    int n;
} Sha1;

#define ROL(v, s) (((v) << (s)) | ((v) >> (32 - (s))))

static void sha1_block(Sha1 *c, const uint8_t *p)
{
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (int i = 16; i < 80; i++) w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3], e = c->h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) f = (b & cc) | (~b & d), k = 0x5A827999;
        else if (i < 40) f = b ^ cc ^ d, k = 0x6ED9EBA1;
        else if (i < 60) f = (b & cc) | (b & d) | (cc & d), k = 0x8F1BBCDC;
        else f = b ^ cc ^ d, k = 0xCA62C1D6;
        uint32_t t = ROL(a, 5) + f + e + k + w[i];
        e = d;
        d = cc;
        cc = ROL(b, 30);
        b = a;
        a = t;
    }
    c->h[0] += a;
    c->h[1] += b;
    c->h[2] += cc;
    c->h[3] += d;
    c->h[4] += e;
}

static void sha1(const uint8_t *data, size_t len, uint8_t out[20])
{
    Sha1 c = {{0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0}, 0, {0}, 0};
    size_t i = 0;
    for (; i + 64 <= len; i += 64) sha1_block(&c, data + i);
    uint8_t tail[128] = {0};
    size_t rest = len - i;
    memcpy(tail, data + i, rest);
    tail[rest] = 0x80;
    size_t blocks = rest + 9 > 64 ? 2 : 1;
    uint64_t bits = (uint64_t)len * 8;
    for (int k = 0; k < 8; k++) tail[blocks * 64 - 1 - k] = (uint8_t)(bits >> (8 * k));
    for (size_t b = 0; b < blocks; b++) sha1_block(&c, tail + b * 64);
    for (int k = 0; k < 5; k++) {
        out[k * 4] = (uint8_t)(c.h[k] >> 24);
        out[k * 4 + 1] = (uint8_t)(c.h[k] >> 16);
        out[k * 4 + 2] = (uint8_t)(c.h[k] >> 8);
        out[k * 4 + 3] = (uint8_t)c.h[k];
    }
}

/* ---- finding and unpacking ---- */

static uint8_t *read_all(const char *path, long *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = *size > 0 ? malloc((size_t)*size) : NULL;
    if (b && fread(b, 1, (size_t)*size, f) != (size_t)*size) {
        free(b);
        b = NULL;
    }
    fclose(f);
    return b;
}

int installer_find(char *path, int pathlen)
{
    DIR *d = opendir(game_root());
    if (!d) return INSTALLER_NONE;
    int result = INSTALLER_NONE;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n < 4 || strcasecmp(e->d_name + n - 4, ".exe") != 0) continue;
        snprintf(path, (size_t)pathlen, "%s/%s", game_root(), e->d_name);
        struct stat st;
        if (stat(path, &st) != 0 || st.st_size != INSTALLER_SIZE) {
            log_printf("installer: %s is not the Icy Tower 1.3.1 installer (size %ld)", e->d_name,
                       (long)(stat(path, &st) == 0 ? st.st_size : -1));
            result = INSTALLER_UNKNOWN;
            continue;
        }
        result = INSTALLER_OK;
        break;
    }
    closedir(d);
    return result;
}

static void *lz_alloc(ISzAllocPtr p, size_t size)
{
    (void)p;
    return malloc(size);
}

static void lz_free(ISzAllocPtr p, void *addr)
{
    (void)p;
    free(addr);
}

static const ISzAlloc g_alloc = {lz_alloc, lz_free};

int installer_extract(const char *path, void (*progress)(int done, int total), char *err, int errlen)
{
    long size = 0;
    uint8_t *exe = read_all(path, &size);
    if (!exe) {
        snprintf(err, (size_t)errlen, "Could not read %s", path);
        return -1;
    }
    uint8_t digest[20];
    sha1(exe, (size_t)size, digest);
    if (size != INSTALLER_SIZE || memcmp(digest, INSTALLER_SHA1, 20) != 0) {
        free(exe);
        snprintf(err, (size_t)errlen, "%s is not the original Icy Tower 1.3.1 installer", path);
        return -1;
    }
    log_printf("installer: unpacking %s", path);
    for (size_t i = 0; i < sizeof(DIRS) / sizeof(DIRS[0]); i++) platform_mkdir(game_path(DIRS[i]));

    int rc = 0;
    for (int i = 0; i < NCHUNKS && rc == 0; i++) {
        const Chunk *c = &CHUNKS[i];
        if (progress) progress(i, NCHUNKS);
        const char *out_path = game_path(c->path);
        struct stat st;
        if (stat(out_path, &st) == 0) continue; /* keep what the player already has */
        uint8_t *out = malloc(c->size);
        if (!out) {
            snprintf(err, (size_t)errlen, "Out of memory");
            rc = -1;
            break;
        }
        SizeT out_len = c->size, in_len = c->packed - LZMA_PROPS_SIZE;
        ELzmaStatus status;
        SRes res = LzmaDecode(out, &out_len, exe + c->offset + LZMA_PROPS_SIZE, &in_len, exe + c->offset,
                              LZMA_PROPS_SIZE, LZMA_FINISH_ANY, &status, &g_alloc);
        if (res != SZ_OK || out_len != c->size) {
            snprintf(err, (size_t)errlen, "Could not unpack %s from the installer", c->path);
            rc = -1;
        } else {
            FILE *f = fopen(out_path, "wb");
            if (!f || fwrite(out, 1, c->size, f) != c->size) {
                snprintf(err, (size_t)errlen, "Could not write %s", out_path);
                rc = -1;
            }
            if (f) fclose(f);
        }
        free(out);
    }
    if (progress) progress(NCHUNKS, NCHUNKS);
    free(exe);
    if (rc == 0) log_printf("installer: done");
    else log_printf("*** installer: %s", err);
    return rc;
}
