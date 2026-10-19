"""Optional, resumable IGDB cover search for Wolodrive's missing artwork.

Security: credentials ONLY via IGDB_CLIENT_ID / IGDB_CLIENT_SECRET in the
process environment. OAuth token and credentials are never written to disk.
IGDB requests run locally (not from the browser or PS5). No image downloads.
"""
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import re
import sys
import time
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from enrich_artwork import load_records, normalize, safe_url

CACHE = ROOT / ".cache/wolodrive-artwork/igdb-results.json"
EXPORT = ROOT / "assets/artwork/igdb-covers.json"

# Names here are console identities, not game-title heuristics. IDs are read
# from IGDB's own platform endpoint and checked on EVERY candidate game.
PLATFORM_NAMES = {
    "sony-ps1": ("PlayStation", "PlayStation 1"),
    "sony-ps2": ("PlayStation 2",),
    "sony-ps3": ("PlayStation 3",),
    "sony-ps4": ("PlayStation 4",),
    "sony-ps5": ("PlayStation 5",),
    "sony-psp": ("PlayStation Portable", "PSP"),
    "nintendo-switch": ("Nintendo Switch",),
    "nintendo-3ds": ("Nintendo 3DS",),
    "nintendo-ds": ("Nintendo DS",),
    "nintendo-gba": ("Game Boy Advance",),
    "nintendo-gb": ("Game Boy",),
    "nintendo-gbc": ("Game Boy Color",),
    "nintendo-n64": ("Nintendo 64",),
    "nintendo-nes": ("Nintendo Entertainment System",),
    "nintendo-snes": ("Super Nintendo Entertainment System",),
    "nintendo-gc": ("Nintendo GameCube",),
    "nintendo-wii": ("Wii",),
    "nintendo-wiiu": ("Wii U",),
    "microsoft-xbox": ("Xbox",),
    "microsoft-xbox360": ("Xbox 360",),
    "sega-megadrive": ("Sega Mega Drive/Genesis", "Sega Mega Drive", "Sega Genesis"),
    "sega-saturn": ("Sega Saturn",),
    "sega-dreamcast": ("Dreamcast",),
    "sega-cd": ("Sega CD",),
    "sega-32x": ("Sega 32X",),
    "sega-gamegear": ("Game Gear",),
    "sega-master": ("Sega Master System",),
    "atari-2600": ("Atari 2600",),
    "atari-5200": ("Atari 5200",),
    "atari-7800": ("Atari 7800",),
}

def identity(platform, title):
    """Same console + original cleaned title. Never join different platforms."""
    return platform + "|" + normalize(title)


# Distribution labels and update versions are not part of the game name.
# This is deliberately restricted to trailing metadata so that sequels,
# remasters, subtitles and Deluxe/Console editions remain distinct.
REGION_LANGUAGE = re.compile(
    r"\s*\[(?:US|USA|EU|EUR|UK|JP|JPN|ASIA|WORLD|"
    r"EN|ENG|FR|FRE|DE|GER|ES|SPA|IT|ITA|PT|RU|RUS|JA|JPN|"
    r"MULTI\d*|ENGLISH|FRENCH|GERMAN|SPANISH)\]\s*$", re.I)
PATCH_VERSION = re.compile(r"\s*\[(?:v)?\d{1,3}(?:\.\d{1,3}){1,3}\]\s*$", re.I)
CONSOLE_SUFFIX = {
    "sony-ps3": re.compile(r"\s+(?:PS3|PlayStation 3)$", re.I),
    "sony-ps4": re.compile(r"\s+(?:PS4|PlayStation 4)$", re.I),
    "sony-ps5": re.compile(r"\s+(?:PS5|PlayStation 5)$", re.I),
    "sony-psp": re.compile(r"\s+(?:PSP|PlayStation Portable)$", re.I),
    "nintendo-switch": re.compile(r"\s+(?:Switch NSP(?:/XCI)?|Switch XCI|Nintendo Switch)$", re.I),
    "nintendo-3ds": re.compile(r"\s+(?:3DS CIA|3DS)$", re.I),
}


def search_title(platform, title):
    """Drop only known trailing source metadata; preserve game identity."""
    value = str(title or "").strip()
    if not value:
        return ""
    # Strip storefront/distribution tags only when trailing, then repeat:
    # e.g. "Citizen Sleeper - [EU] [EN] [1.05]" -> "Citizen Sleeper".
    # Do not strip DLC labels or editions; DLC needs manual identification.
    for _ in range(12):
        previous = value
        value = re.sub(r"\s*\((?:eShop|update)\)\s*$", "", value, flags=re.I)
        value = re.sub(r"\s*\[(?:Update|eShop)\]\s*$", "", value, flags=re.I)
        value = PATCH_VERSION.sub("", value)
        value = REGION_LANGUAGE.sub("", value)
        suffix = CONSOLE_SUFFIX.get(platform)
        if suffix:
            value = suffix.sub("", value)
        value = re.sub(r"\s*[-–—]\s*$", "", value).strip()
        if value == previous:
            break
    return value


def load_json(path, default):
    return json.loads(path.read_text(encoding="utf-8")) if path.is_file() else default

def atomic_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(".tmp")
    temp.write_text(json.dumps(data, ensure_ascii=False, separators=(",", ":")) + "\n", encoding="utf-8")
    temp.replace(path)

def eligible_titles(records, matches, platforms=None):
    """Missing-only, deduplicated across all sources and editions."""
    names = {}
    for source, game in records:
        if platforms and game["platform"] not in platforms:
            continue
        if game["platform"] not in PLATFORM_NAMES:
            continue
        if game.get("coverUrl") or game.get("boxArtUrl"):
            continue
        if source + "|" + game["id"] in matches:
            continue
        title = game["title"].strip()
        key = identity(game["platform"], title)
        if not normalize(title) or len(title) < 3 or len(title) > 140:
            continue
        names.setdefault(key, (game["platform"], title))
    return names

def cover_url(image_id):
    if not isinstance(image_id, str) or not re.fullmatch(r"co[a-zA-Z0-9]{3,24}", image_id):
        return None
    return f"https://images.igdb.com/igdb/image/upload/t_cover_big/{image_id}.jpg"

def escape_search(value):
    value = re.sub(r"[\x00-\x1f]", " ", value)
    value = value.replace("\\", " ").replace('"', " ")
    value = value.strip()
    if not 2 <= len(value) <= 140:
        raise ValueError("IGDB search title out of bounds")
    return value

def select_match(platform, title, candidates, platform_id):
    """Require unambiguous exact cleaned name, same IGDB platform, cover."""
    if not platform_id:
        return {"status": "skipped-platform"}
    expected = normalize(search_title(platform, title))
    candidates_with_cover = []
    for game in candidates:
        if not isinstance(game, dict) or normalize(game.get("name")) != expected:
            continue
        if platform_id not in (game.get("platforms") or []):
            continue
        image = (game.get("cover") or {}).get("image_id")
        link = cover_url(image)
        if link and isinstance(game.get("id"), int):
            candidates_with_cover.append((game["id"], image, link))
    unique_images = {image for _, image, _ in candidates_with_cover}
    if len(unique_images) == 1:
        game_id, _, link = candidates_with_cover[0]
        return {"status": "matched", "url": link, "gameId": game_id,
                "provider": "igdb-exact-platform-title", "confidence": 95}
    if len(unique_images) > 1:
        return {"status": "review"}
    return {"status": "not-found"}

class IGDBClient:
    """Sequential, 3 req/s ceiling, OAuth secret never appears in request URL."""
    def __init__(self, client_id, client_secret, *, opener=urlopen, sleep=time.sleep,
                 clock=time.monotonic):
        self.client_id, self.client_secret = client_id, client_secret
        self.opener, self.sleep, self.clock = opener, sleep, clock
        self.token = None
        self.last_request = 0.0
        self.calls = 0

    def authenticate(self):
        if self.token:
            return
        body = urlencode({"client_id": self.client_id, "client_secret": self.client_secret,
                          "grant_type": "client_credentials"}).encode("utf-8")
        req = Request("https://id.twitch.tv/oauth2/token", data=body,
                      headers={"Content-Type": "application/x-www-form-urlencoded"},
                      method="POST")
        try:
            with self.opener(req, timeout=20) as response:
                result = json.load(response)
            self.token = result.get("access_token")
            if not isinstance(self.token, str) or not self.token:
                raise RuntimeError("Twitch did not return an access token")
        except HTTPError as error:
            raise RuntimeError(f"IGDB authentication failed (HTTP {error.code})") from None

    def query(self, endpoint, body):
        self.authenticate()
        if endpoint not in ("games", "platforms"):
            raise ValueError("Unrecognised IGDB endpoint")
        elapsed = self.clock() - self.last_request
        if self.calls and elapsed < .35:
            self.sleep(.35 - elapsed)
        headers = {"Client-ID": self.client_id, "Authorization": "Bearer " + self.token,
                   "Accept": "application/json", "Content-Type": "text/plain"}
        req = Request("https://api.igdb.com/v4/" + endpoint, data=body.encode("utf-8"),
                      headers=headers, method="POST")
        for retry in range(4):
            try:
                self.last_request = self.clock()
                self.calls += 1
                with self.opener(req, timeout=25) as response:
                    data = response.read(1024 * 1024 + 1)
                if len(data) > 1024 * 1024:
                    raise ValueError("IGDB response exceeded 1 MiB")
                result = json.loads(data)
                if not isinstance(result, list):
                    raise ValueError("IGDB returned unexpected content")
                return result
            except HTTPError as error:
                if error.code not in (429, 500, 502, 503, 504) or retry == 3:
                    raise RuntimeError(f"IGDB {endpoint} request failed (HTTP {error.code})") from None
                self.sleep(min(8, 1.5 * (retry + 1)))
            except (TimeoutError, URLError):
                if retry == 3:
                    raise RuntimeError(f"IGDB {endpoint} network timeout") from None
                self.sleep(1.5 * (retry + 1))
        raise RuntimeError("IGDB retry failed")

    def platforms(self):
        rows = self.query("platforms", "fields id,name,slug; limit 500;")
        result = {}
        for platform, aliases in PLATFORM_NAMES.items():
            found = {row["id"] for row in rows
                     if isinstance(row.get("id"), int) and
                     normalize(row.get("name")) in {normalize(v) for v in aliases}}
            if len(found) == 1:
                result[platform] = next(iter(found))
        return result

    def search(self, title, platform_id):
        term = escape_search(title)
        body = (f'fields id,name,platforms,cover.image_id; search "{term}"; '
                f'where platforms = ({int(platform_id)}); limit 50;')
        return self.query("games", body)

def scan(client, names, cache, max_queries=50, console_ids=None, progress=None):
    """Only count new game queries. Save cache after each successful lookup."""
    console_ids = console_ids if console_ids is not None else client.platforms()
    entries = cache.setdefault("results", {})
    examined = 0
    for key, (platform, title) in names.items():
        if platform not in console_ids:
            continue
        clean = search_title(platform, title)
        previous = entries.get(key)
        if previous:
            if previous.get("status") == "matched":
                continue
            if previous.get("status") in {"not-found", "review"}:
                # A legacy failed search used the noisy catalogue title.
                # Retry it exactly once if removing distribution labels
                # actually changes the query. Successful matches stay saved.
                last_query = previous.get("searchedTitle", title)
                if normalize(last_query) == normalize(clean):
                    continue
            else:
                continue
        if examined >= max_queries:
            break
        if not clean or len(clean) < 3 or re.search(r"\[(?:DLC|Addon|Add-on)\]", clean, re.I):
            entries[key] = {"status": "skipped-metadata", "searchedTitle": clean}
            atomic_json(CACHE, cache)
            continue
        result = select_match(platform, clean, client.search(clean, console_ids[platform]),
                              console_ids[platform])
        entries[key] = {**result, "searchedTitle": clean}
        examined += 1
        if progress:
            progress(examined, key, result["status"])
        # Resumability is intentional: interruption keeps earlier lookups.
        atomic_json(CACHE, cache)
    return examined

def export_cache(cache, keys=None, path=None):
    path = path if path is not None else EXPORT
    allowed = keys if keys is not None else cache.get("results", {}).keys()
    selected = {key: {"url": row["url"], "provider": "igdb-exact-platform-title",
                      "confidence": 95}
                for key, row in cache.get("results", {}).items()
                if key in allowed and row.get("status") == "matched" and
                row.get("provider") == "igdb-exact-platform-title" and
                isinstance(row.get("confidence"), int) and row["confidence"] >= 95 and
                cover_url(row.get("url", "").rsplit("/", 1)[-1].removesuffix(".jpg")) == row.get("url")}
    atomic_json(path, {"schemaVersion": 1, "matches": selected})
    return len(selected)

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true", help="Show eligible unique titles without credentials")
    parser.add_argument("--max-queries", type=int, default=50, help="Maximum new game queries (default 50)")
    parser.add_argument("--platforms", help="Comma-separated Wolodrive platform IDs")
    parser.add_argument("--retry-unmatched", action="store_true",
                        help="Retry cached not-found/review results; never clear matched results")
    args = parser.parse_args(argv)
    if not 0 <= args.max_queries <= 2000:
        parser.error("--max-queries must be 0..2000")
    requested = set(args.platforms.split(",")) if args.platforms else None
    if requested and (requested - PLATFORM_NAMES.keys()):
        parser.error("Unknown Games/Roms console: " + ", ".join(sorted(requested - PLATFORM_NAMES.keys())))
    records = load_records()
    baseline = load_json(ROOT / "assets/artwork/cover-matches.json", {"matches": {}})["matches"]
    candidates = eligible_titles(records, baseline, requested)
    cache = load_json(CACHE, {"schemaVersion": 1, "results": {}})
    if cache.get("schemaVersion") != 1:
        raise RuntimeError("Unexpected IGDB local cache format")
    if args.retry_unmatched:
        for key in candidates:
            if cache["results"].get(key, {}).get("status") in {"not-found", "review"}:
                del cache["results"][key]
    def needs_query(key):
        row = cache["results"].get(key)
        if row is None:
            return True
        if row.get("status") not in {"not-found", "review"}:
            return False
        original = candidates[key][1]
        return normalize(row.get("searchedTitle", original)) != normalize(
            search_title(candidates[key][0], original))

    pending = [key for key in candidates if needs_query(key)]
    retries = sum(key in cache["results"] for key in pending)
    print(f"Unique uncovered game/platform titles: {len(candidates):,}; "
          f"pending: {len(pending):,} (including {retries:,} improved-title retries); "
          f"already processed: {len(candidates)-len(pending):,}", flush=True)
    if args.dry_run:
        changes = [(name, search_title(platform, name)) for platform, name in candidates.values()
                   if normalize(name) != normalize(search_title(platform, name))]
        print(f"Titles with removable source suffixes: {len(changes):,}")
        for old, new in changes[:8]:
            print(f"  {old!r} -> {new!r}")
    print("Pending by console:", dict(Counter(key.split("|", 1)[0] for key in pending).most_common(12)))
    if args.dry_run:
        return 0
    if args.max_queries == 0:
        # Rebuild an export from local successful lookups without credentials.
        # Do not filter to the currently uncovered catalogue: successful
        # results become "covered" after enrich_artwork.py merges them.
        count = export_cache(cache)
        print(f"IGDB cached matches exported: {count}. No API requests made.")
        print("Run python scripts/enrich_artwork.py --offline to merge covers into Wolodrive.")
        return 0
    cid, secret = os.getenv("IGDB_CLIENT_ID"), os.getenv("IGDB_CLIENT_SECRET")
    if not cid or not secret:
        parser.error("Set IGDB_CLIENT_ID and IGDB_CLIENT_SECRET in your local shell (never in Git)")
    client = IGDBClient(cid, secret)
    ids = client.platforms()
    unavailable = sorted({plat for plat, _ in candidates.values()} - ids.keys())
    if unavailable:
        print("Unresolved IGDB console names (skipped):", ", ".join(unavailable), file=sys.stderr)
    requested_count = scan(client, candidates, cache, args.max_queries, ids,
                           lambda count, key, status: print(f"IGDB {count}/{args.max_queries}: {status} ({key[:85]})",
                                                           flush=True) if count % 10 == 0 else None)
    # Never restrict the export to current missing titles: each enrichment
    # makes earlier successful titles disappear from eligible_titles().
    # Exporting only eligible_titles() would silently erase those covers on
    # the next batch, even though they remain in the IGDB result cache.
    matches = export_cache(cache)
    print(f"Queried: {requested_count}; IGDB cached matches exported: {matches}.", flush=True)
    print("Run python scripts/enrich_artwork.py --offline to merge covers into Wolodrive.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
