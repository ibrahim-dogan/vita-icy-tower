#include "datafile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define F_PACK_MAGIC 0x736C6821u
#define F_NOPACK_MAGIC 0x736C682Eu
#define DAT_MAGIC DAT_ID('A', 'L', 'L', '.')
#define DAT_PROP DAT_ID('p', 'r', 'o', 'p')
#define DAT_NAME DAT_ID('N', 'A', 'M', 'E')

/* Same mask Allegro 4 applies to the magic number of encrypted packfiles. */
static uint32_t encrypt_id(uint32_t x, const char *pw)
{
    uint32_t mask = 0;
    if (!pw || !pw[0]) return x;
    for (int i = 0; pw[i]; i++) mask ^= (uint32_t)(uint8_t)pw[i] << ((i & 3) * 8);
    for (int i = 0, pos = 0; i < 4; i++) {
        mask ^= (uint32_t)(uint8_t)pw[pos++] << (24 - i * 8);
        if (!pw[pos]) pos = 0;
    }
    mask ^= 42;
    return x ^ mask;
}

/* Allegro's LZSS decoder (4 KB window, 18 byte max match). */
static uint8_t *lzss_unpack(const uint8_t *src, size_t len, size_t *out_len)
{
    enum { N = 4096, F = 18, THRESHOLD = 2 };
    uint8_t text[N];
    memset(text, 0, sizeof(text));
    size_t cap = len * 4 + 1024, n = 0, p = 0;
    uint8_t *out = malloc(cap);
    if (!out) return NULL;
    int r = N - F;
    unsigned flags = 0;
    for (;;) {
        if (((flags >>= 1) & 256) == 0) {
            if (p >= len) break;
            flags = src[p++] | 0xFF00;
        }
        if (n + F + 1 > cap) {
            cap *= 2;
            uint8_t *grown = realloc(out, cap);
            if (!grown) {
                free(out);
                return NULL;
            }
            out = grown;
        }
        if (flags & 1) {
            if (p >= len) break;
            uint8_t c = src[p++];
            text[r++] = c;
            r &= N - 1;
            out[n++] = c;
        } else {
            if (p + 1 >= len) break;
            int i = src[p], j = src[p + 1];
            p += 2;
            i |= (j & 0xF0) << 4;
            j = (j & 0x0F) + THRESHOLD;
            for (int k = 0; k <= j; k++) {
                uint8_t c = text[(i + k) & (N - 1)];
                text[r++] = c;
                r &= N - 1;
                out[n++] = c;
            }
        }
    }
    *out_len = n;
    return out;
}

static uint8_t *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = sz > 0 ? malloc((size_t)sz) : NULL;
    if (buf && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    *len = (size_t)sz;
    return buf;
}

static int parse(Datafile *df, const uint8_t *d, size_t len)
{
    if (len < 8 || (uint32_t)dat_be32(d) != DAT_MAGIC) return -1;
    int count = dat_be32(d + 4);
    size_t p = 8;
    if (count < 0 || count > 100000) return -1;
    df->objs = calloc((size_t)count, sizeof(DatObject));
    if (!df->objs) return -1;
    for (int i = 0; i < count; i++) {
        DatObject *o = &df->objs[df->count];
        for (;;) {
            if (p + 4 > len) return -1;
            if ((uint32_t)dat_be32(d + p) != DAT_PROP) break;
            if (p + 12 > len) return -1;
            uint32_t id = (uint32_t)dat_be32(d + p + 4);
            int32_t plen = dat_be32(d + p + 8);
            if (plen < 0 || p + 12 + (size_t)plen > len) return -1;
            if (id == DAT_NAME) {
                size_t n = (size_t)plen < sizeof(o->name) - 1 ? (size_t)plen : sizeof(o->name) - 1;
                memcpy(o->name, d + p + 12, n);
                o->name[n] = 0;
            }
            p += 12 + (size_t)plen;
        }
        if (p + 12 > len) return -1;
        o->type = (uint32_t)dat_be32(d + p);
        int32_t fsize = dat_be32(d + p + 4);
        int32_t dsize = dat_be32(d + p + 8);
        p += 12;
        if (fsize < 0 || p + (size_t)fsize > len) return -1;
        if (dsize < 0) {
            o->data = lzss_unpack(d + p, (size_t)fsize, &o->size);
        } else {
            o->data = malloc((size_t)fsize + 1);
            if (o->data) memcpy(o->data, d + p, (size_t)fsize);
            o->size = (size_t)fsize;
        }
        if (!o->data) return -1;
        p += (size_t)fsize;
        df->count++;
    }
    return 0;
}

int dat_load(Datafile *df, const char *path, const char *password)
{
    memset(df, 0, sizeof(*df));
    size_t len;
    uint8_t *raw = read_file(path, &len);
    if (!raw || len < 4) {
        free(raw);
        return -1;
    }
    /* New-style encryption XORs every byte of the file, header included. */
    if (password && password[0]) {
        size_t pl = strlen(password);
        for (size_t i = 0; i < len; i++) raw[i] ^= (uint8_t)password[i % pl];
    }
    uint32_t magic = (uint32_t)dat_be32(raw);
    int rc = -1;
    if (magic == encrypt_id(F_PACK_MAGIC, password)) {
        size_t ulen;
        uint8_t *u = lzss_unpack(raw + 4, len - 4, &ulen);
        if (u) rc = parse(df, u, ulen);
        free(u);
    } else if (magic == encrypt_id(F_NOPACK_MAGIC, password)) {
        rc = parse(df, raw + 4, len - 4);
    }
    free(raw);
    if (rc != 0) dat_free(df);
    return rc;
}

void dat_free(Datafile *df)
{
    for (int i = 0; i < df->count; i++) free(df->objs[i].data);
    free(df->objs);
    memset(df, 0, sizeof(*df));
}

const DatObject *dat_find(const Datafile *df, const char *name)
{
    for (int i = 0; i < df->count; i++)
        if (strcmp(df->objs[i].name, name) == 0) return &df->objs[i];
    return NULL;
}
