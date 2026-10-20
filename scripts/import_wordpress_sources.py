"""Full public WordPress metadata index for supported Games/Roms consoles.

Read-only, resumable by published site index rather than download mirrors.
No binary files, download links or credentials are saved.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import hashlib
import html
import json
from pathlib import Path
import re
import time
import urllib.error
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
PLATFORM_CONFIG = ROOT / "config/wordpress_sources.json"
USER_AGENT = "Poldrive/2.0 public catalog metadata importer"

def page(url):
    """Retrieve a public API page, obeying Retry-After on temporary failures."""
    for attempt in range(4):
        request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
        try:
            with urllib.request.urlopen(request, timeout=25) as response:
                if response.status != 200:
                    raise ValueError("Unexpected WP status")
                blob = response.read(4 * 1024 * 1024 + 1)
                if len(blob) > 4 * 1024 * 1024:
                    raise ValueError("WordPress response exceeds safe limit")
                return json.loads(blob), {k.lower(): v for k, v in response.headers.items()}
        except urllib.error.HTTPError as error:
            if error.code not in (429, 500, 502, 503, 504) or attempt == 3:
                raise
            wait = error.headers.get("Retry-After", "")
            time.sleep(min(10, int(wait) if wait.isdigit() else 1 + attempt * 2))
        except (TimeoutError, urllib.error.URLError):
            if attempt == 3:
                raise
            time.sleep(1 + attempt * 2)
    raise RuntimeError("Unexpected WordPress retry exit")

def index_one(source, max_workers=3, on_progress=None):
    root = source["url"].rstrip("/")
    host = urllib.parse.urlsplit(root).netloc.lower()
    category_platforms = {int(k): v for k, v in source["categories"].items()}
    supported = set(json.loads((ROOT / "config/axekin_platforms.json").read_text(encoding="utf8"))[key]["platform"]
                    for key in json.loads((ROOT / "config/axekin_platforms.json").read_text(encoding="utf8")))
    if set(category_platforms.values()) - supported:
        raise ValueError("WordPress console not supported by Games/Roms")

    def url_for(n):
        return root + "/wp-json/wp/v2/posts?" + urllib.parse.urlencode({
            "per_page": 100, "page": n,
            "_fields": "id,link,title,categories,date_gmt"
        })

    first, headers = page(url_for(1))
    total = int(headers.get("x-wp-total", -1))
    pages = int(headers.get("x-wp-totalpages", -1))
    if not isinstance(first, list) or not 1 <= pages <= 500 or not 1 <= total <= 50000:
        raise ValueError("WordPress page metadata missing or unexpectedly large")
    results = {1: first}
    with ThreadPoolExecutor(max_workers=max_workers) as pool:
        future_map = {pool.submit(page, url_for(i)): i for i in range(2, pages + 1)}
        for future in as_completed(future_map):
            n = future_map[future]
            posts, hdr = future.result()
            if not isinstance(posts, list):
                raise ValueError(f"WordPress page {n}: invalid JSON")
            if int(hdr.get("x-wp-totalpages", -1)) != pages:
                raise ValueError("WordPress pagination changed during crawl")
            results[n] = posts
            if on_progress and (len(results) % 25 == 0 or len(results) == pages):
                on_progress(source["id"], len(results), pages)

    posts_seen = set()
    entries = []
    excluded = 0
    for n in range(1, pages + 1):
        posts = results[n]
        if not posts:
            raise ValueError(f"Empty WordPress page {n}/{pages}")
        for post in posts:
            identity = post.get("id")
            if not isinstance(identity, int):
                excluded += 1
                continue
            if identity in posts_seen:
                continue
            posts_seen.add(identity)
            link = post.get("link") or ""
            if urllib.parse.urlsplit(link).netloc.lower() != host:
                excluded += 1
                continue
            title = html.unescape(re.sub(r"<[^>]*>", " ", post.get("title", {}).get("rendered") or "")).strip()
            if not title:
                excluded += 1
                continue
            cats = post.get("categories") or []
            platforms = sorted({category_platforms[c] for c in cats if c in category_platforms})
            if not platforms:
                excluded += 1
                continue
            timestamp = post.get("date_gmt") or ""
            published = timestamp + "Z" if re.fullmatch(r"\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d", timestamp) else ""
            for plat in platforms:
                key = f"{source['id']}:{identity}:{plat}"
                entries.append({
                    "id": "EXT-" + hashlib.sha256(key.encode()).hexdigest()[:20],
                    "title": title,
                    "platform": plat,
                    "format": "catalog",
                    "sourceUrl": link,
                    **({"createdAt": published} if published else {})
                })
    if len(posts_seen) != total:
        raise ValueError(f"WordPress crawl incomplete: {len(posts_seen)}/{total}")
    return {"schemaVersion": 1, "sources": [{
        "id": source["id"], "name": source["name"], "website": source["url"],
        "status": "ready",
        "coverage": {"posts": total, "pages": pages, "excludedPosts": excluded,
                     "indexedEntries": len(entries), "supportedPlatforms": sorted(set(category_platforms.values()))},
        "packages": entries
    }]}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", action="append", choices=("superpsx", "downloadgamepsp", "ziperto"),
                        help="Select sites; default all three")
    parser.add_argument("--workers", type=int, default=3)
    args = parser.parse_args()
    if args.workers < 1 or args.workers > 6:
        parser.error("--workers must be between 1 and 6")
    sources = json.loads(PLATFORM_CONFIG.read_text(encoding="utf8"))
    for source in sources:
        if args.site and source["id"] not in args.site:
            continue
        print(f"Indexing {source['name']}...", flush=True)
        output = index_one(source, args.workers,
                           lambda name, done, total: print(f"{name}: {done}/{total} pages", flush=True))
        folder = ROOT / "assets/catalogs"
        target = folder / (source["id"] + ".json")
        temporary = target.with_suffix(".json.tmp")
        temporary.write_text(json.dumps(output, ensure_ascii=False, separators=(",", ":")) + "\n",
                             encoding="utf8")
        temporary.replace(target)
        stats = output["sources"][0]["coverage"]
        print(f"{source['name']}: {stats['indexedEntries']} supported entries / "
              f"{stats['posts']} posts ({stats['pages']} pages); excluded {stats['excludedPosts']}", flush=True)

if __name__ == "__main__":
    main()
