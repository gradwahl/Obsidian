"""IGDB Multi-Query API contract, concurrency and cache-resume tests (offline)."""
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import igdb_artwork as igdb


def game(index, platform=48):
    return {"id": index + 10, "name": f"Game {index:02d}", "platforms": [platform],
            "cover": {"image_id": f"co{index + 10000}"}}


class MultiQueryTests(unittest.TestCase):
    def test_query_format_platform_scope_and_out_of_order_named_response(self):
        client = igdb.IGDBClient("test-client", "test-secret")
        seen = []

        def query(endpoint, body):
            self.assertEqual(endpoint, "multiquery")
            seen.append(body)
            return [{"name": "game1", "result": [game(1, 167)]},
                    {"name": "game0", "result": [game(0)]}]
        client.query = query
        rows = client.multi_search([('Game "00"', 48), ("Game 01", 167)])
        self.assertIn('query games "game0"', seen[0])
        self.assertIn('query games "game1"', seen[0])
        self.assertIn('search "Game 00";', seen[0])
        self.assertIn("where platforms = (48);", seen[0])
        self.assertIn("where platforms = (167);", seen[0])
        self.assertEqual(rows[0][0]["id"], 10)
        self.assertEqual(rows[1][0]["id"], 11)
        self.assertEqual(igdb.escape_search('Test"; } query games "evil" {'),
                         "Test query games evil")

    def test_sparse_blocks_fall_back_without_losing_real_covers(self):
        client = igdb.IGDBClient("test", "fake")
        request = [("Game 00", 48), ("Game 01", 48), ("Game 02", 167)]
        searched = []
        client.query = lambda *_args: [{"name": "game1", "result": []},
                                       {"name": "game0", "result": [game(0)]}]
        def individual(title, pid):
            searched.append((title, pid))
            return [game(2, 167)] if title == "Game 02" else []
        client.search = individual
        rows = client.multi_search(request)
        self.assertEqual([len(row) for row in rows], [1, 0, 1])
        self.assertEqual(rows[2][0]["platforms"], [167])
        self.assertEqual(searched, [("Game 02", 167)])
        self.assertEqual(client.fallback_searches, 1)
        # An entirely empty Multi-Query is also handled by individual
        # checks, never by blindly assigning 'not-found'.
        client.query = lambda *_args: []
        client.search = lambda title, pid: []
        self.assertEqual(client.multi_search(request), [[], [], []])
        self.assertEqual(client.fallback_searches, 4)

    def test_absent_result_list_and_invalid_names_do_not_silently_cache(self):
        client = igdb.IGDBClient("test", "fake")
        request = [("Game 00", 48), ("Game 01", 48)]
        client.query = lambda *_args: [{"name": "game1"},
                                       {"name": "game0", "result": [game(0)]}]
        client.search = lambda title, pid: []
        self.assertEqual([len(row) for row in client.multi_search(request)], [1, 0])
        for result in [
            [{"name": "game0", "result": []}, {"name": "game0", "result": []}],
            [{"name": "game0", "result": []}, {"name": "unexpected", "result": []}],
            [{"name": "game0", "result": {}}, {"name": "game1", "result": []}],
        ]:
            client.query = lambda *_args, result=result: result
            with self.assertRaises(ValueError):
                client.multi_search(request)
        with self.assertRaises(ValueError):
            client.multi_search([])
        with self.assertRaises(ValueError):
            client.multi_search([("Good", 48)] * 11)

    def test_failed_verification_not_copied_into_cache(self):
        client = igdb.IGDBClient("test", "fake")
        client.query = lambda *_args: [{"name": "game0", "result": [game(0)]}]
        def fail(*_args):
            raise RuntimeError("individual API unavailable")
        client.search = fail
        names = {
            "sony-ps4|game00": ("sony-ps4", "Game 00"),
            "sony-ps4|game01": ("sony-ps4", "Game 01"),
        }
        cache = {"schemaVersion": 1, "results": {}}
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "cache.json"
            with patch.object(igdb, "CACHE", path):
                with self.assertRaisesRegex(RuntimeError, "unavailable"):
                    igdb.scan(client, names, cache, 2, {"sony-ps4": 48})
                self.assertEqual(cache["results"], {})
                self.assertFalse(path.exists())

    def test_exactly_ten_searches_per_request_and_resumable_parallel_cache(self):
        names = {f"sony-ps4|game{i:02d}": ("sony-ps4", f"Game {i:02d}")
                 for i in range(25)}
        cache = {"schemaVersion": 1, "results": {}}
        class Client:
            def __init__(self):
                self.groups = []
                self.active = 0
                self.peak = 0
                self.lock = threading.Lock()
            def multi_search(self, requests):
                with self.lock:
                    self.groups.append(requests)
                    self.active += 1
                    self.peak = max(self.peak, self.active)
                time.sleep(.025)
                with self.lock:
                    self.active -= 1
                return [[{"id": int(re.search(r"\d+", title)[0]) + 100,
                          "name": title, "platforms": [pid],
                          "cover": {"image_id": "coabcde"}}] for title, pid in requests]
        client = Client()
        with tempfile.TemporaryDirectory() as folder:
            with patch.object(igdb, "CACHE", Path(folder) / "cache.json"):
                self.assertEqual(igdb.scan(client, names, cache, 21, {"sony-ps4": 48},
                                           batch_size=10, workers=3), 21)
                self.assertEqual(sorted(len(group) for group in client.groups), [1, 10, 10])
                self.assertEqual(client.peak, 3)
                self.assertEqual(len(json.loads((Path(folder) / "cache.json").read_text())["results"]), 21)
                self.assertEqual(igdb.scan(client, names, cache, 25, {"sony-ps4": 48},
                                           batch_size=10, workers=2), 4)
                self.assertEqual(igdb.scan(client, names, cache, 25, {"sony-ps4": 48}), 0)
                self.assertEqual(len(cache["results"]), 25)
                self.assertEqual({row["status"] for row in cache["results"].values()}, {"matched"})
                self.assertEqual(igdb.export_cache(cache, path=Path(folder) / "export.json"), 25)

    def test_incomplete_batch_never_written_as_missing(self):
        names = {"sony-ps4|game00": ("sony-ps4", "Game 00"),
                 "sony-ps4|game01": ("sony-ps4", "Game 01")}
        class Bad:
            def multi_search(self, requests):
                return [[]]  # response lost one of two games
        cache = {"schemaVersion": 1, "results": {}}
        with tempfile.TemporaryDirectory() as folder:
            with patch.object(igdb, "CACHE", Path(folder) / "cache.json"):
                with self.assertRaisesRegex(ValueError, "Incomplete"):
                    igdb.scan(Bad(), names, cache, 2, {"sony-ps4": 48})
                self.assertEqual(cache["results"], {})
                self.assertFalse((Path(folder) / "cache.json").exists())

    def test_real_client_shared_rate_limiter_with_parallel_requests(self):
        requests = []
        class Clock:
            def __init__(self):
                self.value = 100
                self.lock = threading.Lock()
            def now(self):
                with self.lock:
                    return self.value
            def sleep(self, delay):
                with self.lock:
                    self.value += delay
        clock = Clock()
        def opener(req, timeout=None):
            requests.append((req.full_url, clock.now()))
            class Response:
                def __enter__(self): return self
                def __exit__(self, *args): return None
                def read(self, *args):
                    return b"[]"
            return Response()
        client = igdb.IGDBClient("test", "test", opener=opener,
                                 sleep=clock.sleep, clock=clock.now)
        client.token = "mock-token"
        with ThreadPoolExecutor(max_workers=3) as pool:
            list(pool.map(lambda _: client.query("multiquery", 'query games "game0" {};'), range(6)))
        self.assertEqual(client.calls, 6)
        times = sorted(t for _, t in requests)
        self.assertTrue(all(b-a >= .349 for a,b in zip(times, times[1:])), times)
        self.assertTrue(all(url.endswith("/multiquery") for url,_ in requests))


if __name__ == "__main__":
    unittest.main()
