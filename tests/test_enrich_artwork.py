"""Offline confidence, cache, filtering and output tests for artwork matching."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import enrich_artwork as art


class ArtworkTests(unittest.TestCase):
    def test_title_key_retains_editions(self):
        self.assertEqual(art.normalize("Mario Kart DS (USA)"), art.normalize("Mario Kart DS"))
        self.assertNotEqual(art.normalize("Mario Kart DS"), art.normalize("Mario Kart 7"))
        self.assertNotEqual(art.normalize("Zelda: Ocarina of Time"), art.normalize("Zelda: Majora's Mask"))
        self.assertEqual(art.normalize("Pokémon (Europe)"), art.normalize("Pokemon"))
        self.assertFalse(art.safe_url("http://example.org/image.png"))
        self.assertFalse(art.safe_url("javascript:alert(1)"))

    def test_console_match_and_ambiguity(self):
        title = "Example Game"
        other = "Different Edition"
        logo = "https://images.example/game.webp"
        usa = "https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/Example%20Game%20(USA).png"
        europe = "https://thumbnails.libretro.com/Sony%20-%20PlayStation/Named_Boxarts/Example%20Game%20(Europe).png"
        records = [
            ("Source A", {"id": "1", "platform": "sony-ps1", "title": title, "coverUrl": logo}),
            ("Source B", {"id": "2", "platform": "sony-ps1", "title": title}),
            ("Source C", {"id": "3", "platform": "sony-ps2", "title": title}),
            ("Source D", {"id": "4", "platform": "sony-ps2", "title": other}),
        ]
        index = {
            "sony-ps2": {
                "strict": {art.normalize(title, tags=False): {usa}},
                "relaxed": {art.normalize(title): {usa, europe},
                            art.normalize(other): {usa, europe}},
            }
        }
        matches, report = art.match_records(records, index)
        self.assertEqual(matches["Source B|2"]["url"], logo)
        self.assertEqual(matches["Source C|3"]["url"], usa)
        self.assertNotIn("Source D|4", matches)
        self.assertEqual(report["status"]["review"], 1)
        self.assertEqual(report["status"]["direct"], 1)
        self.assertEqual(report["status"]["matched"], 2)

    def test_directory_parsing_and_offline_index(self):
        listing = ('<a href="Pokemon%20Red%20(USA).png">Red</a>'
                   '<a href="Pokemon%20Red%20(Japan).png">JP</a>'
                   '<a href="../">Up</a>'
                   '<a href="hidden.jpg">JPEG</a>')
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            (folder / "nintendo-gb.html").write_text(listing, encoding="utf-8")
            result = art.libretro_index("nintendo-gb", offline=True, cache=folder)
            self.assertEqual(result["count"], 2)
            self.assertEqual(len(result["relaxed"][art.normalize("Pokemon Red")]), 2)
            self.assertEqual(len(result["strict"][art.normalize("Pokemon Red (USA)", tags=False)]), 1)
            self.assertIsNone(art.libretro_index("sony-ps2", offline=True, cache=folder))

    def test_manifest_is_only_missing_covers_and_native_subset(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            catalogs = root / "assets/catalogs"
            catalogs.mkdir(parents=True)
            docs = root / "docs"
            docs.mkdir(parents=True)
            main = {"schemaVersion": 1, "sources": [
                {"name": "Axekin", "packages": [
                    {"id": "ABCD", "title": "Example Game", "platform": "sony-ps1"}]}
            ]}
            (catalogs / "additional.json").write_text(json.dumps(main), encoding="utf-8")
            data = {"schemaVersion": 1, "sources": [{"name": "Axekin", "packages": [
                {"id": "ABCD", "title": "Example Game", "platform": "sony-ps1"}]}]}
            (catalogs / "axekin.json").write_text(json.dumps(data), encoding="utf-8")
            matches = {"Axekin|ABCD": {
                "url": "https://example.net/front.png", "provider": "libretro-exact", "confidence": 95
            }, "Other|567": {
                "url": "https://example.net/other.png", "provider": "libretro-exact", "confidence": 95
            }}
            report = {"total": 2, "status": {"matched": 2}, "providers": {"libretro-exact": 2},
                      "sources": {"Axekin": {"matched": 1}}, "platforms": {"sony-ps1": {"matched": 1}},
                      "reviewSamples": []}
            art.write_outputs(matches, report, root)
            lines = (root / "assets/artwork/cover-matches.tsv").read_text(encoding="utf-8").splitlines()
            self.assertEqual(lines, ["Axekin\tABCD\thttps://example.net/front.png"])
            self.assertEqual(len(json.loads((root / "assets/artwork/cover-matches.json").read_text())["matches"]), 2)

    def test_generated_coverage_consistent(self):
        root = Path(__file__).resolve().parents[1]
        doc = json.loads((root / "assets/artwork/cover-matches.json").read_text(encoding="utf-8"))
        self.assertEqual(doc["schemaVersion"], 1)
        self.assertGreater(len(doc["matches"]), 10000)
        self.assertTrue(all(value["url"].startswith("https://") and value["confidence"] >= 85
                            for value in doc["matches"].values()))
        self.assertGreater((root / "assets/artwork/cover-matches.tsv").stat().st_size, 0)


if __name__ == "__main__":
    unittest.main()
