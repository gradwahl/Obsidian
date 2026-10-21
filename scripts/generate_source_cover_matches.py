#!/usr/bin/env python3
"""Build metadata-only artwork matches for public multi-console source catalogues.

Uses the archive's Git *tree*, not its binary blobs. Matches require a
same-console exact normalized title and a unique artwork candidate; ambiguous
editions remain unassigned. No image data or authentication is embedded.
"""
from __future__ import annotations

import json
from pathlib import Path

from generate_archive_artwork import RAW, HASH_SUFFIX, normalize, encode_path, tree_paths

ROOT = Path(__file__).resolve().parents[1]
ARTWORK_OUT = ROOT / "src/archiveSourceCovers.json"

CONSOLES = {
    "sony-ps1": "Sony/PlayStation",
    "sony-ps2": "Sony/PlayStation 2",
    "sony-ps3": "Sony/PlayStation 3",
    "sony-ps4": "Sony/PlayStation 4",
    "sony-ps5": "Sony/PlayStation 5",
    "sony-psp": "Sony/PlayStation Portable",
    "nintendo-switch": "Nintendo/Nintendo Switch",
    "nintendo-ds": "Nintendo/Nintendo DS",
    "nintendo-3ds": "Nintendo/Nintendo 3DS",
    "nintendo-nes": "Nintendo/NES",
    "nintendo-snes": "Nintendo/SNES",
    "nintendo-n64": "Nintendo/Nintendo 64",
    "nintendo-gc": "Nintendo/GameCube",
    "nintendo-wii": "Nintendo/Wii",
    "nintendo-wiiu": "Nintendo/Wii U",
    "nintendo-gb": "Nintendo/Game Boy",
    "nintendo-gbc": "Nintendo/Game Boy Color",
    "nintendo-gba": "Nintendo/Game Boy Advance",
    "nintendo-pokemini": "Nintendo/Pokemon Mini",
    "microsoft-xbox": "Microsoft/Xbox",
    "microsoft-xbox360": "Microsoft/Xbox 360",
    "sega-megadrive": "Sega/Mega Drive - Genesis",
    "sega-dreamcast": "Sega/Dreamcast",
    "sega-saturn": "Sega/Saturn",
    "sega-gamegear": "Sega/Game Gear",
    "sega-cd": "Sega/Sega CD",
    "sega-32x": "Sega/32X",
    "atari-2600": "Atari/Atari 2600",
    "atari-5200": "Atari/Atari 5200",
    "atari-7800": "Atari/Atari 7800",
}


def build(paths: list[str], catalogues: list[dict]) -> tuple[dict[str, str], dict[str, int]]:
    candidates: dict[tuple[str, str], list[tuple[str, str]]] = {}
    for path in paths:
        if not path.endswith(".art") or not path.startswith("assets/covers/"):
            continue
        for platform, folder in CONSOLES.items():
            prefix = "assets/covers/" + folder + "/"
            if path.startswith(prefix):
                name = path[len(prefix):-4]
                if name and "/" not in name:
                    candidates.setdefault((platform, normalize(name)), []).append((path, name))
                break

    matched: dict[str, str] = {}
    missing = 0
    ambiguous = 0
    ignored = 0
    for game in catalogues:
        platform = str(game.get("platform", ""))
        title = str(game.get("title", "")).strip()
        if not title or platform not in CONSOLES:
            ignored += 1
            continue
        options = candidates.get((platform, normalize(title)), [])
        plain = [p for p, name in options if not HASH_SUFFIX.search(name)]
        if len(plain) == 1:
            selected = plain[0]
        elif len(options) == 1:
            selected = options[0][0]
        else:
            if options: ambiguous += 1
            else: missing += 1
            continue
        key = platform + "|" + title
        url = encode_path(selected, RAW)
        if len(url.encode("utf8")) >= 512:
            ignored += 1
            continue
        if key in matched and matched[key] != url:
            # Do not silently pick between different artworks with same key.
            del matched[key]
            ambiguous += 1
            continue
        # Store URL-encoded paths only; the runtime owns the public HTTPS origin.
        matched[key] = url[len(RAW):]
    return dict(sorted(matched.items())), {
        "missing": missing, "ambiguous": ambiguous, "unsupported_or_invalid": ignored
    }


def main() -> None:
    source_dir = ROOT / "assets/catalogs"
    catalogues = []
    full_ids = {p.stem for p in source_dir.glob("*.json") if p.stem != "additional"}
    for file in sorted(source_dir.glob("*.json")):
        snapshot = json.loads(file.read_text("utf8"))
        for source in snapshot.get("sources", []):
            if file.stem == "additional" and source["id"] in full_ids:
                continue
            if source["id"] == "retrogamechampion":
                continue
            catalogues.extend(source.get("packages", []))
    matched, coverage = build(tree_paths(), catalogues)
    ARTWORK_OUT.write_text(
        json.dumps(matched, ensure_ascii=False, separators=(",", ":")) + "\n",
        encoding="utf8", newline="\n"
    )
    print("Verified public source artwork:", len(matched),
          "unique platform/title matches from", len(catalogues), "catalogue entries")
    print("Unmatched or excluded:", coverage)
    print("Metadata output:", ARTWORK_OUT.name, ARTWORK_OUT.stat().st_size, "bytes")


if __name__ == "__main__":
    main()
