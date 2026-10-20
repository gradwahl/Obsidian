"""Validate generated metadata indexes without fetching game files.

This test checks source coverage, document integrity, and the distinction
between public game-page links and unsupported direct package downloads.
"""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / "assets/catalogs"
FULL = (
    "axekin", "superpsx", "downloadgamepsp", "ziperto", "nopaystation",
    "psn-content", "alvro", "maelly-pooh", "romstime",
)


class CatalogueIndexes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sources = {x["id"]: x for x in json.loads(
            (ROOT / "config/additional_sources.json").read_text("utf8"))}
        cls.additional = json.loads((FOLDER / "additional.json").read_text("utf8"))
        cls.bounded = {x["id"]: x for x in cls.additional["sources"]}

    def test_source_registry_is_complete(self):
        self.assertEqual(set(self.sources), set(self.bounded))
        self.assertEqual(len(self.sources), 17)

    def test_full_snapshots_have_real_entries(self):
        minimum = {"axekin": 18000, "ziperto": 15000, "romstime": 20000,
                   "nopaystation": 11000, "psn-content": 5000,
                   "superpsx": 3500, "downloadgamepsp": 1800,
                   "alvro": 60, "maelly-pooh": 120}
        for source_id in FULL:
            with self.subTest(source=source_id):
                data = json.loads((FOLDER / (source_id + ".json")).read_text("utf8"))
                self.assertEqual(data["schemaVersion"], 1)
                self.assertEqual(len(data["sources"]), 1)
                source = data["sources"][0]
                self.assertEqual(source["id"], source_id)
                self.assertEqual(source["status"], "ready")
                self.assertGreaterEqual(len(source["packages"]), minimum[source_id])

    def test_metadata_only_and_public_page_links(self):
        for path in FOLDER.glob("*.json"):
            with self.subTest(file=path.name):
                data = json.loads(path.read_text("utf8"))
                for src in data["sources"]:
                    for item in src["packages"]:
                        self.assertEqual(item.get("format"), "catalog")
                        self.assertTrue(item.get("title"))
                        self.assertTrue(item.get("id"))
                        self.assertNotIn("downloadUrl", item)
                        self.assertNotIn("installPath", item)
                        if item.get("sourceUrl"):
                            self.assertTrue(item["sourceUrl"].startswith("https://"))

    def test_native_source_index_has_bounded_real_records(self):
        header = (ROOT / "src/native_source_index.h").read_text("utf8")
        rows = len(re.findall(r"^ \{", header, re.M))
        self.assertGreater(rows, 1300)
        self.assertLess(rows, 3000)
        self.assertNotIn("download_url", header)
        self.assertIn("NATIVE_SOURCE_CATALOGUE_COUNT", header)


if __name__ == "__main__":
    unittest.main()
