"""Index PS3 artwork from dlpsgame.com's public category; never fetch game files.

WordPress post bodies are parsed in memory only. The cache contains allowlisted
display metadata and the first primary image, never download links or page HTML.
"""
import argparse
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from html.parser import HTMLParser
import json
import re
from urllib.parse import urlencode, urlsplit, unquote
from urllib.request import Request, urlopen

from enrich_artwork import ROOT, load_records, safe_url
from igdb_artwork import atomic_json, search_title
from launchbox_artwork import title_key
from import_sources import clean
import download_artwork as downloader

CACHE = ROOT / ".cache/wolodrive-artwork/dlpsgame-ps3.json"
EXPORT = ROOT / "assets/artwork/dlpsgame-covers.json"
CATEGORY = 64  # Linked by the user's /category/ps3/ page, not a guessed ID.
EXCLUSIONS = {(row["sourceUrl"], row["url"]) for row in json.loads(
    (ROOT / "config/dlpsgame_artwork_exclusions.json").read_text(encoding="utf-8"))}
HOST = "dlpsgame.com"
IMAGE_HOSTS = {HOST, "www." + HOST, "blogger.googleusercontent.com"} | {
    f"{i}.bp.blogspot.com" for i in range(1, 5)}


def safe_post(url):
    if not isinstance(url, str):
        return False
    try:
        parts = urlsplit(url)
    except ValueError:
        return False
    return (parts.scheme == "https" and parts.netloc == HOST and not parts.query and not parts.fragment
            and bool(re.fullmatch(r"/(?:[a-z0-9-]|%[0-9a-fA-F]{2})+/", parts.path))
            and unquote(parts.path).count("/") == 2 and ".." not in unquote(parts.path))


def post_title_key(title):
    # PSN is this PS3 category's storefront suffix, not an edition. Other
    # identity-bearing words (HD, Demo, Mod, DLC, Collection) remain intact.
    title = re.sub(r"\s+PSN\s*$", "", title.replace("\u200b", ""), flags=re.I)
    return title_key(search_title("sony-ps3", title))


def safe_image(url):
    if not isinstance(url, str):
        return False
    try:
        parts = urlsplit(url)
        port = parts.port
    except ValueError:
        return False
    return (parts.scheme == "https" and parts.hostname in IMAGE_HOSTS and
            parts.username is None and parts.password is None and port in (None, 443)
            and bool(re.search(r"\.(?:jpg|jpeg|png|webp)$", parts.path, re.I))
            and (parts.hostname not in {HOST, "www." + HOST} or parts.path.startswith("/wp-content/uploads/")))


class Cover(HTMLParser):
    def __init__(self):
        super().__init__()
        self.url = ""

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "img" and not self.url:
            url = attrs.get("data-src") or attrs.get("src", "")
            # Normalise legacy HTTP image links without making HTTP requests.
            if url.startswith("http://"):
                url = "https://" + url[7:]
            if safe_image(url):
                self.url = url


def page(number):
    query = urlencode({"categories": CATEGORY, "per_page": 100, "page": number,
                       "_fields": "id,link,title,categories,content.rendered"})
    url = "https://dlpsgame.com/wp-json/wp/v2/posts?" + query
    with urlopen(Request(url, headers={"User-Agent": "Wolodrive-Artwork/1.0"}), timeout=25) as response:
        data = response.read(16 * 1024 * 1024 + 1)
        total, pages = int(response.headers["X-WP-Total"]), int(response.headers["X-WP-TotalPages"])
    if len(data) > 16 * 1024 * 1024 or not 1 <= pages <= 100 or not 1 <= total <= 10000:
        raise ValueError("Unexpected DLPSGame pagination/response size")
    rows = json.loads(data)
    if not isinstance(rows, list):
        raise ValueError("Unexpected DLPSGame post response")
    records = []
    for row in rows:
        link = row.get("link", "")
        if (CATEGORY not in row.get("categories", []) or type(row.get("id")) is not int or
                not safe_post(link)):
            continue
        title = clean(row.get("title", {}).get("rendered", ""))
        parser = Cover()
        parser.feed(row.get("content", {}).get("rendered", ""))
        records.append({"postId": row["id"], "title": title, "platform": "sony-ps3",
                        "sourceUrl": link, "url": parser.url})
    return records, total, pages, len(rows)


def build_index(refresh=False):
    if CACHE.is_file() and not refresh:
        data = json.loads(CACHE.read_text(encoding="utf-8"))
        if data.get("schemaVersion") != 1 or data.get("category") != CATEGORY or not data.get("complete"):
            raise ValueError("Unexpected DLPSGame cache; use --refresh")
        return data["posts"]
    first, total, pages, received = page(1)
    records = list(first)
    seen = {row["postId"] for row in first}
    with ThreadPoolExecutor(max_workers=2) as pool:
        jobs = {pool.submit(page, number): number for number in range(2, pages + 1)}
        for future in as_completed(jobs):
            rows, current_total, current_pages, count = future.result()
            if current_total != total or current_pages != pages or not count:
                raise ValueError("DLPSGame pagination changed; preserving previous cache")
            received += count
            for row in rows:
                if row["postId"] in seen:
                    raise ValueError("Duplicate DLPSGame post across pages")
                records.append(row)
                seen.add(row["postId"])
    if received != total or len(records) != total:
        raise ValueError("Incomplete PS3 metadata; preserving previous export")
    records.sort(key=lambda row: row["postId"])
    atomic_json(CACHE, {"schemaVersion": 1, "category": CATEGORY, "complete": True, "posts": records})
    return records


def candidates(records, baseline, posts):
    index = defaultdict(list)
    for post in posts:
        key = post_title_key(post["title"])
        if post["platform"] == "sony-ps3" and key:
            index[key].append(post)
    matches, counts = {}, Counter()
    for source, row in records:
        identity = source + "|" + row["id"]
        if (row["platform"] != "sony-ps3" or identity in baseline or
                safe_url(row.get("boxArtUrl") or row.get("coverUrl"))):
            continue
        key = title_key(search_title("sony-ps3", row["title"]))
        choices = index.get(key, []) if key else []
        # A second post is ambiguous even when only one post has a cover.
        if len(choices) != 1:
            counts["ambiguous" if choices else "not-found"] += 1
            continue
        post, = choices
        if (post["sourceUrl"], post["url"]) in EXCLUSIONS:
            counts["withheld-image"] += 1
            continue
        if not safe_image(post["url"]):
            counts["no-cover"] += 1
            continue
        matches[identity] = {"url": post["url"], "sourceUrl": post["sourceUrl"],
                             "provider": "dlpsgame-exact-ps3-title", "confidence": 95}
        counts["candidate"] += 1
    return matches, dict(counts)


def trusted_match(row):
    return (isinstance(row, dict) and row.get("provider") == "dlpsgame-exact-ps3-title" and
            type(row.get("confidence")) is int and row["confidence"] >= 95 and safe_image(row.get("url")) and
            safe_post(row.get("sourceUrl")) and (row["sourceUrl"], row["url"]) not in EXCLUSIONS)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--offline", action="store_true", help="Use local metadata/images only; no requests")
    parser.add_argument("--refresh", action="store_true", help="Refresh all PS3 category metadata")
    args = parser.parse_args(argv)
    if args.offline and (args.refresh or not CACHE.is_file()):
        parser.error("Offline mode requires the saved cache and cannot refresh it")
    posts = build_index(args.refresh)
    print(f"DLPSGame PS3 posts: {len(posts)}", flush=True)
    baseline = json.loads((ROOT / "assets/artwork/cover-matches.json").read_text(encoding="utf-8"))["matches"]
    pending, counts = candidates(load_records(), baseline, posts)
    print("DLPSGame candidates:", counts, flush=True)
    previous = json.loads(EXPORT.read_text(encoding="utf-8"))["matches"] if EXPORT.is_file() else {}
    matches = {k: v for k, v in previous.items() if trusted_match(v)}
    output = ROOT / "assets/covers"
    output.mkdir(parents=True, exist_ok=True)
    groups = defaultdict(list)
    for identity, row in pending.items():
        if identity not in matches:
            groups[row["url"]].append(identity)
    results = {}
    def download(url):
        if args.offline:
            try:
                kind = downloader.image_type((output / downloader.cache_name(url)).read_bytes())
                return {"status": "cached", "type": kind}
            except (OSError, ValueError):
                return {"status": "unavailable-offline"}
        return downloader.download(url, output)
    with ThreadPoolExecutor(max_workers=3) as pool:
        jobs = {pool.submit(download, url): url for url in groups}
        for future in as_completed(jobs):
            url = jobs[future]
            result = results[url] = future.result()
            if result["status"] in {"cached", "downloaded"}:
                for identity in groups[url]:
                    matches[identity] = pending[identity]
            if len(results) % 25 == 0:
                print(f"Checked {len(results)}/{len(groups)} covers", flush=True)
    atomic_json(EXPORT, {"schemaVersion": 1, "matches": dict(sorted(matches.items()))})
    atomic_json(CACHE.parent / "dlpsgame-downloads.json", {"results": results})
    print("DLPSGame image results:", dict(Counter(row["status"] for row in results.values())), flush=True)
    import enrich_artwork
    return enrich_artwork.main(["--offline"])


if __name__ == "__main__":
    raise SystemExit(main())
