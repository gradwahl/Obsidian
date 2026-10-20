"""Offline coverage and privacy assertions for full public metadata snapshots."""
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
FILES = {
    "axekin": 18000, "superpsx": 3500, "downloadgamepsp": 1800,
    "ziperto": 16000, "nopaystation": 11000, "psn-content": 5300,
    "romstime": 23000, "alvro": 50, "maelly-pooh": 100,
}

class FullSourceCoverageTests(unittest.TestCase):
    def test_full_snapshot_coverage_and_platforms(self):
        platforms = {row["platform"] for row in json.loads(
            (ROOT / "config/axekin_platforms.json").read_text(encoding="utf8")).values()}
        for name, minimum in FILES.items():
            with self.subTest(source=name):
                path = ROOT / "assets/catalogs" / (name + ".json")
                doc = json.loads(path.read_text(encoding="utf8"))
                self.assertEqual(doc["schemaVersion"], 1)
                self.assertEqual(len(doc["sources"]), 1)
                source = doc["sources"][0]
                self.assertEqual(source["id"], name)
                self.assertEqual(source["status"], "ready")
                coverage = source["coverage"]
                entries = source["packages"]
                self.assertGreaterEqual(len(entries), minimum)
                self.assertEqual(coverage["indexedEntries"], len(entries))
                self.assertEqual(len({r["id"] for r in entries}), len(entries))
                for entry in entries:
                    self.assertIn(entry["platform"], platforms)
                    self.assertEqual(entry["format"], "catalog")
                    self.assertTrue(entry["title"])
                    self.assertTrue(entry["sourceUrl"].startswith("https://"))
                    self.assertFalse(any(
                        key in entry for key in (
                            "downloadLinks", "downloadUrl", "pkgUrl", "rap",
                            "licenseKey", "installPath", "binaryUrl"
                        )))
        self.assertEqual(len(json.loads((ROOT / "assets/catalogs/romstime.json").read_text())["sources"][0]["packages"]), 24097)

    def test_verified_small_sources_not_truncated(self):
        data = json.loads((ROOT / "assets/catalogs/additional.json").read_text(encoding="utf8"))
        sources = {item["id"]: item for item in data["sources"]}
        for name, minimum in [("gfs", 600), ("romheaven", 140)]:
            with self.subTest(name=name):
                source = sources[name]
                self.assertEqual(source["status"], "ready")
                self.assertIn("coverage", source)
                self.assertGreaterEqual(len(source["packages"]), minimum)
                self.assertEqual(source["coverage"]["indexed"], len(source["packages"]))

if __name__ == "__main__":
    unittest.main()
