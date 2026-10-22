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
            self.assertIn(source["adapter"], ("archive", "gfs", "html", "tsv", "vimm", "axekin", "romheaven", "retro-collections"))

    def test_snapshot_contains_only_metadata(self):
        self.assertEqual(self.snapshot["schemaVersion"], 1)
        for source in self.snapshot["sources"]:
            self.assertIn(source["status"], {"ready", "partial", "empty", "error", "stale"})
            if source["id"] == "gfs":
                self.assertEqual(len(source["packages"]), source["coverage"]["indexed"])
            else:
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

    def test_public_game_links_choose_one_title_and_ignore_comment_navigation(self):
        parser = importer.PublicGameLinks("https://cdromance.org/", "cdromance")
        parser.feed(
            '<a href="/psp/example-game/" title="Example Game">'
            '<img alt="Example Game PSP">Example Game PSP</a>'
            '<a href="/psp/other-game/"><img alt="Other Game PSP">Other Game</a>'
            '<a href="/psp/image-only/"><img alt="Image Only Game"></a>'
            '<a href="/psp/example-game/?xf-comments=1">comments 20</a>'
            '<a href="/psp/example-game/#comments">comments 30</a>'
            '<a href="/psp/bad-name/">Download Now</a>'
            '<a href="/psp/more-comments/">comments 12</a>')
        self.assertEqual(parser.rows, [
            ("Example Game", "https://cdromance.org/psp/example-game/"),
            ("Other Game", "https://cdromance.org/psp/other-game/"),
            ("Image Only Game", "https://cdromance.org/psp/image-only/")])

    def test_reviewed_title_corrections_are_exact_and_preserve_record_identity(self):
        source = {"id": "axekin"}
        page = "https://www.axekin.com/games/marble-saga-kororinpamarbles-balance-challengepal"
        raw = "Marble Saga: KororinpaMarbles! Balance ChallengePAL"
        expected_id = "EXT-" + importer.hashlib.sha256(("axekin" + page + raw).encode()).hexdigest()[:20]
        row = importer.record(source, raw, page, system="nintendo-wii")
        self.assertEqual(row["title"], "Marble Saga: Kororinpa")
        self.assertEqual(row["id"], expected_id)
        for wrong_source, system, url, title in [
            ("ziperto", "nintendo-wii", page, raw),
            ("axekin", "sony-ps3", page, raw),
            ("axekin", "nintendo-wii", page + "-different", raw),
            ("axekin", "nintendo-wii", page, raw + " Demo"),
        ]:
            self.assertEqual(importer.correct_source_title(wrong_source, system, url, title), title)

    def test_platform_classification(self):
        self.assertEqual(importer.platform("Mario Kart (Nintendo DS)"), "nintendo-ds")
        self.assertEqual(importer.platform("Super Mario 64 N64"), "nintendo-n64")
        self.assertEqual(importer.platform("Ratchet and Clank PSP"), "sony-psp")
        self.assertEqual(importer.platform("Unclassified game", "sony-ps4"), "sony-ps4")


    def test_axekin_inertia_metadata_without_download_links(self):
        import html
        from unittest.mock import patch
        page = {
            "component": "games/index",
            "props": {"games": {
                "meta": {"total": 3},
                "data": [{
                    "title": "Example Game", "slug": "example-game",
                    "description": "<p>Adventure &amp; puzzles</p>",
                    "coverImage": "/uploads/games/example.webp",
                    "fileSize": "1.5 GB", "version": "3.4",
                    "platforms": [{"name": "Nintendo Switch"}],
                    "downloadLinks": [{"url": "https://downloads.example/bad.pkg"}]
                }]
            }}
        }
        markup = '<div id="app" data-page="' + html.escape(json.dumps(page), quote=True) + '"></div>'
        source = {"id": "axekin", "name": "Axekin", "url": "https://www.axekin.com/",
                  "index_url": "https://www.axekin.com/games"}
        with patch.object(importer, "fetch", return_value=markup) as fetch:
            rows, partial = importer.axekin(source, 1)
        self.assertTrue(partial)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["title"], "Example Game")
        self.assertEqual(rows[0]["sourceUrl"], "https://www.axekin.com/games/example-game")
        self.assertEqual(rows[0]["platform"], "nintendo-switch")
        self.assertEqual(rows[0]["sizeBytes"], 1_500_000_000)
        self.assertEqual(rows[0]["version"], "3.4 / PKG")
        self.assertNotIn("downloadLinks", rows[0])
        self.assertNotIn("bad.pkg", json.dumps(rows[0]))
        self.assertEqual(fetch.call_count, 1)

    def test_romheaven_csv_metadata(self):
        from unittest.mock import patch
        csv_fixture = ('filename,display_name,iconUrl,compressed_size,description\n'
                       '"13 Sentinels Aegis Rim","13 Sentinels: Aegis Rim",'
                       '"https://example.test/icon.png",4205855773,"A strategy game"\n'
                       '"1 2 Switch","1-2-Switch",,1344997965,"Party"\n')
        source = {"id": "romheaven", "name": "RomHeaven", "url": "https://romheaven.com/",
                  "feed_url": "https://romheaven.com/data/switch.csv",
                  "index_url": "https://romheaven.com/switch"}
        with patch.object(importer, "fetch", return_value=csv_fixture):
            rows, partial = importer.romheaven(source, 1)
        self.assertTrue(partial)
        self.assertEqual(rows[0]["title"], "13 Sentinels: Aegis Rim")
        self.assertEqual(rows[0]["platform"], "nintendo-switch")
        self.assertEqual(rows[0]["sourceUrl"], "https://romheaven.com/switch/13_sentinels_aegis_rim")
        self.assertEqual(rows[0]["sizeBytes"], 4205855773)
        with self.assertRaisesRegex(ValueError, "own host"):
            importer.romheaven({**source, "feed_url": "https://evil.example/data/switch.csv"}, 1)

    def test_retro_game_champion_collections_not_fake_games(self):
        from unittest.mock import patch
        page = ('<div data-hook="item-action" aria-label="Amstrad GX4000 game console" role="link"></div>'
                '<div data-hook="item-action" aria-label="Atari 2600 game console" role="link"></div>'
                '<div data-hook="item-action" aria-label="Atari 2600 game console" role="link"></div>'
                '<div data-hook="item-action" aria-label="Unrelated heading" role="presentation"></div>')
        source = {"id": "retrogamechampion", "name": "Retro Game Champion",
                  "url": "https://www.retrogamechampion.com/",
                  "index_url": "https://www.retrogamechampion.com/games"}
        with patch.object(importer, "fetch", return_value=page):
            rows, partial = importer.retro_collections(source, 64)
        self.assertTrue(partial)
        self.assertEqual([row["title"] for row in rows],
                         ["Amstrad GX4000 Collection", "Atari 2600 Collection"])
        self.assertTrue(all("individual games have not been indexed" in row["description"]
                            for row in rows))
        self.assertTrue(all(row["sourceUrl"].endswith("/games") for row in rows))

    def test_archive_seed_discovers_uploader_without_persisting_email(self):
        from unittest.mock import patch
        from urllib.parse import parse_qs, urlsplit
        source = {"id": "maelly-pooh", "name": "Archive.org / maelly_pooh",
                  "url": "https://archive.org/details/@maelly_pooh",
                  "seed_identifier": "ps4-fpkg-collection-english-c"}
        def fake_fetch(url):
            if url.endswith("/metadata/ps4-fpkg-collection-english-c"):
                return json.dumps({"metadata": {"uploader": "uploader@example.org"}})
            if "advancedsearch.php?" in url:
                self.assertEqual(parse_qs(urlsplit(url).query)["q"],
                                 ['uploader:"uploader@example.org"'])
                return json.dumps({"response": {"docs": [{"identifier": "my-archive",
                                                           "title": "PS4 sample collection"}],
                                                 "numFound": 1}})
            if url.endswith("/metadata/my-archive"):
                return json.dumps({"metadata": {"uploader": "uploader@example.org"},
                                   "files": [{"name": "Example Game.pkg", "size": "1024"},
                                             {"name": "private.pkg", "private": True}]})
            raise AssertionError("Unexpected network request")
        with patch.object(importer, "fetch", side_effect=fake_fetch):
            rows, partial = importer.archive(source, 20)
        self.assertFalse(partial)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["title"], "Example Game")
        self.assertEqual(rows[0]["sourceUrl"], "https://archive.org/details/my-archive")
        self.assertNotIn("uploader@example.org", json.dumps(rows))
        self.assertNotIn("uploader@example.org", json.dumps(source))


    def test_axekin_full_snapshot_only_uses_games_roms_platforms(self):
        full = json.loads((ROOT / "assets/catalogs/axekin.json").read_text(encoding="utf8"))
        self.assertEqual(full["schemaVersion"], 1)
        source = full["sources"][0]
        self.assertEqual(source["id"], "axekin")
        self.assertEqual(source["status"], "ready")
        coverage = source["coverage"]
        self.assertEqual(coverage["pages"], (coverage["advertisedGames"] + 24) // 25)
        self.assertEqual(coverage["visitedGames"], coverage["advertisedGames"])
        self.assertEqual(coverage["indexedEntries"], len(source["packages"]))
        self.assertGreater(len(source["packages"]), 18000)
        mapping = importer.axekin_supported_platforms()
        self.assertNotIn("ps-vita", mapping)
        self.assertEqual(set(mapping), set(json.loads(
            (ROOT / "config/axekin_platforms.json").read_text(encoding="utf8"))))
        ids = set()
        for item in source["packages"]:
            self.assertIn(item["platform"], set(mapping.values()))
            self.assertNotIn("downloadUrl", item)
            self.assertNotIn("downloadLinks", item)
            self.assertNotIn("description", item)
            self.assertTrue(item["sourceUrl"].startswith("https://www.axekin.com/games/"))
            self.assertNotIn(item["id"], ids)
            ids.add(item["id"])

    def test_full_crawl_visits_all_pages_and_filters_out_unmatched_consoles(self):
        from unittest.mock import patch
        base = {"id": "axekin", "name": "Axekin", "url": "https://www.axekin.com/",
                "index_url": "https://www.axekin.com/games"}
        def game(n, platforms):
            return {"id": n, "title": f"Game {n}", "slug": f"game-{n}", "version": "1.2",
                    "platforms": [{"slug": slug} for slug in platforms],
                    "downloadLinks": [{"url": "https://bad.example/rom.zip"}]}
        pages = {
            1: {"meta": {"lastPage": 3, "total": 4},
                "data": [game(1, ["switch"]), game(2, ["ps-vita"])]},
            2: {"meta": {"lastPage": 3, "total": 4},
                "data": [game(3, ["ps5", "ps-vita"])]},
            3: {"meta": {"lastPage": 3, "total": 4},
                "data": [game(4, ["atari-2600", "switch"])]},
        }
        with patch.object(importer, "axekin_page", side_effect=lambda source, page: pages[page]) as mocked:
            rows, stats = importer.axekin_full(base, workers=2)
        self.assertEqual(mocked.call_count, 3)
        self.assertEqual(stats["pages"], 3)
        self.assertEqual(stats["visitedGames"], 4)
        self.assertEqual(stats["excludedGames"], 1)
        self.assertEqual({(x["title"], x["platform"]) for x in rows},
                         {("Game 1", "nintendo-switch"),
                          ("Game 3", "sony-ps5"),
                          ("Game 4", "atari-2600"),
                          ("Game 4", "nintendo-switch")})
        self.assertTrue(all("bad.example" not in json.dumps(x) for x in rows))
        self.assertTrue(all(x["version"] == "1.2 / ZIP" for x in rows))

if __name__ == "__main__":
    unittest.main()
