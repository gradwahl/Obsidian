"""Optional, resumable IGDB cover search for Wolodrive's missing artwork.

Security: credentials ONLY via IGDB_CLIENT_ID / IGDB_CLIENT_SECRET in the
process environment. OAuth token and credentials are never written to disk.
IGDB requests run locally (not from the browser or PS5). No image downloads.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
from itertools import islice
from threading import Lock
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
from enrich_artwork import IGDB_PROVIDERS, load_records, normalize, safe_url

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

# Match whole distribution suffixes only, on their actual console. Unknown
# tokens (DLC, UNDUB, translations, hacks, editions) prevent removal.
REGION_GROUP = r"\((?:USA|EUR|EU|JPN|JP)(?:/(?:USA|EUR|EU|JPN|JP))*\)"
PACKAGING_SUFFIX = {
    "nintendo-3ds": re.compile(
        r"\s+(?:(?:\[Decrypted\]|Decrypted)\s+)?3DS\s+"
        r"(?:(?:CIA|ROM|Legit|Decrypted|\[Decrypted\]|&|eShop|\(eShop\)|"
        r"Region Free|\(Region Free\)|\[RF\]|\(NEW3DS ONLY\)|\(Updated\)|" +
        REGION_GROUP + r")\s*)+$", re.I),
    "nintendo-wiiu": re.compile(
        r"\s+(?:for\s+)?WII\s+U\s+ISO\s*"
        r"(?:(?:\(Loadiine\)|\(UPDATE(?:\s+v\d+)?\)|" +
        REGION_GROUP + r")\s*)*$", re.I),
}
PRODUCT_SUFFIX = {
    "sony-psp": re.compile(r"\s*\((?:ULUS|ULES|ULJM|ULJS|UCUS|UCES|UCJS|NPJH|NPUH|NPEH)-?\d{5}\)$", re.I),
    "sony-ps3": re.compile(r"\s*\((?:BLUS|BLES|BLAS|BLJM|BCUS|BCES|BCAS|BCJS|NPUB|NPEB|NPJB)-?\d{5}\)$", re.I),
}


def match_name(title):
    """Canonicalise a trailing English article without dropping title words."""
    title = re.sub(r"^(.+?),\s*(The|An|A)(?=:|$)(.*)$", r"\2 \1\3",
                   str(title or "").strip(), flags=re.I)
    return normalize(title)


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
        packaging = PACKAGING_SUFFIX.get(platform)
        if packaging:
            value = packaging.sub("", value)
        product = PRODUCT_SUFFIX.get(platform)
        if product:
            value = product.sub("", value)
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
    # APICalypse uses braces, semicolons and quotes as query syntax.
    # Never allow catalogue titles to escape a named Multi-Query block.
    value = re.sub(r'[\\\"{};]', " ", value)
    value = re.sub(r"\s+", " ", value).strip()
    if not 2 <= len(value) <= 140:
        raise ValueError("IGDB search title out of bounds")
    return value

def game_names(game, aliases=False):
    """Canonical title plus published alternatives, excluding non-title labels."""
    names = {match_name(game.get("name"))}
    if aliases:
        for row in game.get("alternative_names", []):
            comment = row.get("comment", "")
            if re.search(r"acronym|abbreviat|working|development|prototype|executable|filename|misspell|incorrect|erroneous",
                         comment, re.I):
                continue
            name = match_name(row["name"])
            if len(name) >= 3:
                names.add(name)
    return names - {""}


def select_match(platform, title, candidates, platform_id, *, aliases=False):
    """Require unambiguous exact cleaned name, same IGDB platform, cover."""
    if not platform_id:
        return {"status": "skipped-platform"}
    expected = match_name(search_title(platform, title))
    candidates_with_cover = []
    matching_games = set()
    used_alias = False
    for game in candidates:
        if not isinstance(game, dict) or expected not in game_names(game, aliases):
            continue
        if platform_id not in (game.get("platforms") or []):
            continue
        if type(game.get("id")) is not int:
            continue
        matching_games.add(game["id"])
        used_alias |= match_name(game.get("name")) != expected
        image = (game.get("cover") or {}).get("image_id")
        link = cover_url(image)
        if link and isinstance(game.get("id"), int):
            candidates_with_cover.append((game["id"], image, link))
    # An alias can identify a different edition/game, even if only one of
    # those games has a cover or their current cover images happen to agree.
    if aliases and used_alias and len(matching_games) > 1:
        return {"status": "review"}
    unique_images = {image for _, image, _ in candidates_with_cover}
    if len(unique_images) == 1:
        game_id, _, link = candidates_with_cover[0]
        return {"status": "matched", "url": link, "gameId": game_id,
                "provider": "igdb-exact-platform-alias" if used_alias else "igdb-exact-platform-title",
                "confidence": 95}
    if len(unique_images) > 1:
        return {"status": "review"}
    return {"status": "not-found"}

class IGDBClient:
    """Thread-safe ~3 requests/s across workers; tokens never written to disk."""
    def __init__(self, client_id, client_secret, *, opener=urlopen, sleep=time.sleep,
                 clock=time.monotonic):
        self.client_id, self.client_secret = client_id, client_secret
        self.opener, self.sleep, self.clock = opener, sleep, clock
        self.token = None
        self.last_request = 0.0
        self.calls = 0
        self.fallback_searches = 0
        self.request_lock = Lock()

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
        if endpoint not in ("games", "platforms", "multiquery"):
            raise ValueError("Unrecognised IGDB endpoint")
        headers = {"Client-ID": self.client_id, "Authorization": "Bearer " + self.token,
                   "Accept": "application/json", "Content-Type": "text/plain"}
        req = Request("https://api.igdb.com/v4/" + endpoint, data=body.encode("utf-8"),
                      headers=headers, method="POST")
        for retry in range(4):
            try:
                # Space *all* requests, including retries, across every worker.
                # Hold the lock for pacing only, never for network I/O.
                with self.request_lock:
                    elapsed = self.clock() - self.last_request
                    if self.calls and elapsed < .35:
                        self.sleep(.35 - elapsed)
                    self.last_request = self.clock()
                    self.calls += 1
                with self.opener(req, timeout=25) as response:
                    data = response.read(4 * 1024 * 1024 + 1)
                if len(data) > 4 * 1024 * 1024:
                    raise ValueError("IGDB response exceeded 4 MiB")
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

    def multi_search(self, requests):
        """Search up to ten games with verified handling of sparse responses.

        IGDB can omit named result blocks for searches without hits. An
        omitted block is *not* evidence the game is absent, so query only
        those titles individually before caching any 'not-found' result.
        Never silently accept unknown, duplicated, or malformed blocks.
        """
        if not 1 <= len(requests) <= 10:
            raise ValueError("IGDB Multi-Query requires 1-10 game searches")
        queries = []
        for number, (title, platform_id) in enumerate(requests):
            term = escape_search(title)
            queries.append(
                f'query games "game{number}" {{ '
                f'fields id,name,platforms,cover.image_id; search "{term}"; '
                f'where platforms = ({int(platform_id)}); limit 50; }};'
            )
        response = self.query("multiquery", "\n".join(queries))
        results = {}
        for block in response:
            if not isinstance(block, dict):
                raise ValueError("Invalid IGDB Multi-Query response block")
            name = block.get("name")
            match = re.fullmatch(r"game(\d+)", name) if isinstance(name, str) else None
            if not match or int(match[1]) >= len(requests) or name in results:
                raise ValueError("Unrecognised or duplicate IGDB Multi-Query result")
            rows = block.get("result")
            if rows is not None and not isinstance(rows, list):
                raise ValueError("Invalid IGDB Multi-Query game rows")
            results[name] = rows

        # Some Multi-Query responses exclude a name entirely when no games
        # matched. Verify absent blocks (or a block missing 'result') with
        # the normal /games endpoint; its [] is a confirmed empty search.
        # These requests use the same shared rate limiter and retry policy.
        for index, (title, platform_id) in enumerate(requests):
            name = f"game{index}"
            if results.get(name) is None:
                rows = self.search(title, platform_id)
                if not isinstance(rows, list):
                    raise ValueError("Invalid IGDB individual fallback response")
                results[name] = rows
                with self.request_lock:
                    self.fallback_searches += 1
        return [results[f"game{i}"] for i in range(len(requests))]

    def catalogue_page(self, platform_id, after_id, *, aliases=False):
        fields = 'id,name,platforms,cover.image_id'
        if aliases:
            fields += ',alternative_names.name,alternative_names.comment'
        return self.query("games", (
            f'fields {fields}; '
            f'where platforms = ({int(platform_id)}) & id > {int(after_id)}; '
            'sort id asc; limit 500;'))


def batches(values, size):
    """Yield bounded chunks; no repeated or unbounded in-flight queries."""
    iterator = iter(values)
    while chunk := list(islice(iterator, size)):
        yield chunk


def load_catalogue(client, platform_id, *, refresh=False, progress=None, aliases=False):
    """Checkpoint pages by ascending ID; never match an incomplete catalogue."""
    directory = "igdb-catalogues-aliases" if aliases else "igdb-catalogues"
    path = CACHE.parent / directory / f"{platform_id}.json"
    empty = {"schemaVersion": 1, "platformId": platform_id,
             "complete": False, "games": []}
    catalogue = empty if refresh else load_json(path, empty)
    if (catalogue.get("schemaVersion") != 1 or
            catalogue.get("platformId") != platform_id or
            type(catalogue.get("complete")) is not bool or
            not isinstance(catalogue.get("games"), list)):
        raise ValueError("Invalid IGDB catalogue cache; use --refresh-catalogue")
    games = catalogue["games"]

    def validate(rows, after_id):
        for row in rows:
            if (not isinstance(row, dict) or type(row.get("id")) is not int or
                    row["id"] <= after_id or not isinstance(row.get("name"), str) or
                    not isinstance(row.get("platforms"), list) or
                    platform_id not in row["platforms"] or
                    (row.get("cover") is not None and not isinstance(row["cover"], dict))):
                raise ValueError("Invalid or unordered IGDB catalogue rows")
            after_id = row["id"]
            if aliases:
                alternatives = row.get("alternative_names", [])
                if (not isinstance(alternatives, list) or any(
                        not isinstance(alias, dict) or not isinstance(alias.get("name"), str) or
                        not isinstance(alias.get("comment", ""), str) for alias in alternatives)):
                    raise ValueError("Invalid IGDB alternative names; expanded name rows required")
        return after_id

    cursor = validate(games, 0)
    while not catalogue["complete"]:
        page = (client.catalogue_page(platform_id, cursor, aliases=True) if aliases
                else client.catalogue_page(platform_id, cursor))
        if not isinstance(page, list) or len(page) > 500:
            raise ValueError("Invalid IGDB catalogue page")
        cursor = validate(page, cursor)
        games.extend(page)
        catalogue["complete"] = len(page) < 500
        atomic_json(path, catalogue)
        if progress:
            progress(len(games), catalogue["complete"])
    return games


def scan_bulk(client, names, cache, console_ids, *, refresh=False, progress=None, aliases=False):
    """Match all uncovered titles locally, including prior unsuccessful searches.

    Only successes enter the shared search cache. Catalogue misses/reviews
    remain eligible for individual searches; no search is made automatically.
    """
    entries = cache.setdefault("results", {})
    totals = Counter()
    for platform in sorted({platform for platform, _ in names.values()}):
        pid = console_ids.get(platform)
        if not pid:
            continue
        targets = [(key, title) for key, (console, title) in names.items()
                   if console == platform and entries.get(key, {}).get("status") != "matched"]
        if not targets:
            continue
        rows = load_catalogue(client, pid, refresh=refresh, aliases=aliases, progress=(
            (lambda count, done: progress(
                f"{platform}: {count:,} catalogue games cached" + (" (complete)" if done else "")))
            if progress else None))
        index = {}
        for row in rows:
            for name in game_names(row, aliases):
                index.setdefault(name, []).append(row)
        statuses = Counter()
        for key, title in targets:
            clean = search_title(platform, title)
            if len(clean) < 3 or re.search(r"\[(?:DLC|Addon|Add-on)\]", clean, re.I):
                statuses["skipped-metadata"] += 1
                continue
            result = select_match(platform, clean, index.get(match_name(clean), []), pid, aliases=aliases)
            statuses[result["status"]] += 1
            if result["status"] == "matched":
                entries[key] = {**result, "searchedTitle": clean}
        atomic_json(CACHE, cache)
        # Export after each console so completed work survives later failures.
        export_cache(cache)
        totals.update(statuses)
        if progress:
            progress(f"{platform}: " + ", ".join(f"{k}={v}" for k, v in sorted(statuses.items())))
    return totals


def scan(client, names, cache, max_queries=50, console_ids=None, progress=None,
         batch_size=10, workers=3):
    """Multi-Query 10 titles/request, at most 3 in flight; checkpoint each batch.

    Every successful response is committed on the main thread. A failed or
    partial batch never poisons the cache as 'not-found'. Existing success
    records and improved-name retries remain unchanged.
    """
    if not 1 <= batch_size <= 10 or not 1 <= workers <= 4:
        raise ValueError("Expected batch_size 1..10 and workers 1..4")
    console_ids = console_ids if console_ids is not None else client.platforms()
    entries = cache.setdefault("results", {})
    prepared = []
    skipped = 0
    for key, (platform, title) in names.items():
        if platform not in console_ids:
            continue
        clean = search_title(platform, title)
        previous = entries.get(key)
        if previous:
            if previous.get("status") == "matched":
                continue
            if previous.get("status") in {"not-found", "review"}:
                last_query = previous.get("searchedTitle", title)
                if normalize(last_query) == normalize(clean):
                    continue
            else:
                continue
        if len(prepared) >= max_queries:
            break
        if not clean or len(clean) < 3 or re.search(r"\[(?:DLC|Addon|Add-on)\]", clean, re.I):
            entries[key] = {"status": "skipped-metadata", "searchedTitle": clean}
            skipped += 1
            continue
        prepared.append((key, platform, clean, console_ids[platform]))
    if skipped:
        atomic_json(CACHE, cache)
    if not prepared:
        return 0

    def fetch(group):
        rows = client.multi_search([(clean, pid) for _, _, clean, pid in group])
        if not isinstance(rows, list) or len(rows) != len(group):
            raise ValueError("Incomplete IGDB Multi-Query batch; results not cached")
        return [(key, clean, select_match(platform, clean, result, pid))
                for (key, platform, clean, pid), result in zip(group, rows)]

    examined = 0
    futures = []
    with ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(fetch, group) for group in batches(prepared, batch_size)]
        try:
            for future in as_completed(futures):
                complete = future.result()
                for key, clean, result in complete:
                    entries[key] = {**result, "searchedTitle": clean}
                examined += len(complete)
                # Each completed batch is saved immediately; the only writer
                # is this main thread, regardless of completion order.
                atomic_json(CACHE, cache)
                if progress:
                    statuses = Counter(result["status"] for _, _, result in complete)
                    summary = ", ".join(f"{label}={count}" for label, count in sorted(statuses.items()))
                    progress(examined, complete[-1][0], summary)
        except BaseException:
            for future in futures:
                future.cancel()
            raise
    return examined

def export_cache(cache, keys=None, path=None):
    path = path if path is not None else EXPORT
    allowed = keys if keys is not None else cache.get("results", {}).keys()
    selected = {key: {"url": row["url"], "provider": row["provider"],
                      "confidence": 95}
                for key, row in cache.get("results", {}).items()
                if key in allowed and row.get("status") == "matched" and
                row.get("provider") in IGDB_PROVIDERS and
                isinstance(row.get("confidence"), int) and row["confidence"] >= 95 and
                cover_url(row.get("url", "").rsplit("/", 1)[-1].removesuffix(".jpg")) == row.get("url")}
    atomic_json(path, {"schemaVersion": 1, "matches": selected})
    return len(selected)

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true", help="Show eligible unique titles without credentials")
    parser.add_argument("--bulk", action="store_true",
                        help="Cache console catalogues (500 games/page) and match all missing titles locally")
    parser.add_argument("--aliases", action="store_true",
                        help="With --bulk, fetch and match official IGDB alternative names in a separate cache")
    parser.add_argument("--refresh-catalogue", action="store_true",
                        help="With --bulk, rebuild selected console catalogue caches from the first page")
    parser.add_argument("--max-queries", type=int, default=50,
                        help="Maximum individual game searches (default 50); ignored with --bulk")
    parser.add_argument("--batch-size", type=int, default=10, help="Game searches per IGDB request, 1-10 (default 10)")
    parser.add_argument("--workers", type=int, default=3, help="Concurrent requests, 1-4 (default 3)")
    parser.add_argument("--platforms", help="Comma-separated Wolodrive platform IDs")
    parser.add_argument("--retry-unmatched", action="store_true",
                        help="Retry cached not-found/review results; never clear matched results")
    args = parser.parse_args(argv)
    if args.aliases and not args.bulk:
        parser.error("--aliases requires --bulk")
    if args.refresh_catalogue and not args.bulk:
        parser.error("--refresh-catalogue requires --bulk")
    if args.bulk and args.retry_unmatched:
        parser.error("--bulk already rechecks unsuccessful titles; omit --retry-unmatched")
    if not 0 <= args.max_queries <= 2000:
        parser.error("--max-queries must be 0..2000")
    if not 1 <= args.batch_size <= 10 or not 1 <= args.workers <= 4:
        parser.error("--batch-size must be 1..10 and --workers must be 1..4")
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
    if args.bulk:
        print(f"Bulk mode: {len(candidates):,} uncovered titles; "
              "previously unsuccessful searches are rechecked locally.", flush=True)
    else:
        print(f"Unique uncovered game/platform titles: {len(candidates):,}; "
              f"pending: {len(pending):,} (including {retries:,} improved-title retries); "
              f"already processed: {len(candidates)-len(pending):,}", flush=True)
    if args.dry_run:
        changes = [(name, search_title(platform, name)) for platform, name in candidates.values()
                   if normalize(name) != normalize(search_title(platform, name))]
        print(f"Titles with removable source suffixes: {len(changes):,}")
        for old, new in changes[:8]:
            print(f"  {old!r} -> {new!r}")
    print("Uncovered by console:" if args.bulk else "Pending by console:",
          dict(Counter(key.split("|", 1)[0] for key in
                       (candidates if args.bulk else pending)).most_common(12)))
    if args.dry_run:
        return 0
    if args.max_queries == 0 and not args.bulk:
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
    if args.bulk:
        print("Bulk catalogue: 500 games/page, shared ~3 requests/second limiter; "
              "no individual title searches", flush=True)
        if args.aliases:
            print("Official alternative names enabled; using separate alias catalogue cache", flush=True)
        totals = scan_bulk(client, candidates, cache, ids,
                           refresh=args.refresh_catalogue, aliases=args.aliases,
                           progress=lambda message: print(message, flush=True))
        matches = export_cache(cache)
        print(f"Bulk results: {dict(totals)}; IGDB cached matches exported: {matches}.", flush=True)
        print("Run python scripts/enrich_artwork.py --offline to merge covers into Wolodrive.")
        return 0
    print(f"Multi-Query: {args.batch_size} searches/request, {args.workers} workers, "
          "shared ~3 requests/second limiter", flush=True)
    requested_count = scan(
        client, candidates, cache, args.max_queries, ids,
        lambda count, key, status: print(
            f"IGDB {count}/{args.max_queries}: {status} ({key[:85]})", flush=True),
        batch_size=args.batch_size, workers=args.workers)
    # Never restrict the export to current missing titles: each enrichment
    # makes earlier successful titles disappear from eligible_titles().
    # Exporting only eligible_titles() would silently erase those covers on
    # the next batch, even though they remain in the IGDB result cache.
    matches = export_cache(cache)
    print(f"Queried: {requested_count}; IGDB cached matches exported: {matches}; "
          f"missing Multi-Query blocks verified individually: {client.fallback_searches}.", flush=True)
    print("Run python scripts/enrich_artwork.py --offline to merge covers into Wolodrive.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
