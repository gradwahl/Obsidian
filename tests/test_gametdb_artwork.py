"""Offline GameTDB identity, download admission and source-metadata tests."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import gametdb_artwork as db
import enrich_artwork as art
import import_tsv_sources as tsv
import import_sources as sources
import cleanup_artwork_sources as cleanup


class GameTDBTests(unittest.TestCase):
    def test_provider_outage_defers_work_and_keeps_unverified_images_out(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            directory = root / "assets/artwork"
            directory.mkdir(parents=True)
            (directory / "cover-matches.json").write_text('{"matches":{}}', encoding="utf-8")
            pending = {f"S|{i}": {"urls": [db.cover_url("sony-ps3", f"NPUB{i:05d}", "US")],
                                    "provider": "gametdb-exact-product-id", "confidence": 95}
                       for i in range(8)}
            with (patch.object(db, "ROOT", root), patch.object(db, "CACHE", root / "cache"),
                  patch.object(db, "EXPORT", directory / "gametdb-covers.json"),
                  patch.object(db, "fetch_database", return_value=root / "fake.zip"),
                  patch.object(db, "read_database", return_value=({}, {})),
                  patch.object(db, "load_records", return_value=[]),
                  patch.object(db, "candidates", return_value=(pending, {})),
                  patch.object(db, "download_candidate", return_value=(None, {"status": "failed", "error": "timed out"})),
                  patch.object(art, "main", return_value=0)):
                self.assertEqual(db.main(["--workers", "1"]), 1)
            results = json.loads((root / "cache/gametdb-downloads.json").read_text())["results"]
            self.assertTrue(any(row["status"] == "deferred" for row in results.values()))
            self.assertFalse(json.loads((directory / "gametdb-covers.json").read_text())["matches"])

    def test_xml_console_boundary_and_title_collisions(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "db.zip"
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("ps3tdb.xml", '''<data>
                <game><id>NPUB12345</id><type>PS3</type><locale lang="EN"><title>Example</title></locale></game>
                <game><id>NPEB12345</id><type>PS3</type><locale lang="EN"><title>Example</title></locale></game>
                <game><id>NPUB11111</id><type>PSP</type><locale lang="EN"><title>Other</title></locale></game></data>''')
            products, names = db.read_database("sony-ps3", path)
            self.assertEqual(set(products), {"NPUB12345", "NPEB12345"})
            self.assertEqual(len(names["example"]), 2)
            records = [("S", {"id": "1", "title": "Example", "platform": "sony-ps3"}),
                       ("S", {"id": "2", "title": "Example", "platform": "sony-ps3", "productId": "NPUB12345"}),
                       ("S", {"id": "3", "title": "Example Demo", "platform": "sony-ps3", "productId": "NPUB12345"})]
            candidates, counts = db.candidates(records, {}, {"sony-ps3": (products, names)})
            self.assertEqual(set(candidates), {"S|2"})
            self.assertEqual(candidates["S|2"]["provider"], "gametdb-exact-product-id")
            self.assertEqual(counts["release-label-review"], 1)
            self.assertFalse(db.candidates(records, {"S|2": {}}, {"sony-ps3": (products, names)})[0])

    def test_no_export_without_download_and_exact_id_on_fallback(self):
        row = {"urls": [db.cover_url("sony-ps3", "NPUB12345", "US"),
                         db.cover_url("sony-ps3", "NPUB12345", "EN")],
               "provider": "gametdb-exact-product-id", "confidence": 95, "productId": "NPUB12345"}
        with patch.object(db.downloader, "download", return_value={"status": "failed"}):
            self.assertIsNone(db.download_candidate(row)[0])
        with patch.object(db.downloader, "download", side_effect=[{"status": "failed"}, {"status": "downloaded"}]):
            match, _ = db.download_candidate(row)
            self.assertEqual(match["url"], row["urls"][1])
            self.assertTrue(db.trusted_match(match))
            self.assertFalse(db.trusted_match(dict(match, url="https://evil.test/a.jpg")))
        with self.assertRaises(ValueError):
            db.cover_url("sony-psp", "NPUB12345", "US")
        with self.assertRaises(ValueError):
            db.cover_url("sony-ps3", "../NPUB12345", "US")
        with patch.object(db.downloader, "download", return_value={"status": "failed", "error": "timed out"}) as download:
            self.assertIsNone(db.download_candidate(row)[0])
            self.assertEqual(download.call_count, 1)
        self.assertTrue(db.provider_unavailable({"status": "failed", "error": "HTTP Error 429"}))
        self.assertFalse(db.provider_unavailable({"status": "failed", "error": "HTTP Error 404"}))

    def test_merge_respects_existing_artwork_and_review_evidence(self):
        row = {"id": "1", "title": "Example", "platform": "sony-ps3"}
        candidate = {"url": db.cover_url("sony-ps3", "NPUB12345", "US"),
                     "provider": "gametdb-exact-product-id", "confidence": 95}
        result, report = art.match_records([("S", row)], {}, gametdb_matches={"S|1": candidate})
        self.assertEqual(result["S|1"], candidate)
        index = {"sony-ps3": {"strict": {"example": {"https://a.test/a.png", "https://b.test/b.png"}}, "relaxed": {}}}
        result, report = art.match_records([("S", row)], index, gametdb_matches={"S|1": candidate})
        self.assertFalse(result)
        self.assertEqual(report["status"], {"review": 1})
        self.assertEqual(report["reviewEntries"][0]["identity"], "S|1")
        self.assertEqual(len(report["reviewEntries"][0]["candidates"]), 3)
        supplied = {**row, "coverUrl": "https://source.test/a.jpg"}
        self.assertFalse(art.match_records([("S", supplied)], {}, gametdb_matches={"S|1": candidate})[0])


class SourceMetadataTests(unittest.TestCase):
    def test_restore_public_ids_keeps_membership_identity_and_private_fields_out(self):
        item = {"Title ID": "NPUB12345", "Region": "US", "Name": "Example", "Content ID": "UP0001-NPUB12345_00-EXAMPLE"}
        identity = tsv.metadata_identity("test", "sony-ps3", item)
        expected = "EXT-" + hashlib.sha256(("test|sony-ps3|" + item["Content ID"]).encode()).hexdigest()[:20]
        self.assertEqual(identity, expected)
        row = {"id": identity, "title": "Example", "platform": "sony-ps3", "sourceUrl": "https://source.test/"}
        document = {"sources": [{"packages": [row]}]}
        source = {"id": "test", "feeds": [["sony-ps3", "https://source.test/metadata.tsv"]]}
        content = ("Title ID\tRegion\tName\tContent ID\tRAP\tPKG direct link\n"
                   + "\t".join(item[k] for k in ("Title ID", "Region", "Name", "Content ID")) + "\tSECRET\thttps://files.test/game.pkg\n")
        before = copy.deepcopy(row)
        with patch.object(tsv, "fetch", return_value=content):
            self.assertEqual(tsv.restore_product_ids(source, document), 1)
            self.assertEqual(tsv.restore_product_ids(source, document), 0)
        self.assertEqual({k: row[k] for k in before}, before)
        self.assertEqual(set(row) - set(before), {"productId", "productRegion"})
        self.assertEqual(tsv.product_metadata({**item, "Title ID": "NPEB12345"}), {})

    def test_navigation_filter_and_exact_cleanup_preserve_game_ids(self):
        parser = sources.Listings("https://romshq.com/")
        parser.feed('<h2><a href="/game/a">Download Now</a></h2><h2><a href="/game/b">Example</a></h2>')
        self.assertEqual([r[0] for r in parser.rows], ["Example"])
        document = {"sources": [{"id": "axekin", "packages": [{"id": "fixed-id", "platform": "sony-ps3",
            "title": "Joust[broken anchor]", "sourceUrl": "https://www.axekin.com/games/joustbroken-anchor-1"}]}]}
        self.assertEqual(cleanup.clean_snapshot(document), (1, 0))
        self.assertEqual(document["sources"][0]["packages"][0]["id"], "fixed-id")
        self.assertEqual(document["sources"][0]["packages"][0]["title"], "Joust")
        self.assertEqual(cleanup.clean_snapshot(document), (0, 0))


if __name__ == "__main__":
    unittest.main()
