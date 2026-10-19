"""Download catalog card artwork into Wolodrive's existing URL-keyed disk cache."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from http.client import HTTPException
import json
from pathlib import Path
import sys
import urllib.parse
import urllib.request

DEFAULT_CATALOG = "https://raw.githubusercontent.com/pegasus-ps5/pegasus-dl/refs/heads/main/catalogs/default-sources.json"
DEFAULT_OUTPUT = Path(__file__).resolve().parents[1] / "assets" / "covers"
IMAGE_LIMIT = 16 * 1024 * 1024
CATALOG_LIMIT = 32 * 1024 * 1024
CARD_FIELDS = ("boxArtUrl", "boxartUrl", "boxCoverUrl", "posterUrl", "coverUrl",
               "iconUrl", "image", "cover", "icon", "landscapeUrl", "cardUrl", "bannerUrl")


def cache_name(url):
    """Match artwork_hash in src/artwork.c (FNV-1a over UTF-8 bytes)."""
    value = 14695981039346656037
    for byte in url.encode("utf-8"):
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return f"{value:016x}.art"


def fetch(url, limit):
    if urllib.parse.urlsplit(url).scheme not in ("http", "https"):
        raise ValueError("Only HTTP(S) URLs are supported")
    request_url = urllib.parse.quote(url, safe=":/?#[]@!$&'()*+,;=%")
    request = urllib.request.Request(request_url, headers={"User-Agent": "Wolodrive-Artwork/1.0"})
    with urllib.request.urlopen(request, timeout=12) as response:
        if urllib.parse.urlsplit(response.url).scheme not in ("http", "https"):
            raise ValueError("Unsupported redirect scheme")
        data = response.read(limit + 1)
    if len(data) > limit:
        raise ValueError(f"Response exceeds {limit} bytes")
    return data


def image_type(data):
    # Reject error pages before writing the shared cache. SDL verifies full decoding on use.
    if len(data) >= 24 and data.startswith(b"\x89PNG\r\n\x1a\n"):
        return "png"
    if len(data) >= 4 and data.startswith(b"\xff\xd8\xff") and data.endswith(b"\xff\xd9"):
        return "jpg"
    if len(data) >= 16 and data.startswith(b"RIFF") and data[8:12] == b"WEBP":
        return "webp"
    raise ValueError("Response is not a supported PNG, JPEG or WebP image")


def collect(catalogs):
    pending = list(catalogs)
    visited = set()
    images = {}
    errors = []
    while pending:
        url = pending.pop(0)
        if url in visited:
            continue
        if len(visited) >= 64:
            errors.append("Stopped at the 64-catalog limit")
            break
        visited.add(url)
        print(f"Reading catalog: {url}", flush=True)
        try:
            document = json.loads(fetch(url, CATALOG_LIMIT))
        except (OSError, ValueError, HTTPException) as error:
            errors.append(f"{url}: {error}")
            continue

        def walk(value, source):
            if isinstance(value, list):
                for item in value:
                    walk(item, source)
            elif isinstance(value, dict):
                containers = [value[k] for k in ("packages", "games", "sources", "catalogs") if k in value]
                if containers:
                    for container in containers:
                        walk(container, value.get("name", source))
                    return
                title = value.get("title") or value.get("name")
                identity = next((value[k] for k in ("titleId", "title_id", "id", "code") if value.get(k)), "")
                urls = {value[k] for k in CARD_FIELDS if isinstance(value.get(k), str)
                        and urllib.parse.urlsplit(value[k]).scheme in ("http", "https")}
                # Follow source references, never game package download links.
                child = value.get("catalogUrl") or value.get("sourceUrl") or value.get("url", "")
                if isinstance(child, str) and child.startswith(("http://", "https://")) and (
                        ".json" in child.lower() or (not identity and not urls)):
                    pending.append(child)
                if title and urls:
                    for image_url in urls:
                        images.setdefault(image_url, []).append({"title": title, "title_id": identity, "source": source})

        walk(document, url)
    return images, errors


def download(url, output):
    destination = output / cache_name(url)
    try:
        if destination.is_file():
            kind = image_type(destination.read_bytes())
            return {"file": destination.name, "type": kind, "status": "cached"}
    except (OSError, ValueError):
        pass
    temporary = destination.with_suffix(".bulk.tmp")
    for attempt in range(2):
        try:
            data = fetch(url, IMAGE_LIMIT)
            kind = image_type(data)
            temporary.write_bytes(data)
            temporary.replace(destination)
            return {"file": destination.name, "type": kind, "status": "downloaded"}
        except (OSError, ValueError, HTTPException) as error:
            if attempt:
                return {"file": destination.name, "status": "failed", "error": str(error)}
        finally:
            temporary.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", action="append", help="Catalog/source-list URL (repeatable)")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="Cache folder (default: repository assets/covers)")
    parser.add_argument("--workers", type=int, choices=range(1, 9), default=4)
    args = parser.parse_args(argv)
    args.output.mkdir(parents=True, exist_ok=True)
    images, errors = collect(args.catalog or [DEFAULT_CATALOG])
    print(f"Found {len(images)} unique artwork URLs. Destination: {args.output.resolve()}", flush=True)
    results = {}
    counts = {"cached": 0, "downloaded": 0, "failed": 0}
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        jobs = {pool.submit(download, url, args.output): url for url in images}
        for job in as_completed(jobs):
            url = jobs[job]
            result = job.result()
            results[url] = {**result, "games": images[url]}
            counts[result["status"]] += 1
            completed = sum(counts.values())
            if completed <= 5 or completed % 25 == 0 or completed == len(images):
                print(f"{completed}/{len(images)}: {counts}", flush=True)
    manifest = args.output / "artwork-index.json"
    temporary = manifest.with_suffix(".json.tmp")
    temporary.write_text(json.dumps({"catalog_errors": errors, "artwork": results}, ensure_ascii=False, indent=2), encoding="utf-8")
    temporary.replace(manifest)
    for error in errors:
        print(f"Catalog failed: {error}", file=sys.stderr)
    print(f"Finished: {counts}. Index: {manifest.resolve()}", flush=True)
    return 1 if errors or counts["failed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
