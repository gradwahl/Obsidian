"""Official alternative names: fetching, strict matching, collision and export proof."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import igdb_artwork as igdb
import enrich_artwork as artwork


def game(number=1, name="Canonical Game", alias="Regional Name", comment="European title", image="co12345"):
    return {"id": number, "name": name, "platforms": [37],
            "cover": {"image_id": image},
            "alternative_names": [{"name": alias, "comment": comment}]}


class AliasTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        for name, path in [("CACHE", self.root / "results.json"),
                           ("EXPORT", self.root / "export.json")]:
            patcher = patch.object(igdb, name, path)
            patcher.start()
            self.addCleanup(patcher.stop)

    def select(self, title, games, pid=37):
        return igdb.select_match("nintendo-3ds", title, games, pid, aliases=True)

    def test_exact_alias_and_same_platform_required(self):
        row = game()
        result = self.select("Regional Name 3DS (EUR) CIA", [row])
        self.assertEqual(result["status"], "matched")
        self.assertEqual(result["provider"], "igdb-exact-platform-alias")
        self.assertEqual(result["gameId"], 1)
        self.assertEqual(self.select("Canonical Game", [row])["provider"], "igdb-exact-platform-title")
        self.assertEqual(self.select("Regional Name", [row], 48)["status"], "not-found")
        for title in ["Regional", "Regional Name Deluxe Edition", "Regional Name 2", "Regional Name (UNDUB)"]:
            self.assertEqual(self.select(title, [row])["status"], "not-found")
        self.assertEqual(igdb.select_match("nintendo-3ds", "Regional Name", [row], 37)["status"],
                         "not-found")

    def test_collisions_with_alias_or_canonical_are_review_even_without_two_covers(self):
        a = game()
        others = [game(2), game(2, name="Regional Name", alias="Other"),
                  dict(game(2), cover={}), game(2, image="co54321")]
        for b in others:
            with self.subTest(other=b):
                self.assertEqual(self.select("Regional Name", [a, b])["status"], "review")
        # Duplicated alternative-name spellings on one game aren't a collision.
        a["alternative_names"].append({"name": "Regional Name", "comment": "Other region"})
        self.assertEqual(self.select("Regional Name", [a])["status"], "matched")

    def test_non_published_and_very_short_aliases_are_excluded(self):
        for comment in ["Acronym", "Abbreviation", "Working title", "Development title",
                        "Prototype", "Executable filename", "Misspelling", "Incorrect name"]:
            self.assertEqual(self.select("Regional Name", [game(comment=comment)])["status"], "not-found")
        self.assertEqual(self.select("RG", [game(alias="RG")])["status"], "not-found")
        self.assertEqual(self.select("Regional Name", [game(image="invalid")])["status"], "not-found")

    def test_alias_query_expands_names_and_comments(self):
        client = igdb.IGDBClient("unused", "unused")
        with patch.object(client, "query", return_value=[]) as query:
            client.catalogue_page(37, 123, aliases=True)
        query.assert_called_once_with("games", (
            'fields id,name,platforms,cover.image_id,alternative_names.name,alternative_names.comment; '
            'where platforms = (37) & id > 123; sort id asc; limit 500;'))

    def test_alias_cache_separate_from_canonical_and_invalid_expansion_not_saved(self):
        client = Mock()
        client.catalogue_page.return_value = [game()]
        igdb.load_catalogue(client, 37)
        client.reset_mock()
        self.assertEqual(igdb.load_catalogue(client, 37, aliases=True), [game()])
        client.catalogue_page.assert_called_once_with(37, 0, aliases=True)
        client.reset_mock()
        igdb.load_catalogue(client, 37, aliases=True)
        client.catalogue_page.assert_not_called()
        for invalid in [[123], [{"id": 2}], [{"name": "Alias", "comment": None}], None]:
            client.catalogue_page.return_value = [dict(game(), alternative_names=invalid)]
            with self.assertRaises(ValueError):
                igdb.load_catalogue(client, 37, aliases=True, refresh=True)
        saved = json.loads((self.root / "igdb-catalogues-aliases/37.json").read_text())
        self.assertEqual(saved["games"], [game()])

    def test_alias_download_resumes_without_matching_partial_catalogue(self):
        client = Mock()
        client.catalogue_page.side_effect = [[game(i, alias=f"Alias {i}") for i in range(1, 501)],
                                           RuntimeError("network interrupted")]
        cache = {"schemaVersion": 1, "results": {}}
        names = {"nintendo-3ds|alias1": ("nintendo-3ds", "Alias 1")}
        with self.assertRaises(RuntimeError):
            igdb.scan_bulk(client, names, cache, {"nintendo-3ds": 37}, aliases=True)
        self.assertEqual(cache["results"], {})
        client.reset_mock()
        client.catalogue_page.side_effect = [[]]
        totals = igdb.scan_bulk(client, names, cache, {"nintendo-3ds": 37}, aliases=True)
        client.catalogue_page.assert_called_once_with(37, 500, aliases=True)
        self.assertEqual(totals["matched"], 1)

    def test_cli_alias_export_enrich_and_offline_export_remain_cumulative(self):
        records = [("Source", {"id": "1", "title": "Regional Name", "platform": "nintendo-3ds"})]
        old = {"status": "matched", "provider": "igdb-exact-platform-title",
               "confidence": 95, "url": igdb.cover_url("co99999")}
        igdb.atomic_json(igdb.CACHE, {"schemaVersion": 1, "results": {"nintendo-3ds|old": old}})
        client = Mock()
        client.platforms.return_value = {"nintendo-3ds": 37}
        client.catalogue_page.return_value = [game()]
        with (patch.object(igdb, "ROOT", self.root),
              patch.object(igdb, "load_records", return_value=records),
              patch.object(igdb, "IGDBClient", return_value=client),
              patch.dict("os.environ", {"IGDB_CLIENT_ID": "fake", "IGDB_CLIENT_SECRET": "fake"})):
            self.assertEqual(igdb.main(["--bulk", "--aliases"]), 0)
            with patch.dict("os.environ", {}, clear=True):
                self.assertEqual(igdb.main(["--max-queries", "0"]), 0)
        exported = json.loads(igdb.EXPORT.read_text())["matches"]
        self.assertEqual(len(exported), 2)
        self.assertEqual(exported["nintendo-3ds|old"]["provider"], "igdb-exact-platform-title")
        matches, report = artwork.match_records(records, {}, exported)
        self.assertEqual(matches["Source|1"]["provider"], "igdb-exact-platform-alias")
        self.assertEqual(report["providers"]["igdb-exact-platform-alias"], 1)
        client.multi_search.assert_not_called()

    def test_aliases_flag_requires_bulk(self):
        with self.assertRaises(SystemExit) as error:
            igdb.main(["--aliases"])
        self.assertEqual(error.exception.code, 2)

    def test_alias_enrichment_keeps_existing_art_and_rejects_untrusted_links(self):
        row = {"id": "1", "title": "Regional Name", "platform": "nintendo-3ds"}
        match = {"provider": "igdb-exact-platform-alias", "confidence": 95,
                 "url": igdb.cover_url("co12345")}
        records = [("Source", row)]
        for bad in [dict(match, url="https://other.test/image.jpg"),
                    dict(match, confidence=85), dict(match, provider="unknown"),
                    dict(match, provider=[])]:
            matches, _ = artwork.match_records(records, {}, {"nintendo-3ds|regionalname": bad})
            self.assertEqual(matches, {})
        matches, report = artwork.match_records(
            [("Source", dict(row, coverUrl="https://existing.test/cover.jpg"))], {},
            {"nintendo-3ds|regionalname": match})
        self.assertTrue(matches["Source|1"]["fallbackOnly"])
        self.assertEqual(report["status"]["direct"], 1)


if __name__ == "__main__":
    unittest.main()
