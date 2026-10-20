"""Enrich game metadata with trusted artwork links; never fetch game files.

Priority: existing art, unique same-console/title cover, unambiguous Libretro
Named_Boxarts match. Downloads are explicitly opt-in and bounded.
"""
import argparse
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from html.parser import HTMLParser
import html
import json
from pathlib import Path
import re
import sys
import time
import unicodedata
from urllib.parse import quote, unquote, urljoin, urlsplit
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
IGDB_PROVIDERS = ("igdb-exact-platform-title", "igdb-exact-platform-alias")
CACHE = ROOT / ".cache" / "wolodrive-artwork"
FULL = ("axekin", "superpsx", "downloadgamepsp", "ziperto", "nopaystation",
        "psn-content", "alvro", "maelly-pooh", "romstime")
SYSTEMS = {
    "nintendo-nes": "Nintendo - Nintendo Entertainment System",
    "nintendo-snes": "Nintendo - Super Nintendo Entertainment System",
    "nintendo-n64": "Nintendo - Nintendo 64",
    "nintendo-gc": "Nintendo - GameCube",
    "nintendo-wii": "Nintendo - Wii",
    "nintendo-wiiu": "Nintendo - Wii U",
    "nintendo-switch": "Nintendo - Nintendo Switch",
    "nintendo-3ds": "Nintendo - Nintendo 3DS",
    "nintendo-ds": "Nintendo - Nintendo DS",
    "nintendo-gba": "Nintendo - Game Boy Advance",
    "nintendo-gbc": "Nintendo - Game Boy Color",
    "nintendo-gb": "Nintendo - Game Boy",
    "nintendo-pokemini": "Nintendo - Pokemon Mini",
    "sony-ps1": "Sony - PlayStation",
    "sony-ps2": "Sony - PlayStation 2",
    "sony-psp": "Sony - PlayStation Portable",
    "sony-ps3": "Sony - PlayStation 3",
    "sega-megadrive": "Sega - Mega Drive - Genesis",
    "sega-cd": "Sega - Mega-CD - Sega CD",
    "sega-saturn": "Sega - Saturn",
    "sega-dreamcast": "Sega - Dreamcast",
    "sega-gamegear": "Sega - Game Gear",
    "sega-master": "Sega - Master System - Mark III",
    "sega-32x": "Sega - 32X",
    "atari-2600": "Atari - 2600",
    "atari-5200": "Atari - 5200",
    "atari-7800": "Atari - 7800",
    "microsoft-xbox": "Microsoft - Xbox",
    "microsoft-xbox360": "Microsoft - Xbox 360",
}
TAG = re.compile(r"\s*(?:\((?:USA|Europe|Japan|World|Australia|Asia|Rev\s*[\w.]+|Disc\s*\d+|Disk\s*\d+|Beta|Proto|Unl)\)|\[(?:!|b\d+|a\d+|h\d+)\])\s*$", re.I)
EXT = re.compile(r"\.(?:zip|7z|rar|iso|cso|chd|pkg|nsp|xci|cia|nds|gba|gbc|gb|nes|sfc|smc|n64)$", re.I)


def normalize(value, *, tags=True):
    value = html.unescape(str(value or "")).strip()
    value = EXT.sub("", value)
    if tags:
        while TAG.search(value):
            value = TAG.sub("", value).strip()
    value = unicodedata.normalize("NFKD", value).casefold()
    value = "".join(ch for ch in value if not unicodedata.combining(ch))
    return re.sub(r"[^a-z0-9]+", "", value)


def safe_url(url):
    return isinstance(url, str) and url.startswith("https://") and bool(urlsplit(url).netloc)


def load_records(root=ROOT):
    folder = root / "assets/catalogs"
    full = {name for name in FULL if (folder / (name + ".json")).exists()}
    files = [folder / "additional.json"] + [folder / (s + ".json") for s in sorted(full)]
    records = []
    for file in files:
        for source in json.loads(file.read_text(encoding="utf-8"))["sources"]:
            if file.name == "additional.json" and source["id"] in full:
                continue
            for game in source.get("packages", []):
                if game.get("id") and game.get("title") and game.get("platform"):
                    records.append((source["name"], game))
    return records


class Links(HTMLParser):
    def __init__(self):
        super().__init__()
        self.files = []

    def handle_starttag(self, tag, attrs):
        if tag != "a":
            return
        href = dict(attrs).get("href", "")
        name = unquote(href.split("?", 1)[0])
        if name.lower().endswith(".png") and "/" not in name and "\\" not in name:
            self.files.append((name[:-4], href))


def libretro_index(platform, offline=False, cache=CACHE):
    cache.mkdir(parents=True, exist_ok=True)
    file = cache / (platform + ".html")
    directory = "https://thumbnails.libretro.com/" + quote(SYSTEMS[platform], safe="") + "/Named_Boxarts/"
    if not file.exists() or (not offline and time.time() - file.stat().st_mtime > 604800):
        if offline:
            return None
        req = Request(directory, headers={"User-Agent": "Wolodrive-Artwork/1.0"})
        with urlopen(req, timeout=25) as response:
            data = response.read(20 * 1024 * 1024 + 1)
        if len(data) > 20 * 1024 * 1024:
            raise ValueError(f"Libretro index too large: {platform}")
        tmp = file.with_suffix(".tmp")
        tmp.write_bytes(data)
        tmp.replace(file)
    parser = Links()
    parser.feed(file.read_text(encoding="utf-8", errors="replace"))
    if not parser.files:
        raise ValueError(f"Empty Libretro index: {platform}")
    strict, relaxed = defaultdict(set), defaultdict(set)
    for title, href in parser.files:
        url = urljoin(directory, href)
        if not safe_url(url) or urlsplit(url).netloc != "thumbnails.libretro.com":
            continue
        a, b = normalize(title, tags=False), normalize(title)
        if a:
            strict[a].add(url)
        if b:
            relaxed[b].add(url)
    return {"strict": strict, "relaxed": relaxed, "count": len(parser.files)}


def match_records(records, indexes, igdb_matches=None):
    igdb_matches = igdb_matches or {}
    existing = defaultdict(set)
    for name, game in records:
        url = game.get("boxArtUrl") or game.get("coverUrl")
        key = (game["platform"], normalize(game["title"]))
        if safe_url(url) and key[1]:
            existing[key].add(url)

    matches, totals = {}, Counter()
    by_source, by_platform, providers = defaultdict(Counter), defaultdict(Counter), Counter()
    review = []
    for name, game in records:
        key = (game["platform"], normalize(game["title"]))
        url = game.get("boxArtUrl") or game.get("coverUrl")
        status, via, confidence = "missing", "", 0
        if safe_url(url):
            status = "direct"
        else:
            shared = existing.get(key, set())
            if len(shared) == 1:
                url, = shared
                status, via, confidence = "matched", "same-console-title", 96
            elif len(shared) > 1:
                status = "review"
            else:
                index = indexes.get(game["platform"])
                if index:
                    strict = index["strict"].get(normalize(game["title"], tags=False), set())
                    loose = index["relaxed"].get(key[1], set())
                    if len(strict) == 1:
                        url, = strict
                        status, via, confidence = "matched", "libretro-exact", 95
                    elif len(strict) > 1:
                        status = "review"
                    elif len(loose) == 1:
                        url, = loose
                        status, via, confidence = "matched", "libretro-unique", 85
                    elif len(loose) > 1:
                        status = "review"
        if status == "missing":
            igdb = igdb_matches.get(key[0] + "|" + key[1])
            if (isinstance(igdb, dict) and igdb.get("provider") in IGDB_PROVIDERS
                    and igdb.get("confidence", 0) >= 95 and
                    safe_url(igdb.get("url")) and
                    urlsplit(igdb["url"]).hostname == "images.igdb.com"):
                url = igdb["url"]
                status, via, confidence = "matched", igdb["provider"], 95
        totals[status] += 1
        by_source[name][status] += 1
        by_platform[game["platform"]][status] += 1
        if status == "matched":
            providers[via] += 1
            matches[name + "|" + game["id"]] = {"url": url, "provider": via, "confidence": confidence}
        elif status == "review" and len(review) < 100:
            review.append({"source": name, "title": game["title"], "platform": game["platform"]})
    return matches, {"total": len(records), "status": dict(totals), "providers": dict(providers),
                     "sources": {name: dict(v) for name, v in sorted(by_source.items())},
                     "platforms": {name: dict(v) for name, v in sorted(by_platform.items())},
                     "reviewSamples": review}


def write_outputs(matches, report, root=ROOT):
    directory = root / "assets/artwork"
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / "cover-matches.json"
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps({"schemaVersion": 1, "matches": matches}, ensure_ascii=False,
                              separators=(",", ":")) + "\n", encoding="utf-8")
    tmp.replace(path)
    native = set()
    for filename in ("additional.json", "axekin.json"):
        file = root / "assets/catalogs" / filename
        if file.exists():
            for src in json.loads(file.read_text(encoding="utf-8"))["sources"]:
                native.update((src["name"], game["id"]) for game in src.get("packages", []))
    lines = []
    for source, identity in sorted(native):
        found = matches.get(source + "|" + identity)
        if found and not any(ch in source + identity + found["url"] for ch in "\t\n\r"):
            lines.append("\t".join((source, identity, found["url"])))
    tsv = directory / "cover-matches.tsv"
    tmp = tsv.with_suffix(".tmp")
    tmp.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")
    tmp.replace(tsv)
    report["nativeOverrideCount"] = len(lines)
    document = root / "docs/artwork_coverage.md"
    def coverage(c):
        good = c.get("direct", 0) + c.get("matched", 0)
        return f"{good * 100 / max(1, sum(c.values())):.1f}%"
    output = [
        "# Artwork matching coverage", "",
        "These counts describe metadata matches, not downloaded or verified images.",
        "Ambiguous edition/region covers are withheld for manual review.", "",
        f"**Entries:** {report['total']:,} | **direct art:** {report['status'].get('direct',0):,} | "
        f"**matched:** {report['status'].get('matched',0):,} | "
        f"**review:** {report['status'].get('review',0):,} | "
        f"**missing:** {report['status'].get('missing',0):,}",
        "", f"Native artwork overrides: {len(lines):,}", "",
        "## Sources", "",
        "| Source | Direct | Matched | Review | Missing | Coverage |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for name, counts in report["sources"].items():
        output.append(f"| {name} | {counts.get('direct',0):,} | {counts.get('matched',0):,} | "
                      f"{counts.get('review',0):,} | {counts.get('missing',0):,} | {coverage(counts)} |")
    output += ["", "## Consoles", "", "| Console | Direct | Matched | Review | Missing | Coverage |",
               "|---|---:|---:|---:|---:|---:|"]
    for name, counts in report["platforms"].items():
        output.append(f"| {name} | {counts.get('direct',0):,} | {counts.get('matched',0):,} | "
                      f"{counts.get('review',0):,} | {counts.get('missing',0):,} | {coverage(counts)} |")
    output += ["", "## Providers", ""]
    output += [f"- {name}: {count:,}" for name, count in sorted(report["providers"].items())]
    output += ["", "## Ambiguous samples (not automatically assigned)", ""]
    output += [f"- {x['platform']} / {x['source']}: {x['title']}" for x in report["reviewSamples"][:30]]
    document.write_text("\n".join(output) + "\n", encoding="utf-8")
    return path, tsv


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--systems", default="all", help="Comma-separated platform keys, or all")
    parser.add_argument("--offline", action="store_true", help="Use cached Libretro indexes only")
    parser.add_argument("--download", type=int, default=0, help="Bounded opt-in image downloads (0-1000)")
    parser.add_argument("--workers", type=int, default=4, choices=range(1, 7))
    args = parser.parse_args(argv)
    if not 0 <= args.download <= 1000:
        parser.error("--download must be 0 to 1000")
    records = load_records()
    wanted = {entry["platform"] for _, entry in records} & SYSTEMS.keys()
    if args.systems != "all":
        keys = set(args.systems.split(","))
        if keys - SYSTEMS.keys():
            parser.error("Unknown system(s): " + ", ".join(keys - SYSTEMS.keys()))
        wanted &= keys
    print(f"Scanning {len(records):,} entries against {len(wanted)} Libretro system indexes.", flush=True)
    indexes, failures = {}, {}
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        jobs = {pool.submit(libretro_index, system, offline=args.offline): system for system in sorted(wanted)}
        for future in as_completed(jobs):
            system = jobs[future]
            try:
                data = future.result()
                if data:
                    indexes[system] = data
                else:
                    failures[system] = "Index not cached"
            except Exception as error:
                failures[system] = str(error)
            print(f"{system}: {'available' if system in indexes else 'unavailable'}", flush=True)
    # Do not overwrite a complete manifest when a temporary CDN outage
    # prevents reading most/all indexes. Switch has no Libretro boxart index.
    failed_expected = {"nintendo-switch"}
    unexpected = set(failures) - failed_expected
    if unexpected and args.systems == "all":
        print("Artwork refresh incomplete; preserving previous manifest: " +
              json.dumps({k: failures[k] for k in sorted(unexpected)}), file=sys.stderr)
        return 1
    igdb_file = ROOT / "assets/artwork/igdb-covers.json"
    igdb = {}
    if igdb_file.is_file():
        exported = json.loads(igdb_file.read_text(encoding="utf-8"))
        if exported.get("schemaVersion") != 1 or not isinstance(exported.get("matches"), dict):
            raise ValueError("IGDB artwork export has an unexpected schema")
        igdb = exported["matches"]
    matches, report = match_records(records, indexes, igdb)
    path, native = write_outputs(matches, report)
    print(f"Results: {report['status']}; providers: {report['providers']}", flush=True)
    print(f"Manifest: {path}; native TSV: {native}", flush=True)
    if args.download:
        sys.path.insert(0, str(ROOT / "scripts"))
        import download_artwork
        folder = ROOT / "assets/covers"
        folder.mkdir(parents=True, exist_ok=True)
        urls = list(dict.fromkeys(value["url"] for value in matches.values()))[:args.download]
        counts = Counter()
        for url in urls:
            result = download_artwork.download(url, folder)
            counts[result["status"]] += 1
        print(f"Optional downloads: {dict(counts)}", flush=True)
    if failures:
        print("Unindexed systems: " + json.dumps(failures), file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
