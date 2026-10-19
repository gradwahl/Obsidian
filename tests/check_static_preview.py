"""Check the committed preview through a plain Python HTTP server, without Node."""

import functools
import http.server
import pathlib
import re
import subprocess
import threading
import urllib.request

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
            # Verify every full source index is delivered as a separate JS chunk.
            full_sources = (
                "axekin", "superpsx", "downloadgamepsp", "ziperto",
                "nopaystation", "psn-content", "alvro", "maelly-pooh", "romstime"
            )
            assert not (PREVIEW / "assets/catalogs").exists(), "No duplicate raw catalog JSON in preview"
            for source in full_sources:
                chunks = list((PREVIEW / "assets").glob(source + "-*.js"))
                assert len(chunks) == 1, f"Missing/duplicated full index: {source}"
                links.extend("/assets/" + chunk.name for chunk in chunks)
            artwork_chunks = list((PREVIEW / "assets").glob("cover-matches-*.js"))
            assert len(artwork_chunks) == 1, "Artwork override manifest must be lazy-loaded"
            assert not (PREVIEW / "assets/artwork/cover-matches.json").exists()
            assert not (PREVIEW / "assets/artwork/cover-matches.tsv").exists()
            links.extend("/assets/" + chunk.name for chunk in artwork_chunks)
            for name in sorted(set(links + ["/" + path for path in artwork])):
                with urllib.request.urlopen(base + name) as response:
                    assert response.read() == (PREVIEW / name.lstrip("/")).read_bytes(), name
                    if name.endswith(".js"):
                        assert response.headers.get_content_type() in (
                            "text/javascript", "application/javascript"
                        )
                    if name.endswith(".css"):
                        assert response.headers.get_content_type() == "text/css"
        finally:
            server.shutdown()
            thread.join()
    print(f"PASS: static HTML, JS, CSS and {len(artwork)} artwork files served without Node")


if __name__ == "__main__":
    main()
