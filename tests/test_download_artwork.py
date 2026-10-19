"""Bulk downloader checks against a local server, on native Windows and Linux."""
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import base64
import json
from pathlib import Path
import sys
import tempfile
import threading
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import download_artwork as artwork

PNG = base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+a5WQAAAAASUVORK5CYII=")


class DownloadArtworkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.requests = Counter()

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_GET(self):
                cls.requests[self.path] += 1
                root = f"http://127.0.0.1:{self.server.server_port}"
                if self.path == "/sources.json":
                    data = {"sources": [{"name": "Fixture", "url": root + "/catalog.json"}]}
                elif self.path == "/catalog.json":
                    data = {"name": "Fixture", "games": [
                        {"titleId": "PPSA12345", "title": "中文 Game", "boxArtUrl": root + "/box",
                         "posterUrl": root + "/cover", "downloadLinks": [{"url": root + "/never-download-package"}]},
                        {"titleId": "PPSA54321", "title": "Variant", "iconUrl": root + "/cover"},
                        {"catalogUrl": root + "/sources.json"}]}
                elif self.path in ("/box", "/cover"):
                    data = PNG
                else:
                    data = b"<html>Not an image</html>"
                self.send_response(200)
                self.end_headers()
                self.wfile.write(data if isinstance(data, bytes) else json.dumps(data).encode())

        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.root = f"http://127.0.0.1:{cls.server.server_port}"

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.thread.join()
        cls.server.server_close()

    def test_hash_and_file_types(self):
        self.assertEqual(artwork.cache_name("hello"), "a430d84680aabd0b.art")
        self.assertEqual(artwork.image_type(PNG), "png")
        with self.assertRaises(ValueError):
            artwork.image_type(b"<html>Error</html>")

    def test_url_spaces_and_protocol_errors(self):
        from http.client import InvalidURL
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(artwork, "fetch", side_effect=InvalidURL("bad URL")):
                self.assertEqual(artwork.download(self.root + "/bad", Path(directory))["status"], "failed")
        # Encoding happens only for the request; the cache hash retains the catalog URL.
        self.assertEqual(artwork.fetch(self.root + "/cover image", 1000), b"<html>Not an image</html>")

    def test_bulk_download_resume_and_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "artwork folder"
            argv = ["--catalog", self.root + "/sources.json", "--output", str(output)]
            self.assertEqual(artwork.main(argv), 0)
            manifest = json.loads((output / "artwork-index.json").read_text(encoding="utf-8"))
            self.assertEqual(len(manifest["artwork"]), 2)
            self.assertEqual(len(manifest["artwork"][self.root + "/cover"]["games"]), 2)
            self.assertEqual(len(list(output.glob("*.art"))), 2)
            before = self.requests["/box"] + self.requests["/cover"]
            self.assertEqual(artwork.main(argv), 0)
            self.assertEqual(before, self.requests["/box"] + self.requests["/cover"])
            self.assertEqual(self.requests["/never-download-package"], 0)
            # Invalid existing cache files are replaced on the next run.
            (output / artwork.cache_name(self.root + "/box")).write_bytes(b"broken")
            self.assertEqual(artwork.main(argv), 0)
            self.assertEqual((output / artwork.cache_name(self.root + "/box")).read_bytes(), PNG)
            self.assertFalse(list(output.glob("*.tmp")))

    def test_failed_image_preserves_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            url = self.root + "/bad"
            self.assertEqual(artwork.download(url, output)["status"], "failed")
            self.assertFalse((output / artwork.cache_name(url)).exists())
            self.assertFalse(list(output.glob("*.tmp")))


if __name__ == "__main__":
    unittest.main()
