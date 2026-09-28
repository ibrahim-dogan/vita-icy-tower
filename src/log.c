#include "log.h"

#include <stdarg.h>
#include <stdio.h>

#include "platform.h"

static FILE *g_log;

void log_open(void)
{
    g_log = fopen(game_path("log.txt"), "w");
}

void log_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (g_log) {
        vfprintf(g_log, fmt, ap);
        fputc('\n', g_log);
        fflush(g_log);
    }
    va_end(ap);
#ifndef __vita__
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
#endif
}

void log_close(void)
{
    if (g_log) fclose(g_log);
    g_log = NULL;
}
