"""Index full public RomsTime game sitemap, retaining only known Games/Roms consoles.

The sitemap lists source detail pages, not file or mirror URLs. No downloads.
"""
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path
import re
import sys
import urllib.parse
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from import_sources import fetch

NS = {"s": "http://www.sitemaps.org/schemas/sitemap/0.9"}

def get_urls(url):
    tree = ET.fromstring(fetch(url))
    return [e.text for e in tree.findall(".//s:loc", NS) if e.text]

def import_sitemap(workers=5):
    base = "https://romstime.com"
    mapping = json.loads((ROOT / "config/romstime_platforms.json").read_text(encoding="utf8"))
    supported = {v["platform"] for v in json.loads(
        (ROOT / "config/axekin_platforms.json").read_text(encoding="utf8")).values()}
    if set(mapping.values()) - supported:
        raise ValueError("RomsTime includes an unsupported Games/Roms console")
    feeds = get_urls(base + "/sitemap.xml")
    if not feeds or len(feeds) > 100 or any(not f.startswith(base + "/sitemap/") for f in feeds):
        raise ValueError("Unexpected RomsTime sitemap index")
    results = {}
    with ThreadPoolExecutor(max_workers=workers) as pool:
        jobs = {pool.submit(get_urls, u): i for i, u in enumerate(feeds)}
        for task in as_completed(jobs):
            results[jobs[task]] = task.result()
    entries = []
    seen = set()
    all_game_urls = 0
    excluded = 0
    for i in range(len(feeds)):
        for url in results[i]:
            match = re.fullmatch(r"https://romstime\.com/roms/([a-z0-9-]+)/([a-z0-9-]+)/?", url)
            if not match:
                continue
            all_game_urls += 1
            category, slug = match.groups()
            if category not in mapping:
                excluded += 1
                continue
            platform = mapping[category]
            if url in seen:
                continue
            seen.add(url)
            title = urllib.parse.unquote(slug).replace("-", " ").title()
            entries.append({
                "id": "EXT-" + hashlib.sha256(("romstime|" + url).encode()).hexdigest()[:20],
                "title": title,
                "platform": platform,
                "format": "catalog",
                "sourceUrl": url
            })
    if all_game_urls < 1000:
        raise ValueError("RomsTime sitemap appears incomplete")
    return {"schemaVersion": 1, "sources": [{
        "id": "romstime", "name": "RomsTime", "website": base + "/",
        "status": "ready", "coverage": {
            "sitemapFiles": len(feeds), "gameUrls": all_game_urls,
            "unsupportedUrls": excluded, "indexedEntries": len(entries)},
        "packages": entries
    }]}

if __name__ == "__main__":
    print("Reading RomsTime public sitemap...", flush=True)
    data = import_sitemap()
    target = ROOT / "assets/catalogs/romstime.json"
    temporary = target.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(data, separators=(",", ":"), ensure_ascii=False)+"\n", encoding="utf8")
    temporary.replace(target)
    print(data["sources"][0]["coverage"], flush=True)
