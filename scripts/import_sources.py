"""Import bounded public catalog metadata for the native app and static React preview.

No package files, account data, license keys, or download-link payloads are stored.
Only allowlisted display metadata and public source-page links enter the snapshots.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import csv
from datetime import datetime, timezone
import hashlib
import html
from html.parser import HTMLParser
import io
import json
from pathlib import Path
import re
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
MAX_RESPONSE = 24 * 1024 * 1024


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


def archive(source, limit):
    params = {"q": source["query"], "output": "json", "rows": 50,
              "fl[]": ["identifier", "title", "subject", "mediatype"]}
    endpoint = "https://archive.org/advancedsearch.php?" + urllib.parse.urlencode(params, doseq=True)
    document = json.loads(fetch(endpoint))["response"]
    rows = []
    for item in document["docs"]:
        identity = item["identifier"]; title = str(item.get("title", identity))
        system = platform(title + " " + str(item.get("subject", "")))
        metadata = json.loads(fetch("https://archive.org/metadata/" + urllib.parse.quote(identity, safe="")))
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
        adapters = {"tsv": tsv, "html": website, "gfs": gfs, "archive": archive, "vimm": vimm}
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
    args = parser.parse_args(argv)
    if not 1 <= args.limit <= 400:
        parser.error("--limit must be between 1 and 400 to preserve native catalog capacity")
    sources = json.loads((ROOT / "config/additional_sources.json").read_text(encoding="utf8"))
    output = ROOT / "assets/catalogs/additional.json"
    previous = json.loads(output.read_text(encoding="utf8")) if output.exists() else {"sources": []}
    old = {item["id"]: item for item in previous["sources"]}
    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(lambda source: import_source(source, args.limit, old.get(source["id"])), sources))
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(".json.tmp")
    temporary.write_text(json.dumps({"schemaVersion": 1, "sources": results}, ensure_ascii=False, indent=2) + "\n", encoding="utf8", newline="\n")
    temporary.replace(output)
    for result in results:
        print(f"{result['name']}: {len(result['packages'])} entries ({result['status']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
