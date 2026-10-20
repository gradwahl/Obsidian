"""Offline bulk catalogue pagination, resume, matching and CLI proofs."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import igdb_artwork as igdb


def game(number, name="Example", platform=48, image="co12345"):
    return {"id": number, "name": name, "platforms": [platform],
            "cover": {"image_id": image}}


class BulkTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        for name, path in [("CACHE", self.root / "results.json"),
                           ("EXPORT", self.root / "export.json")]:
            patcher = patch.object(igdb, name, path)
            patcher.start()
            self.addCleanup(patcher.stop)

    def test_query_uses_platform_and_id_cursor_with_500_limit(self):
        client = igdb.IGDBClient("unused", "unused")
        with patch.object(client, "query", return_value=[]) as query:
            self.assertEqual(client.catalogue_page(48, 123), [])
        query.assert_called_once_with("games", (
            'fields id,name,platforms,cover.image_id; '
            'where platforms = (48) & id > 123; sort id asc; limit 500;'))

    def test_interrupted_pages_resume_and_completed_cache_needs_no_pages(self):
        first = [game(i) for i in range(1, 501)]
        client = Mock()
        client.catalogue_page.side_effect = [first, RuntimeError("interrupted")]
        with self.assertRaisesRegex(RuntimeError, "interrupted"):
            igdb.load_catalogue(client, 48)
        path = self.root / "igdb-catalogues/48.json"
        saved = json.loads(path.read_text())
        self.assertFalse(saved["complete"])
        self.assertEqual(len(saved["games"]), 500)
        client.reset_mock()
        client.catalogue_page.side_effect = [[game(501)]]
        self.assertEqual(len(igdb.load_catalogue(client, 48)), 501)
        client.catalogue_page.assert_called_once_with(48, 500)
        client.reset_mock()
        self.assertEqual(len(igdb.load_catalogue(client, 48)), 501)
        client.catalogue_page.assert_not_called()
        client.catalogue_page.side_effect = [[game(600)]]
        self.assertEqual(igdb.load_catalogue(client, 48, refresh=True), [game(600)])
        client.catalogue_page.assert_called_once_with(48, 0)

    def test_full_final_page_requires_confirmed_end(self):
        client = Mock()
        client.catalogue_page.side_effect = [[game(i) for i in range(1, 501)], []]
        self.assertEqual(len(igdb.load_catalogue(client, 48)), 500)
        self.assertEqual(client.catalogue_page.call_count, 2)

    def test_bad_page_never_checkpointed_or_used_for_matches(self):
        bad_pages = [None, [game(2), game(1)], [game(1), game(1)],
                     [game(1, platform=167)], [{"id": 1}],
                     [dict(game(1), cover="bad")], [game(i) for i in range(1, 502)]]
        for page in bad_pages:
            with self.subTest(page=str(page)[:70]):
                client = Mock()
                client.catalogue_page.return_value = page
                with self.assertRaises(ValueError):
                    igdb.load_catalogue(client, 48)
                self.assertFalse((self.root / "igdb-catalogues/48.json").exists())

    def test_matches_old_misses_but_preserves_ambiguity_editions_and_successes(self):
        client = Mock()
        client.catalogue_page.return_value = [
            game(1, "Found"), game(2, "Ambiguous", image="coabcde"),
            game(3, "Ambiguous", image="co54321"), game(4, "Edition"),
        ]
        names = {"sony-ps4|" + title: ("sony-ps4", title) for title in
                 ["Found PS4", "Ambiguous", "Edition Deluxe Edition", "Missing", "Kept"]}
        old = {"status": "matched", "provider": "igdb-exact-platform-title",
               "confidence": 95, "url": igdb.cover_url("co99999")}
        cache = {"schemaVersion": 1, "results": {
            "sony-ps4|Found PS4": {"status": "not-found", "searchedTitle": "Found"},
            "sony-ps4|Kept": old.copy()}}
        totals = igdb.scan_bulk(client, names, cache, {"sony-ps4": 48})
        self.assertEqual(dict(totals), {"matched": 1, "review": 1, "not-found": 2})
        self.assertEqual(cache["results"]["sony-ps4|Found PS4"]["status"], "matched")
        self.assertEqual(cache["results"]["sony-ps4|Kept"], old)
        self.assertNotIn("sony-ps4|Missing", cache["results"])
        self.assertNotIn("sony-ps4|Ambiguous", cache["results"])
        exported = json.loads(igdb.EXPORT.read_text())["matches"]
        self.assertEqual(set(exported), {"sony-ps4|Found PS4", "sony-ps4|Kept"})
        client.multi_search.assert_not_called()
        client.search.assert_not_called()

    def test_cli_bulk_matches_all_despite_search_cap_and_exports(self):
        records = [("Source", {"id": str(i), "title": name, "platform": "sony-ps4"})
                   for i, name in enumerate(["First", "Second", "Missing"])]
        client = Mock()
        client.platforms.return_value = {"sony-ps4": 48}
        client.catalogue_page.return_value = [game(1, "First"), game(2, "Second")]
        with (patch.object(igdb, "load_records", return_value=records),
              patch.object(igdb, "ROOT", self.root),
              patch.object(igdb, "IGDBClient", return_value=client),
              patch.dict("os.environ", {"IGDB_CLIENT_ID": "fake", "IGDB_CLIENT_SECRET": "fake"})):
            self.assertEqual(igdb.main(["--bulk", "--max-queries", "1", "--platforms", "sony-ps4"]), 0)
        self.assertEqual(len(json.loads(igdb.EXPORT.read_text())["matches"]), 2)
        client.multi_search.assert_not_called()

    def test_incomplete_catalogue_cannot_export_partial_matches(self):
        client = Mock()
        client.catalogue_page.side_effect = [
            [game(i, "Found" if i == 1 else "Other") for i in range(1, 501)],
            RuntimeError("network failure")]
        cache = {"schemaVersion": 1, "results": {}}
        with self.assertRaises(RuntimeError):
            igdb.scan_bulk(client, {"sony-ps4|found": ("sony-ps4", "Found")}, cache,
                           {"sony-ps4": 48})
        self.assertEqual(cache["results"], {})
        self.assertFalse(igdb.EXPORT.exists())

    def test_completed_console_export_survives_next_console_failure(self):
        client = Mock()
        client.catalogue_page.side_effect = [[game(1, "Found")], RuntimeError("failure")]
        cache = {"schemaVersion": 1, "results": {}}
        names = {"sony-ps4|found": ("sony-ps4", "Found"),
                 "sony-ps5|found": ("sony-ps5", "Found")}
        with self.assertRaises(RuntimeError):
            igdb.scan_bulk(client, names, cache, {"sony-ps4": 48, "sony-ps5": 167})
        self.assertEqual(set(json.loads(igdb.EXPORT.read_text())["matches"]),
                         {"sony-ps4|found"})
        self.assertEqual(set(json.loads(igdb.CACHE.read_text())["results"]),
                         {"sony-ps4|found"})

    def test_bulk_indexes_articles_and_cleaned_distribution_suffixes(self):
        client = Mock()
        client.catalogue_page.return_value = [
            game(1, "The Example", platform=37), game(2, "Another Game", platform=37)]
        names = {"nintendo-3ds|example": ("nintendo-3ds", "Example, The 3DS (USA) CIA"),
                 "nintendo-3ds|another": ("nintendo-3ds", "Another Game Decrypted 3DS (EUR) ROM")}
        cache = {"schemaVersion": 1, "results": {}}
        self.assertEqual(igdb.scan_bulk(client, names, cache, {"nintendo-3ds": 37})["matched"], 2)
        self.assertEqual(len(json.loads(igdb.EXPORT.read_text())["matches"]), 2)

    def test_bulk_dry_run_needs_no_credentials_or_client(self):
        with (patch.object(igdb, "load_records", return_value=[]),
              patch.object(igdb, "ROOT", self.root),
              patch.object(igdb, "IGDBClient") as client,
              patch.dict("os.environ", {}, clear=True)):
            self.assertEqual(igdb.main(["--bulk", "--dry-run"]), 0)
        client.assert_not_called()
        self.assertFalse(igdb.CACHE.exists())


if __name__ == "__main__":
    unittest.main()
