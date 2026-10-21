"""Offline tests for path-only cover matching across imported consoles."""
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from generate_source_cover_matches import build, CONSOLES  # noqa: E402


class SourceCoverMatches(unittest.TestCase):
    def test_same_platform_unique_title(self):
        paths = [
            "assets/covers/Sony/PlayStation 5/Example Game.art",
            "assets/covers/Sony/PlayStation 4/Example Game.art",
        ]
        rows = [
            {"title": "Example Game", "platform": "sony-ps5"},
            {"title": "Example Game", "platform": "sony-ps4"},
        ]
        matches, excluded = build(paths, rows)
        self.assertEqual(len(matches), 2)
        self.assertIn("PlayStation%205", matches["sony-ps5|Example Game"])
        self.assertIn("PlayStation%204", matches["sony-ps4|Example Game"])
        self.assertEqual(excluded["ambiguous"], 0)

    def test_ambiguous_editions_skipped(self):
        paths = [
            "assets/covers/Nintendo/Nintendo Switch/Test [1111111111111111].art",
            "assets/covers/Nintendo/Nintendo Switch/Test [2222222222222222].art",
        ]
        matches, excluded = build(paths, [{"platform": "nintendo-switch", "title": "Test"}])
        self.assertFalse(matches)
        self.assertEqual(excluded["ambiguous"], 1)

    def test_plain_image_preferred_to_edition_suffix(self):
        paths = [
            "assets/covers/Sony/PlayStation Portable/Test.art",
            "assets/covers/Sony/PlayStation Portable/Test [1111111111111111].art",
        ]
        matches, _ = build(paths, [{"platform": "sony-psp", "title": "Test"}])
        self.assertTrue(matches["sony-psp|Test"].endswith("/Test.art"))

    def test_generated_output_paths_only(self):
        data = json.loads((ROOT / "src/archiveSourceCovers.json").read_text("utf8"))
        self.assertGreaterEqual(len(data), 48000)
        self.assertLess((ROOT / "src/archiveSourceCovers.json").stat().st_size, 8 * 1024 * 1024)
        self.assertGreaterEqual(len(CONSOLES), 25)
        for filename in data.values():
            self.assertTrue(filename.startswith("assets/covers/"))
            self.assertTrue(filename.endswith(".art"))
            self.assertNotIn("..", filename.split("/"))
            self.assertNotIn("://", filename)

    def test_native_seed_embeds_public_cover_url_not_images(self):
        data = (ROOT / "src/native_source_index.h").read_text("utf8")
        self.assertIn("raw.githubusercontent.com/gradwahl/Obsidian-archive", data)
        self.assertIn("const char *id, *title, *source", data)
        self.assertNotIn("Authorization:", data)
        self.assertNotIn("github_pat_", data)


if __name__ == "__main__":
    unittest.main()
