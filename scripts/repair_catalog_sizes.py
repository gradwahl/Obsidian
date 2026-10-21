#!/usr/bin/env python3
"""Repair offline seed sizes from explicit source metadata, never estimates."""
import argparse
import json
import math
import re
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "src/catalogData.json"
SIZE_LINE = re.compile(r"^\s*Size\s*:\s*(\d+(?:[.,]\d+)?)\s*(B|KB|MB|GB|TB|KiB|MiB|GiB|TiB)\s*$", re.I | re.M)

def description_size_gb(description):
    matches = SIZE_LINE.findall(description or "")
    if len(matches) != 1:
        return 0.0
    amount, unit = matches[0]
    amount = float(amount.replace(",", "."))
    unit = unit.upper()
    powers = {"B": 1, "KB": 1e3, "KIB": 1024, "MB": 1e6, "GB": 1e9, "TB": 1e12, "MIB": 1024**2, "GIB": 1024**3, "TIB": 1024**4}
    value = amount * powers[unit] / 1e9
    return value if math.isfinite(value) and value > 0 else 0.0

def corrected_size(item):
    if item.get("source_name") == "Pippo .exFAT":
        return description_size_gb(item.get("description"))
    if item.get("source_name") == "Pegasus-DL / Obsidian":
        # Original demonstration records have no package-size source evidence.
        return 0.0
    value = item.get("size_gb", 0)
    return value if isinstance(value, (int, float)) and math.isfinite(value) and value > 0 else 0.0

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    rows = json.loads(CATALOG.read_text(encoding="utf-8"))
    changed = 0
    for item in rows:
        size = corrected_size(item)
        if item.get("size_gb") != size:
            changed += 1
            item["size_gb"] = size
    if args.check and changed:
        raise SystemExit(f"{changed} incorrect catalogue sizes; run python scripts/repair_catalog_sizes.py")
    if not args.check and changed:
        CATALOG.write_text(json.dumps(rows, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Catalog sizes: {len(rows)} audited, {changed} corrected, "
          f"{sum(item['size_gb'] > 0 for item in rows)} source-reported, "
          f"{sum(item['size_gb'] == 0 for item in rows)} unknown")

if __name__ == "__main__":
    main()
