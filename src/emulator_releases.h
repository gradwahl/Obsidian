/* Generated from public GitHub Releases by scripts/generate_emulator_releases.py */
#ifndef OBSIDIAN_EMULATOR_RELEASES_H
#define OBSIDIAN_EMULATOR_RELEASES_H
#define EMULATOR_RELEASES_URL "https://github.com/gradwahl/Obsidian-Emulators/releases"
typedef struct { const char *platform, *label, *asset_url, *filename, *sha256; unsigned long long size; } EmulatorReleaseAsset;
static const EmulatorReleaseAsset EMULATOR_RELEASE_ASSETS[] = {
    {"sony-ps1", "v2", "https://github.com/gradwahl/Obsidian-Emulators/releases/download/v2/PSXS5-v2.2.0.zip", "PSXS5-v2.2.0.zip", "8904a734f99ecd837071f78167214cb2fa867870936afe996f6f3c95f7b4af60", 18360114ULL},
    {"sony-ps2", "v3", "https://github.com/gradwahl/Obsidian-Emulators/releases/download/v3/PS5SX2Installer.elf", "PS5SX2Installer.elf", "6b1be3ea24ec8144b27b2a1f3e279cd17574f3093e9431abf8579dcc6c0bfae9", 1196432ULL},
    {"sony-ps3", "V4", "https://github.com/gradwahl/Obsidian-Emulators/releases/download/V4/PS5SX3-Installer.elf", "PS5SX3-Installer.elf", "6f0715149504ce85438e4993314390316089f2ee6954e81f77a7fc9bf0ddf2a1", 985248ULL},
    {"microsoft-xbox", "v1", "https://github.com/gradwahl/Obsidian-Emulators/releases/download/v1/PPSA97358.zip", "PPSA97358.zip", "494682ff5073985422fd8961cc617b8e3bdbdb2dedd85b69dadbd06f709953e9", 21413061ULL},
    {"microsoft-xbox360", "v6", "https://github.com/gradwahl/Obsidian-Emulators/releases/download/v6/PPSA50011.zip", "PPSA50011.zip", "b2bf7a7d242428cfa002ec557210dc885403580b58bda8d67a436a2550b67dd2", 24745309ULL},
    {"nintendo-wiiu", "v5", "https://github.com/gradwahl/Obsidian-Emulators/releases/download/v5/PS5-ECAM-v5.0.0.zip", "PS5-ECAM-v5.0.0.zip", "5e29977effb819b884630aaeda212e10b082b243a72c6440b21ed7d286432299", 81474275ULL},
    {"nintendo-ds", "v5", "https://github.com/gradwahl/Obsidian-Emulators/releases/download/v5/PS5-ECAM-v5.0.0.zip", "PS5-ECAM-v5.0.0.zip", "5e29977effb819b884630aaeda212e10b082b243a72c6440b21ed7d286432299", 81474275ULL},
    {"nintendo-3ds", "v5", "https://github.com/gradwahl/Obsidian-Emulators/releases/download/v5/PS5-ECAM-v5.0.0.zip", "PS5-ECAM-v5.0.0.zip", "5e29977effb819b884630aaeda212e10b082b243a72c6440b21ed7d286432299", 81474275ULL},
    {"nintendo-switch", "v5", "https://github.com/gradwahl/Obsidian-Emulators/releases/download/v5/PS5-ECAM-v5.0.0.zip", "PS5-ECAM-v5.0.0.zip", "5e29977effb819b884630aaeda212e10b082b243a72c6440b21ed7d286432299", 81474275ULL},
};
#define EMULATOR_RELEASE_ASSET_COUNT (sizeof(EMULATOR_RELEASE_ASSETS)/sizeof(EMULATOR_RELEASE_ASSETS[0]))
static const EmulatorReleaseAsset EMULATOR_RETROARCH_ASSET = {"retroarch", "v4", "https://github.com/gradwahl/Obsidian-Emulators/releases/download/v4/PS5_RetroArch-v4.0.0-beta-4.zip", "PS5_RetroArch-v4.0.0-beta-4.zip", "dad4f20204446a66dd05edb67cdf720988ca06119396f79a5e9ed876dc3d6012", 1553672845ULL};
static const char *const EMULATOR_RETROARCH_SYSTEM_NAMES[] = {
    "PlayStation Portable (PSP)",
    "SG-1000",
    "Master System",
    "Mega Drive / Genesis",
    "Game Gear",
    "Mega-CD / Sega CD",
    "Sega 32X",
    "Sega Saturn",
    "Dreamcast",
    "NES / Famicom",
    "SNES / Super Famicom",
    "Nintendo 64",
    "GameCube",
    "Wii",
    "Game Boy",
    "Game Boy Color",
    "Game Boy Advance",
    "Virtual Boy",
    "Pokémon Mini",
    "Atari 2600",
    "Atari 5200",
    "Atari 7800",
    "Atari Lynx",
    "Atari Jaguar",
    "Neo Geo AES",
    "Neo Geo CD",
    "Neo Geo Pocket",
    "Neo Geo Pocket Color",
    "PC Engine / TurboGrafx-16",
    "PC Engine CD / TurboGrafx-CD",
    "SuperGrafx",
    "PC-FX",
    "WonderSwan",
    "WonderSwan Color",
};
#define EMULATOR_RETROARCH_SYSTEM_COUNT (sizeof(EMULATOR_RETROARCH_SYSTEM_NAMES)/sizeof(EMULATOR_RETROARCH_SYSTEM_NAMES[0]))
#endif
