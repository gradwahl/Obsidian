"""Match unresolved catalogue entries to LaunchBox front covers, without credentials.

Download Metadata.zip once, stream its XML locally, and export source/id matches.
Only one distinct game on the exact console may match a canonical/official alias.
Existing source artwork and approved matches are never replaced.
"""
import argparse
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import html
import re
import sys
import unicodedata
from urllib.request import Request, urlopen
import xml.etree.ElementTree as ET
import zipfile

from enrich_artwork import ROOT, TAG, EXT, load_records, safe_url
from igdb_artwork import atomic_json, search_title

ARCHIVE = ROOT / ".cache/wolodrive-artwork/launchbox-Metadata.zip"
EXPORT = ROOT / "assets/artwork/launchbox-covers.json"
METADATA_URL = "https://gamesdb.launchbox-app.com/Metadata.zip"
IMAGE_BASE = "https://images.launchbox-app.com/"
PLATFORMS = {
    "Sony Playstation": "sony-ps1", "Sony Playstation 2": "sony-ps2",
    "Sony Playstation 3": "sony-ps3", "Sony Playstation 4": "sony-ps4",
    "Sony Playstation 5": "sony-ps5", "Sony PSP": "sony-psp",
    "Nintendo Switch": "nintendo-switch", "Nintendo 3DS": "nintendo-3ds",
    "Nintendo DS": "nintendo-ds", "Nintendo Game Boy": "nintendo-gb",
    "Nintendo Game Boy Color": "nintendo-gbc", "Nintendo Game Boy Advance": "nintendo-gba",
    "Nintendo GameCube": "nintendo-gc", "Nintendo 64": "nintendo-n64",
    "Nintendo Entertainment System": "nintendo-nes", "Super Nintendo Entertainment System": "nintendo-snes",
    "Nintendo Wii": "nintendo-wii", "Nintendo Wii U": "nintendo-wiiu",
    "Nintendo Pokemon Mini": "nintendo-pokemini", "Microsoft Xbox": "microsoft-xbox",
    "Microsoft Xbox 360": "microsoft-xbox360", "Sega Genesis": "sega-megadrive",
    "Sega CD": "sega-cd", "Sega 32X": "sega-32x", "Sega Saturn": "sega-saturn",
    "Sega Dreamcast": "sega-dreamcast", "Sega Game Gear": "sega-gamegear",
    "Sega Master System": "sega-master", "Atari 2600": "atari-2600",
    "Atari 5200": "atari-5200", "Atari 7800": "atari-7800",
}


def title_key(title):
    # Preserve non-Latin letters: dropping them can turn unrelated titles into
    # the same numeric key. Keep edition/DLC words; never use fuzzy matching.
    value = html.unescape(str(title or "")).strip()
    value = EXT.sub("", value)
    while TAG.search(value):
        value = TAG.sub("", value).strip()
    value = re.sub(r"^(.+?),\s*(The|An|A)(?=:|$)(.*)$", r"\2 \1\3", value, flags=re.I)
    value = unicodedata.normalize("NFKD", value).casefold()
    return "".join(c for c in value if c.isalnum())


def image_url(filename):
    if not isinstance(filename, str) or not re.fullmatch(
            r"[a-fA-F0-9]{8}(?:-[a-fA-F0-9]{4}){3}-[a-fA-F0-9]{12}\.(?:jpg|png|webp)", filename):
        return None
    return IMAGE_BASE + filename


def trusted_match(row):
    return (isinstance(row, dict) and row.get("provider") in
            ("launchbox-exact-platform-title", "launchbox-exact-platform-alias")
            and type(row.get("confidence")) is int and row["confidence"] >= 95
            and isinstance(row.get("url"), str)
            and row["url"].startswith(IMAGE_BASE)
            and image_url(row["url"][len(IMAGE_BASE):]) == row["url"])


def fetch_archive(path, refresh=False):
    if path.is_file() and not refresh:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".download.tmp")
    try:
        with urlopen(Request(METADATA_URL, headers={"User-Agent": "Obsidian-Artwork/1.0"}), timeout=60) as response:
            with temporary.open("wb") as output:
                total = 0
                while chunk := response.read(1024 * 1024):
                    total += len(chunk)
                    if total > 256 * 1024 * 1024:
                        raise ValueError("LaunchBox archive exceeds 256 MiB")
                    output.write(chunk)
        with zipfile.ZipFile(temporary) as archive:
            if "Metadata.xml" not in archive.namelist():
                raise ValueError("LaunchBox archive has no Metadata.xml")
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def read_database(path, wanted):
    games, names, covers = {}, defaultdict(set), defaultdict(list)
    with zipfile.ZipFile(path) as archive:
        if archive.getinfo("Metadata.xml").file_size > 1024 * 1024 * 1024:
            raise ValueError("LaunchBox Metadata.xml exceeds 1 GiB")
        with archive.open("Metadata.xml") as stream:
            events = ET.iterparse(stream, events=("start", "end"))
            _, root = next(events)
            depth = 1
            for event, node in events:
                if event == "start":
                    depth += 1
                    continue
                if depth == 2:
                    identity = node.findtext("DatabaseID", "")
                    if node.tag == "Game":
                        platform = PLATFORMS.get(node.findtext("Platform", ""))
                        if platform:
                            name = title_key(node.findtext("Name", ""))
                            games[identity] = (platform, name)
                            if (platform, name) in wanted:
                                names[(platform, name)].add((identity, "title"))
                    elif node.tag == "GameAlternateName" and identity in games:
                        platform = games[identity][0]
                        name = title_key(node.findtext("AlternateName", ""))
                        if (platform, name) in wanted:
                            names[(platform, name)].add((identity, "alias"))
                    elif node.tag == "GameImage" and node.findtext("Type") == "Box - Front":
                        url = image_url(node.findtext("FileName"))
                        if url and identity in games:
                            covers[identity].append((node.findtext("Region", "") or "", url))
                    node.clear()
                    root.clear()
                depth -= 1
    return names, covers


def select_cover(title, covers):
    # Explicit source regions win. With no explicit region, select an English
    # front scan deterministically after proving a single game identity.
    regions = []
    tags = " ".join(re.findall(r"[\[(]([^\])]+)[\])]", title))
    for pattern, region in ((r"\b(?:USA|US|NTSC-U)\b", "North America"),
                            (r"\b(?:EUR|Europe|PAL)\b", "Europe"),
                            (r"\b(?:Japan|JPN|JP|NTSC-J)\b", "Japan")):
        if re.search(pattern, tags, re.I):
            regions.append(region)
    if len(regions) > 1:
        return None
    if regions:
        return next((url for region, url in sorted(covers) if region == regions[0]), None)
    priorities = {"World": 0, "North America": 1, "Europe": 2, "": 3}
    return min(covers, key=lambda row: (priorities.get(row[0], 4), row[0], row[1]))[1] if covers else None


def match_records(records, baseline, path):
    pending = [(source + "|" + game["id"], game) for source, game in records
               if source + "|" + game["id"] not in baseline
               and not safe_url(game.get("boxArtUrl") or game.get("coverUrl"))]
    wanted = {(g["platform"], title_key(search_title(g["platform"], g["title"]))) for _, g in pending}
    names, covers = read_database(path, wanted)
    matches, counts = {}, Counter()
    for identity, game in pending:
        key = (game["platform"], title_key(search_title(game["platform"], game["title"])))
        candidates = names.get(key, set()) if key[1] else set()
        ids = {row[0] for row in candidates}
        if len(ids) != 1:
            counts["ambiguous" if ids else "not-found"] += 1
            continue
        game_id, = ids
        url = select_cover(game["title"], covers.get(game_id, []))
        if not url:
            counts["no-front-cover-or-region"] += 1
            continue
        via = "title" if (game_id, "title") in candidates else "alias"
        matches[identity] = {"url": url, "provider": "launchbox-exact-platform-" + via,
                             "confidence": 95, "databaseId": game_id}
        counts["matched"] += 1
    return matches, dict(counts)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--offline", action="store_true", help="Use the saved metadata archive only")
    parser.add_argument("--refresh", action="store_true", help="Replace the cached metadata archive")
    parser.add_argument("--download", action="store_true", help="Download merged LaunchBox covers into assets/covers")
    parser.add_argument("--workers", type=int, choices=range(1, 5), default=3)
    args = parser.parse_args(argv)
    if args.offline and args.refresh:
        parser.error("--offline and --refresh cannot be combined")
    if args.offline and not ARCHIVE.is_file():
        parser.error("No cached LaunchBox metadata archive")
    if not args.offline:
        fetch_archive(ARCHIVE, args.refresh)
    baseline = json.loads((ROOT / "assets/artwork/cover-matches.json").read_text(encoding="utf-8"))["matches"]
    # Include earlier LaunchBox rows when re-running so exports stay stable.
    previous = json.loads(EXPORT.read_text(encoding="utf-8"))["matches"] if EXPORT.is_file() else {}
    prior = {k: previous.get(k, v) if trusted_match(previous.get(k))
             and previous[k]["url"] == v["url"] else v
             for k, v in baseline.items() if trusted_match(v)}
    matches, counts = match_records(load_records(), baseline, ARCHIVE)
    atomic_json(EXPORT, {"schemaVersion": 1, "matches": dict(sorted({**prior, **matches}.items()))})
    print("LaunchBox candidates:", counts, flush=True)
    import enrich_artwork
    if enrich_artwork.main(["--offline"]):
        return 1
    if args.download:
        import download_artwork as downloader
        manifest = json.loads((ROOT / "assets/artwork/cover-matches.json").read_text(encoding="utf-8"))["matches"]
        urls = sorted({row["url"] for row in manifest.values() if trusted_match(row)})
        output = ROOT / "assets/covers"
        output.mkdir(parents=True, exist_ok=True)
        results = {}
        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            jobs = {pool.submit(downloader.download, url, output): url for url in urls}
            for future in as_completed(jobs):
                results[jobs[future]] = future.result()
                if len(results) % 50 == 0:
                    print(f"Downloaded/checked {len(results)}/{len(urls)}", flush=True)
        atomic_json(output / "launchbox-downloads.json", {"artwork": results})
        counts = Counter(row["status"] for row in results.values())
        print("LaunchBox downloads:", dict(counts), flush=True)
        return int(bool(counts["failed"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
