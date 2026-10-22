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
from import_sources import fetch, platform, archive_alvro_display_version, archive_collection_ids

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
    identifiers = archive_collection_ids(source)
    uploader = None
    if identifiers is not None:
        documents = [{"identifier": identity} for identity in identifiers]
        expected = len(documents)
    else:
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
    restricted = 0
    collections = {}

    def load(document):
        identity = document.get("identifier", "")
        if not re.fullmatch(r"[-_.a-zA-Z0-9]+", identity):
            raise ValueError("Invalid archive item ID")
        data = json.loads(fetch("https://archive.org/metadata/" + urllib.parse.quote(identity, safe="")))
        item_metadata = data.get("metadata", {})
        if uploader and str(item_metadata.get("uploader") or "").strip().lower() != uploader:
            raise ValueError("Archive uploader changed during crawl")
        if identifiers is not None and str(item_metadata.get("creator") or "").strip().lower() != "alvro":
            raise ValueError("Archive collection is no longer attributed to AlvRo")
        return document, item_metadata, data.get("files", [])

    with ThreadPoolExecutor(max_workers=workers) as pool:
        jobs = [pool.submit(load, document) for document in documents]
        for future in as_completed(jobs):
            document, metadata, files = future.result()
            archive = document["identifier"]
            title = str(metadata.get("title") or document.get("title") or "")
            system = platform(title+" "+str(metadata.get("subject") or document.get("subject") or ""))
            stats = {"identifier": archive,
                     "url": "https://archive.org/download/" + archive,
                     "title": title, "publicGameFiles": 0, "restrictedGameFiles": 0}
            collections[archive] = stats
            for file in files:
                name = file.get("name", "")
                if not FORMATS.search(name) or name.startswith("__"):
                    continue
                if file.get("private"):
                    restricted += 1
                    stats["restrictedGameFiles"] += 1
                    continue
                files_found += 1
                stats["publicGameFiles"] += 1
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
                    "format": "catalog", "sourceUrl": page,
                    **({"version": archive_alvro_display_version(name)} if source["id"] == "alvro" else {})
                })
    entries.sort(key=lambda item: (item["platform"], item["title"].lower(), item["id"]))
    reports = [collections[item["identifier"]] for item in documents]
    restricted_collections = sum(report["restrictedGameFiles"] > 0 and
                                 report["publicGameFiles"] == 0 for report in reports)
    return {"schemaVersion": 1, "sources": [{
        "id": source["id"], "name": source["name"], "website": source["url"],
        "status": "ready",
        "note": (f"{expected} collections; {restricted_collections} restricted; "
                 f"{len(entries)} public games indexed") if identifiers is not None else "",
        "coverage": {"archiveItems": expected, "gameFiles": files_found,
                     "restrictedFiles": restricted, "excludedFiles": skipped,
                     "indexedEntries": len(entries), "collections": reports},
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
