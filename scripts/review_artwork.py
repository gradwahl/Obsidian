"""Prepare local contact sheets for withheld artwork without approving matches.

Needs Pillow for PNG sheets. The HTML index includes every candidate and source
link; image URLs are downloaded only with --download. No browser is controlled.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
import html
import json
import os
from pathlib import Path
from urllib.parse import urlsplit, quote, unquote

from enrich_artwork import ROOT
from igdb_artwork import atomic_json
import download_artwork as downloader

OUTPUT = ROOT / ".cache/wolodrive-artwork/review"
HOSTS = {"thumbnails.libretro.com", "images.igdb.com", "images.launchbox-app.com",
         "art.gametdb.com", "www.axekin.com", "axekin.com"}


def local_image(url):
    return ROOT / "assets/covers" / downloader.cache_name(url)


def candidate_urls(entries):
    return sorted({row["url"] for entry in entries for row in entry["candidates"]
                   if urlsplit(row["url"]).scheme == "https" and urlsplit(row["url"]).hostname in HOSTS})


def caption_lines(text, font, width=330):
    """Wrap by rendered width, including full-width Japanese/Chinese glyphs."""
    lines, current = [], ""
    for word in text.split():
        joined = (current + " " + word).strip()
        if font.getlength(joined) <= width:
            current = joined
            continue
        if current:
            lines.append(current)
            current = ""
        for character in word:
            if current and font.getlength(current + character) > width:
                lines.append(current)
                current = ""
            current += character
    if current:
        lines.append(current)
    return lines


def render(entries, output, font_path=None):
    try:
        from PIL import Image, ImageDraw, ImageFont, ImageOps, PngImagePlugin
    except ImportError as error:
        raise RuntimeError("PNG contact sheets require Pillow: python -m pip install Pillow") from error
    # Some original scans contain large Adobe text metadata. Bound decoding;
    # do not alter the cached originals.
    PngImagePlugin.MAX_TEXT_CHUNK = 8 * 1024 * 1024
    fonts = ((str(font_path),) if font_path else ()) + (
             "C:/Windows/Fonts/YuGothM.ttc", "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
             "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
             "C:/Windows/Fonts/arial.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf",
             "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")
    if font_path and not Path(font_path).is_file():
        raise ValueError("Requested contact-sheet font does not exist")
    font = next((ImageFont.truetype(p, 15) for p in fonts if Path(p).is_file()), ImageFont.load_default())
    tiles = [(entry, row) for entry in entries for row in entry["candidates"]]
    pages, errors = [], []
    for start in range(0, len(tiles), 16):
        page = Image.new("RGB", (1400, 1800), "#1d2430")
        draw = ImageDraw.Draw(page)
        for position, (entry, row) in enumerate(tiles[start:start + 16]):
            x, y = (position % 4) * 350, (position // 4) * 450
            path = local_image(row["url"])
            try:
                with Image.open(path) as image:
                    image = ImageOps.contain(image.convert("RGB"), (330, 275))
                    page.paste(image, (x + (350 - image.width) // 2, y + 8))
            except (OSError, ValueError) as error:
                draw.text((x + 12, y + 115), "Image unavailable locally", fill="#f4af76", font=font)
                errors.append({"identity": entry["identity"], "url": row["url"], "error": str(error)})
            labels = [entry["title"], entry["platform"] + " / " + entry["source"],
                      row["provider"], entry["identity"].split("|", 1)[1],
                      unquote(row["url"].rsplit("/", 1)[-1])]
            lines = [line for label in labels for line in caption_lines(label, font)]
            for i, line in enumerate(lines[:9]):
                draw.text((x + 10, y + 290 + i * 17), line, fill="white", font=font)
        name = f"sheet-{start // 16 + 1:03d}.jpg"
        page.save(output / name, quality=88)
        pages.append(name)
    return pages, errors


def write_index(entries, output, pages, unavailable_urls=()):
    parts = ['<!doctype html><html lang="en"><meta charset="utf-8"><title>Withheld artwork review</title>',
             '<style>body{background:#17202c;color:#eee;font:16px system-ui;margin:30px}a{color:#9dcaff}'
             'article{padding:20px;border-top:1px solid #687888}section{display:flex;flex-wrap:wrap;gap:18px}'
             'figure{width:220px;margin:8px}img{max-width:220px;height:250px;object-fit:contain}'
             'figcaption{overflow-wrap:anywhere}</style>',
             f'<h1>Withheld artwork: {len(entries)} entries</h1><p>Nothing on this page is approved. '
             'Compare title, console, edition and region before recording a decision. '
             'Images are local cached files; missing candidates remain listed.</p>',
             '<h2>Contact sheets</h2><p>' + ' '.join(f'<a href="{name}">{i + 1}</a>' for i, name in enumerate(pages)) + '</p>']
    for entry in entries:
        parts.append('<article><h2>' + html.escape(entry["title"]) + '</h2><p>' +
                     html.escape(entry["platform"] + ' / ' + entry["identity"] + ' / ' + entry["reason"]) + '</p>')
        if entry.get("sourceUrl", "").startswith("https://"):
            parts.append('<p><a href="' + html.escape(entry["sourceUrl"], quote=True) + '">Original source page</a></p>')
        parts.append('<section>')
        for row in entry["candidates"]:
            path = local_image(row["url"])
            label = html.escape(row["provider"] + ' — ' + row["url"])
            image = ('<img loading="lazy" src="' + quote(Path(os.path.relpath(path, output)).as_posix(), safe="/.") + '" alt="">') if path.is_file() and row["url"] not in unavailable_urls else '<p>Image unavailable locally</p>'
            parts.append('<figure>' + image + '<figcaption>' + label + '</figcaption></figure>')
        parts.append('</section></article>')
    parts.append('</html>')
    (output / "index.html").write_text("\n".join(parts), encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--download", action="store_true", help="Fetch missing candidate images without approving them")
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--font", type=Path, help="Optional Unicode TrueType/OpenType font for source-title labels")
    args = parser.parse_args(argv)
    entries = json.loads((ROOT / "assets/artwork/review-queue.json").read_text(encoding="utf-8"))["entries"]
    args.output.mkdir(parents=True, exist_ok=True)
    results = {}
    if args.download:
        urls = candidate_urls(entries)
        with ThreadPoolExecutor(max_workers=3) as pool:
            jobs = {pool.submit(downloader.download, url, ROOT / "assets/covers"): url for url in urls}
            for future in as_completed(jobs):
                results[jobs[future]] = future.result()
                if len(results) % 100 == 0:
                    print(f"Review images checked {len(results)}/{len(urls)}", flush=True)
        atomic_json(args.output / "downloads.json", {"results": results})
        print("Candidate image results:", dict(Counter(r["status"] for r in results.values())), flush=True)
    # Enrichment may finish while the independent candidate downloads run.
    entries = json.loads((ROOT / "assets/artwork/review-queue.json").read_text(encoding="utf-8"))["entries"]
    pages, errors = render(entries, args.output, args.font)
    write_index(entries, args.output, pages, {row["url"] for row in errors})
    atomic_json(args.output / "render-report.json", {"entries": len(entries), "sheets": len(pages), "unavailable": errors})
    print(f"Review prepared: {len(entries)} entries, {len(pages)} sheets, {len(errors)} unavailable candidate tiles", flush=True)
    print(args.output / "index.html")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
