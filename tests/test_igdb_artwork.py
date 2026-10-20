"""Offline IGDB matcher, identity, API and confidentiality tests; no credentials used."""
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import igdb_artwork as igdb
import enrich_artwork as artwork


class IGDBTests(unittest.TestCase):
    def test_platform_and_title_matching_strict(self):
        game = {"id": 101, "name": "Example Game", "platforms": [48, 49],
                "cover": {"image_id": "co12345"}}
        result = igdb.select_match("sony-ps4", "Example Game", [game], 48)
        self.assertEqual(result["status"], "matched")
        self.assertEqual(result["provider"], "igdb-exact-platform-title")
        self.assertEqual(result["url"], "https://images.igdb.com/igdb/image/upload/t_cover_big/co12345.jpg")
        self.assertEqual(igdb.select_match("sony-ps3", "Example Game", [game], 9)["status"], "not-found")
        self.assertEqual(igdb.select_match("sony-ps4", "Example Game: Remastered", [game], 48)["status"],
                         "not-found")
        self.assertEqual(igdb.select_match("sony-ps4", "Example Game", [game], None)["status"],
                         "skipped-platform")
        self.assertEqual(igdb.select_match("sony-ps4", "Example Game",
                         [{**game, "cover": {"image_id": "invalid/../../../file"}}], 48)["status"], "not-found")

    def test_ambiguity_and_region_variants(self):
        a = {"id": 1, "name": "Example (USA)", "platforms": [48],
             "cover": {"image_id": "co10000"}}
        b = {"id": 2, "name": "Example (Europe)", "platforms": [48],
             "cover": {"image_id": "co20000"}}
        self.assertEqual(igdb.select_match("sony-ps4", "Example", [a, b], 48)["status"], "review")
        self.assertEqual(igdb.select_match("sony-ps4", "Example", [a, dict(b, cover=a["cover"])], 48)
                         ["status"], "matched")

    def test_dedup_and_covered_entries_skipped(self):
        records = [
            ("Axekin", {"id": "1", "title": "Same Game (USA)", "platform": "sony-ps4"}),
            ("Ziperto", {"id": "2", "title": "Same Game", "platform": "sony-ps4"}),
            ("SuperPSX", {"id": "3", "title": "Same Game", "platform": "sony-ps5"}),
            ("GFS", {"id": "4", "title": "Has Cover", "platform": "sony-ps5",
                     "coverUrl": "https://example.test/box.jpg"}),
            ("Other", {"id": "5", "title": "Already Matched", "platform": "sony-ps4"}),
        ]
        targets = igdb.eligible_titles(records, {"Other|5": {"url": "https://example.test/box.jpg"}})
        self.assertEqual(len(targets), 2)
        self.assertIn("sony-ps4|samegame", targets)
        self.assertIn("sony-ps5|samegame", targets)

    def test_client_does_not_leak_credentials_into_url_and_checks_platforms(self):
        seen = []
        def open_fake(req, timeout=None):
            seen.append(req)
            class Response:
                def __enter__(self): return self
                def __exit__(self, *args): pass
                def read(self, *_args):
                    if "oauth2/token" in req.full_url:
                        return b'{"access_token":"fake-temporary-token"}'
                    if "platforms" in req.full_url:
                        return b'[{"id":48,"name":"PlayStation 4"},{"id":49,"name":"PlayStation 5"}]'
                    return b'[{"id":123,"name":"Example","platforms":[48],"cover":{"image_id":"co12345"}}]'
            return Response()
        class CounterClock:
            now = 0.0
            def __call__(self): return self.now
            def sleep(self, seconds): self.now += seconds
        clk = CounterClock()
        client = igdb.IGDBClient("public-test-id", "never-log-this-value", opener=open_fake,
                                 sleep=clk.sleep, clock=clk)
        self.assertEqual(client.platforms()["sony-ps4"], 48)
        self.assertEqual(client.platforms()["sony-ps5"], 49)
        self.assertEqual(client.search("Example", 48)[0]["name"], "Example")
        self.assertTrue(all("never-log-this-value" not in req.full_url for req in seen))
        self.assertEqual(seen[0].full_url, "https://id.twitch.tv/oauth2/token")
        self.assertIn(b"never-log-this-value", seen[0].data)
        self.assertGreaterEqual(clk.now, .35)

    def test_source_packaging_suffixes_not_part_of_igdb_game_name(self):
        samples = [
            ("sony-ps4", "Carto - [US] [EN] [1.04]", "Carto"),
            ("sony-ps4", "Citizen Sleeper - [EU] [EN] [1.05]", "Citizen Sleeper"),
            ("sony-ps4", "Crash Bandicoot N Sane Trilogy - [EU] [EN] [1.07]",
             "Crash Bandicoot N Sane Trilogy"),
            ("sony-ps4", "Wolfenstein II The New Colossus - [EU] [EN] [1.07]",
             "Wolfenstein II The New Colossus"),
            ("sony-ps4", "WWE 2K Battlegrounds - [US] [EN] [16.00]",
             "WWE 2K Battlegrounds"),
            ("sony-ps5", "Phasmophobia PS5", "Phasmophobia"),
            ("sony-ps5", "SteamWorld Heist II PS5", "SteamWorld Heist II"),
            ("sony-ps5", "LEGO Builders Journey PS5", "LEGO Builders Journey"),
            ("sony-ps5", "WRC 10 Deluxe Edition PS5", "WRC 10 Deluxe Edition"),
            ("sony-ps5", "Don't Starve Together Console Edition PS5",
             "Don't Starve Together Console Edition"),
            ("nintendo-switch", "Example Switch NSP [Update] (eShop)", "Example"),
            ("sony-ps5", "Example PS4", "Example PS4"),
            ("sony-ps4", "Resident Evil 4 Remake", "Resident Evil 4 Remake"),
        ]
        for platform, raw, expected in samples:
            with self.subTest(title=raw):
                self.assertEqual(igdb.search_title(platform, raw), expected)
        self.assertNotEqual(igdb.identity("sony-ps5", "Phasmophobia PS5"),
                            igdb.identity("sony-ps5", "Phasmophobia"))
        row = {"id": 2, "name": "Phasmophobia", "platforms": [167],
               "cover": {"image_id": "coabcde"}}
        self.assertEqual(igdb.select_match("sony-ps5", "Phasmophobia PS5", [row], 167)
                         ["status"], "matched")
        self.assertEqual(igdb.select_match("sony-ps4", "Phasmophobia PS5", [row], 48)
                         ["status"], "not-found")

    def test_packaging_suffixes_only_on_their_console(self):
        samples = [
            ("nintendo-3ds", "Rhythm Thief & the Emperor's Treasure [Decrypted] 3DS (EUR/USA) ROM",
             "Rhythm Thief & the Emperor's Treasure"),
            ("nintendo-3ds", "Paper Mario: Sticker Star Decrypted 3DS (USA) ROM",
             "Paper Mario: Sticker Star"),
            ("nintendo-3ds", "Super Mario 3D Land 3DS Legit CIA (USA)", "Super Mario 3D Land"),
            ("nintendo-3ds", "League of Heroes 3DS CIA & Decrypted (EUR/USA) ROM eShop",
             "League of Heroes"),
            ("nintendo-3ds", "BlockForm 3DS (USA) CIA (NEW3DS ONLY)", "BlockForm"),
            ("nintendo-3ds", "Shakedown: Hawaii 3DS (EUR/USA) CIA (Updated)", "Shakedown: Hawaii"),
            ("nintendo-wiiu", "PixelMaker WII U ISO (Loadiine) (UPDATE v16) (USA)", "PixelMaker"),
            ("nintendo-wiiu", "How to Train Your Dragon 2 for WII U ISO (Loadiine) (EUR)",
             "How to Train Your Dragon 2"),
            ("sony-psp", "After Burner: Black Falcon (ULES-00753)", "After Burner: Black Falcon"),
            ("sony-ps3", "Need For Speed SHIFT(BLAS-50137)", "Need For Speed SHIFT"),
        ]
        for platform, raw, clean in samples:
            with self.subTest(title=raw):
                self.assertEqual(igdb.search_title(platform, raw), clean)
                other = "sony-ps5" if platform != "sony-ps5" else "sony-ps4"
                self.assertNotEqual(igdb.search_title(other, raw), clean)

    def test_distribution_cleanup_preserves_game_variants(self):
        samples = [
            ("nintendo-3ds", "Example (UNDUB) 3DS (EUR) CIA", "Example (UNDUB)"),
            ("nintendo-3ds", "Example Deluxe Edition 3DS (USA) CIA", "Example Deluxe Edition"),
            ("nintendo-3ds", "Example: Update v1.1 3DS (USA) CIA", "Example: Update v1.1"),
            ("nintendo-3ds", "Example (English) 3DS (USA) CIA", "Example (English)"),
            ("nintendo-3ds", "Example 3DS (USA) CIA [DLC]", None),
            ("nintendo-3ds", "Example 3DS ROM Randomizer", None),
            ("nintendo-wiiu", "Example WII U ISO (Loadiine) (DLCs)", None),
            ("nintendo-switch", "Example Switch NSP [DLC/Update] (eShop)", None),
            ("sony-psp", "Example (BLAS-50137)", None),
        ]
        base = {"id": 1, "name": "Example", "platforms": [48], "cover": {"image_id": "co12345"}}
        for platform, raw, clean in samples:
            with self.subTest(title=raw):
                if clean:
                    self.assertEqual(igdb.search_title(platform, raw), clean)
                self.assertNotEqual(igdb.search_title(platform, raw), "Example")
                self.assertEqual(igdb.select_match(platform, raw, [base], 48)["status"], "not-found")

    def test_additional_distribution_labels_do_not_change_game_identity(self):
        samples = [
            ("nintendo-switch", "World to the West Switch NSP [Update] (eShop) [US/JP]", "World to the West"),
            ("nintendo-switch", "Example Switch (XCI) (UPDATE) (JPN)", "Example"),
            ("nintendo-switch", "Example Switch NSP (ASIA/JAPAN)", "Example"),
            ("nintendo-switch", "Example SWITCH NSP (RF) [eShop]", "Example"),
            ("nintendo-switch", "Example Switch NSP [UPDATE] eShop", "Example"),
            ("nintendo-3ds", "Pokemon X 3DS ROM (EUR) (Gateway/Sky3DS+)", "Pokemon X"),
            ("nintendo-3ds", "Rodea the Sky Soldier ROM & 3DS (USA) CIA (Region Free)", "Rodea the Sky Soldier"),
            ("sony-ps3", "BioShock 2 full game", "BioShock 2"),
            ("sony-ps3", "flOw Downloadable Game", "flOw"),
            ("sony-ps3", "Dead Space 2 ( EN - DE - FR )", "Dead Space 2"),
            ("sony-psp", "Digi-Tiles (Minis)", "Digi-Tiles"),
        ]
        for platform, raw, expected in samples:
            with self.subTest(title=raw):
                self.assertEqual(igdb.search_title(platform, raw), expected)
        for platform, raw in [
            ("nintendo-switch", "Example Switch NSP [DLC/Update] (eShop) [US/JP]"),
            ("nintendo-switch", "Example Deluxe Edition Switch (XCI) (JPN)"),
            ("nintendo-3ds", "Example 3DS ROM (ENGLISH PATCHED) (Gateway/Sky3DS+)"),
            ("nintendo-3ds", "Example Randomizer 3DS ROM (USA) (Gateway/Sky3DS+)"),
            ("sony-ps3", "Example Demo full game"),
            ("sony-psp", "Example (Minis) (Trial Ver.)"),
            ("sony-ps4", "Example Downloadable Game"),
            ("sony-ps4", "Example (Minis)"),
            ("nintendo-switch", "Example eShop"),
        ]:
            with self.subTest(preserved=raw):
                self.assertNotEqual(igdb.search_title(platform, raw), "Example")

    def test_trailing_article_keeps_subtitles_and_ambiguity(self):
        self.assertEqual(igdb.match_name("Addams Family, The"), igdb.match_name("The Addams Family"))
        self.assertEqual(igdb.match_name("Addams Family, The: Pugsley's Scavenger Hunt"),
                         igdb.match_name("The Addams Family: Pugsley's Scavenger Hunt"))
        self.assertNotEqual(igdb.match_name("The Addams Family"), igdb.match_name("Addams Family"))
        a = {"id": 1, "name": "The Example", "platforms": [48], "cover": {"image_id": "co12345"}}
        b = {**a, "id": 2, "name": "Example, The", "cover": {"image_id": "co54321"}}
        self.assertEqual(igdb.select_match("sony-ps4", "Example, The", [a], 48)["status"], "matched")
        self.assertEqual(igdb.select_match("sony-ps4", "Example, The", [a, b], 48)["status"], "review")
        self.assertEqual(igdb.select_match("sony-ps4", "Example, The", [a], 167)["status"], "not-found")

    def test_old_failed_lookups_retry_cleaned_name_only_once(self):
        cache = {"schemaVersion": 1, "results": {
            "sony-ps5|phasmophobiaps5": {"status": "not-found"},
            "sony-ps5|endlessdungeon": {"status": "not-found"},
            "sony-ps5|visageps5": {"status": "matched", "provider": "igdb-exact-platform-title",
                                   "url": "https://images.igdb.com/igdb/image/upload/t_cover_big/coabcde.jpg",
                                   "confidence": 95},
        }}
        names = {
            "sony-ps5|phasmophobiaps5": ("sony-ps5", "Phasmophobia PS5"),
            "sony-ps5|endlessdungeon": ("sony-ps5", "Endless Dungeon"),
            "sony-ps5|visageps5": ("sony-ps5", "Visage PS5"),
        }
        class Fake:
            def __init__(self): self.searched = []
            def multi_search(self, requests):
                self.searched.extend(requests)
                return [[{"id": 23, "name": "Phasmophobia", "platforms": [167],
                          "cover": {"image_id": "coabcde"}}] if title == "Phasmophobia"
                        else [] for title, _ in requests]
        with tempfile.TemporaryDirectory() as folder:
            fake = Fake()
            with patch.object(igdb, "CACHE", Path(folder) / "igdb.json"):
                self.assertEqual(igdb.scan(fake, names, cache, 20, {"sony-ps5": 167}), 1)
                self.assertEqual(fake.searched, [("Phasmophobia", 167)])
                self.assertEqual(cache["results"]["sony-ps5|phasmophobiaps5"]["status"], "matched")
                self.assertEqual(igdb.scan(fake, names, cache, 20, {"sony-ps5": 167}), 0)
            self.assertEqual(cache["results"]["sony-ps5|visageps5"]["status"], "matched")

    def test_cache_resume_and_export(self):
        cache = {"schemaVersion": 1, "results": {}}
        class Fake:
            def multi_search(self, requests):
                return [[{"id": 10, "name": "Matched", "platforms": [48],
                          "cover": {"image_id": "coabcde"}}] if title == "Matched"
                        else [] for title, _ in requests]
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "cache.json"
            out = Path(folder) / "artwork.json"
            names = {"sony-ps4|matched": ("sony-ps4", "Matched"),
                     "sony-ps4|missing": ("sony-ps4", "Missing")}
            with patch.object(igdb, "CACHE", path):
                self.assertEqual(igdb.scan(Fake(), names, cache, 1, {"sony-ps4": 48}), 1)
                self.assertEqual(igdb.scan(Fake(), names, cache, 2, {"sony-ps4": 48}), 1)
                self.assertEqual(igdb.scan(Fake(), names, cache, 2, {"sony-ps4": 48}), 0)
            self.assertEqual(igdb.export_cache(cache, set(names), out), 1)
            self.assertEqual(json.loads(out.read_text())["matches"]["sony-ps4|matched"]["confidence"], 95)
            self.assertNotIn("sony-ps4|missing", json.loads(out.read_text())["matches"])

    def test_offline_export_preserves_matches_after_enrichment(self):
        """An enriched cover must not vanish after the next IGDB batch."""
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            directory = root / "assets/artwork"
            directory.mkdir(parents=True)
            result_cache = root / "cache.json"
            portable = directory / "igdb-covers.json"
            def matched(image):
                return {
                    "status": "matched",
                    "provider": "igdb-exact-platform-title",
                    "confidence": 95,
                    "url": igdb.cover_url(image),
                }
            result_cache.write_text(json.dumps({
                "schemaVersion": 1,
                "results": {
                    "sony-ps4|firstgame": matched("coabcde"),
                    "sony-ps4|secondgame": matched("co12345"),
                },
            }), encoding="utf-8")
            # After the first batch and enrich_artwork, First Game appears
            # in the main manifest and is no longer eligible for API lookup.
            (directory / "cover-matches.json").write_text(json.dumps({
                "schemaVersion": 1,
                "matches": {"Axekin|1": {"url": igdb.cover_url("coabcde")}},
            }), encoding="utf-8")
            games = [
                ("Axekin", {"id": "1", "title": "First Game", "platform": "sony-ps4"}),
                ("Ziperto", {"id": "2", "title": "Second Game", "platform": "sony-ps4"}),
            ]
            with (patch.object(igdb, "ROOT", root),
                  patch.object(igdb, "CACHE", result_cache),
                  patch.object(igdb, "EXPORT", portable),
                  patch.object(igdb, "load_records", return_value=games),
                  patch.dict("os.environ", {}, clear=True)):
                self.assertEqual(igdb.main(["--max-queries", "0", "--platforms", "sony-ps4"]), 0)
            exported = json.loads(portable.read_text(encoding="utf-8"))["matches"]
            self.assertEqual(set(exported), {"sony-ps4|firstgame", "sony-ps4|secondgame"})
            self.assertEqual(exported["sony-ps4|firstgame"]["url"], igdb.cover_url("coabcde"))

    def test_igdb_overrides_only_last_resort_and_reported(self):
        records = [
            ("Other", {"id": "1", "platform": "sony-ps4", "title": "Example"}),
            ("Direct", {"id": "2", "platform": "sony-ps4", "title": "Has Cover",
                        "coverUrl": "https://existing.test/cover.jpg"}),
        ]
        lookup = {"sony-ps4|example": {
            "url": "https://images.igdb.com/igdb/image/upload/t_cover_big/coabcde.jpg",
            "provider": "igdb-exact-platform-title", "confidence": 95}}
        matched, report = artwork.match_records(records, {}, lookup)
        self.assertEqual(matched["Other|1"]["provider"], "igdb-exact-platform-title")
        self.assertEqual(report["providers"]["igdb-exact-platform-title"], 1)
        self.assertNotIn("Direct|2", matched)
        bad = {"sony-ps4|example": {**lookup["sony-ps4|example"], "url": "https://evil.test/im.jpg"}}
        matched, report = artwork.match_records(records, {}, bad)
        self.assertNotIn("Other|1", matched)


if __name__ == "__main__":
    unittest.main()
