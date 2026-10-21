"""Check the committed preview through a plain Python HTTP server, without Node."""

import functools
import http.server
import hashlib
import json
import pathlib
import re
import subprocess
import threading
import urllib.request
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parents[1]
PREVIEW = ROOT / "static-preview"


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *_args):
        pass


def main():
    artwork = subprocess.check_output(
        ["git", "ls-files", "-z", "--", "assets"], cwd=ROOT
    ).decode().strip("\0").split("\0")
    artwork = [name for name in artwork if not name.replace("\\", "/").startswith(("assets/catalogs/", "assets/artwork/"))]
    for name in artwork:
        assert (PREVIEW / name).read_bytes() == (ROOT / name).read_bytes(), name
    assert not list(PREVIEW.rglob("*.tsx")), "Preview must contain compiled code"

    handler = functools.partial(QuietHandler, directory=str(PREVIEW))
    with http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f"http://127.0.0.1:{server.server_port}"
        try:
            with urllib.request.urlopen(base + "/") as response:
                html = response.read().decode()
                assert response.headers.get_content_type() == "text/html"
            assert '/src/' not in html and '.tsx' not in html
            links = re.findall(r'(?:src|href)="(/[^\"]+)"', html)
            assert any(link.endswith(".js") for link in links)
            assert any(link.endswith(".css") for link in links)
            # Catalogs now ship as bounded JSON pages, fetched at the URL root.
            # Check the real deployment contract rather than removed JS chunks.
            full_sources = (
                "axekin", "superpsx", "downloadgamepsp", "ziperto",
                "nopaystation", "psn-content", "alvro", "maelly-pooh", "romstime"
            )
            assert not (PREVIEW / "assets/catalogs").exists(), "No duplicate raw catalog JSON in preview"
            with urllib.request.urlopen(base + "/poldrive-pages/manifest.json") as response:
                manifest_bytes = response.read()
                assert response.headers.get_content_type() == "application/json"
            generated = ROOT / "public/poldrive-pages"
            assert manifest_bytes == (generated / "manifest.json").read_bytes()
            manifest = json.loads(manifest_bytes)
            assert manifest["schemaVersion"] == 2
            source_ids = [source["id"] for source in manifest["sources"]]
            assert len(source_ids) == len(set(source_ids)), "Duplicate source metadata"
            assert set(full_sources).issubset(source_ids), "Missing full source indexes"
            links.append("/poldrive-pages/manifest.json")
            page_names = set()
            records = 0
            covered = 0
            for source in manifest["sources"]:
                assert len(source["indexes"]) == len(source["pages"])
                for index_name in source["indexes"]:
                    assert re.fullmatch(r"[a-z0-9-]+-[a-f0-9]{12}-index-\d{3}-[a-f0-9]{10}\.json", index_name), index_name
                    assert index_name not in page_names
                    page_names.add(index_name)
                    data = (PREVIEW / "poldrive-pages" / index_name).read_bytes()
                    assert data == (generated / index_name).read_bytes()
                    assert hashlib.sha256(data).hexdigest()[:10] == index_name[-15:-5]
                    assert len(data) < 400000
                    index_rows = json.loads(data)
                    assert 0 < len(index_rows) <= 384
                    links.append("/poldrive-pages/" + index_name)
                for name in source["pages"]:
                    assert re.fullmatch(r"[a-z0-9-]+-[a-f0-9]{12}-\d{3}-[a-f0-9]{10}\.json", name), name
                    assert name not in page_names, f"Duplicate catalog page: {name}"
                    page_names.add(name)
                    data = (PREVIEW / "poldrive-pages" / name).read_bytes()
                    assert data == (generated / name).read_bytes(), name
                    assert hashlib.sha256(data).hexdigest()[:10] == name[-15:-5], name
                    assert len(data) < 400000, f"Oversized catalog page: {name}"
                    entries = json.loads(data)
                    assert isinstance(entries, list) and 0 < len(entries) <= 384, name
                    index = json.loads((PREVIEW / "poldrive-pages" / source["indexes"][source["pages"].index(name)]).read_bytes())
                    assert [row[:2] for row in index] == [[entry["id"], entry["title"]] for entry in entries]
                    records += len(entries)
                    covered += sum(bool(entry.get("archiveCoverPath")) for entry in entries)
                    links.append("/poldrive-pages/" + name)
            assert records >= 80000, "Missing published catalog records"
            assert covered >= 40000, "Missing bundled artwork mappings"
            assert {p.name for p in (PREVIEW / "poldrive-pages").glob("*.json")} == page_names | {"manifest.json"}
            assert not (PREVIEW / "assets/artwork/cover-matches.json").exists()
            assert not (PREVIEW / "assets/artwork/cover-matches.tsv").exists()
            for name in sorted(set(links + ["/" + path for path in artwork])):
                try:
                    response = urllib.request.urlopen(base + urllib.parse.quote(name, safe="/%"))
                except urllib.error.HTTPError as error:
                    raise AssertionError(f"Missing static resource {name}: {error}") from error
                with response:
                    assert response.read() == (PREVIEW / name.lstrip("/")).read_bytes(), name
                    if name.endswith(".js"):
                        assert response.headers.get_content_type() in (
                            "text/javascript", "application/javascript"
                        )
                    if name.endswith(".css"):
                        assert response.headers.get_content_type() == "text/css"
                    if name.endswith(".json"):
                        assert response.headers.get_content_type() == "application/json"
        finally:
            server.shutdown()
            thread.join()
    print(f"PASS: static HTML/JS/CSS, {records} catalog records in {len(page_names)} JSON pages, "
          f"and {len(artwork)} artwork files served without Node")


if __name__ == "__main__":
    main()
