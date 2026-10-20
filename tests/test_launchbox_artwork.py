"""Offline LaunchBox matching, merge priority and archive safety checks."""
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import launchbox_artwork as lb
import enrich_artwork as art

FRONT = "11111111-1111-1111-1111-111111111111.jpg"
OTHER = "22222222-2222-2222-2222-222222222222.png"


def game(identity, name, platform="Sony Playstation 3"):
    return f"<Game><DatabaseID>{identity}</DatabaseID><Name>{name}</Name><Platform>{platform}</Platform></Game>"


def image(identity, filename=FRONT, kind="Box - Front", region="North America"):
    return (f"<GameImage><DatabaseID>{identity}</DatabaseID><FileName>{filename}</FileName>"
            f"<Type>{kind}</Type><Region>{region}</Region></GameImage>")


class LaunchBoxTests(unittest.TestCase):
    def run_match(self, xml, rows, baseline=None):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "metadata.zip"
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("Metadata.xml", "<LaunchBox>" + xml + "</LaunchBox>")
            records = [("Source", {"id": str(i), "title": name, "platform": platform})
                       for i, (name, platform) in enumerate(rows)]
            return lb.match_records(records, baseline or {}, path)

    def test_console_alias_front_only_and_editions(self):
        xml = (game("1", "Canonical") + game("2", "Example", "Sony Playstation 4") +
               '<GameAlternateName><DatabaseID>1</DatabaseID><AlternateName>Example</AlternateName></GameAlternateName>' +
               image("1") + image("1", OTHER, "Screenshot - Gameplay") + image("2", OTHER))
        matches, counts = self.run_match(xml, [("Example PS3", "sony-ps3"),
                                               ("Example", "sony-ps4"),
                                               ("Example Deluxe", "sony-ps3")])
        self.assertEqual(matches["Source|0"]["url"], lb.IMAGE_BASE + FRONT)
        self.assertEqual(matches["Source|0"]["provider"], "launchbox-exact-platform-alias")
        self.assertEqual(matches["Source|1"]["url"], lb.IMAGE_BASE + OTHER)
        self.assertEqual(counts, {"matched": 2, "not-found": 1})

    def test_collision_with_unillustrated_game_is_still_ambiguous(self):
        matches, counts = self.run_match(game("1", "Example") + game("2", "Example") + image("1"),
                                         [("Example", "sony-ps3")])
        self.assertFalse(matches)
        self.assertEqual(counts["ambiguous"], 1)

    def test_unicode_names_do_not_collapse_to_numbers(self):
        self.assertNotEqual(lb.title_key("日本語 2"), lb.title_key("別の 2"))
        self.assertEqual(lb.title_key("Pokémon"), lb.title_key("Pokemon"))
        matches, _ = self.run_match(game("1", "日本語 2") + game("2", "別の 2") +
                                    image("1") + image("2", OTHER), [("日本語 2", "sony-ps3")])
        self.assertEqual(matches["Source|0"]["url"], lb.IMAGE_BASE + FRONT)

    def test_regions_are_tags_not_words_in_game_title(self):
        covers = [("Europe", lb.IMAGE_BASE + OTHER), ("North America", lb.IMAGE_BASE + FRONT)]
        self.assertEqual(lb.select_cover("The Last of Us", covers), lb.IMAGE_BASE + FRONT)
        self.assertEqual(lb.select_cover("Example (Europe)", covers), lb.IMAGE_BASE + OTHER)
        self.assertIsNone(lb.select_cover("Example (Japan)", covers))
        self.assertIsNone(lb.select_cover("Example (EUR/USA)", covers))

    def test_unsafe_filename_and_wrong_types_never_exported(self):
        for filename in ("../test.jpg", "https://evil.test/a.jpg", FRONT + "?x=1", None):
            self.assertIsNone(lb.image_url(filename))
        matches, counts = self.run_match(game("1", "Example") + image("1", "../test.jpg") +
                                         image("1", FRONT, "Box - Back"), [("Example", "sony-ps3")])
        self.assertFalse(matches)
        self.assertEqual(counts["no-front-cover-or-region"], 1)

    def test_baseline_matches_excluded_and_merge_preserves_review(self):
        matches, _ = self.run_match(game("1", "Example") + image("1"), [("Example", "sony-ps3")],
                                    {"Source|0": {"url": "https://prior.test/a.jpg"}})
        self.assertFalse(matches)
        candidate = {"url": lb.IMAGE_BASE + FRONT, "provider": "launchbox-exact-platform-title", "confidence": 95}
        rows = [("Source", {"id": "1", "title": "Example", "platform": "sony-ps3"})]
        result, report = art.match_records(rows, {}, launchbox_matches={"Source|1": candidate})
        self.assertEqual(result["Source|1"], candidate)
        igdb = {"sony-ps3|example": {"url": "https://images.igdb.com/igdb/image/upload/t_cover_big/co12345.jpg",
                                    "provider": "igdb-exact-platform-title", "confidence": 95}}
        self.assertEqual(art.match_records(rows, {}, igdb, {"Source|1": candidate})[0]["Source|1"], igdb["sony-ps3|example"])
        index = {"sony-ps3": {"strict": {"example": {"https://a.test/a.png", "https://b.test/b.png"}}, "relaxed": {}}}
        result, report = art.match_records(rows, index, launchbox_matches={"Source|1": candidate})
        self.assertFalse(result)
        self.assertEqual(report["status"], {"review": 1})
        for changed in (dict(candidate, url="https://evil.test/" + FRONT), dict(candidate, confidence=85),
                        dict(candidate, provider="unknown")):
            self.assertFalse(art.match_records(rows, {}, launchbox_matches={"Source|1": changed})[0])

    def test_invalid_refresh_preserves_cached_archive(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "metadata.zip"
            path.write_bytes(b"previous cache")
            with patch.object(lb, "urlopen", return_value=io.BytesIO(b"not a zip")):
                with self.assertRaises(zipfile.BadZipFile):
                    lb.fetch_archive(path, refresh=True)
            self.assertEqual(path.read_bytes(), b"previous cache")
            self.assertFalse(path.with_suffix(".download.tmp").exists())


if __name__ == "__main__":
    unittest.main()
