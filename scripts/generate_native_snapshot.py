#!/usr/bin/env python3
"""Generate an offline C catalogue/source snapshot from the React preview data.

Both Linux and PS5 use this compiled snapshot, so no web server or dynamic
catalogue file is required at runtime. No URLs are fetched by this generator.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "src" / "catalogData.json"
SOURCES = ROOT / "config" / "additional_sources.json"
OUTPUT = ROOT / "src" / "native_catalog_snapshot.h"


def quote(value):
    # JSON and C both support these core string escapes. UTF-8 is kept literal.
    return json.dumps(str(value or ""), ensure_ascii=False).replace("\u2028", "\\u2028").replace("\u2029", "\\u2029")


def byte_trim(value, maximum):
    value = str(value or "").replace("\x00", "")
    return value.encode("utf-8")[:maximum].decode("utf-8", "ignore")


def format_index(value):
    return {"exfat": 0, "ffpkg": 1, "pkg": 2, "catalog": 3}[value]


def main():
    items = json.loads(CATALOG.read_text(encoding="utf-8"))
    sources = json.loads(SOURCES.read_text(encoding="utf-8"))
    names = sorted({entry["name"] for entry in sources} | {item["source_name"] for item in items}, key=str.casefold)
    if len(names) > 40:
        raise ValueError("Native source registry exceeds catalog_source_names[40].")

    lines = [
        "/* Generated from src/catalogData.json and config/additional_sources.json.",
        "   Regenerate with: python3 scripts/generate_native_snapshot.py",
        "   Do not edit this header by hand. No network calls or download links. */",
        "#ifndef WOLODRIVE_NATIVE_CATALOG_SNAPSHOT_H",
        "#define WOLODRIVE_NATIVE_CATALOG_SNAPSHOT_H",
        "#include <stdint.h>",
        "typedef struct {",
        " const char *id, *title, *version, *source, *description, *cover, *path;",
        " uint64_t added; float size_gb;",
        " unsigned char format, is_game, is_dlc, r, g, b;",
        "} NativeCatalogSeed;",
        "static const char *const NATIVE_SOURCE_NAMES[] = {",
    ]
    lines.extend(" " + quote(n) + "," for n in names)
    lines.extend(["};", "static const NativeCatalogSeed NATIVE_CATALOG_SEEDS[] = {"])

    for item in items:
        fields = [
            byte_trim(item.get("id_code"), 31),
            byte_trim(item.get("title"), 127),
            byte_trim(item.get("version"), 31),
            byte_trim(item.get("source_name"), 63),
            byte_trim(item.get("description"), 4095),
            byte_trim(item.get("cover_url"), 511),
            byte_trim(item.get("install_path"), 255),
        ]
        assert fields[0] and fields[1] and fields[3]
        added = int(item.get("catalog_added_at") or 0)
        size = float(item.get("size_gb") or 0)
        if not (0 <= added <= 99999999999999 and 0 <= size <= 100000):
            raise ValueError("Out-of-range catalogue metadata")
        scalars = [
            str(added) + "ULL", f"{size:.9f}f",
            str(format_index(item.get("format"))),
            str(int(bool(item.get("is_game")))),
            str(int(bool(item.get("is_dlc")))),
            *[str(max(0, min(255, int(item.get(k) or 0)))) for k in ("cover_r", "cover_g", "cover_b")],
        ]
        lines.append(" {" + ", ".join([*(quote(x) for x in fields), *scalars]) + "},")
    lines.extend([
        "};",
        f"#define NATIVE_SOURCE_COUNT {len(names)}",
        f"#define NATIVE_CATALOG_SEED_COUNT {len(items)}",
        "#endif",
        "",
    ])
    output = "\n".join(lines)
    if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != output:
        OUTPUT.write_text(output, encoding="utf-8", newline="\n")
    print(f"Native snapshot: {len(items)} catalogue rows, {len(names)} source names")


if __name__ == "__main__":
    main()
