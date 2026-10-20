"""Resolve missing PS3/Switch covers using public GameTDB metadata and product IDs.

Only downloaded images enter the export. Existing artwork and review decisions
retain priority; exact unique console/title matching is used only without an ID.
"""
import argparse
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
import io
import json
import re
from urllib.request import Request, urlopen
import xml.etree.ElementTree as ET
import zipfile

from enrich_artwork import ROOT, load_records, safe_url
from igdb_artwork import atomic_json, search_title
from launchbox_artwork import title_key
import download_artwork as downloader

SYSTEMS = {"sony-ps3": ("ps3", "PS3", r"[A-Z]{4}\d{5}"),
           "nintendo-switch": ("switch", "Switch", r"[A-Z0-9]{5}")}
EXPORT = ROOT / "assets/artwork/gametdb-covers.json"
CACHE = ROOT / ".cache/wolodrive-artwork"


def cover_url(platform, product, language):
    if platform not in SYSTEMS or not re.fullmatch(SYSTEMS[platform][2], product):
        raise ValueError("Invalid GameTDB product ID or platform")
    if language not in {"US", "EN", "JA"}:
        raise ValueError("Unsupported cover language")
    return f"https://art.gametdb.com/{SYSTEMS[platform][0]}/coverM/{language}/{product}.jpg"


def trusted_match(row):
    if not isinstance(row, dict) or row.get("provider") not in {
            "gametdb-exact-product-id", "gametdb-exact-platform-title"}:
        return False
    url = row.get("url", "")
    return (type(row.get("confidence")) is int and row["confidence"] >= 95 and
            isinstance(url, str) and bool(re.fullmatch(
                r"https://art\.gametdb\.com/(?:ps3/coverM/(?:US|EN|JA)/[A-Z]{4}\d{5}|"
                r"switch/coverM/(?:US|EN|JA)/[A-Z0-9]{5})\.jpg", url)))


def fetch_database(platform, refresh=False):
    console = SYSTEMS[platform][0]
    path = CACHE / (console + "tdb.zip")
    if path.is_file() and not refresh:
        return path
    with urlopen(Request(f"https://www.gametdb.com/{console}tdb.zip?LANG=EN&TIME=1",
                         headers={"User-Agent": "Wolodrive-Artwork/1.0"}), timeout=60) as response:
        data = response.read(32 * 1024 * 1024 + 1)
    if len(data) > 32 * 1024 * 1024:
        raise ValueError("GameTDB archive exceeds 32 MiB")
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        archive.getinfo(console + "tdb.xml")
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".tmp")
    temporary.write_bytes(data)
    temporary.replace(path)
    return path


def read_database(platform, path):
    console, kind, pattern = SYSTEMS[platform]
    products, names = {}, defaultdict(set)
    with zipfile.ZipFile(path) as archive:
        if archive.getinfo(console + "tdb.xml").file_size > 256 * 1024 * 1024:
            raise ValueError("GameTDB XML exceeds 256 MiB")
        with archive.open(console + "tdb.xml") as stream:
            events = ET.iterparse(stream, events=("start", "end"))
            _, root = next(events)
            for event, node in events:
                if event != "end" or node.tag != "game":
                    continue
                product = node.findtext("id", "")
                if node.findtext("type") == kind and re.fullmatch(pattern, product):
                    titles = {locale.findtext("title", "") for locale in node.findall("locale")}
                    keys = {title_key(title) for title in titles if title_key(title)}
                    # Conflicting records for one product cannot prove identity.
                    if product in products and products[product] != keys:
                        raise ValueError("Conflicting GameTDB product records")
                    products[product] = keys
                    for key in keys:
                        names[key].add(product)
                node.clear()
                root.clear()
    return products, names


def candidates(records, baseline, databases):
    matches, counts = {}, Counter()
    protected = re.compile(r"\b(?:demo|trial|DLC|bundle|edition|collection|remaster(?:ed)?|unlock(?:er)?|theme|update|UNDUB|hack|translation)\b", re.I)
    for source, row in records:
        identity = source + "|" + row["id"]
        platform = row["platform"]
        if (platform not in databases or identity in baseline or
                safe_url(row.get("boxArtUrl") or row.get("coverUrl"))):
            continue
        products, names = databases[platform]
        key = title_key(search_title(platform, row["title"]))
        product = row.get("productId", "")
        via = "gametdb-exact-product-id"
        if product:
            if product not in products:
                counts["unknown-product-id"] += 1
                continue
            if protected.search(row["title"]) and key not in products[product]:
                counts["release-label-review"] += 1
                continue
        else:
            choices = names.get(key, set()) if key else set()
            if len(choices) != 1:
                counts["ambiguous-title" if choices else "not-found"] += 1
                continue
            product, = choices
            via = "gametdb-exact-platform-title"
        region = row.get("productRegion", "")
        language = "JA" if region == "JP" else "EN" if region == "EU" else "US"
        # Same exact product ID in both URLs; never substitute a retail ID for
        # a digital release or guess another console/edition.
        languages = list(dict.fromkeys((language, "US", "EN")))[:2]
        matches[identity] = {"urls": [cover_url(platform, product, lang) for lang in languages],
                             "productId": product, "provider": via, "confidence": 95}
        counts["candidate"] += 1
    return matches, dict(counts)


def download_candidate(row):
    for url in row["urls"]:
        result = downloader.download(url, ROOT / "assets/covers")
        if result["status"] != "failed":
            return {**{k: v for k, v in row.items() if k != "urls"}, "url": url}, result
        if provider_unavailable(result):
            break
    return None, result


def provider_unavailable(result):
    error = result.get("error", "").lower()
    return result.get("status") == "failed" and any(
        text in error for text in ("timed out", "name resolution", "getaddrinfo", "http error 403", "http error 429"))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--offline", action="store_true", help="Use cached metadata and export only; no requests")
    parser.add_argument("--refresh", action="store_true", help="Download current metadata archives")
    parser.add_argument("--workers", type=int, default=3, choices=range(1, 5))
    args = parser.parse_args(argv)
    if args.offline and args.refresh:
        parser.error("--offline and --refresh cannot be combined")
    databases = {}
    for platform, (console, _, _) in SYSTEMS.items():
        path = CACHE / (console + "tdb.zip")
        if not args.offline:
            path = fetch_database(platform, args.refresh)
        databases[platform] = read_database(platform, path)
    manifest = json.loads((ROOT / "assets/artwork/cover-matches.json").read_text(encoding="utf-8"))["matches"]
    pending, counts = candidates(load_records(), manifest, databases)
    print("GameTDB candidates:", counts, flush=True)
    previous = json.loads(EXPORT.read_text(encoding="utf-8"))["matches"] if EXPORT.is_file() else {}
    matches = {k: v for k, v in previous.items() if trusted_match(v)}
    results = {}
    (ROOT / "assets/covers").mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        # Share each exact URL pair across duplicate source entries.
        groups = defaultdict(list)
        for identity, row in pending.items():
            if identity not in matches:
                groups[tuple(row["urls"])].append(identity)
        if args.offline:
            groups = {urls: ids for urls, ids in groups.items()
                      if any((ROOT / "assets/covers" / downloader.cache_name(u)).is_file() for u in urls)}
        def fetch(urls):
            row = pending[groups[urls][0]]
            if args.offline:
                row = {**row, "urls": [u for u in urls if (ROOT / "assets/covers" / downloader.cache_name(u)).is_file()]}
                # Offline mode never lets an invalid cached file trigger a fetch.
                for u in row["urls"]:
                    try:
                        downloader.image_type((ROOT / "assets/covers" / downloader.cache_name(u)).read_bytes())
                        return {**{k: v for k, v in row.items() if k != "urls"}, "url": u}, {"status": "cached"}
                    except (OSError, ValueError):
                        pass
                return None, {"status": "failed", "error": "No valid cached cover"}
            return download_candidate(row)
        jobs = {pool.submit(fetch, urls): urls for urls in groups}
        consecutive_outages = 0
        for future in as_completed(jobs):
            urls = jobs[future]
            match, result = future.result()
            results[urls[0]] = result
            consecutive_outages = consecutive_outages + 1 if provider_unavailable(result) else 0
            if match:
                for identity in groups[urls]:
                    matches[identity] = {**match, "provider": pending[identity]["provider"]}
            if len(results) % 25 == 0:
                print(f"Checked {len(results)}/{len(groups)} products: " +
                      str(dict(Counter(r["status"] for r in results.values()))), flush=True)
                atomic_json(EXPORT, {"schemaVersion": 1, "matches": dict(sorted(matches.items()))})
            if consecutive_outages >= 3:
                print("GameTDB image host unavailable; deferring remaining requests. Cached files are retained.", flush=True)
                for job, remaining_urls in jobs.items():
                    if remaining_urls[0] not in results:
                        job.cancel()
                        results[remaining_urls[0]] = {"status": "deferred", "error": "Provider unavailable; retry later"}
                break
    atomic_json(EXPORT, {"schemaVersion": 1, "matches": dict(sorted(matches.items()))})
    atomic_json(CACHE / "gametdb-downloads.json", {"results": results})
    print("GameTDB cover results:", dict(Counter(r["status"] for r in results.values())), flush=True)
    import enrich_artwork
    status = enrich_artwork.main(["--offline"])
    return status or int(any(row["status"] == "deferred" for row in results.values()))


if __name__ == "__main__":
    raise SystemExit(main())
