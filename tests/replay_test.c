/* Plays .itr replays through the engine and compares with their headers.
 * Usage: replay_test file.itr... */
#include <stdio.h>

#include "replay.h"

int main(int argc, char **argv)
{
    int bad = 0;
    for (int i = 1; i < argc; i++) {
        Replay r;
        if (replay_load(&r, argv[i]) != 0) {
            printf("LOADFAIL %s\n", argv[i]);
            bad++;
            continue;
        }
        int s, f, c;
        int ok = replay_validate(&r, &s, &f, &c);
        int match = s == r.score && f == r.floor && c == r.combo;
        printf("%s %-8s header %d/%d/%d sim %d/%d/%d  %s\n", ok && match ? "OK  " : "FAIL", r.name, r.score, r.floor,
               r.combo, s, f, c, argv[i]);
        if (!(ok && match)) bad++;
        /* round trip through our writer */
        if (replay_save(&r, "/tmp/rt.itr") == 0) {
            Replay r2;
            if (replay_load(&r2, "/tmp/rt.itr") != 0) printf("  ROUNDTRIP FAIL\n");
            else replay_free(&r2);
        }
        replay_free(&r);
    }
    return bad;
}
