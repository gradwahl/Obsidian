"""Offline IGDB matcher, identity, API and confidentiality tests; no credentials used."""
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import igdb_artwork as igdb
import enrich_artwork as artwork


class IGDBTests(unittest.TestCase):
    def test_platform_and_title_matching_strict(self):
        game = {"id": 101, "name": "Example Game", "platforms": [48, 49],
                "cover": {"image_id": "co12345"}}
        result = igdb.select_match("sony-ps4", "Example Game", [game], 48)
        self.assertEqual(result["status"], "matched")
        self.assertEqual(result["provider"], "igdb-exact-platform-title")
        self.assertEqual(result["url"], "https://images.igdb.com/igdb/image/upload/t_cover_big/co12345.jpg")
        self.assertEqual(igdb.select_match("sony-ps3", "Example Game", [game], 9)["status"], "not-found")
        self.assertEqual(igdb.select_match("sony-ps4", "Example Game: Remastered", [game], 48)["status"],
                         "not-found")
        self.assertEqual(igdb.select_match("sony-ps4", "Example Game", [game], None)["status"],
                         "skipped-platform")
        self.assertEqual(igdb.select_match("sony-ps4", "Example Game",
                         [{**game, "cover": {"image_id": "invalid/../../../file"}}], 48)["status"], "not-found")

    def test_ambiguity_and_region_variants(self):
        a = {"id": 1, "name": "Example (USA)", "platforms": [48],
             "cover": {"image_id": "co10000"}}
        b = {"id": 2, "name": "Example (Europe)", "platforms": [48],
             "cover": {"image_id": "co20000"}}
        self.assertEqual(igdb.select_match("sony-ps4", "Example", [a, b], 48)["status"], "review")
        self.assertEqual(igdb.select_match("sony-ps4", "Example", [a, dict(b, cover=a["cover"])], 48)
                         ["status"], "matched")

    def test_dedup_and_covered_entries_skipped(self):
        records = [
            ("Axekin", {"id": "1", "title": "Same Game (USA)", "platform": "sony-ps4"}),
            ("Ziperto", {"id": "2", "title": "Same Game", "platform": "sony-ps4"}),
            ("SuperPSX", {"id": "3", "title": "Same Game", "platform": "sony-ps5"}),
            ("GFS", {"id": "4", "title": "Has Cover", "platform": "sony-ps5",
                     "coverUrl": "https://example.test/box.jpg"}),
            ("Other", {"id": "5", "title": "Already Matched", "platform": "sony-ps4"}),
        ]
        targets = igdb.eligible_titles(records, {"Other|5": {"url": "https://example.test/box.jpg"}})
        self.assertEqual(len(targets), 2)
        self.assertIn("sony-ps4|samegame", targets)
        self.assertIn("sony-ps5|samegame", targets)

    def test_client_does_not_leak_credentials_into_url_and_checks_platforms(self):
        seen = []
        def open_fake(req, timeout=None):
            seen.append(req)
            class Response:
                def __enter__(self): return self
                def __exit__(self, *args): pass
                def read(self, *_args):
                    if "oauth2/token" in req.full_url:
                        return b'{"access_token":"fake-temporary-token"}'
                    if "platforms" in req.full_url:
                        return b'[{"id":48,"name":"PlayStation 4"},{"id":49,"name":"PlayStation 5"}]'
                    return b'[{"id":123,"name":"Example","platforms":[48],"cover":{"image_id":"co12345"}}]'
            return Response()
        class CounterClock:
            now = 0.0
            def __call__(self): return self.now
            def sleep(self, seconds): self.now += seconds
        clk = CounterClock()
        client = igdb.IGDBClient("public-test-id", "never-log-this-value", opener=open_fake,
                                 sleep=clk.sleep, clock=clk)
        self.assertEqual(client.platforms()["sony-ps4"], 48)
        self.assertEqual(client.platforms()["sony-ps5"], 49)
        self.assertEqual(client.search("Example", 48)[0]["name"], "Example")
        self.assertTrue(all("never-log-this-value" not in req.full_url for req in seen))
        self.assertEqual(seen[0].full_url, "https://id.twitch.tv/oauth2/token")
        self.assertIn(b"never-log-this-value", seen[0].data)
        self.assertGreaterEqual(clk.now, .35)

    def test_cache_resume_and_export(self):
        cache = {"schemaVersion": 1, "results": {}}
        class Fake:
            def search(self, title, platform_id):
                if title == "Matched": return [{"id": 10, "name": "Matched", "platforms": [48],
                                                "cover": {"image_id": "coabcde"}}]
                return []
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "cache.json"
            out = Path(folder) / "artwork.json"
            names = {"sony-ps4|matched": ("sony-ps4", "Matched"),
                     "sony-ps4|missing": ("sony-ps4", "Missing")}
            with patch.object(igdb, "CACHE", path):
                self.assertEqual(igdb.scan(Fake(), names, cache, 1, {"sony-ps4": 48}), 1)
                self.assertEqual(igdb.scan(Fake(), names, cache, 2, {"sony-ps4": 48}), 1)
                self.assertEqual(igdb.scan(Fake(), names, cache, 2, {"sony-ps4": 48}), 0)
            self.assertEqual(igdb.export_cache(cache, set(names), out), 1)
            self.assertEqual(json.loads(out.read_text())["matches"]["sony-ps4|matched"]["confidence"], 95)
            self.assertNotIn("sony-ps4|missing", json.loads(out.read_text())["matches"])

    def test_igdb_overrides_only_last_resort_and_reported(self):
        records = [
            ("Other", {"id": "1", "platform": "sony-ps4", "title": "Example"}),
            ("Direct", {"id": "2", "platform": "sony-ps4", "title": "Has Cover",
                        "coverUrl": "https://existing.test/cover.jpg"}),
        ]
        lookup = {"sony-ps4|example": {
            "url": "https://images.igdb.com/igdb/image/upload/t_cover_big/coabcde.jpg",
            "provider": "igdb-exact-platform-title", "confidence": 95}}
        matched, report = artwork.match_records(records, {}, lookup)
        self.assertEqual(matched["Other|1"]["provider"], "igdb-exact-platform-title")
        self.assertEqual(report["providers"]["igdb-exact-platform-title"], 1)
        self.assertNotIn("Direct|2", matched)
        bad = {"sony-ps4|example": {**lookup["sony-ps4|example"], "url": "https://evil.test/im.jpg"}}
        matched, report = artwork.match_records(records, {}, bad)
        self.assertNotIn("Other|1", matched)


if __name__ == "__main__":
    unittest.main()
