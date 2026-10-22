"""Refresh display labels without changing snapshot identity, ordering or sizes.

Only versions and allowlisted format names survive the public payload. No
download URLs are written to the offline native/browser catalogues.
"""
from concurrent.futures import ThreadPoolExecutor, as_completed
import argparse
from datetime import datetime, timezone
import json
import re
import urllib.parse

from import_sources import (ROOT, AXEKIN_MAX_PAGES, AXEKIN_MAX_GAMES,
                            axekin_page, axekin_labels, axekin_label_version,
                            write_snapshot)


def collect_labels(source, resolve_files=False):
    first = axekin_page(source, 1)
    pages = int(first["meta"]["lastPage"])
    total = int(first["meta"]["total"])
    if not 1 <= pages <= AXEKIN_MAX_PAGES or not 1 <= total <= AXEKIN_MAX_GAMES:
        raise ValueError("Unsafe Axekin page/game count")
    games = list(first["data"])
    with ThreadPoolExecutor(max_workers=5) as pool:
        jobs = [pool.submit(axekin_page, source, page) for page in range(2, pages + 1)]
        for done, job in enumerate(as_completed(jobs), 2):
            data = job.result()["data"]
            if not data:
                raise ValueError("Empty Axekin page; snapshot left unchanged")
            games.extend(data)
            if done % 100 == 0:
                print(f"Axekin labels: {done}/{pages} pages", flush=True)
    if len(games) < total * 0.90:
        raise ValueError("Incomplete Axekin scan; snapshot left unchanged")
    # Keep network operations in the importer; emitted snapshots stay static.
    with ThreadPoolExecutor(max_workers=5) as pool:
        rows = []
        for done, labels in enumerate(pool.map(lambda game: axekin_labels(game, resolve_files), games), 1):
            rows.append(labels)
            if resolve_files and done % 100 == 0:
                print(f"Axekin file metadata: {done}/{len(games)} games", flush=True)
    return rows


def apply_labels(snapshot, labels):
    by_slug = {}
    for row in labels:
        slug = row.get("slug", "")
        if not re.fullmatch(r"[a-z0-9][a-z0-9-]*", slug):
            continue
        value = axekin_label_version(row)
        if slug in by_slug and by_slug[slug] != value:
            raise ValueError("Conflicting Axekin labels; snapshot left unchanged")
        by_slug[slug] = value
    source = next(source for source in snapshot["sources"] if source["id"] == "axekin")
    updates = []
    for entry in source["packages"]:
        url = urllib.parse.urlsplit(entry["sourceUrl"])
        if url.hostname not in {"axekin.com", "www.axekin.com"}:
            continue
        slug = url.path.removeprefix("/games/").rstrip("/")
        value = by_slug.get(slug)
        previous = entry.get("version", "")
        # A version-only refresh must preserve a verified format for that same
        # version, rather than erasing it when a file host is unavailable.
        if value and "/" not in value and "/" in previous:
            old_version, old_format = previous.split("/", 1)
            if value.strip() == old_version.strip():
                value += " / " + old_format.strip()
        if value and value != entry.get("version"):
            updates.append((entry, value))
    for entry, value in updates:
        entry["version"] = value
    source["labelCoverage"] = {
        "checkedAt": datetime.now(timezone.utc).isoformat(),
        "matchedEntries": sum(urllib.parse.urlsplit(e["sourceUrl"]).path.removeprefix("/games/").rstrip("/") in by_slug
                              for e in source["packages"]),
        "entriesWithLabels": sum(bool(e.get("version")) for e in source["packages"]),
    }
    return len(updates)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--search", help="Refresh matching games, e.g. NHL 26")
    parser.add_argument("--platform", help="Axekin platform slug, e.g. ps5")
    parser.add_argument("--file-formats", action="store_true",
                        help="Read public file-host HTML headings for explicit formats")
    args = parser.parse_args()
    source = next(s for s in json.loads((ROOT / "config/additional_sources.json").read_text(encoding="utf8"))
                  if s["id"] == "axekin")
    query = {key: value for key, value in {"search": args.search, "platform": args.platform}.items() if value}
    if query:
        source["index_url"] += "?" + urllib.parse.urlencode(query)
    path = ROOT / "assets/catalogs/axekin.json"
    snapshot = json.loads(path.read_text(encoding="utf8"))
    count = apply_labels(snapshot, collect_labels(source, args.file_formats))
    snapshot["sources"][0]["labelCoverage"]["scope"] = query or {"catalog": "all"}
    write_snapshot(path, snapshot)
    print(f"Axekin: updated {count} labels; identities, ordering and sizes preserved")


if __name__ == "__main__":
    main()
