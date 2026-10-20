"""Apply exact reviewed title corrections and remove a confirmed navigation row.

Only local metadata is edited. IDs, game URLs and existing artwork are retained.
Suspicious duplicate titles without authoritative identities are left for review.
"""
from import_sources import ROOT, correct_source_title, write_snapshot
import json


def clean_snapshot(document):
    corrected = removed = 0
    for source in document["sources"]:
        kept = []
        for row in source.get("packages", []):
            if (source["id"] == "romshq" and row["title"] == "Download Now" and
                    row.get("sourceUrl") == "https://romshq.com/game/grand-theft-auto-v-nsp"):
                removed += 1
                continue
            title = correct_source_title(source["id"], row["platform"], row.get("sourceUrl", ""), row["title"])
            if title != row["title"]:
                row["title"] = title
                corrected += 1
            kept.append(row)
        source["packages"] = kept
        if source.get("coverage"):
            for field in ("indexed", "indexedEntries"):
                if field in source["coverage"]:
                    source["coverage"][field] = len(kept)
    return corrected, removed


def main():
    for path in sorted((ROOT / "assets/catalogs").glob("*.json")):
        document = json.loads(path.read_text(encoding="utf-8"))
        corrected, removed = clean_snapshot(document)
        if corrected or removed:
            if path.name == "additional.json":
                temporary = path.with_suffix(".json.tmp")
                temporary.write_text(json.dumps(document, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
                temporary.replace(path)
            else:
                write_snapshot(path, document)
            print(f"{path.name}: corrected {corrected}, removed navigation {removed}")


if __name__ == "__main__":
    main()
