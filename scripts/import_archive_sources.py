"""Index every public game file listed in matching Archive.org collections.

Only game filename/title and archive item page URL are retained; no download URL,
archive file content, or public uploader identifier is written into snapshots.
"""
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path
import re
import sys
import urllib.parse

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from import_sources import fetch, platform

FORMATS = re.compile(r"\.(?:7z|zip|rar|iso|chd|cso|pbp|pkg|nsp|xci|cia|nds|gba|gbc|nes|sfc|n64)$", re.I)

def archive_query(source):
    if not source.get("seed_identifier"):
        return source["query"], None
    seed = source["seed_identifier"]
    if not re.fullmatch(r"[-_a-zA-Z0-9]+", seed):
        raise ValueError("Invalid archive identifier")
    metadata = json.loads(fetch("https://archive.org/metadata/" + seed))
    uploader = str(metadata.get("metadata", {}).get("uploader") or "").strip().lower()
    if not re.fullmatch(r"[a-z0-9._%+-]+@[a-z0-9.-]+", uploader):
        raise ValueError("Archive seed has no public uploader identifier")
    return 'uploader:"' + uploader + '"', uploader

def index_source(source, workers=4):
    query, uploader = archive_query(source)
    url = "https://archive.org/advancedsearch.php?" + urllib.parse.urlencode({
        "q": query, "rows": 1000, "page": 1,
        "fl[]": ["identifier", "title", "subject", "mediatype"], "output": "json"
    }, doseq=True)
    response = json.loads(fetch(url))["response"]
    documents = response.get("docs", [])
    expected = int(response.get("numFound", -1))
    if expected < 0 or expected > 1000 or len(documents) != expected:
        raise ValueError(f"Archive result set incomplete: {len(documents)}/{expected}")

    supported = {v["platform"] for v in json.loads(
        (ROOT / "config/axekin_platforms.json").read_text(encoding="utf8")).values()}
    seen = set()
    entries = []
    files_found = 0
    skipped = 0

    def load(document):
        identity = document.get("identifier", "")
        if not re.fullmatch(r"[-_.a-zA-Z0-9]+", identity):
            raise ValueError("Invalid archive item ID")
        data = json.loads(fetch("https://archive.org/metadata/" + urllib.parse.quote(identity, safe="")))
        if uploader and str(data.get("metadata", {}).get("uploader") or "").strip().lower() != uploader:
            raise ValueError("Archive uploader changed during crawl")
        return document, data.get("files", [])

    with ThreadPoolExecutor(max_workers=workers) as pool:
        jobs = [pool.submit(load, document) for document in documents]
        for future in as_completed(jobs):
            document, files = future.result()
            archive = document["identifier"]
            system = platform(str(document.get("title") or "")+" "+str(document.get("subject") or ""))
            for file in files:
                name = file.get("name", "")
                if not FORMATS.search(name) or name.startswith("__") or file.get("private"):
                    continue
                files_found += 1
                entry_system = platform(name, system)
                if not entry_system or entry_system not in supported:
                    skipped += 1
                    continue
                title = Path(name).stem.strip()
                if len(title) < 2:
                    skipped += 1
                    continue
                key = (archive, name, entry_system)
                if key in seen:
                    continue
                seen.add(key)
                page = "https://archive.org/details/" + urllib.parse.quote(archive, safe="")
                entries.append({
                    "id": "EXT-" + hashlib.sha256(
                        (source["id"] + ":" + archive + ":" + name).encode()).hexdigest()[:20],
                    "title": title[:200], "platform": entry_system,
                    "format": "catalog", "sourceUrl": page
                })
    entries.sort(key=lambda item: (item["platform"], item["title"].lower(), item["id"]))
    return {"schemaVersion": 1, "sources": [{
        "id": source["id"], "name": source["name"], "website": source["url"],
        "status": "ready",
        "coverage": {"archiveItems": expected, "gameFiles": files_found,
                     "excludedFiles": skipped, "indexedEntries": len(entries)},
        "packages": entries
    }]}

def main():
    source_list = json.loads((ROOT / "config/additional_sources.json").read_text(encoding="utf8"))
    for source in source_list:
        if source["id"] not in ("alvro", "maelly-pooh"):
            continue
        print("Indexing " + source["name"], flush=True)
        snapshot = index_source(source)
        target = ROOT / "assets/catalogs" / (source["id"] + ".json")
        temporary = target.with_suffix(".json.tmp")
        temporary.write_text(json.dumps(snapshot, separators=(",", ":"), ensure_ascii=False) + "\n",
                             encoding="utf8")
        temporary.replace(target)
        print(source["id"], snapshot["sources"][0]["coverage"], flush=True)

if __name__ == "__main__":
    main()
