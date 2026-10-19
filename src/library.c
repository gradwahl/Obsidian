#include "library.h"
#include <stddef.h>

#define COUNT(items) ((int)(sizeof(items) / sizeof((items)[0])))

static const LibraryConsole sony[] = {
    {"PlayStation", NULL, "assets/library/consoles/sony-ps1.png"},
    {"PlayStation 2", NULL, "assets/library/consoles/sony-ps2.png"},
    {"PlayStation 3", NULL, "assets/library/consoles/sony-ps3.jpg"},
    {"PlayStation 4", "CUSA", "assets/library/consoles/sony-ps4.png"},
    {"PlayStation 5", "PPSA", "assets/library/consoles/sony-ps5.png"},
    {"PSP", NULL, "assets/library/consoles/sony-psp.png"},
    {"PlayStation Vita", NULL, "assets/library/consoles/sony-vita.png"}
};
static const LibraryConsole microsoft[] = {
    {"Xbox", NULL, "assets/library/consoles/microsoft-xbox.png"},
    {"Xbox 360", NULL, "assets/library/consoles/microsoft-xbox360.png"},
    {"Xbox One", NULL, "assets/library/consoles/microsoft-xboxone.jpg"},
    {"Xbox Series X / S", NULL, "assets/library/consoles/microsoft-series.png"}
};
static const LibraryConsole sega[] = {
    {"Master System", NULL, "assets/library/consoles/sega-master.png"},
    {"Mega Drive / Genesis", NULL, "assets/library/consoles/sega-megadrive.jpg"},
    {"Game Gear", NULL, "assets/library/consoles/sega-gamegear.png"},
    {"Sega CD / Mega-CD", NULL, "assets/library/consoles/sega-cd.jpg"},
    {"32X", NULL, "assets/library/consoles/sega-32x.jpg"},
    {"Saturn", NULL, "assets/library/consoles/sega-saturn.png"},
    {"Dreamcast", NULL, "assets/library/consoles/sega-dreamcast.png"}
};
static const LibraryConsole nintendo[] = {
    {"NES / Famicom", NULL, "assets/library/consoles/nintendo-nes.png"},
    {"Super Nintendo", NULL, "assets/library/consoles/nintendo-snes.png"},
    {"Nintendo 64", NULL, "assets/library/consoles/nintendo-n64.png"},
    {"GameCube", NULL, "assets/library/consoles/nintendo-gamecube.png"},
    {"Wii", NULL, "assets/library/consoles/nintendo-wii.png"},
    {"Wii U", NULL, "assets/library/consoles/nintendo-wiiu.png"},
    {"Nintendo Switch", NULL, "assets/library/consoles/nintendo-switch.png"},
    {"Nintendo Switch 2", NULL, "assets/library/consoles/nintendo-switch2.jpg"},
    {"Game Boy", NULL, "assets/library/consoles/nintendo-gameboy.png"},
    {"Game Boy Color", NULL, "assets/library/consoles/nintendo-gbc.png"},
    {"Game Boy Advance", NULL, "assets/library/consoles/nintendo-gba.png"},
    {"Nintendo DS", NULL, "assets/library/consoles/nintendo-ds.png"},
    {"Nintendo 3DS", NULL, "assets/library/consoles/nintendo-3ds.png"},
    {"Virtual Boy", NULL, "assets/library/consoles/nintendo-virtualboy.png"}
};
static const LibraryConsole atari[] = {
    {"Atari 2600", NULL, "assets/library/consoles/atari-2600.png"},
    {"Atari 5200", NULL, "assets/library/consoles/atari-5200.jpg"},
    {"Atari 7800", NULL, "assets/library/consoles/atari-7800.png"},
    {"Atari Lynx", NULL, "assets/library/consoles/atari-lynx.png"},
    {"Atari Jaguar", NULL, "assets/library/consoles/atari-jaguar.png"}
};
static const LibraryConsole snk[] = {
    {"Neo Geo AES / MVS", NULL, "assets/library/consoles/snk-neogeo.png"},
    {"Neo Geo CD", NULL, "assets/library/consoles/snk-cd.png"},
    {"Neo Geo Pocket", NULL, "assets/library/consoles/snk-pocket.jpg"},
    {"Neo Geo Pocket Color", NULL, "assets/library/consoles/snk-pocketcolor.jpg"}
};
static const LibraryCompany companies[] = {
    {"Sony", 0, 112, 204, sony, COUNT(sony), "assets/library/companies/sony.png"},
    {"Microsoft", 16, 124, 16, microsoft, COUNT(microsoft), "assets/library/companies/microsoft.png"},
    {"Sega", 0, 82, 170, sega, COUNT(sega), "assets/library/companies/sega.png"},
    {"Nintendo", 220, 0, 35, nintendo, COUNT(nintendo), "assets/library/companies/nintendo.png"},
    {"Atari", 210, 32, 38, atari, COUNT(atari), "assets/library/companies/atari.png"},
    {"SNK", 0, 143, 208, snk, COUNT(snk), "assets/library/companies/snk.png"}
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
