#!/usr/bin/env python3
"""Compile a bounded, verified metadata-only catalogue seed for native SDL2/PS5.

Full web source files may contain tens of thousands of records. The native
runtime has a fixed MAX_CATALOG_ITEMS array and MAX_ITEMS_PER_ROW; it must not
silently overflow those. The native seed provides an explicitly bounded
selection per source and console until a paged native index is implemented.
"""
import json
import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOGS = ROOT / "assets/catalogs"
OUTPUT = ROOT / "src/native_source_index.h"
NATIVE_AxEKIN_LIMIT = 512


def c_string(value, length=511):
    data = str(value or "").encode("utf8")[:length]
    data = data.decode("utf8", errors="ignore")
    # C compilers accept literal UTF-8 and JSON-compatible string escapes.
    return json.dumps(data, ensure_ascii=False)


def main():
    main_index = json.loads((CATALOGS / "additional.json").read_text("utf8"))
    sources = [dict(src) for src in main_index["sources"]]
    axekin = CATALOGS / "axekin.json"
    if axekin.is_file():
        full = json.loads(axekin.read_text("utf8"))["sources"][0]
        # Preserve diverse consoles: first 64 supported titles per platform.
        per_platform = defaultdict(int)
        sample = []
        for item in full["packages"]:
            plat = item.get("platform", "")
            if per_platform[plat] >= 64 or len(sample) >= NATIVE_AxEKIN_LIMIT:
                continue
            sample.append(item)
            per_platform[plat] += 1
        for src in sources:
            if src["id"] == "axekin":
                src["packages"] = sample
                src["status"] = "partial"
                break
    archive_file = ROOT / "src/archiveSourceCovers.json"
    archive_map = json.loads(archive_file.read_text("utf8")) if archive_file.exists() else {}
    archive_origin = "https://raw.githubusercontent.com/PolDrive/PolDrive-archive/main/"
    rows = []
    for source in sources:
        if source["id"] == "retrogamechampion":
            # A system collection is not a game-level catalogue record.
            continue
        for item in source.get("packages", []):
            if not item.get("id") or not item.get("title"): continue
            date = (item.get("createdAt") or "")[:10]
            if not re.fullmatch(r"\d{4}-\d\d-\d\d", date): date = ""
            source_url = item.get("sourceUrl", "")
            if not str(source_url).startswith("https://"): source_url = source["website"]
            # Separate public page information from any package download URL.
            desc = "Catalog metadata only. Source page: " + str(source_url)
            path = archive_map.get(str(item.get("platform", "")) + "|" + str(item["title"]))
            boxart = archive_origin + path if path and len((archive_origin + path).encode("utf8")) < 512 else ""
            rows.append((
                item["id"],item["title"],source["name"],item.get("platform",""),
                item.get("version",""),item.get("coverUrl",""),boxart,
                desc, date, item.get("sizeBytes",0),
                item.get("category","game")!="dlc"
            ))
    from math import isfinite
    header = [
        "/* Generated source-page metadata; no game package URLs. */",
        "#ifndef POLDRIVE_NATIVE_SOURCE_INDEX_H",
        "#define POLDRIVE_NATIVE_SOURCE_INDEX_H",
        "typedef struct {",
        " const char *id, *title, *source, *platform, *version, *cover, *boxart, *description, *date;",
        " float size_gb; unsigned char is_game;",
        "} NativeSourceSeed;",
        "static const NativeSourceSeed NATIVE_SOURCE_CATALOGUE[] = {",
    ]
    for id_, title, source, plat, ver, cover, boxart, desc, date, size, is_game in rows:
        try:size_gb=float(size)/1073741824
        except (ValueError,TypeError):size_gb=0
        if not isfinite(size_gb) or size_gb<0:size_gb=0
        fields = [c_string(v,limit) for v,limit in zip(
            [id_,title,source,plat,ver,cover,boxart,desc,date],[31,127,63,47,31,511,511,500,23])]
        header.append(" {" + ",".join(fields)+f",{size_gb:.4f}f,{int(is_game)}"+"},")
    header.extend([
        "};",
        "#define NATIVE_SOURCE_CATALOGUE_COUNT (sizeof(NATIVE_SOURCE_CATALOGUE) / sizeof(NATIVE_SOURCE_CATALOGUE[0]))",
        "#endif",""
    ])
    OUTPUT.write_text("\n".join(header),encoding="utf8",newline="\n")
    print("Native source seed:",len(rows),"metadata entries across",len(sources),
          "source adapters; full indexes remain in the web-only chunks")


if __name__ == "__main__":main()
