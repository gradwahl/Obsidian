"""Offline tests for the archive's filename-only artwork matcher."""
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "archive_matcher", ROOT / "scripts/generate_archive_artwork.py"
)
archive = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(archive)


class ArchiveArtworkTests(unittest.TestCase):
    def test_unique_exact_console_match(self):
        tree = [
            "assets/covers/Sony/PlayStation 5/Marvels Spider Man 2.art",
            "assets/covers/Sony/PlayStation 4/Marvels Spider Man 2.art",
        ]
        catalog = [{"id_code": "PPSA08338", "title": "Marvel's Spider-Man 2"}]
        results, rejected = archive.build(tree, catalog)
        self.assertEqual(len(results), 1)
        url = results["PPSA08338|Marvel's Spider-Man 2"]
        self.assertIn("PlayStation%205", url)
        self.assertNotIn("PlayStation%204", url)
        self.assertIn("Marvels%20Spider%20Man%202.art", url)
        self.assertEqual(rejected, {})

    def test_ambiguous_editions_skip(self):
        tree = [
            "assets/covers/Sony/PlayStation 5/Game [1111111111111111].art",
            "assets/covers/Sony/PlayStation 5/Game [2222222222222222].art",
        ]
        found, missing = archive.build(tree, [{"id_code": "PPSA00999", "title": "Game"}])
        self.assertFalse(found)
        self.assertEqual(missing["ambiguous"], 1)

    def test_clean_filename_preferred_to_hashed_variant(self):
        tree = [
            "assets/covers/Sony/PlayStation 5/Game.art",
            "assets/covers/Sony/PlayStation 5/Game [2222222222222222].art",
        ]
        found, missing = archive.build(tree, [{"id_code": "PPSA00999", "title": "Game"}])
        self.assertEqual(len(found), 1)
        self.assertTrue(found["PPSA00999|Game"].endswith("/Game.art"))
        self.assertFalse(missing)

    def test_no_artwork_bytes_or_credentials_in_generated_map(self):
        blob = (ROOT / "src/archiveArtworkUrls.json").read_bytes()
        header = (ROOT / "src/archive_artwork_index.h").read_bytes()
        self.assertLess(len(blob) + len(header), 500_000)
        self.assertNotIn(b"Authorization", blob)
        self.assertNotIn(b"github_pat_", blob)
        self.assertNotIn(b"-----BEGIN", header)


if __name__ == "__main__":
    unittest.main()
