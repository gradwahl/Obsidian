"""Offline DLPSGame artwork identity, privacy and merge-boundary tests."""
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import dlpsgame_artwork as dlps
import enrich_artwork as art

URL = "https://dlpsgame.com/wp-content/uploads/2024/08/example.jpg"
POST = {"postId": 1, "title": "Example PS3", "platform": "sony-ps3",
        "sourceUrl": "https://dlpsgame.com/example/", "url": URL}


class DLPSTests(unittest.TestCase):
    def test_exact_console_editions_and_duplicate_posts(self):
        rows = [("S", {"id": str(i), "title": title, "platform": platform}) for i, (title, platform) in enumerate([
            ("Example", "sony-ps3"), ("Example", "sony-ps4"), ("Example Demo", "sony-ps3"),
            ("Example Deluxe Edition", "sony-ps3"), ("Example Mod", "sony-ps3")])]
        matches, counts = dlps.candidates(rows, {}, [POST])
        self.assertEqual(set(matches), {"S|0"})
        self.assertEqual(counts["not-found"], 3)
        self.assertFalse(dlps.candidates(rows, {"S|0": {}}, [POST])[0])
        self.assertFalse(dlps.candidates(rows, {}, [POST, {**POST, "postId": 2, "url": ""}])[0])
        self.assertEqual(dlps.post_title_key("Example\u200b PSN"), dlps.post_title_key("Example"))
        self.assertNotEqual(dlps.post_title_key("Example HD PSN"), dlps.post_title_key("Example"))
        self.assertNotEqual(dlps.post_title_key("Example Demo PSN"), dlps.post_title_key("Example"))
        self.assertTrue(dlps.safe_post("https://dlpsgame.com/example%e2%80%8b-psn/"))
        self.assertFalse(dlps.safe_post("https://dlpsgame.com/example%2fother/"))

    def test_only_allowlisted_image_links_and_sanitized_display_metadata(self):
        parser = dlps.Cover()
        parser.feed('<img src="https://ads.test/banner.jpg"><img src="http://dlpsgame.com/wp-content/uploads/a.jpg">'
                    '<a href="https://files.test/game.pkg">Download</a><img src="https://dlpsgame.com/wp-content/uploads/b.jpg">')
        self.assertEqual(parser.url, "https://dlpsgame.com/wp-content/uploads/a.jpg")
        for bad in ("https://evil.test/a.jpg", "https://dlpsgame.com/file.pkg", "https://dlpsgame.com:bad/wp-content/uploads/a.jpg", "http://1.bp.blogspot.com/a.jpg"):
            self.assertFalse(dlps.safe_image(bad))
        row = {"id": 1, "link": POST["sourceUrl"], "title": {"rendered": "Example &amp; More"},
               "categories": [64], "content": {"rendered": '<img src="' + URL + '"><a href="https://files.test/game.pkg">SECRET</a>'}}
        class Response(io.BytesIO):
            headers = {"X-WP-Total": "1", "X-WP-TotalPages": "1"}
        with patch.object(dlps, "urlopen", return_value=Response(json.dumps([row]).encode())):
            posts, total, pages, received = dlps.page(1)
        self.assertEqual((total, pages, received), (1, 1, 1))
        self.assertEqual(posts[0]["title"], "Example & More")
        self.assertEqual(set(posts[0]), {"postId", "title", "platform", "sourceUrl", "url"})
        self.assertNotIn("SECRET", json.dumps(posts))
        self.assertNotIn("game.pkg", json.dumps(posts))

    def test_incomplete_refresh_does_not_replace_cached_index(self):
        with tempfile.TemporaryDirectory() as folder:
            cache = Path(folder) / "cache.json"
            cache.write_text('previous cache', encoding="utf-8")
            with patch.object(dlps, "CACHE", cache), patch.object(dlps, "page", side_effect=[([POST], 2, 2, 1), ([], 2, 2, 0)]):
                with self.assertRaises(ValueError):
                    dlps.build_index(refresh=True)
            self.assertEqual(cache.read_text(), "previous cache")

    def test_merge_preserves_approved_and_review_and_rejects_other_consoles(self):
        candidate = {"url": URL, "sourceUrl": POST["sourceUrl"], "provider": "dlpsgame-exact-ps3-title", "confidence": 95}
        row = {"id": "1", "title": "Example", "platform": "sony-ps3"}
        self.assertTrue(dlps.trusted_match(candidate))
        with patch.object(dlps, "EXCLUSIONS", {(POST["sourceUrl"], URL)}):
            self.assertFalse(dlps.trusted_match(candidate))
            self.assertFalse(dlps.candidates([("S", row)], {}, [POST])[0])
        self.assertEqual(art.match_records([("S", row)], {}, dlpsgame_matches={"S|1": candidate})[0]["S|1"]["url"], URL)
        self.assertFalse(art.match_records([("S", {**row, "platform": "sony-ps4"})], {}, dlpsgame_matches={"S|1": candidate})[0])
        self.assertFalse(art.match_records([("S", {**row, "coverUrl": "https://source.test/a.jpg"})], {}, dlpsgame_matches={"S|1": candidate})[0])
        index = {"sony-ps3": {"strict": {"example": {"https://a.test/a.png", "https://b.test/b.png"}}, "relaxed": {}}}
        matches, report = art.match_records([("S", row)], index, dlpsgame_matches={"S|1": candidate})
        self.assertFalse(matches)
        self.assertEqual(report["status"], {"review": 1})


if __name__ == "__main__":
    unittest.main()
