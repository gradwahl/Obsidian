"""Offline checks for curated public metadata importers and source registry."""
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("import_sources", ROOT / "scripts/import_sources.py")
importer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(importer)

REQUIRED_IDS = {
    "alvro", "superpsx", "pkgps4", "gfs", "downloadgamepsp", "axekin",
    "maelly-pooh", "vimm", "romheaven", "ziperto", "wowroms", "romstime",
    "retrogamechampion", "romshq", "cdromance"
}


class SourceImportTests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads((ROOT / "config/additional_sources.json").read_text(encoding="utf8"))
        self.snapshot = json.loads((ROOT / "assets/catalogs/additional.json").read_text(encoding="utf8"))

    def test_requested_sources_registered_once(self):
        ids = [source["id"] for source in self.config]
        self.assertTrue(REQUIRED_IDS <= set(ids))
        self.assertEqual(len(ids), len(set(ids)))
        self.assertEqual({source["id"] for source in self.config},
                         {source["id"] for source in self.snapshot["sources"]})
        for source in self.config:
            self.assertTrue(source["url"].startswith("https://"))
            self.assertIn(source["adapter"], ("archive", "gfs", "html", "tsv", "vimm"))

    def test_snapshot_contains_only_metadata(self):
        self.assertEqual(self.snapshot["schemaVersion"], 1)
        for source in self.snapshot["sources"]:
            self.assertIn(source["status"], {"ready", "partial", "empty", "error", "stale"})
            self.assertLessEqual(len(source["packages"]), 400)
            self.assertTrue(source["website"].startswith("https://"))
            for item in source["packages"]:
                self.assertEqual(item["format"], "catalog")
                self.assertTrue(item["sourceUrl"].startswith("https://"))
                self.assertNotIn("downloadUrl", item)
                self.assertNotIn("downloadLinks", item)
                self.assertNotIn("installPath", item)
                self.assertTrue(item["title"])

    def test_vimm_index_is_specific_to_game_pages(self):
        parser = importer.VaultLinks()
        parser.feed('<a href="/vault/27852">Ace Combat X</a>'
                    '<a href="/vault/PSP/A">Index</a>'
                    '<a href="https://other.example/vault/28766">Not Vimm</a>')
        self.assertEqual(parser.rows, [("Ace Combat X", "https://vimm.net/vault/27852")])

    def test_platform_classification(self):
        self.assertEqual(importer.platform("Mario Kart (Nintendo DS)"), "nintendo-ds")
        self.assertEqual(importer.platform("Super Mario 64 N64"), "nintendo-n64")
        self.assertEqual(importer.platform("Ratchet and Clank PSP"), "sony-psp")
        self.assertEqual(importer.platform("Unclassified game", "sony-ps4"), "sony-ps4")


if __name__ == "__main__":
    unittest.main()
