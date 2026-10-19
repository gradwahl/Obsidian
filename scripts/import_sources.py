"""Import bounded public catalog metadata for the native app and static React preview.

No package files, account data, license keys, or download-link payloads are stored.
Only allowlisted display metadata and public source-page links enter the snapshots.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import csv
from datetime import datetime, timezone
import hashlib
import html
from html.parser import HTMLParser
import io
import json
from pathlib import Path
import re
import time
import urllib.error
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
MAX_RESPONSE = 24 * 1024 * 1024
AXEKIN_MAX_PAGES = 2000
AXEKIN_MAX_GAMES = 30000


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "Wolodrive/2.0 catalog metadata importer"})
    with urllib.request.urlopen(request, timeout=20) as response:
        data = response.read(MAX_RESPONSE + 1)
    if len(data) > MAX_RESPONSE:
        raise ValueError("Catalog response exceeds 24 MiB")
    return data.decode("utf-8-sig", errors="replace")


def clean(text):
    return html.unescape(re.sub(r"<[^>]+>", " ", str(text or ""))).strip()


def platform(text, fallback=""):
    text = text.lower()
    for key, pattern in (("sony-psvita", r"ps\s*vita"), ("sony-psp", r"\bpsp\b|playstation portable"),
                         ("sony-ps5", r"\bps5\b|playstation 5"), ("sony-ps4", r"\bps4\b|playstation 4"),
                         ("sony-ps3", r"\bps3\b|playstation 3"), ("sony-ps2", r"\bps2\b|playstation 2"),
                         ("sony-ps1", r"\bpsx\b|\bps1\b|playstation 1"),
                         ("nintendo-switch", r"\bswitch\b|\bnsp\b|\bxci\b"),
                         ("nintendo-3ds", r"\b3ds\b|\bcia\b"),
                         ("nintendo-ds", r"\bnds\b|nintendo ds|\bds rom\b"),
                         ("nintendo-gba", r"\bgba\b|game boy advance"),
                         ("nintendo-gbc", r"\bgbc\b|game boy color"),
                         ("nintendo-gb", r"\bgame boy\b"),
                         ("nintendo-gc", r"\bgamecube\b|\bngc\b"),
                         ("nintendo-wiiu", r"\bwii\s*u\b"),
                         ("nintendo-wii", r"\bwii\b"),
                         ("nintendo-n64", r"\bn64\b|nintendo 64"),
                         ("nintendo-snes", r"\bsnes\b|super nintendo"),
                         ("nintendo-nes", r"\bnes\b|\bfamicom\b"),
                         ("sega-dreamcast", r"\bdreamcast\b"),
                         ("sega-saturn", r"\bsaturn\b"),
                         ("microsoft-xbox360", r"xbox\s*360"), ("microsoft-xbox", r"\bxbox\b")):
        if re.search(pattern, text):
            return key
    return fallback


def record(source, title, page, identity="", system="", cover="", description="", size=0, original_format=""):
    title = clean(title)
    identity = identity or "EXT-" + hashlib.sha256((source["id"] + page + title).encode()).hexdigest()[:20]
    return {"id": identity, "title": title, "platform": system, "category": "game",
            "format": "catalog", "version": "", "sizeBytes": max(0, int(size or 0)),
            "coverUrl": cover if str(cover).startswith(("http://", "https://")) else "",
            "sourceUrl": page, "description": (clean(description)[:2500] +
              f"\nSource: {page}\nPlatform: {system or 'Unspecified'}\nCatalog metadata only" +
              (f"; original format: {original_format}" if original_format else ""))}


def tsv(source, limit):
    records = []
    budget = max(1, limit // len(source["feeds"]))
    for system, url in source["feeds"]:
        rows = csv.DictReader(io.StringIO(fetch(url), newline=None), delimiter="\t")
        for row in rows:
            if not row.get("Name") or not row.get("Title ID"):
                continue
            title = row["Name"]
            records.append(record(source, title, source["url"], row["Title ID"],
                                  platform(title, system), description=row.get("Region", ""),
                                  size=row.get("File Size") or 0, original_format="pkg"))
            if sum(item["platform"] == platform(title, system) for item in records[-budget:]) >= budget:
                break
            if len(records) >= budget * (source["feeds"].index([system, url]) + 1):
                break
    return records[:limit], True


class Listings(HTMLParser):
    def __init__(self, base):
        super().__init__(); self.base = base; self.heading = False; self.link = None
        self.text = []; self.rows = []; self.image = ""

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag in ("h2", "h3"):
            self.heading = True
        if tag == "img":
            self.image = urllib.parse.urljoin(self.base, attrs.get("data-src") or attrs.get("src") or "")
        if tag == "a" and self.heading and attrs.get("href"):
            self.link = urllib.parse.urljoin(self.base, attrs["href"])
            self.text = []; self.full_title = attrs.get("title", "")

    def handle_data(self, text):
        if self.link:
            self.text.append(text)

    def handle_endtag(self, tag):
        if tag == "a" and self.link:
            title = clean(self.full_title or "".join(self.text))
            if title and urllib.parse.urlsplit(self.link).netloc == urllib.parse.urlsplit(self.base).netloc:
                self.rows.append((title, self.link, self.image))
            self.link = None
        if tag in ("h2", "h3"):
            self.heading = False


class PublicGameLinks(HTMLParser):
    """Only public internal detail pages, never download endpoints or host links."""
    def __init__(self, base, source_id):
        super().__init__()
        self.base = base
        self.source_id = source_id
        self.link = None
        self.parts = []
        self.rows = []

    def valid(self, url):
        parts = urllib.parse.urlsplit(url)
        if parts.netloc.lower() != urllib.parse.urlsplit(self.base).netloc.lower():
            return False
        path = parts.path.lower()
        if self.source_id == "pkgps4":
            return bool(re.fullmatch(r"/[a-z0-9][a-z0-9-]{6,}/", path))
        if self.source_id == "cdromance":
            return bool(re.fullmatch(r"/(?:psp|ps2|psx|ps1|gamecube|gba|nds|dreamcast|saturn|wii|snes|n64)/[a-z0-9-]+/?", path))
        if self.source_id == "romshq":
            return bool(re.fullmatch(r"/game/[a-z0-9-]+/?", path))
        if self.source_id == "romstime":
            return bool(re.fullmatch(r"/roms/[a-z0-9-]+/[a-z0-9-]+/?", path))
        return False

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "a" and self.link is None and attrs.get("href"):
            link = urllib.parse.urljoin(self.base, attrs["href"])
            if self.valid(link):
                self.link = link
                self.parts = []
                if attrs.get("title"):
                    self.parts.append(attrs["title"])
        elif tag == "img" and self.link and attrs.get("alt"):
            self.parts.append(attrs["alt"])

    def handle_data(self, data):
        if self.link:
            self.parts.append(data)

    def handle_endtag(self, tag):
        if tag == "a" and self.link:
            title = clean(" ".join(self.parts))
            title = re.sub(r"\s+", " ", title)
            if 4 <= len(title) <= 160 and not re.search(
                r"^(?:download|learn more|view|read more|open|new releases|see all)$", title, re.I
            ):
                self.rows.append((title, self.link))
            self.link = None


def website(source, limit):
    index_url = source.get("index_url", source["url"])
    parser = Listings(index_url); parser.feed(fetch(index_url))
    rows = []
    for title, page, cover in parser.rows:
        if any(part in page for part in ("/category/", "/tag/", "/page/")):
            continue
        if re.search(r"jailbreak|firmware|guide|\bstore\b|tutorial|notification", title, re.I):
            continue
        system = platform(title + " " + urllib.parse.urlsplit(page).path, source.get("platform", ""))
        if not system:
            continue
        rows.append(record(source, title, page, system=system, cover=cover))
    if source["id"] in {"pkgps4", "cdromance", "romshq", "romstime"}:
        fallback = PublicGameLinks(index_url, source["id"])
        fallback.feed(fetch(index_url))
        existing = {(item["title"], item["sourceUrl"]) for item in rows}
        for title, page in fallback.rows:
            if len(rows) >= limit:
                break
            system = platform(title + " " + urllib.parse.urlsplit(page).path, source.get("platform", ""))
            entry = record(source, title, page, system=system)
            if (entry["title"], entry["sourceUrl"]) not in existing:
                rows.append(entry)
                existing.add((entry["title"], entry["sourceUrl"]))
    return rows[:limit], True  # Website index only; not a claim to have crawled all posts.


class InertiaPage(HTMLParser):
    """Read Inertia's server-rendered game metadata; do not follow package links."""
    def __init__(self):
        super().__init__()
        self.page = None

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "div" and attrs.get("id") == "app" and attrs.get("data-page"):
            self.page = json.loads(attrs["data-page"])


def file_size_bytes(text):
    match = re.fullmatch(r"\s*([\d.]+)\s*(B|KB|MB|GB|TB|KiB|MiB|GiB|TiB)\s*", str(text or ""), re.I)
    if not match:
        return 0
    value, unit = match.groups()
    base = 1024 if "i" in unit.lower() else 1000
    exponent = {"B": 0, "KB": 1, "MB": 2, "GB": 3, "TB": 4}[unit.replace("i", "").upper()]
    try:
        return max(0, int(float(value) * base ** exponent))
    except (ValueError, OverflowError):
        return 0


def axekin(source, limit):
    # Axekin serves a paginated Inertia payload in an HTML attribute (25 games/page).
    rows = []
    seen = set()
    total = None
    for page in range(1, (limit + 24) // 25 + 1):
        index = source.get("index_url", source["url"])
        endpoint = index + ("&" if "?" in index else "?") + urllib.parse.urlencode({"page": page})
        parser = InertiaPage()
        parser.feed(fetch(endpoint))
        if not parser.page or parser.page.get("component") != "games/index":
            raise ValueError("Axekin Inertia game index not found")
        listing = parser.page.get("props", {}).get("games", {})
        games = listing.get("data", [])
        if not isinstance(games, list):
            raise ValueError("Axekin game data is not a list")
        total = listing.get("meta", {}).get("total", total)
        for game in games:
            title = game.get("title", "")
            slug = game.get("slug", "")
            if not title or not re.fullmatch(r"[a-z0-9][a-z0-9-]*", slug):
                continue
            page_url = urllib.parse.urljoin(source["url"], "/games/" + slug)
            platforms = game.get("platforms") or []
            if not isinstance(platforms, list):
                platforms = []
            system = next((platform(p.get("name", "")) for p in platforms
                           if isinstance(p, dict) and platform(p.get("name", ""))), "")
            if page_url in seen:
                continue
            seen.add(page_url)
            cover = game.get("coverImage") or ""
            if cover and not str(cover).startswith(("https://", "http://")):
                cover = urllib.parse.urljoin(source["url"], str(cover))
            rows.append(record(source, title, page_url, system=system, cover=cover,
                               description=game.get("description") or "",
                               size=file_size_bytes(game.get("fileSize"))))
            if len(rows) >= limit:
                break
        if len(rows) >= limit or not games:
            break
    return rows, bool(total is None or total > len(rows))


def axekin_supported_platforms():
    """Allow exactly the consoles that the Games/Roms view can resolve."""
    mappings = json.loads((ROOT / "config/axekin_platforms.json").read_text(encoding="utf8"))
    library = (ROOT / "src/libraryData.ts").read_text(encoding="utf8")
    native = (ROOT / "src/library.c").read_text(encoding="utf8")
    for slug, entry in mappings.items():
        if not re.fullmatch(r"[a-z0-9-]+", slug) or not re.fullmatch(r"[a-z0-9-]+", entry["platform"]):
            raise ValueError("Invalid Axekin platform mapping")
        name = entry["console"]
        if (f"name: '{name}', platform_id: '{entry['platform']}'" not in library or
                f'{{"{name}", "{entry["platform"]}"}}' not in native):
            raise ValueError(f"Axekin platform {slug} has no matching Games/Roms console: {name}")
    return {slug: entry["platform"] for slug, entry in mappings.items()}


def axekin_page(source, page, attempts=3):
    index = source.get("index_url", source["url"])
    endpoint = index + ("&" if "?" in index else "?") + urllib.parse.urlencode({"page": page})
    for attempt in range(attempts):
        try:
            parser = InertiaPage()
            parser.feed(fetch(endpoint))
            if not parser.page or parser.page.get("component") != "games/index":
                raise ValueError(f"Axekin index missing from page {page}")
            listing = parser.page.get("props", {}).get("games", {})
            if not isinstance(listing.get("data"), list):
                raise ValueError(f"Axekin game data invalid at page {page}")
            meta = listing.get("meta", {})
            if int(meta.get("currentPage", page)) != page:
                raise ValueError(f"Axekin page mismatch at {page}")
            return listing
        except (urllib.error.URLError, TimeoutError, ConnectionError) as exc:
            if attempt == attempts - 1:
                raise RuntimeError(f"Axekin page {page} failed after {attempts} tries") from exc
            time.sleep(min(5, 1.25 * (attempt + 1)))


def axekin_full(source, workers=5, progress=None):
    """Crawl every page, keeping only Games/Roms-supported platforms.

    Fail atomically on missing pages: a partial crawl must never be marked full.
    """
    mappings = axekin_supported_platforms()
    first = axekin_page(source, 1)
    total_pages = int(first.get("meta", {}).get("lastPage", 0))
    total_games = int(first.get("meta", {}).get("total", 0))
    if not 1 <= total_pages <= AXEKIN_MAX_PAGES or not 1 <= total_games <= AXEKIN_MAX_GAMES:
        raise ValueError(f"Axekin declared unsafe page/game count: {total_pages}/{total_games}")
    results = {1: first}
    completed = 1
    if progress:
        progress(completed, total_pages)
    # Each page is checked for correct pagination metadata before entering the index.
    with ThreadPoolExecutor(max_workers=workers) as pool:
        futures = {pool.submit(axekin_page, source, page): page
                   for page in range(2, total_pages + 1)}
        for task in as_completed(futures):
            page = futures[task]
            results[page] = task.result()
            completed += 1
            if progress and (completed % 50 == 0 or completed == total_pages):
                progress(completed, total_pages)

    rows, seen, skipped, visited = [], set(), 0, 0
    for page in range(1, total_pages + 1):
        games = results[page]["data"]
        if not games:
            raise ValueError(f"Axekin returned an empty page at {page}")
        visited += len(games)
        for game in games:
            title, slug = game.get("title", ""), game.get("slug", "")
            if not title or not re.fullmatch(r"[a-z0-9][a-z0-9-]*", slug):
                skipped += 1
                continue
            platforms = game.get("platforms") or []
            if not isinstance(platforms, list):
                skipped += 1
                continue
            compatible = {mappings[p["slug"]] for p in platforms
                          if isinstance(p, dict) and isinstance(p.get("slug"), str)
                          and p["slug"] in mappings}
            if not compatible:
                skipped += 1
                continue
            game_page = urllib.parse.urljoin(source["url"], "/games/" + slug)
            cover = game.get("coverImage") or ""
            if cover and not str(cover).startswith(("https://", "http://")):
                cover = urllib.parse.urljoin(source["url"], str(cover))
            # One entry per supported console, including multi-platform games.
            for system in sorted(compatible):
                identity = "EXT-" + hashlib.sha256((source["id"] + str(game.get("id", slug)) + system).encode()).hexdigest()[:20]
                if identity in seen:
                    continue
                seen.add(identity)
                entry = record(source, title, game_page, identity=identity, system=system,
                               cover=cover, description="", size=file_size_bytes(game.get("fileSize")))
                # The bulk snapshot keeps only display fields to limit native and web memory.
                entry.pop("category", None)
                entry.pop("sizeBytes", None) if entry["sizeBytes"] == 0 else None
                entry.pop("description", None)
                entry.pop("version", None)
                rows.append(entry)
    if visited < total_games * 0.90:
        raise ValueError(f"Axekin crawl yielded only {visited}/{total_games} advertised games")
    return rows, {"pages": total_pages, "advertisedGames": total_games,
                  "visitedGames": visited, "excludedGames": skipped,
                  "indexedEntries": len(rows), "supportedConsoles": len(mappings)}


def write_snapshot(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(data, ensure_ascii=False, separators=(",", ":")) + "\n",
                         encoding="utf8", newline="\n")
    temporary.replace(path)


def romheaven(source, limit):
    # Use the same public title CSV the website's Svelte application reads.
    endpoint = source["feed_url"]
    if urllib.parse.urlsplit(endpoint).netloc != urllib.parse.urlsplit(source["url"]).netloc:
        raise ValueError("RomHeaven feed must be on its own host")
    reader = csv.DictReader(io.StringIO(fetch(endpoint)))
    if not reader.fieldnames or not {"filename", "display_name"}.issubset(reader.fieldnames):
        raise ValueError("RomHeaven switch CSV has changed format")
    rows = []
    more = False
    for game in reader:
        filename = (game.get("filename") or "").strip()
        title = (game.get("display_name") or filename).strip()
        if not filename or not title:
            continue
        if len(rows) >= limit:
            more = True
            break
        slug = re.sub(r"[^a-z0-9]+", "_", filename.lower()).strip("_")
        page = urllib.parse.urljoin(source["url"], "/switch/" + slug) if slug else source["index_url"]
        cover = game.get("iconUrl") or ""
        try:
            size = max(0, int(game.get("compressed_size") or 0))
        except ValueError:
            size = 0
        rows.append(record(source, title, page, system="nintendo-switch", cover=cover,
                           description=game.get("description") or "", size=size))
    return rows, more


class RetroCollectionLinks(HTMLParser):
    """Wix gallery uses labelled divs, not anchors, for console collection cards."""
    def __init__(self):
        super().__init__()
        self.names = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if (tag == "div" and attrs.get("data-hook") == "item-action" and
                attrs.get("role") == "link" and attrs.get("aria-label")):
            self.names.append(clean(attrs["aria-label"]))


def retro_collections(source, limit):
    # These are SYSTEM-LEVEL collections; there is no verified per-game listing.
    parser = RetroCollectionLinks()
    parser.feed(fetch(source.get("index_url", source["url"])))
    rows, seen = [], set()
    for label in parser.names:
        if label in seen or len(label) < 3 or len(rows) >= limit:
            continue
        seen.add(label)
        name = re.sub(r"\s+(?:game console|handheld|arcade board)\s*$", "", label, flags=re.I)
        rows.append(record(
            source, name + " Collection", source.get("index_url", source["url"]),
            system=platform(name), description="Curated console-level collection listing; "
            "individual games have not been indexed from this source."
        ))
    return rows, True


class VaultLinks(HTMLParser):
    """Extract public game detail links from Vimm's Lair vault indexes."""
    def __init__(self):
        super().__init__()
        self.link = ""
        self.parts = []
        self.rows = []

    def handle_starttag(self, tag, attrs):
        if tag != "a":
            return
        href = dict(attrs).get("href", "")
        if re.fullmatch(r"/vault/\d+", href):
            self.link = href
            self.parts = []

    def handle_data(self, data):
        if self.link:
            self.parts.append(data)

    def handle_endtag(self, tag):
        if tag == "a" and self.link:
            title = clean("".join(self.parts))
            if title:
                self.rows.append((title, "https://vimm.net" + self.link))
            self.link = ""


def vimm(source, limit):
    # Vault landing page is navigation; letter indexes contain actual game entries.
    systems = [("PSP", "sony-psp"), ("PS1", "sony-ps1"),
               ("PS2", "sony-ps2"), ("GBA", "nintendo-gba"),
               ("SNES", "nintendo-snes"), ("N64", "nintendo-n64"),
               ("DS", "nintendo-ds"), ("GameCube", "nintendo-gc")]
    budget = max(1, limit // len(systems))
    rows = []
    for section, system in systems:
        parser = VaultLinks()
        try:
            parser.feed(fetch("https://vimm.net/vault/" + section + "/A"))
        except Exception:
            if not rows:
                raise
            continue
        for title, page in parser.rows[:budget]:
            rows.append(record(source, title, page, system=system))
    return rows[:limit], True  # First alphabetic index only, not an entire vault.


def gfs(source, limit):
    rows = []
    budget = max(1, limit // len(source["packs"]))
    for pack in source["packs"]:
        loaded = 0; page = 1
        while loaded < budget:
            endpoint = source["url"] + "api/packages?" + urllib.parse.urlencode({"page": page, "limit": 10, "pack": pack})
            document = json.loads(fetch(endpoint))
            packages = document.get("packages", [])
            for item in packages[:budget - loaded]:
                result = record(source, item["title"], source["url"], item.get("titleId", ""), "sony-ps5",
                                item.get("posterUrl", ""), item.get("description", ""), item.get("sizeBytes", 0), pack)
                # Keep source variants distinct without exposing host download links.
                result["version"] = str(item.get("version", "")) + " / " + pack
                rows.append(result); loaded += 1
            if not packages or page >= document.get("totalPages", 1):
                break
            page += 1
    return rows[:limit], True


def gfs_full(source, page_size=100):
    """Read every page of every public GFS pack, verifying the total count."""
    rows = []
    coverage = {}
    for pack in source["packs"]:
        first = json.loads(fetch(source["url"] + "api/packages?" +
                                 urllib.parse.urlencode({"page": 1, "limit": page_size, "pack": pack})))
        total = int(first.get("total", -1))
        pages = int(first.get("totalPages", 0))
        if total < 0 or pages < 1 or pages > 200:
            raise ValueError("GFS page count missing or unexpectedly large")
        all_items = list(first.get("packages", []))
        for page in range(2, pages + 1):
            result = json.loads(fetch(source["url"] + "api/packages?" +
                                      urllib.parse.urlencode({"page": page, "limit": page_size, "pack": pack})))
            if int(result.get("page", page)) != page:
                raise ValueError(f"GFS {pack} returned mismatched page {page}")
            if int(result.get("total", -1)) != total:
                raise ValueError(f"GFS {pack} changed during pagination")
            all_items.extend(result.get("packages", []))
        if len(all_items) != total:
            raise ValueError(f"GFS {pack} partial: {len(all_items)}/{total}")
        coverage[pack] = {"pages": pages, "items": total}
        for item in all_items:
            if not item.get("title") or not item.get("id"):
                continue
            # GFS currently publishes PS5 packs. Do not treat them as every
            # platform just because their title includes a console keyword.
            result = record(source, item["title"], source["url"],
                            identity="EXT-" + hashlib.sha256(
                                (source["id"] + str(item["id"]) + pack).encode()).hexdigest()[:20],
                            system="sony-ps5", cover=item.get("posterUrl") or "",
                            size=item.get("sizeBytes") or 0)
            result["version"] = str(item.get("version") or "") + " / " + pack
            rows.append(result)
    return rows, {"packs": coverage, "indexed": len(rows)}


def romheaven_full(source):
    """Consume the complete published CSV and verify its playable rows."""
    rows, more = romheaven(source, 100000)
    if more:
        raise ValueError("RomHeaven CSV unexpectedly exceeded full importer capacity")
    text = fetch(source["feed_url"])
    data = list(csv.DictReader(io.StringIO(text)))
    expected = sum(bool((row.get("filename") or "").strip() and
                        (row.get("display_name") or row.get("filename") or "").strip())
                   for row in data)
    if len(rows) != expected:
        raise ValueError(f"RomHeaven incomplete: {len(rows)}/{expected}")
    return rows, {"csvRecords": len(data), "indexed": len(rows)}


def archive(source, limit):
    uploader = ""
    if source.get("seed_identifier"):
        seed = source["seed_identifier"]
        if not re.fullmatch(r"[a-zA-Z0-9_-]+", seed):
            raise ValueError("Invalid Archive.org seed identifier")
        metadata = json.loads(fetch("https://archive.org/metadata/" + seed))
        uploader = str(metadata.get("metadata", {}).get("uploader", "")).strip().lower()
        if not re.fullmatch(r"[a-z0-9._%+-]+@[a-z0-9.-]+", uploader):
            raise ValueError("Archive.org seed has no public uploader identifier")
    # The profile @handle is not the uploader index key. Look up the public
    # identifier from one known item, without persisting an email in config.
    query = 'uploader:"' + uploader + '"' if uploader else source["query"]
    params = {"q": query, "output": "json", "rows": 50,
              "fl[]": ["identifier", "title", "subject", "mediatype"]}
    endpoint = "https://archive.org/advancedsearch.php?" + urllib.parse.urlencode(params, doseq=True)
    document = json.loads(fetch(endpoint))["response"]
    rows = []
    for item in document["docs"]:
        identity = item["identifier"]; title = str(item.get("title", identity))
        system = platform(title + " " + str(item.get("subject", "")))
        metadata = json.loads(fetch("https://archive.org/metadata/" + urllib.parse.quote(identity, safe="")))
        if uploader and str(metadata.get("metadata", {}).get("uploader", "")).strip().lower() != uploader:
            continue
        for file in metadata.get("files", []):
            name = file.get("name", "")
            if not re.search(r"\.(?:zip|7z|rar|iso|chd|cso|pbp|pkg|nes|sfc|n64|gba)$", name, re.I):
                continue
            if file.get("private") or name.startswith("__"):
                continue
            page = "https://archive.org/details/" + urllib.parse.quote(identity, safe="")
            rows.append(record(source, Path(name).stem, page, system=platform(name, system),
                               description=title, size=file.get("size", 0), original_format=name.rsplit(".", 1)[-1]))
            if len(rows) >= limit:
                return rows, True
    return rows, document.get("numFound", 0) > 50


def import_source(source, limit, previous=None):
    output = {"id": source["id"], "name": source["name"], "website": source["url"],
              "checkedAt": datetime.now(timezone.utc).isoformat(), "entryLimit": limit}
    try:
        adapters = {"tsv": tsv, "html": website, "gfs": gfs, "archive": archive,
                    "vimm": vimm, "axekin": axekin, "romheaven": romheaven,
                    "retro-collections": retro_collections}
        rows, truncated = adapters[source["adapter"]](source, limit)
        unique = {(row["id"], row["title"], row["platform"], row["version"]): row for row in rows}
        output.update(packages=list(unique.values()), status="partial" if rows and truncated else "ready" if rows else "empty",
                      note="Bounded metadata snapshot" if rows and truncated else "No public entries returned" if not rows else "")
    except Exception as error:
        retained = (previous or {}).get("packages", [])[:limit]
        output.update(packages=retained, status="stale" if retained else "error",
                      note=f"Import failed: {type(error).__name__}: {error}")
    return output


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--limit", type=int, default=256, help="Maximum metadata entries per source (1-400)")
    parser.add_argument("--full-axekin", action="store_true",
                        help="Index every Axekin page into a separate, console-filtered snapshot")
    parser.add_argument("--full-small", action="store_true",
                        help="Exhaustively index GFS and RomHeaven with verified totals")
    args = parser.parse_args(argv)
    if not 1 <= args.limit <= 400:
        parser.error("--limit must be between 1 and 400 to preserve native catalog capacity")
    sources = json.loads((ROOT / "config/additional_sources.json").read_text(encoding="utf8"))
    output = ROOT / "assets/catalogs/additional.json"
    previous = json.loads(output.read_text(encoding="utf8")) if output.exists() else {"sources": []}
    old = {item["id"]: item for item in previous["sources"]}
    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(lambda source: import_source(source, args.limit, old.get(source["id"])), sources))
    if args.full_small:
        for source_id, importer in (("gfs", gfs_full), ("romheaven", romheaven_full)):
            source = next(item for item in sources if item["id"] == source_id)
            print(f"{source['name']}: fetching every published record...", flush=True)
            rows, coverage = importer(source)
            for result in results:
                if result["id"] == source_id:
                    result.update(packages=rows, status="ready", entryLimit=len(rows),
                                  coverage=coverage, note="Complete published structured index")
                    break
    else:
        # A routine bounded refresh must not silently erase previously verified
        # exhaustive snapshots.
        for i, source in enumerate(results):
            previous_source = old.get(source["id"])
            if (source["id"] in {"gfs", "romheaven"} and previous_source and
                    previous_source.get("coverage") and
                    previous_source.get("status") == "ready"):
                results[i] = previous_source
    if args.full_axekin:
        axekin_source = next(source for source in sources if source["id"] == "axekin")
        print("Axekin: crawling full catalogue (only Games/Roms consoles)...", flush=True)
        def report(completed, total):
            print(f"Axekin: verified {completed}/{total} pages", flush=True)
        games, coverage = axekin_full(axekin_source, progress=report)
        output_full = ROOT / "assets/catalogs/axekin.json"
        capacity_match = re.search(r"#define MAX_CATALOG_ITEMS\s+(\d+)",
                                   (ROOT / "src/wolodrive.h").read_text(encoding="utf8"))
        capacity = int(capacity_match.group(1)) if capacity_match else 0
        other_entries = sum(len(s["packages"]) for s in results if s["id"] != "axekin")
        if len(games) + other_entries + 512 > capacity:
            raise ValueError(f"Full Axekin catalog exceeds native app capacity: "
                             f"{len(games)}+{other_entries}+512 > {capacity}")
        write_snapshot(output_full, {"schemaVersion": 1, "sources": [{
            "id": "axekin", "name": axekin_source["name"], "website": axekin_source["url"],
            "status": "ready", "coverage": coverage, "packages": games
        }]})
        print(f"Axekin: saved {len(games)} supported-console entries; excluded "
              f"{coverage['excludedGames']} unsupported games", flush=True)
    # Do not reintroduce the first 64 Axekin games when refreshing other sources.
    axekin_file = ROOT / "assets/catalogs/axekin.json"
    if axekin_file.exists():
        metadata = json.loads(axekin_file.read_text(encoding="utf8"))["sources"][0]
        full_count = len(metadata["packages"])
        for result in results:
            if result["id"] == "axekin":
                result.update(packages=[], status=metadata.get("status", "ready"),
                              note=f"Full Games/Roms-filtered index: {full_count} entries in axekin.json")
                break
    # The native app uses a fixed-size array. Reject index expansions instead
    # of writing a catalogue that silently truncates titles on PS5/Linux.
    axekin_count = 0
    if axekin_file.exists():
        axekin_count = len(json.loads(axekin_file.read_text(encoding="utf8"))["sources"][0]["packages"])
    capacity_match = re.search(r"#define MAX_CATALOG_ITEMS\s+(\d+)",
                               (ROOT / "src/wolodrive.h").read_text(encoding="utf8"))
    capacity = int(capacity_match.group(1)) if capacity_match else 0
    total_entries = axekin_count + sum(len(source["packages"]) for source in results)
    if total_entries + 512 > capacity:
        raise ValueError(f"Catalogue would exceed native capacity: {total_entries}+512 > {capacity}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(".json.tmp")
    temporary.write_text(json.dumps({"schemaVersion": 1, "sources": results}, ensure_ascii=False, indent=2) + "\n", encoding="utf8", newline="\n")
    temporary.replace(output)
    for result in results:
        print(f"{result['name']}: {len(result['packages'])} entries ({result['status']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
