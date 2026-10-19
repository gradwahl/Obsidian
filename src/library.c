#include "library.h"
#include <stddef.h>

#define COUNT(items) ((int)(sizeof(items) / sizeof((items)[0])))

static const LibraryConsole sony[] = {
    {"PlayStation", NULL}, {"PlayStation 2", NULL}, {"PlayStation 3", NULL},
    {"PlayStation 4", "CUSA"}, {"PlayStation 5", "PPSA"},
    {"PSP", NULL}, {"PlayStation Vita", NULL}
};
static const LibraryConsole microsoft[] = {
    {"Xbox", NULL}, {"Xbox 360", NULL}, {"Xbox One", NULL}, {"Xbox Series X / S", NULL}
};
static const LibraryConsole sega[] = {
    {"Master System", NULL}, {"Mega Drive / Genesis", NULL}, {"Game Gear", NULL},
    {"Sega CD / Mega-CD", NULL}, {"32X", NULL}, {"Saturn", NULL}, {"Dreamcast", NULL}
};
static const LibraryConsole nintendo[] = {
    {"NES / Famicom", NULL}, {"Super Nintendo", NULL}, {"Nintendo 64", NULL},
    {"GameCube", NULL}, {"Wii", NULL}, {"Wii U", NULL}, {"Nintendo Switch", NULL},
    {"Nintendo Switch 2", NULL}, {"Game Boy", NULL}, {"Game Boy Color", NULL},
    {"Game Boy Advance", NULL}, {"Nintendo DS", NULL}, {"Nintendo 3DS", NULL},
    {"Virtual Boy", NULL}
};
static const LibraryConsole atari[] = {
    {"Atari 2600", NULL}, {"Atari 5200", NULL}, {"Atari 7800", NULL},
    {"Atari Lynx", NULL}, {"Atari Jaguar", NULL}
};
static const LibraryConsole snk[] = {
    {"Neo Geo AES / MVS", NULL}, {"Neo Geo CD", NULL},
    {"Neo Geo Pocket", NULL}, {"Neo Geo Pocket Color", NULL}
};
static const LibraryCompany companies[] = {
    {"Sony", 0, 112, 204, sony, COUNT(sony)},
    {"Microsoft", 16, 124, 16, microsoft, COUNT(microsoft)},
    {"Sega", 0, 82, 170, sega, COUNT(sega)},
    {"Nintendo", 220, 0, 35, nintendo, COUNT(nintendo)},
    {"Atari", 210, 32, 38, atari, COUNT(atari)},
    {"SNK", 0, 143, 208, snk, COUNT(snk)}
};

int library_company_count(void) { return COUNT(companies); }

const LibraryCompany *library_company_at(int index) {
    return index >= 0 && index < COUNT(companies) ? &companies[index] : NULL;
}

const LibraryConsole *library_console_at(int company, int index) {
    const LibraryCompany *entry = library_company_at(company);
    return entry && index >= 0 && index < entry->console_count ? &entry->consoles[index] : NULL;
}

bool library_console_matches_id(const LibraryConsole *console, const char *title_id) {
    if (!console || !console->title_id_prefix || !title_id) return false;
    for (int i = 0; console->title_id_prefix[i]; i++) {
        char c = title_id[i];
        if (!c) return false;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (c != console->title_id_prefix[i]) return false;
    }
    return true;
}
