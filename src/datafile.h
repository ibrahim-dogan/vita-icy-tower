/* Reader for Allegro 4 datafiles (the .dat files shipped with Icy Tower).
 *
 * Supports the password encryption, LZSS packing (whole file and per object)
 * and the object types the game uses: BMP, PAL, FONT and raw data (OGG). */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define DAT_ID(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))
#define DAT_BMP DAT_ID('B', 'M', 'P', ' ')
#define DAT_PAL DAT_ID('P', 'A', 'L', ' ')
#define DAT_FONT DAT_ID('F', 'O', 'N', 'T')
#define DAT_OGG DAT_ID('O', 'G', 'G', ' ')

typedef struct {
    uint32_t type;
    char name[64];
    uint8_t *data;
    size_t size;
} DatObject;

typedef struct {
    DatObject *objs;
    int count;
} Datafile;

/* Loads a datafile. password may be NULL. Returns 0 on success. */
int dat_load(Datafile *df, const char *path, const char *password);
void dat_free(Datafile *df);
const DatObject *dat_find(const Datafile *df, const char *name);

/* Big-endian helpers for parsing object payloads. */
static inline int dat_be16(const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); }
static inline int32_t dat_be32(const uint8_t *p)
{
    return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]);
}
