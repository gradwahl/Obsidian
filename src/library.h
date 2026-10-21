#ifndef WOLODRIVE_LIBRARY_H
#define WOLODRIVE_LIBRARY_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { LIBRARY_COMPANIES, LIBRARY_CONSOLES, LIBRARY_GAMES } LibraryDepth;

typedef struct {
    const char *name;
    /* NULL means this platform has no supported catalog mapping yet. */
    const char *title_id_prefix;
    const char *artwork_path;
    const char *logo_path;
} LibraryConsole;

typedef struct {
    const char *name;
    uint8_t r, g, b;
    const LibraryConsole *consoles;
    int console_count;
    const char *artwork_path;
} LibraryCompany;

int library_company_count(void);
const LibraryCompany *library_company_at(int index);
const LibraryConsole *library_console_at(int company, int index);
/* Emulators omit PS4/PS5; Games/Roms uses the complete console list.
 * Visible selection indices are dense, so D-pad and page navigation remain valid. */
int library_console_visible_count(int company, bool emulators_view);
const LibraryConsole *library_console_visible_at(int company, int index, bool emulators_view);
bool library_console_matches_id(const LibraryConsole *console, const char *title_id);
bool library_console_matches_platform(const LibraryConsole *console, const char *platform);

#endif
