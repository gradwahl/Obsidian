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
