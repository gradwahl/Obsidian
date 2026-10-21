#!/usr/bin/env python3
"""Generate pinned emulator release links from the public Obsidian GitHub releases.

Standalone systems use their existing dedicated release files. RetroArch
systems listed by the project owner use the single RetroArch v4 release asset.
This is a download association, not verification that every individual
RetroArch core is present/functional on PS5. No files are downloaded.
"""
import json
import re
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = "gradwahl/Obsidian-Emulators"
API = "https://api.github.com/repos/" + REPO + "/releases?per_page=100"
HUB = "https://github.com/" + REPO + "/releases"
# A release is not proof of an emulator's working status on specific PS5 firmware.
TARGETS = {
    "sony-ps1": ("v2", "PSXS5-v2.2.0.zip"),
    "sony-ps2": ("v3", "PS5SX2Installer.elf"),
    "sony-ps3": ("V4", "PS5SX3-Installer.elf"),
    "microsoft-xbox": ("v1", "PPSA97358.zip"),
    "microsoft-xbox360": ("v6", "PPSA50011.zip"),
    "nintendo-wiiu": ("v5", "PS5-ECAM-v5.0.0.zip"),
    "nintendo-ds": ("v5", "PS5-ECAM-v5.0.0.zip"),
    "nintendo-3ds": ("v5", "PS5-ECAM-v5.0.0.zip"),
    "nintendo-switch": ("v5", "PS5-ECAM-v5.0.0.zip"),
}

# User-specified systems. Exact current Obsidian console titles are used so
# systems without catalog platform IDs (SG-1000, Neo Geo, etc.) work too.
# Repeated Game Boy Color / Game Boy Advance in the request are deduplicated.
RETROARCH_SYSTEMS = (
    "PlayStation Portable (PSP)",
    "SG-1000", "Master System", "Mega Drive / Genesis", "Game Gear",
    "Mega-CD / Sega CD", "Sega 32X", "Sega Saturn", "Dreamcast",
    "NES / Famicom", "SNES / Super Famicom", "Nintendo 64",
    "GameCube", "Wii", "Game Boy", "Game Boy Color", "Game Boy Advance",
    "Virtual Boy", "Pokémon Mini",
    "Atari 2600", "Atari 5200", "Atari 7800", "Atari Lynx", "Atari Jaguar",
    "Neo Geo AES", "Neo Geo CD", "Neo Geo Pocket", "Neo Geo Pocket Color",
    "PC Engine / TurboGrafx-16", "PC Engine CD / TurboGrafx-CD",
    "SuperGrafx", "PC-FX", "WonderSwan", "WonderSwan Color",
)
RETROARCH_TAG = "v4"
RETROARCH_ASSET = "PS5_RetroArch-v4.0.0-beta-4.zip"


def main():
    request = urllib.request.Request(API, headers={"User-Agent": "Obsidian-Release-Index/1.0",
                                                    "Accept": "application/vnd.github+json"})
    with urllib.request.urlopen(request, timeout=30) as res:
        releases = json.load(res)
    assets = {}
    groups = []
    for rel in releases:
        if rel.get("draft") or rel.get("prerelease"):
            continue
        tag = rel.get("tag_name", "")
        release_url = rel.get("html_url", "")
        if not tag or not release_url.startswith(HUB + "/tag/"):
            continue
        files = []
        for a in rel.get("assets", []):
            uri = a.get("browser_download_url", "")
            name = a.get("name", "")
            checksum = a.get("digest", "")
            if not re.fullmatch(r"sha256:[a-fA-F0-9]{64}", checksum):
                continue
            if not uri.startswith("https://github.com/" + REPO + "/releases/download/" + tag + "/"):
                continue
            if not re.fullmatch(r"[A-Za-z0-9_.+-]+\.(zip|elf)", name, re.I):
                continue
            file = {"name": name, "url": uri, "size": a["size"],
                    "sha256": checksum.split(":")[1].lower()}
            assets[(tag, name)] = file
            files.append(file)
        groups.append({"tag": tag, "name": rel.get("name") or tag,
                       "url": release_url, "assets": files})
    platforms = {}
    for platform, (tag, name) in TARGETS.items():
        if (tag, name) not in assets:
            raise SystemExit("Missing verified emulator asset: " + platform + " " + tag + "/" + name)
        platforms[platform] = {"release": tag,
                               "releaseUrl": HUB + "/tag/" + tag,
                               "asset": assets[(tag, name)]}
    if len(RETROARCH_SYSTEMS) != len(set(RETROARCH_SYSTEMS)):
        raise SystemExit("Duplicate RetroArch system title")
    if (RETROARCH_TAG, RETROARCH_ASSET) not in assets:
        raise SystemExit("Missing verified public RetroArch release asset")
    retro_asset = assets[(RETROARCH_TAG, RETROARCH_ASSET)]
    retro_record = {"release": RETROARCH_TAG,
                    "releaseUrl": HUB + "/tag/" + RETROARCH_TAG,
                    "asset": retro_asset, "engine": "retroarch"}
    retro_systems = {system: retro_record for system in RETROARCH_SYSTEMS}
    output = {"repository": REPO, "releasesUrl": HUB,
              "platforms": platforms, "systems": retro_systems, "releases": groups}
    (ROOT / "src/emulatorReleases.json").write_text(
        json.dumps(output, indent=2, ensure_ascii=False)+"\n", encoding="utf8", newline="\n")
    header = ["/* Generated from public GitHub Releases by scripts/generate_emulator_releases.py */",
              "#ifndef OBSIDIAN_EMULATOR_RELEASES_H", "#define OBSIDIAN_EMULATOR_RELEASES_H",
              '#define EMULATOR_RELEASES_URL "'+HUB+'"',
              "typedef struct { const char *platform, *label, *asset_url, *filename, *sha256; unsigned long long size; } EmulatorReleaseAsset;",
              "static const EmulatorReleaseAsset EMULATOR_RELEASE_ASSETS[] = {"]
    for platform, rec in platforms.items():
        file = rec["asset"]
        header.append("    {" + ", ".join(json.dumps(value) for value in
          (platform, rec["release"], file["url"], file["name"], file["sha256"])) +
          ", "+str(file["size"])+"ULL},")
    header.extend([
       "};",
       "#define EMULATOR_RELEASE_ASSET_COUNT (sizeof(EMULATOR_RELEASE_ASSETS)/sizeof(EMULATOR_RELEASE_ASSETS[0]))",
       "static const EmulatorReleaseAsset EMULATOR_RETROARCH_ASSET = {" +
           ", ".join(json.dumps(value) for value in
             ("retroarch", RETROARCH_TAG, retro_asset["url"],
              retro_asset["name"], retro_asset["sha256"])) +
           ", " + str(retro_asset["size"]) + "ULL};",
       "static const char *const EMULATOR_RETROARCH_SYSTEM_NAMES[] = {"
    ])
    for system in RETROARCH_SYSTEMS:
        header.append("    " + json.dumps(system, ensure_ascii=False) + ",")
    header.extend([
        "};",
        "#define EMULATOR_RETROARCH_SYSTEM_COUNT (sizeof(EMULATOR_RETROARCH_SYSTEM_NAMES)/sizeof(EMULATOR_RETROARCH_SYSTEM_NAMES[0]))",
        "#endif", ""
    ])
    (ROOT / "src/emulator_releases.h").write_text("\n".join(header),encoding="utf8",newline="\n")
    print("Releases:",len(groups),"assets:",sum(len(g["assets"]) for g in groups),
          "standalone system matches:",len(platforms),
          "RetroArch systems:",len(retro_systems),
          "total mapped:",len(platforms)+len(retro_systems),
          "unmapped visible tiles:",56-len(platforms)-len(retro_systems))


if __name__ == "__main__":
    main()
