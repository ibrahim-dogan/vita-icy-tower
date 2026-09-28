#include "rnd.h"

static unsigned int g_msvc = 1;
static double g_custom;

void rnd_seed_msvc(unsigned int seed) { g_msvc = seed; }

int rnd_msvc(void)
{
    g_msvc = g_msvc * 214013u + 2531011u;
    return (int)((g_msvc >> 16) & 0x7FFF);
}

void rnd_seed_custom(int seed) { g_custom = (double)seed; }

int rnd_custom(void)
{
    double s = 1.4294484665 * g_custom;
    while (s > 65535.0) s -= 65535.0;
    g_custom = s;
    return (int)(65535.0 * (s - (double)(int)s));
}
