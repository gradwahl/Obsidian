#include "library.h"
#include <stddef.h>
#include <string.h>

#define COUNT(items) ((int)(sizeof(items) / sizeof((items)[0])))

static const LibraryConsole sony[] = {
    {"PlayStation 1", NULL, "assets/library/consoles/sony-ps1.png", "assets/library/logos/sony-ps1.png"},
    {"PlayStation 2", NULL, "assets/library/consoles/sony-ps2.png", "assets/library/logos/sony-ps2.png"},
    {"PlayStation 3 (experimental)", NULL, "assets/library/consoles/sony-ps3.png", "assets/library/logos/sony-ps3.png"},
    {"PlayStation 4", "CUSA", "assets/library/consoles/sony-ps4.png", "assets/library/logos/sony-ps4.png"},
    {"PlayStation 5", "PPSA", "assets/library/consoles/sony-ps5.png", "assets/library/logos/sony-ps5.png"},
    {"PlayStation Portable (PSP)", NULL, "assets/library/consoles/sony-psp.png", "assets/library/logos/sony-psp.png"},
};
static const LibraryConsole microsoft[] = {
    {"Original Xbox", NULL, "assets/library/consoles/microsoft-xbox.png", "assets/library/logos/microsoft-xbox.png"},
    {"Xbox 360 (experimental)", NULL, "assets/library/consoles/microsoft-xbox360.png", "assets/library/logos/microsoft-xbox360.png"},
};
static const LibraryConsole sega[] = {
    {"SG-1000", NULL, "assets/library/consoles/sega-sg1000.png", "assets/library/logos/sega-sg1000.png"},
    {"Master System", NULL, "assets/library/consoles/sega-master.png", "assets/library/logos/sega-master.png"},
    {"Mega Drive / Genesis", NULL, "assets/library/consoles/sega-megadrive.png", "assets/library/logos/sega-megadrive.png"},
    {"Game Gear", NULL, "assets/library/consoles/sega-gamegear.png", "assets/library/logos/sega-gamegear.png"},
    {"Mega-CD / Sega CD", NULL, "assets/library/consoles/sega-cd.png", "assets/library/logos/sega-cd.png"},
    {"Sega 32X", NULL, "assets/library/consoles/sega-32x.png", "assets/library/logos/sega-32x.png"},
    {"Sega Saturn", NULL, "assets/library/consoles/sega-saturn.png", "assets/library/logos/sega-saturn.png"},
    {"Dreamcast", NULL, "assets/library/consoles/sega-dreamcast.png", "assets/library/logos/sega-dreamcast.png"}
};
static const LibraryConsole nintendo[] = {
    {"NES / Famicom", NULL, "assets/library/consoles/nintendo-nes.png", "assets/library/logos/nintendo-nes.png"},
    {"SNES / Super Famicom", NULL, "assets/library/consoles/nintendo-snes.png", "assets/library/logos/nintendo-snes.png"},
    {"Nintendo 64", NULL, "assets/library/consoles/nintendo-n64.png", "assets/library/logos/nintendo-n64.png"},
    {"GameCube", NULL, "assets/library/consoles/nintendo-gamecube.png", "assets/library/logos/nintendo-gamecube.png"},
    {"Wii", NULL, "assets/library/consoles/nintendo-wii.png", "assets/library/logos/nintendo-wii.png"},
    {"Wii U", NULL, "assets/library/consoles/nintendo-wiiu.png", "assets/library/logos/nintendo-wiiu.png"},
    {"Nintendo Switch", NULL, "assets/library/consoles/nintendo-switch.png", "assets/library/logos/nintendo-switch.png"},
    {"Game Boy", NULL, "assets/library/consoles/nintendo-gameboy.png", "assets/library/logos/nintendo-gameboy.png"},
    {"Game Boy Color", NULL, "assets/library/consoles/nintendo-gbc.png", "assets/library/logos/nintendo-gbc.png"},
    {"Game Boy Advance", NULL, "assets/library/consoles/nintendo-gba.png", "assets/library/logos/nintendo-gba.png"},
    {"Nintendo DS", NULL, "assets/library/consoles/nintendo-ds.png", "assets/library/logos/nintendo-ds.png"},
    {"Nintendo 3DS", NULL, "assets/library/consoles/nintendo-3ds.png", "assets/library/logos/nintendo-3ds.png"},
    {"Virtual Boy", NULL, "assets/library/consoles/nintendo-virtualboy.png", "assets/library/logos/nintendo-virtualboy.png"},
    {"Pokémon Mini", NULL, "assets/library/consoles/nintendo-pokemini.png", "assets/library/logos/nintendo-pokemini.png"}
};
static const LibraryConsole atari[] = {
    {"Atari 2600", NULL, "assets/library/consoles/atari-2600.png", "assets/library/logos/atari-2600.png"},
    {"Atari 5200", NULL, "assets/library/consoles/atari-5200.png", "assets/library/logos/atari-5200.png"},
    {"Atari 7800", NULL, "assets/library/consoles/atari-7800.png", "assets/library/logos/atari-7800.png"},
    {"Atari Lynx", NULL, "assets/library/consoles/atari-lynx.png", "assets/library/logos/atari-lynx.png"},
    {"Atari Jaguar", NULL, "assets/library/consoles/atari-jaguar.png", "assets/library/logos/atari-jaguar.png"}
};
static const LibraryConsole snk[] = {
    {"Neo Geo AES", NULL, "assets/library/consoles/snk-neogeo.png", "assets/library/logos/snk-neogeo.png"},
    {"Neo Geo CD", NULL, "assets/library/consoles/snk-cd.png", "assets/library/logos/snk-cd.png"},
    {"Neo Geo Pocket", NULL, "assets/library/consoles/snk-pocket.png", "assets/library/logos/snk-pocket.png"},
    {"Neo Geo Pocket Color", NULL, "assets/library/consoles/snk-pocketcolor.png", "assets/library/logos/snk-pocketcolor.png"}
};
static const LibraryConsole nec[] = {
    {"PC Engine / TurboGrafx-16", NULL, "assets/library/consoles/nec-pce.png", "assets/library/logos/nec-pce.png"},
    {"PC Engine CD / TurboGrafx-CD", NULL, "assets/library/consoles/nec-pcecd.png", "assets/library/logos/nec-pcecd.png"},
    {"SuperGrafx", NULL, "assets/library/consoles/nec-supergrafx.png", "assets/library/logos/nec-supergrafx.png"},
    {"PC-FX", NULL, "assets/library/consoles/nec-pcfx.png", "assets/library/logos/nec-pcfx.png"}
};
static const LibraryConsole bandai[] = {
    {"WonderSwan", NULL, "assets/library/consoles/bandai-ws.png", "assets/library/logos/bandai-ws.png"},
    {"WonderSwan Color", NULL, "assets/library/consoles/bandai-wscolor.png", "assets/library/logos/bandai-wscolor.png"}
};
static const LibraryConsole other[] = {
    {"Panasonic 3DO", NULL, "assets/library/consoles/other-3do.png", "assets/library/logos/other-3do.png"},
    {"Philips CD-i", NULL, "assets/library/consoles/other-cdi.png", "assets/library/logos/other-cdi.png"},
    {"ColecoVision", NULL, "assets/library/consoles/other-coleco.png", "assets/library/logos/other-coleco.png"},
    {"Intellivision", NULL, "assets/library/consoles/other-intellivision.png", "assets/library/logos/other-intellivision.png"},
    {"Magnavox Odyssey 2", NULL, "assets/library/consoles/other-odyssey2.png", "assets/library/logos/other-odyssey2.png"},
    {"Fairchild Channel F", NULL, "assets/library/consoles/other-channelf.png", "assets/library/logos/other-channelf.png"},
    {"Epoch Super Cassette Vision", NULL, "assets/library/consoles/other-scv.png", NULL},
    {"Amstrad GX4000", NULL, "assets/library/consoles/other-gx4000.png", "assets/library/logos/other-gx4000.png"}
};
static const LibraryConsole arcade[] = {
    {"Sega NAOMI", NULL, NULL, "assets/library/logos/arcade-naomi.png"},
    {"Sega NAOMI 2", NULL, NULL, "assets/library/logos/arcade-naomi2.png"},
    {"Sega Atomiswave", NULL, NULL, "assets/library/logos/arcade-atomiswave.png"},
    {"Neo Geo MVS", NULL, NULL, "assets/library/logos/snk-neogeo.png"},
    {"Various classic arcade systems", NULL, NULL, "assets/library/logos/arcade-classic.png"}
};
static const LibraryCompany companies[] = {
    {"Sony", 0, 112, 204, sony, COUNT(sony), "assets/library/companies/sony.png"},
    {"Microsoft", 16, 124, 16, microsoft, COUNT(microsoft), "assets/library/companies/microsoft.png"},
    {"Sega", 0, 82, 170, sega, COUNT(sega), "assets/library/companies/sega.png"},
    {"Nintendo", 220, 0, 35, nintendo, COUNT(nintendo), "assets/library/companies/nintendo.png"},
    {"Atari", 210, 32, 38, atari, COUNT(atari), "assets/library/companies/atari.png"},
    {"SNK", 0, 143, 208, snk, COUNT(snk), "assets/library/companies/snk.png"},
    {"NEC", 0, 75, 160, nec, COUNT(nec), "assets/library/companies/nec.png"},
    {"Bandai", 220, 0, 35, bandai, COUNT(bandai), "assets/library/companies/bandai.png"},
    {"Other consoles", 140, 140, 140, other, COUNT(other), NULL},
    {"Arcade systems", 140, 140, 140, arcade, COUNT(arcade), "assets/library/companies/arcade.png"}
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

bool library_console_matches_platform(const LibraryConsole *console, const char *platform) {
    if (!console || !platform || !platform[0]) return false;
    static const struct { const char *name, *platform; } mappings[] = {
        {"PlayStation 1", "sony-ps1"},
        {"PlayStation 2", "sony-ps2"},
        {"PlayStation 3 (experimental)", "sony-ps3"},
        {"PlayStation 4", "sony-ps4"},
        {"PlayStation 5", "sony-ps5"},
        {"PlayStation Portable (PSP)", "sony-psp"},
        {"Original Xbox", "microsoft-xbox"},
        {"Xbox 360 (experimental)", "microsoft-xbox360"},
        {"Sega Saturn", "sega-saturn"},
        {"Dreamcast", "sega-dreamcast"},
        {"NES / Famicom", "nintendo-nes"},
        {"SNES / Super Famicom", "nintendo-snes"},
        {"Nintendo 64", "nintendo-n64"},
        {"GameCube", "nintendo-gc"},
        {"Wii", "nintendo-wii"},
        {"Wii U", "nintendo-wiiu"},
        {"Nintendo Switch", "nintendo-switch"},
        {"Game Boy", "nintendo-gb"},
        {"Game Boy Color", "nintendo-gbc"},
        {"Game Boy Advance", "nintendo-gba"},
        {"Nintendo DS", "nintendo-ds"},
        {"Nintendo 3DS", "nintendo-3ds"}
    };
    for (size_t i = 0; i < sizeof(mappings)/sizeof(mappings[0]); i++)
        if (strcmp(console->name, mappings[i].name) == 0 &&
            strcmp(platform, mappings[i].platform) == 0) return true;
    return false;
}
