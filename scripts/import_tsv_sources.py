"""Build complete, console-filtered metadata snapshots from public TSV indexes.

Never store PKG URLs, license/RAP data, hashes, or other binary-download data.
"""
import csv
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
                "id": "EXT-" + hashlib.sha256(
                    (source["id"] + "|" + system + "|" + identity).encode()).hexdigest()[:20],
                "title": title,
                "platform": system,
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
    sources = json.loads((ROOT / "config/additional_sources.json").read_text(encoding="utf8"))
    for source in sources:
        if source["id"] not in ("nopaystation", "psn-content"):
            continue
        print("Importing " + source["name"], flush=True)
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
