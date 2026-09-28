/* Unpacks the game files from the original Icy Tower 1.3.1 installer
 * (icytower13_install.exe) placed in the game folder, so players only need
 * to copy that one unmodified file to the Vita. */
#pragma once

enum {
    INSTALLER_NONE,    /* no .exe in the game folder */
    INSTALLER_OK,      /* found the 1.3.1 installer */
    INSTALLER_UNKNOWN, /* found an .exe, but not the expected one */
};

/* Looks for the installer in the game folder. path receives the file. */
int installer_find(char *path, int pathlen);

/* Unpacks data/, characters/ and replays/ next to it. Files that already
 * exist are kept. progress(done, total) is called between files. */
int installer_extract(const char *path, void (*progress)(int done, int total), char *err, int errlen);
