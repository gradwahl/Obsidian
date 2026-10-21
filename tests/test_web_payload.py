"""Exercise the actual C HTTP server and byte-for-byte embedding on Linux/WSL."""
import http.client
import gzip
import importlib.util
from pathlib import Path
import socket
import re
import shutil
import subprocess
import tempfile
import time
import unittest
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("embed", ROOT / "scripts/embed_web_preview.py")
embedding = importlib.util.module_from_spec(spec)
spec.loader.exec_module(embedding)


class WebPayloadTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="wolodrive-http-")
        cls.directory = Path(cls.temp.name)
        cls.preview = cls.directory / "preview"
        (cls.preview / "assets").mkdir(parents=True)
        (cls.preview / "index.html").write_bytes(b"<html>embedded preview</html>")
        (cls.preview / "assets/test.js").write_bytes(b"export const value = 1;\n")
        (cls.preview / "assets/test.css").write_bytes(b"body { color: white; }")
        (cls.preview / "assets/large.js").write_bytes(b"export const data = '" + b"x" * 12000 + b"';\n")
        (cls.preview / "assets/binary.png").write_bytes(bytes(range(256)) * 4096)
        (cls.preview / "assets/empty.bin").write_bytes(b"")
        (cls.preview / 'assets/caf\u00e9 "test".json').write_bytes(b'{"ok":true}')
        (cls.preview / "obsidian-pages").mkdir()
        (cls.preview / "obsidian-pages/manifest.json").write_bytes(b'{"schemaVersion":1,"sources":[]}')
        (cls.preview / "obsidian-pages/fixture-abcdef123456-000-0123456789.json").write_bytes(
            b'[{"id":"PPSA12345","title":"Fixture"}]' * 200)
        cls.compile_server(cls.preview, cls.directory)
        cls.process, cls.port = cls.start_server(cls.directory)

    @classmethod
    def compile_server(cls, preview, directory, browser_stub=None,
                       accept_fault=False, webfirst=False, single_service=False):
        embedding.embed(preview, directory / "ps5_web_assets.h")
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        (directory / "port").write_text(str(port))
        extra = ["-DWOLO_WEB_TEST_BROWSER", str(browser_stub)] if browser_stub else []
        if accept_fault:
            extra.append("-DWOLO_WEB_TEST_ACCEPT_FAILURE")
        if webfirst or single_service:
            extra.extend(["-DWOLO_WEB_NO_BROWSER", "-DWOLO_WEB_AUTOSTART",
                          "-DWOLO_WEB_FIXED_PORT"])
        if single_service:
            extra.append("-DWOLO_SINGLE_SERVICE")
        subprocess.run(["gcc", "-D_GNU_SOURCE", f"-DWOLO_WEB_PORT={port}",
                        "-Wall", "-Wextra", "-Werror", "-O2", "-I", str(directory),
                        str(ROOT / "src/web_payload.c"), *extra, "-pthread", "-lz",
                        "-o", str(directory / "server")], check=True)

    @classmethod
    def start_server(cls, directory):
        port = int((directory / "port").read_text())
        # Deliberately start away from preview: all response bytes must be embedded.
        log_path = directory / "server.log"
        with log_path.open("wb") as log:
            proc = subprocess.Popen([str(directory / "server")], cwd=directory,
                                    stdout=subprocess.DEVNULL, stderr=log)
        for _ in range(100):
            if proc.poll() is not None:
                proc.communicate()
                raise RuntimeError(log_path.read_text())
            ready = re.search(r"Listening at http://127\.0\.0\.1:(\d+)/", log_path.read_text())
            if not ready:
                time.sleep(.02)
                continue
            port = int(ready.group(1))
            try:
                with socket.create_connection(("127.0.0.1", port), timeout=.1):
                    return proc, port
            except OSError:
                time.sleep(.02)
        proc.kill()
        proc.communicate()
        raise RuntimeError("HTTP server did not start")

    @classmethod
    def tearDownClass(cls):
        cls.process.terminate()
        cls.process.communicate(timeout=5)
        cls.temp.cleanup()

    def request(self, path, method="GET", port=None, host="127.0.0.1", headers=None):
        conn = http.client.HTTPConnection(host, port or self.port, timeout=5)
        try:
            conn.request(method, path, headers=headers or {})
            response = conn.getresponse()
            return response.status, dict(response.getheaders()), response.read()
        finally:
            conn.close()

    def test_root_and_all_embedded_bytes(self):
        for file in self.preview.rglob("*"):
            if not file.is_file():
                continue
            path = "/" + quote(file.relative_to(self.preview).as_posix(), safe="/-._~")
            status, headers, body = self.request(path)
            self.assertEqual(status, 200)
            self.assertEqual(body, file.read_bytes())
            self.assertEqual(int(headers["Content-Length"]), len(body))
        self.assertEqual(self.request("/")[2], (self.preview / "index.html").read_bytes())
        self.assertEqual(self.request("/assets/test.js?v=2")[1]["Content-Type"],
                         "text/javascript; charset=utf-8")

    def test_gzip_assets_and_cache_boundaries(self):
        original = (self.preview / "assets/large.js").read_bytes()
        status, headers, body = self.request("/assets/large.js", headers={"Accept-Encoding": "gzip"})
        self.assertEqual(status, 200)
        self.assertEqual(headers.get("Content-Encoding"), "gzip")
        self.assertEqual(headers.get("Vary"), "Accept-Encoding")
        self.assertEqual(int(headers["Content-Length"]), len(body))
        self.assertEqual(gzip.decompress(body), original)
        self.assertIn("max-age", headers["Cache-Control"])
        self.assertEqual(self.request("/assets/large.js")[2], original)
        # A browser must request gzip through Accept-Encoding, not merely
        # mention it in an unrelated field, and q=0 must be respected.
        self.assertEqual(self.request("/assets/large.js",
                           headers={"User-Agent": "test-gzip-client"})[2], original)
        self.assertEqual(self.request("/assets/large.js",
                           headers={"Accept-Encoding": "gzip;q=0"})[2], original)
        compressed = self.request("/assets/large.js",
                                  headers={"Accept-Encoding": "GZIP, deflate"})
        self.assertEqual(gzip.decompress(compressed[2]), original)
        self.assertEqual(self.request("/")[1]["Cache-Control"], "no-store")
        self.assertEqual(self.request("/obsidian-pages/manifest.json")[1]["Cache-Control"], "no-store")
        self.assertIn("max-age", self.request(
            "/obsidian-pages/fixture-abcdef123456-000-0123456789.json")[1]["Cache-Control"])
        status, headers, body = self.request("/assets/large.js", method="HEAD",
                                             headers={"Accept-Encoding": "gzip"})
        self.assertEqual(status, 200)
        self.assertEqual(body, b"")
        self.assertEqual(headers.get("Content-Encoding"), "gzip")

    def test_accept_fault_stops_without_log_spam(self):
        directory = self.directory / "accept-fault"
        directory.mkdir()
        self.compile_server(self.preview, directory, accept_fault=True)
        process, port = self.start_server(directory)
        sockets = []
        try:
            # Once the readiness probe has been accepted, deliberately make
            # accept() fail on every following ready event in the fixture.
            for _ in range(12):
                sockets.append(socket.create_connection(("127.0.0.1", port), timeout=2))
            process.wait(timeout=9)
            self.assertEqual(process.returncode, 1)
            log = (directory / "server.log").read_text(errors="replace")
            self.assertIn("Fatal: listener repeatedly failed", log)
            self.assertLessEqual(log.count("accept failed:"), 2)
        finally:
            for sock in sockets:
                sock.close()
            if process.poll() is None:
                process.kill()
                process.wait(timeout=3)

    def test_head_errors_and_no_filesystem_routes(self):
        status, headers, body = self.request("/", "HEAD")
        self.assertEqual(status, 200)
        self.assertEqual(body, b"")
        self.assertGreater(int(headers["Content-Length"]), 0)
        self.assertEqual(self.request("/", "POST")[0], 405)
        self.assertEqual(self.request("/", "POST")[1]["Allow"], "GET, HEAD")
        for path in ("/missing.js", "/settings", "/../etc/passwd", "/%2e%2e/etc/passwd"):
            self.assertEqual(self.request(path)[0], 404)

    def test_partial_and_invalid_requests_then_recovery(self):
        with socket.create_connection(("127.0.0.1", self.port), timeout=5) as sock:
            sock.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
            sock.sendall(b"\r\n")
            self.assertIn(b"200 OK", sock.recv(1024))
        for raw in (b"nonsense\r\n\r\n", b"GET / HTTP/9.9\r\n\r\n", b"x" * 9000):
            with socket.create_connection(("127.0.0.1", self.port), timeout=5) as sock:
                sock.sendall(raw)
                self.assertIn(b"400 Bad Request", sock.recv(1024))
        self.assertEqual(self.request("/")[0], 200)

    def test_second_instance_uses_free_port_and_preserves_first(self):
        directory = self.directory / "second-instance"
        directory.mkdir()
        shutil.copy(self.directory / "server", directory / "server")
        shutil.copy(self.directory / "port", directory / "port")
        second, port = self.start_server(directory)
        try:
            self.assertNotEqual(port, self.port)
            self.assertIn("occupied", (directory / "server.log").read_text())
            self.assertEqual(self.request("/", port=port)[0], 200)
            self.assertEqual(self.request("/")[0], 200)
        finally:
            second.terminate()
            second.communicate(timeout=5)

    def test_webfirst_requires_reserved_port_and_does_not_start_browser(self):
        directory = self.directory / "webfirst"
        directory.mkdir()
        self.compile_server(self.preview, directory, webfirst=True)
        port = int((directory / "port").read_text())
        # A port conflict must be visible and never silently select a random
        # port (which would break a Media tile linked to 8097).
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as reservation:
            reservation.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            reservation.bind(("127.0.0.1", port))
            reservation.listen()
            failed = subprocess.run([str(directory / "server")], cwd=directory,
                                    capture_output=True, timeout=7)
        self.assertNotEqual(failed.returncode, 0)
        self.assertIn(b"Web-first port", failed.stderr)
        self.assertNotIn(b"requesting a free port", failed.stderr)

        proc, active_port = self.start_server(directory)
        try:
            self.assertEqual(active_port, port)
            self.assertEqual(self.request("/", port=port)[0], 200)
            self.assertEqual(self.request("/api/wolodrive/health", port=port)[0], 200)
            self.assertIn("wolodrive-webfirst", self.request(
                "/api/wolodrive/health", port=port)[2].decode("utf8"))
            self.assertIn("max-age", self.request(
                "/obsidian-pages/fixture-abcdef123456-000-0123456789.json", port=port)[1]["Cache-Control"])
            # The recovery service must also accept connections beyond 127.0.0.1.
            self.assertEqual(self.request("/", port=port, host="127.0.0.2")[0], 200)
            duplicate = subprocess.run([str(directory / "server")], cwd=directory,
                                       capture_output=True, timeout=6)
            self.assertEqual(duplicate.returncode, 0)
            self.assertIn(b"Reusing running server", duplicate.stderr)
            self.assertIn(b"LAN catalogue", (directory / "server.log").read_bytes())
            self.assertIn(b"browser launch disabled", (directory / "server.log").read_bytes())
        finally:
            proc.terminate()
            proc.communicate(timeout=5)

    def test_same_build_relaunch_reuses_running_server(self):
        second = subprocess.run([str(self.directory / "server")], cwd=self.directory,
                                capture_output=True, timeout=5)
        self.assertEqual(second.returncode, 0)
        self.assertIn(b"Reusing running server", second.stderr)
        self.assertEqual(self.request("/")[0], 200)

    def test_single_installer_quiet_service_serves_ui_without_opening_browser(self):
        directory = self.directory / "single-service"
        directory.mkdir()
        stub = directory / "browser.c"
        stub.write_text('#include <stdio.h>\nint sceSystemServiceLaunchWebBrowser(const char *url) {'
                        '(void)url; FILE *f=fopen("browser-opened", "w"); if(f)fclose(f); return 0;}\n')
        self.compile_server(self.preview, directory, browser_stub=stub, single_service=True)
        proc, port = self.start_server(directory)
        try:
            self.assertEqual(self.request("/", port=port)[2], (self.preview / "index.html").read_bytes())
            self.assertIn(b"wolodrive-webfirst", self.request("/api/wolodrive/health", port=port)[2])
            self.assertIn("browser launch disabled", (directory / "server.log").read_text())
            self.assertFalse((directory / "browser-opened").exists())
            self.assertEqual(self.request("/", port=port, host="127.0.0.2")[2],
                             (self.preview / "index.html").read_bytes())
            self.assertIn(b"wolodrive-webfirst-v2-files-lan", self.request(
                "/api/wolodrive/health", port=port, host="127.0.0.2")[2])
            duplicate = subprocess.run([str(directory / "server")], cwd=directory,
                                       capture_output=True, timeout=6)
            self.assertEqual(duplicate.returncode, 0)
            self.assertFalse((directory / "browser-opened").exists())
        finally:
            proc.terminate(); proc.communicate(timeout=5)

    def test_fixed_port_never_reuses_same_build_on_another_port(self):
        first = self.directory / "fixed-first"
        second = self.directory / "fixed-second"
        first.mkdir(); second.mkdir()
        self.compile_server(self.preview, first, webfirst=True)
        self.compile_server(self.preview, second, webfirst=True)
        proc_a, port_a = self.start_server(first)
        proc_b = None
        try:
            shutil.copy(first / "wolodrive-web.endpoint", second / "wolodrive-web.endpoint")
            proc_b, port_b = self.start_server(second)
            self.assertNotEqual(port_a, port_b)
            self.assertEqual(port_b, int((second / "port").read_text()))
            self.assertEqual(self.request("/", port=port_b)[0], 200)
        finally:
            if proc_b:
                proc_b.terminate(); proc_b.communicate(timeout=5)
            proc_a.terminate(); proc_a.communicate(timeout=5)

    def test_listener_accepts_connections_beyond_console_loopback_address(self):
        # A server bound only to 127.0.0.1 cannot accept 127.0.0.2 connections.
        status, _, body = self.request("/", host="127.0.0.2")
        self.assertEqual(status, 200)
        self.assertEqual(body, (self.preview / "index.html").read_bytes())
        # Remote control has not been implemented: no action route may claim success.
        self.assertEqual(self.request("/api/install", method="POST", host="127.0.0.2")[0], 405)

    def test_real_static_preview(self):
        preview = ROOT / "static-preview"
        self.assertTrue((preview / "index.html").is_file(), "Run npm run build:static first")
        directory = self.directory / "actual"
        directory.mkdir()
        self.compile_server(preview, directory)
        # A record pointing at an older/different preview must not be reused.
        (directory / "wolodrive-web.endpoint").write_text(str(self.port))
        proc, port = self.start_server(directory)
        try:
            for file in preview.rglob("*"):
                if file.is_file():
                    path = "/" + quote(file.relative_to(preview).as_posix(), safe="/-._~")
                    status, _, body = self.request(path, port=port)
                    self.assertEqual(status, 200, path)
                    self.assertEqual(body, file.read_bytes(), path)
        finally:
            proc.terminate()
            proc.communicate(timeout=5)

    def test_blocked_browser_launch_cannot_block_http_or_shutdown(self):
        directory = self.directory / "blocked-browser"
        directory.mkdir()
        stub = directory / "browser_stub.c"
        stub.write_text('''
#include <stdio.h>
#include <unistd.h>
int sceSystemServiceLaunchWebBrowser(const char *url) {
    FILE *marker = fopen("browser-called.txt", "w");
    if (marker) { fputs(url, marker); fclose(marker); }
    for (;;) sleep(1); /* Simulate the pending PS5 call from the supplied log. */
    return 0;
}
''')
        self.compile_server(self.preview, directory, browser_stub=stub)
        reservation = socket.socket()
        reservation.bind(("127.0.0.1", int((directory / "port").read_text())))
        reservation.listen()
        proc, port = self.start_server(directory)
        try:
            marker = directory / "browser-called.txt"
            for _ in range(100):
                if marker.exists() and marker.read_text():
                    break
                time.sleep(.01)
            self.assertEqual(marker.read_text(), f"http://127.0.0.1:{port}/")
            self.assertIsNone(proc.poll())
            self.assertEqual(self.request("/", port=port)[0], 200)
            self.assertEqual(self.request("/assets/test.js", port=port)[2],
                             (self.preview / "assets/test.js").read_bytes())
        finally:
            proc.terminate()
            proc.communicate(timeout=5)
            reservation.close()
            log = (directory / "server.log").read_bytes()
        self.assertEqual(proc.returncode, 0)
        self.assertIn(b"HTTP request loop active", log)
        self.assertNotIn(b"Browser launch returned", log)


if __name__ == "__main__":
    unittest.main()
