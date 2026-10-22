"""Offline tests for Archive.org maelly_pooh public/private catalogue metadata."""
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import import_archive_sources


class MaellyArchiveAccessTests(unittest.TestCase):
    def setUp(self):
        self.source = next(x for x in json.loads(
            (ROOT / "config/additional_sources.json").read_text(encoding="utf8"))
            if x["id"] == "maelly-pooh")

    def fake_fetch(self, url):
        if url.endswith("/metadata/ps4-fpkg-collection-english-c"):
            return json.dumps({"metadata": {"uploader": "example@example.invalid"}, "files": [
                {"name": "Public Game (US).pkg", "size": "321"}
            ]})
        if "advancedsearch.php" in url:
            return json.dumps({"response": {
                "numFound": 2,
                "docs": [
                    {"identifier": "ps4-fpkg-collection-english-a", "title": "PS4 FPKG Collection [English] [A]"},
                    {"identifier": "ps4-fpkg-collection-english-c", "title": "PS4 FPKG Collection [English] [C]"},
                ]}})
        if url.endswith("/metadata/ps4-fpkg-collection-english-a"):
            return json.dumps({"metadata": {"uploader": "example@example.invalid"}, "files": [
                {"name": "Private Game (US).pkg", "private": "true", "size": "123"},
                {"name": "Not a game.txt", "private": "true"},
            ]})
        if url.endswith("/metadata/ps4-fpkg-collection-english-c"):
            return json.dumps({"metadata": {"uploader": "example@example.invalid"}, "files": [
                {"name": "Public Game (US).pkg", "size": "321"}
            ]})
        raise AssertionError("Unexpected network request: " + url)

    def test_restricted_listing_preserves_file_name_without_download_url(self):
        with patch.object(import_archive_sources, "fetch", side_effect=self.fake_fetch):
            source = import_archive_sources.index_source(self.source, workers=1)["sources"][0]
        rows = {entry["title"]: entry for entry in source["packages"]}
        self.assertEqual(len(rows), 2)
        self.assertEqual(source["coverage"]["archiveItems"], 2)
        self.assertEqual(source["coverage"]["gameFiles"], 1)
        self.assertEqual(source["coverage"]["restrictedFiles"], 1)
        self.assertEqual(source["coverage"]["restrictedEntries"], 1)
        self.assertEqual(rows["Private Game (US)"]["archiveAccess"], "sign-in-required")
        self.assertEqual(rows["Private Game (US)"]["archiveFileName"], "Private Game (US).pkg")
        self.assertEqual(rows["Public Game (US)"]["archiveAccess"], "public")
        for row in rows.values():
            self.assertEqual(row["format"], "catalog")
            self.assertEqual(row["platform"], "sony-ps4")
            self.assertNotIn("downloadUrl", row)
            self.assertNotIn("downloadLinks", row)
            self.assertNotIn("download_url", row)
            self.assertTrue(row["sourceUrl"].startswith("https://archive.org/details/"))

    def test_saved_snapshot_is_complete_but_not_claimed_available(self):
        source = json.loads((ROOT / "assets/catalogs/maelly-pooh.json").read_text(encoding="utf8"))["sources"][0]
        coverage = source["coverage"]
        self.assertEqual(coverage["archiveItems"], 30)
        self.assertEqual(coverage["gameFiles"], 131)
        self.assertEqual(coverage["restrictedFiles"], 770)
        self.assertEqual(coverage["restrictedEntries"], 770)
        self.assertEqual(coverage["publicIndexedEntries"], 131)
        self.assertEqual(coverage["indexedEntries"], len(source["packages"]))
        self.assertEqual(len(source["packages"]), 901)
        self.assertEqual(len({row["id"] for row in source["packages"]}), 901)
        restricted = [x for x in source["packages"] if x["archiveAccess"] == "sign-in-required"]
        self.assertEqual(len(restricted), 770)
        self.assertEqual(sum(x["archiveAccess"] == "public" for x in source["packages"]), 131)
        self.assertTrue(all(r["archiveFileName"].lower().endswith(".pkg") for r in restricted))
        self.assertTrue(all("downloadUrl" not in r for r in source["packages"]))


if __name__ == "__main__":
    unittest.main()
