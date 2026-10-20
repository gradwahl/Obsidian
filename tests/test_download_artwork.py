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
                elif self.path == "/removed-sources.json":
                    data = {"sources": [
                        {"id": "evox-coreos", "url": root + "/broken-id.json"},
                        {"name": "evoX-CoreOS", "url": root + "/broken-name.json"},
                        {"url": root + "/evoX-CoreOS/catalog.json"},
                        *[{"id": name, "url": root + "/" + name + ".json"}
                          for name in ("dlps", "pippo", "pfs", "zer0")]]}
                elif self.path in ("/dlps.json", "/pippo.json", "/pfs.json", "/zer0.json"):
                    data = {"games": []}
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

    def test_removed_source_is_never_requested(self):
        self.requests.clear()
        _images, errors = artwork.collect([self.root + "/removed-sources.json"])
        self.assertEqual(errors, [])
        for name in ("dlps", "pippo", "pfs", "zer0"):
            self.assertEqual(self.requests["/" + name + ".json"], 1)
        for blocked in ("/broken-id.json", "/broken-name.json", "/evoX-CoreOS/catalog.json"):
            self.assertEqual(self.requests[blocked], 0)

    def test_local_collection_includes_primary_fallback_and_native_covers(self):
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "assets/artwork").mkdir(parents=True)
            (root / "src").mkdir()
            (root / "assets/artwork/cover-matches.json").write_text(json.dumps({
                "schemaVersion": 1, "matches": {"Source|1": {
                    "url": self.root + "/box", "fallback": {"url": self.root + "/cover"}}}}))
            (root / "src/catalogData.json").write_text(json.dumps([
                {"title": "Native", "id_code": "NATIVE", "cover_url": self.root + "/cover"},
                {"title": "Local", "cover_url": "/assets/local.png"}]))
            (root / "assets/artwork/igdb-covers.json").write_text(json.dumps({
                "schemaVersion": 1, "matches": {"sony-ps1|approved": {"url": self.root + "/approved"}}}))
            records = [("Source", {"id": "1", "title": "Example", "coverUrl": self.root + "/box"})]
            with patch("enrich_artwork.load_records", return_value=records):
                images, errors = artwork.collect_local(root)
            self.assertEqual(errors, [])
            self.assertEqual(set(images), {self.root + "/box", self.root + "/cover", self.root + "/approved"})
            self.assertEqual(len(images[self.root + "/box"]), 1)
            self.assertEqual(len(images[self.root + "/cover"]), 2)

    def test_known_challenge_route_preserves_cached_images_without_downloading_html(self):
        from unittest.mock import patch
        url = "https://www.axekin.com/uploads/games/example_cover.webp"
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder)
            with patch.object(artwork, "fetch") as fetch:
                self.assertEqual(artwork.download(url, output)["status"], "failed")
                fetch.assert_not_called()
            (output / artwork.cache_name(url)).write_bytes(PNG)
            self.assertEqual(artwork.download(url, output)["status"], "cached")

    def test_hash_and_file_types(self):
        self.assertEqual(artwork.cache_name("hello"), "a430d84680aabd0b.art")
        self.assertEqual(artwork.image_type(PNG), "png")
        self.assertEqual(artwork.image_type(b"\0\0\0\x1cftypavif\0\0\0\0avifmif1miaf"), "avif")
        with self.assertRaises(ValueError):
            artwork.image_type(b"\0\0\0\x1cftypmp42\0\0\0\0isommp42")
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
            progress = [json.loads(line) for line in (output / "artwork-progress.jsonl").read_text(
                encoding="utf-8").splitlines()]
            self.assertEqual({row["url"] for row in progress}, set(manifest["artwork"]))
            self.assertTrue(all(row["status"] == "downloaded" for row in progress))
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
