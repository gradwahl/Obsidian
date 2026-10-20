"""Build complete, console-filtered metadata snapshots from public TSV indexes.

Never store PKG URLs, license/RAP data, hashes, or other binary-download data.
"""
import csv
import argparse
from collections import defaultdict
from datetime import datetime, timezone
import hashlib
import io
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from import_sources import fetch


def metadata_identity(source, system, item):
    identity = (item.get("Content ID") or item.get("Title ID", "").strip() + "|" +
                item.get("Region", "") + "|" + item.get("Name", "").strip())
    return "EXT-" + hashlib.sha256((source + "|" + system + "|" + identity).encode()).hexdigest()[:20]


def product_metadata(item):
    """Public identifiers only; never copy package URLs, RAPs or content hashes."""
    value = (item.get("Title ID") or "").strip().upper()
    if not re.fullmatch(r"[A-Z]{4}\d{5}", value):
        return {}
    content = item.get("Content ID") or ""
    embedded = re.match(r"[A-Z]{2}\d{4}-([A-Z]{4}\d{5})_", content)
    if embedded and embedded[1] != value:
        return {}
    region = (item.get("Region") or "").strip().upper()
    return {"productId": value, **({"productRegion": region} if re.fullmatch(r"[A-Z]{2,8}", region) else {})}


def restore_product_ids(source, document):
    """Enrich existing identities only; do not refresh titles or catalogue membership."""
    rows = {g["id"]: g for s in document["sources"] for g in s.get("packages", [])}
    candidates = defaultdict(set)
    changed = 0
    for system, url in source["feeds"]:
        if system not in {"sony-ps3", "sony-psp", "sony-ps1"}:
            continue
        reader = csv.DictReader(io.StringIO(fetch(url)), delimiter="\t")
        if not {"Name", "Title ID"}.issubset(reader.fieldnames or []):
            raise ValueError("TSV header changed")
        for item in reader:
            row = rows.get(metadata_identity(source["id"], system, item))
            metadata = product_metadata(item)
            if row and row["platform"] == system and metadata and row["title"] == (item.get("Name") or "").strip():
                candidates[row["id"]].add(tuple(sorted(metadata.items())))
    for identity, alternatives in candidates.items():
        if len(alternatives) == 1:
            metadata = dict(next(iter(alternatives)))
            row = rows[identity]
            if any(row.get(k) != v for k, v in metadata.items()):
                row.update(metadata)
                changed += 1
    return changed

def import_tsv_source(source):
    platform_map = {
        "sony-ps3": "PlayStation 3 (experimental)",
        "sony-psp": "PlayStation Portable (PSP)",
        "sony-ps1": "PlayStation 1",
    }
    library = (ROOT / "src/libraryData.ts").read_text(encoding="utf8")
    for system, label in platform_map.items():
        if f"name: '{label}', platform_id: '{system}'" not in library:
            raise ValueError(f"Unsupported Games/Roms console: {system}")
    rows = []
    coverage = {}
    seen = set()
    excluded = 0
    for system, url in source["feeds"]:
        content = fetch(url)
        reader = csv.DictReader(io.StringIO(content), delimiter="\t")
        if not {"Name", "Title ID"}.issubset(reader.fieldnames or []):
            raise ValueError("TSV header changed")
        read = kept = 0
        for item in reader:
            read += 1
            if system not in platform_map:
                excluded += 1
                continue
            title = (item.get("Name") or "").strip()
            title_id = (item.get("Title ID") or "").strip()
            if not title or not title_id:
                excluded += 1
                continue
            # Some catalogues have region variants with a common title ID.
            identity = (item.get("Content ID") or title_id + "|" + item.get("Region", "") + "|" + title)
            key = (system, identity)
            if key in seen:
                continue
            seen.add(key)
            created = (item.get("Last Modification Date") or "").strip()
            date = created.replace(" ", "T") + "Z" if re.fullmatch(
                r"\d{4}-\d\d-\d\d \d\d:\d\d:\d\d", created) else ""
            rows.append({
                "id": metadata_identity(source["id"], system, item),
                "title": title,
                "platform": system,
                **product_metadata(item),
                "format": "catalog",
                "sourceUrl": source["url"],
                **({"createdAt": date} if date else {}),
            })
            kept += 1
        coverage[system] = {"sourceRows": read, "indexed": kept}
    return {"schemaVersion": 1, "sources": [{
        "id": source["id"], "name": source["name"], "website": source["url"],
        "status": "ready",
        "coverage": {"feeds": coverage, "excludedRows": excluded, "indexedEntries": len(rows)},
        "packages": rows,
    }]}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--restore-product-ids", action="store_true",
                        help="Restore public product IDs to existing records without replacing the snapshots")
    args = parser.parse_args()
    sources = json.loads((ROOT / "config/additional_sources.json").read_text(encoding="utf8"))
    for source in sources:
        if source["id"] not in ("nopaystation", "psn-content"):
            continue
        print("Importing " + source["name"], flush=True)
        if args.restore_product_ids:
            from import_sources import write_snapshot
            target = ROOT / "assets/catalogs" / (source["id"] + ".json")
            document = json.loads(target.read_text(encoding="utf-8"))
            changed = restore_product_ids(source, document)
            write_snapshot(target, document)
            print(f"Restored product metadata to {changed} existing entries", flush=True)
            continue
        index = import_tsv_source(source)
        target = ROOT / "assets/catalogs" / (source["id"] + ".json")
        tmp = target.with_suffix(".json.tmp")
        tmp.write_text(json.dumps(index, ensure_ascii=False, separators=(",", ":"))+"\n", encoding="utf8")
        tmp.replace(target)
        coverage = index["sources"][0]["coverage"]
        print(f"{source['name']}: indexed {coverage['indexedEntries']}, "
              f"excluded {coverage['excludedRows']}; per feed {coverage['feeds']}", flush=True)

if __name__ == "__main__":
    main()
