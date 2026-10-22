"""AlvRo public Archive.org metadata labels; offline, no external requests."""
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

import import_sources
import import_archive_sources


class AlvroSourceTests(unittest.TestCase):
    def setUp(self):
        self.source = next(source for source in json.loads(
            (ROOT / "config/additional_sources.json").read_text(encoding="utf8")
        ) if source["id"] == "alvro")
        self.files = [
            {"name": "Example Game (Japan).7z", "size": "12345"},
            {"name": "Another Game (Japan) (v2.00).7z", "size": "98765"},
            {"name": "Private Game.7z", "private": True},
            {"name": "cover.jpg"},
        ]

    def fake_fetch(self, url):
        if "advancedsearch.php?" in url:
            raise AssertionError("AlvRo must use all explicit collection IDs, not creator search")
        identity = url.rsplit("/", 1)[-1]
        if identity not in self.source["archive_identifiers"]:
            raise AssertionError("Unexpected URL: " + url)
        files = self.files if identity == "ps2japanredumpmissing" else [
            {"name": "Restricted Game.7z", "private": "true"},
        ]
        return json.dumps({"files": files, "metadata": {
            "creator": "AlvRo", "title": "Redump - Sony Playstation 2 - Japan",
        }})

    def test_explicit_file_extensions_and_versions_only(self):
        read = import_sources.archive_alvro_display_version
        self.assertEqual(read("Example Game.7z"), " / 7Z")
        self.assertEqual(read("Another Game (v2.00).7z"), "v2.00 / 7Z")
        self.assertEqual(read("Example Game.pkg"), " / PKG")
        self.assertEqual(read("Example Game.zip"), " / ZIP")
        self.assertEqual(read("Example Game"), "")
        self.assertEqual(read("Archive.org PS2.unknown"), "")

    def test_full_and_bounded_imports_agree_and_skip_private_files(self):
        with patch.object(import_archive_sources, "fetch", side_effect=self.fake_fetch):
            full = import_archive_sources.index_source(self.source, workers=1)["sources"][0]
        with patch.object(import_sources, "fetch", side_effect=self.fake_fetch):
            bounded, truncated = import_sources.archive(self.source, 50)
        self.assertFalse(truncated)
        self.assertEqual(len(full["packages"]), len(bounded))
        self.assertEqual(full["coverage"]["gameFiles"], 2)
        self.assertEqual(full["coverage"]["archiveItems"], 8)
        self.assertEqual(full["coverage"]["restrictedFiles"], 8)
        self.assertEqual(len(full["coverage"]["collections"]), 8)
        self.assertEqual(
            [row["url"] for row in full["coverage"]["collections"]],
            ["https://archive.org/download/" + item for item in self.source["archive_identifiers"]])
        expected = {"Example Game (Japan)": " / 7Z",
                    "Another Game (Japan) (v2.00)": "v2.00 / 7Z"}
        self.assertEqual({row["title"]: row["version"] for row in full["packages"]}, expected)
        self.assertEqual({row["title"]: row["version"] for row in bounded}, expected)
        for row in full["packages"] + bounded:
            self.assertEqual(row["platform"], "sony-ps2")
            self.assertEqual(row["sourceUrl"], "https://archive.org/details/ps2japanredumpmissing")
            self.assertNotIn("downloadUrl", row)
            self.assertNotIn("downloadLinks", row)
            self.assertNotIn("private", row["title"].lower())

    def test_collection_identifiers_are_curated_and_validated(self):
        identifiers = import_sources.archive_collection_ids(self.source)
        self.assertEqual(len(identifiers), 8)
        self.assertEqual(identifiers[0], "httpsarchive.orgdetailsps2japanredump4")
        self.assertEqual(identifiers[-1], "ps2usaredump1")
        with self.assertRaises(ValueError):
            import_sources.archive_collection_ids({
                "archive_identifiers": ["ps2japanredump1", "ps2japanredump1"]})
        with self.assertRaises(ValueError):
            import_sources.archive_collection_ids({
                "archive_identifiers": ["https://archive.org/download/ps2japanredump1"]})
        with self.assertRaises(ValueError):
            import_sources.archive_collection_ids({"archive_identifiers": []})

    def test_committed_alvro_snapshots_keep_same_ids_and_formats(self):
        full = json.loads((ROOT / "assets/catalogs/alvro.json").read_text(encoding="utf8"))["sources"][0]
        bounded = next(source for source in json.loads(
            (ROOT / "assets/catalogs/additional.json").read_text(encoding="utf8")
        )["sources"] if source["id"] == "alvro")
        key = lambda row: (row["title"], row["platform"], row["sourceUrl"])
        full_rows = {key(row): row for row in full["packages"]}
        bounded_rows = {key(row): row for row in bounded["packages"]}
        self.assertEqual(set(full_rows), set(bounded_rows))
        self.assertEqual(len(full_rows), 63)
        self.assertEqual(full["coverage"]["indexedEntries"], 63)
        self.assertEqual(full["coverage"]["archiveItems"], 8)
        self.assertEqual(full["coverage"]["restrictedFiles"], 3997)
        self.assertEqual(len(full["coverage"]["collections"]), 8)
        self.assertEqual(sum(row["publicGameFiles"] for row in full["coverage"]["collections"]), 63)
        self.assertEqual(sum(row["restrictedGameFiles"] for row in full["coverage"]["collections"]), 3997)
        for key_, row in full_rows.items():
            self.assertEqual(row["version"], bounded_rows[key_]["version"])
            self.assertTrue(row["version"].endswith(" / 7Z"))
        revision = next(row for row in full_rows.values() if "(v2.00)" in row["title"])
        self.assertEqual(revision["version"], "v2.00 / 7Z")


if __name__ == "__main__":
    unittest.main()
