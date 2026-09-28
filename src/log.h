/* log.txt next to the game data (ux0:data/icytower/log.txt on the Vita),
 * rewritten on every start, like the original's log.txt. */
#pragma once

void log_open(void);
void log_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void log_close(void);
