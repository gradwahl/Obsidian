"""Exercise the single ELF through real loopback loader/health connections.

The loader simulates PS5 process loading; dashboard registration calls are
stubbed, but real installer/config/shortcut C code and ELF bytes are used.
"""
import http.server
import json
import os
from pathlib import Path
import socket
import socketserver
import subprocess
import sys
import tempfile
import threading
import unittest

ROOT = Path(__file__).resolve().parents[1]
HEALTH = b"wolodrive-webfirst-v2-files-lan " + b"a" * 64 + b"\n"
SERVICE = b"\x7fELFquiet-service-fixture"


class SingleInstaller(unittest.TestCase):
    fullscreen = False

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="obsidian-single-")
        self.folder = Path(self.temp.name)
        self.app_root = self.folder / "app"
        self.app_root.mkdir()
        self.system_root = self.folder / "system_app"
        self.system_root.mkdir()
        self.title = self.app_root / "WOLO00001"
        self.config = self.folder / "pldmgr_config.txt"
        self.autoload = self.folder / "autoload.txt"
        self.service = self.folder / "obsidian-web-service.elf"
        self.status = self.folder / "shortcut.status"
        self.original_list = b"local/ftp\n!1000\nlocal/another-payload\n"
        self.original_config = b"AUTOLOAD_ENABLED=0\nAUTO_BROWSER_OPEN=0\nAUTOLOAD_DELAY=7\nUNKNOWN=42\n"
        self.autoload.write_bytes(self.original_list)
        self.config.write_bytes(self.original_config)
        self.transfers = []
        self.fail_registration = False
        self.start_service = True
        self.health_body = HEALTH
        self.health_started = False
        self.errors = []
        owner = self

        class HealthHandler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                if self.path != "/api/wolodrive/health":
                    owner.errors.append("unexpected health request " + self.path)
                self.protocol_version = "HTTP/1.1"
                self.send_response(200)
                self.send_header("Content-Length", str(len(owner.health_body)))
                self.send_header("Connection", "close")
                self.end_headers()
                self.wfile.write(owner.health_body)

            def log_message(self, *_):
                pass

        # Bound but not listening until the quiet-service ELF is uploaded.
        self.health = http.server.ThreadingHTTPServer(("127.0.0.1", 0), HealthHandler,
                                                       bind_and_activate=False)
        self.health.server_bind()

        class LoaderHandler(socketserver.BaseRequestHandler):
            def handle(self):
                try:
                    payload = bytearray()
                    while True:
                        data = self.request.recv(65536)
                        if not data:
                            break
                        payload.extend(data)
                    if payload == SERVICE:
                        owner.transfers.append("SERVICE")
                        if owner.start_service:
                            owner.start_health()
                    elif payload == owner.helper.read_bytes():
                        owner.transfers.append("SHORTCUT")
                        env = dict(os.environ)
                        env["FAIL_REGISTER"] = "1" if owner.fail_registration else "0"
                        result = subprocess.run([str(owner.helper)], env=env, capture_output=True)
                        if result.returncode not in (0, 1):
                            owner.errors.append(result.stderr.decode())
                    else:
                        owner.errors.append("unexpected loader bytes")
                except Exception as error:
                    owner.errors.append(repr(error))

        self.loader = socketserver.ThreadingTCPServer(("127.0.0.1", 0), LoaderHandler)
        self.loader_thread = threading.Thread(target=self.loader.serve_forever, daemon=True)
        self.loader_thread.start()
        subprocess.run([sys.executable, str(ROOT / "scripts/generate_ps5_shortcut_icon.py"),
                        str(self.folder / "icon.png"), str(self.folder / "ps5_shortcut_icon.h")], check=True)
        stub = self.folder / "appinst.c"
        stub.write_text('''#include <stdlib.h>
int sceAppInstUtilInitialize(void) { return 0; }
int sceAppInstUtilTerminate(void) { return 0; }
int sceAppInstUtilAppInstallTitleDir(const char *title, const char *root, void *params) {
    (void)title; (void)root; (void)params;
    return getenv("FAIL_REGISTER")[0] == '1' ? -1 : 0;
}
''')
        self.helper = self.folder / "helper"
        variant = ["-DWOLO_FULLSCREEN_WEBAPP"] if self.fullscreen else []
        subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", "-DWOLO_SINGLE_SHORTCUT", *variant,
                        f'-DWOLO_APP_ROOT="{self.app_root}"',
                        f'-DWOLO_SYSTEM_APP_ROOT="{self.system_root}"',
                        f'-DWOLO_INSTALL_LOG="{self.folder}/shortcut.log"',
                        f'-DSHORTCUT_STATUS="{self.status}"', "-I", str(self.folder),
                        str(ROOT / "src/web_shortcut_installer.c"), str(stub),
                        "-o", str(self.helper)], check=True)
        shortcut_header = "ps5_fullscreen_shortcut_helper.h" if self.fullscreen else "ps5_shortcut_helper.h"
        service_header = "ps5_fullscreen_service_payload.h" if self.fullscreen else "ps5_service_payload.h"
        subprocess.run([sys.executable, str(ROOT / "scripts/embed_shortcut_helper.py"),
                        str(self.helper), str(self.folder / shortcut_header)], check=True)
        (self.folder / service_header).write_text(
            'static const unsigned char service_payload_elf[] = {' +
            ','.join(str(n) for n in SERVICE) + '};\n' +
            '#define OBSIDIAN_EXPECTED_HEALTH "' + HEALTH.decode().strip() + '\\n"\n')
        definitions = [*variant, f'-DOBSIDIAN_LOADER_PORT={self.loader.server_address[1]}',
                       f'-DOBSIDIAN_SHORTCUT_STATUS="{self.status}"',
                       f'-DOBSIDIAN_LAUNCH_LOG="{self.folder}/launcher.log"',
                       '-DOBSIDIAN_SHORTCUT_WAIT_ATTEMPTS=5']
        obj = self.folder / "launcher.o"
        subprocess.run(["gcc", "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Werror",
                        *definitions, "-Dmain=obsidian_install_shortcut", "-I", str(self.folder),
                        "-c", str(ROOT / "src/ps5_unified_launcher.c"), "-o", str(obj)], check=True)
        self.binary = self.folder / "obsidian.elf"
        subprocess.run(["gcc", "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Werror", *definitions,
                        f'-DOBSIDIAN_INSTALL_LOG="{self.folder}/install.log"',
                        f'-DOBSIDIAN_TITLE_ROOT="{self.title}"',
                        f'-DOBSIDIAN_SERVICE_PATH="{self.service}"',
                        f'-DOBSIDIAN_AUTOLOAD_PATH="{self.autoload}"',
                        f'-DOBSIDIAN_CONFIG_PATH="{self.config}"',
                        f'-DOBSIDIAN_HEALTH_PORT={self.health.server_address[1]}',
                        '-DOBSIDIAN_READY_ATTEMPTS=10', '-DOBSIDIAN_READY_DELAY_US=1000',
                        '-I', str(self.folder), str(ROOT / "src/ps5_single_installer.c"), str(obj),
                        "-o", str(self.binary)], check=True)

    def start_health(self):
        if not self.health_started:
            self.health.server_activate()
            self.health_thread = threading.Thread(target=self.health.serve_forever, daemon=True)
            self.health_thread.start()
            self.health_started = True

    def run_installer(self):
        result = subprocess.run([str(self.binary)], capture_output=True, timeout=8)
        self.assertFalse(self.errors)
        return result

    def tearDown(self):
        self.loader.shutdown()
        self.loader.server_close()
        self.loader_thread.join()
        if self.health_started:
            self.health.shutdown()
            self.health_thread.join()
        self.health.server_close()
        self.temp.cleanup()

    def assert_no_dashboard_change(self):
        self.assertFalse(self.status.exists())
        self.assertFalse((self.title / "sce_sys/param.json").exists())

    def test_one_elf_installs_starts_configures_and_registers_without_ui(self):
        self.assertEqual(self.run_installer().returncode, 0)
        self.assertEqual(self.transfers, ["SERVICE", "SHORTCUT"])
        self.assertEqual(self.service.read_bytes(), SERVICE)
        self.assertTrue(self.service.stat().st_mode & 0o100)
        param = json.loads((self.title / "sce_sys/param.json").read_text())
        uri_field = "webAppUri" if self.fullscreen else "deeplinkUri"
        self.assertEqual(param[uri_field], "http://127.0.0.1:8098/")
        self.assertNotIn("loadpayload", param[uri_field])
        self.assertEqual(param["applicationCategoryType"], 66048 if self.fullscreen else 65536)
        if self.fullscreen:
            self.assertEqual(param["downloadDataSize"], 256)
            self.assertNotIn("deeplinkUri", param)
            system = self.system_root / "WOLO00001"
            self.assertEqual((system / "eboot.bin").read_bytes(), b"")
            self.assertEqual((system / "sce_sys/param.json").read_bytes(),
                             (self.title / "sce_sys/param.json").read_bytes())
        else:
            self.assertNotIn("webAppUri", param)
            self.assertNotIn("downloadDataSize", param)
        self.assertEqual(self.autoload.read_bytes(), self.original_list + str(self.service).encode() + b"\n")
        self.assertEqual(self.config.read_bytes(), self.original_config.split(b"\n", 1)[1] + b"AUTOLOAD_ENABLED=1\n")
        for path, original in ((self.autoload, self.original_list), (self.config, self.original_config)):
            self.assertEqual(Path(str(path) + ".obsidian-backup").read_bytes(), original)
        self.assertIn("No browser/native UI started", (self.folder / "install.log").read_text())

    def test_repeated_install_reuses_exact_service_and_preserves_backups(self):
        self.assertEqual(self.run_installer().returncode, 0)
        self.assertEqual(self.run_installer().returncode, 0)
        self.assertEqual(self.transfers, ["SERVICE", "SHORTCUT", "SHORTCUT"])
        self.assertEqual(self.autoload.read_text().count(str(self.service)), 1)
        self.assertEqual(Path(str(self.config) + ".obsidian-backup").read_bytes(), self.original_config)

    def test_unrelated_service_or_stale_build_never_replaces_dashboard(self):
        self.health_body = HEALTH.replace(b"a", b"b")
        self.start_health()
        self.assertEqual(self.run_installer().returncode, 1)
        self.assertEqual(self.transfers, [])
        self.assertFalse(self.service.exists())
        self.assertEqual(self.config.read_bytes(), self.original_config)
        self.assert_no_dashboard_change()

    def test_old_loopback_service_with_same_assets_is_not_reused(self):
        self.health_body = HEALTH.replace(b"webfirst-v2", b"webfirst-v1")
        self.start_health()
        self.assertEqual(self.run_installer().returncode, 1)
        self.assertEqual(self.transfers, [])
        self.assertFalse(self.service.exists())
        self.assertEqual(self.config.read_bytes(), self.original_config)
        self.assert_no_dashboard_change()
        self.assertIn("different service/build", (self.folder / "install.log").read_text())

    def test_failed_service_start_never_configures_or_registers(self):
        self.start_service = False
        self.assertEqual(self.run_installer().returncode, 1)
        self.assertEqual(self.transfers, ["SERVICE"])
        self.assertEqual(self.config.read_bytes(), self.original_config)
        self.assertEqual(self.autoload.read_bytes(), self.original_list)
        self.assert_no_dashboard_change()

    def test_binary_config_is_preserved_and_cancels_registration(self):
        original = b"AUTOLOAD_ENABLED=0\x00secret\n"
        self.config.write_bytes(original)
        self.assertEqual(self.run_installer().returncode, 1)
        self.assertEqual(self.config.read_bytes(), original)
        self.assertEqual(self.autoload.read_bytes(), self.original_list)
        self.assert_no_dashboard_change()

    def test_registration_failure_does_not_claim_success(self):
        self.fail_registration = True
        self.assertEqual(self.run_installer().returncode, 1)
        self.assertFalse(self.status.exists())
        self.assertFalse((self.title / "sce_sys/wolodrive-shortcut.registered").exists())
        self.assertIn("registration unconfirmed", (self.folder / "install.log").read_text())

    def test_unowned_title_prevents_service_and_startup_changes(self):
        self.title.mkdir()
        self.assertEqual(self.run_installer().returncode, 1)
        self.assertEqual(self.transfers, [])
        self.assertFalse(self.service.exists())
        self.assertEqual(self.config.read_bytes(), self.original_config)

    def test_existing_service_entry_and_delays_are_preserved_without_duplicates(self):
        original = b"!1000\n  " + str(self.service).encode() + b" \r\nlocal/ftp"
        self.autoload.write_bytes(original)
        self.config.write_bytes(b"  AUTOLOAD_ENABLED = 0\r\nOTHER=1")
        self.assertEqual(self.run_installer().returncode, 0)
        self.assertEqual(self.autoload.read_bytes(), original)
        self.assertEqual(self.config.read_bytes(), b"OTHER=1\nAUTOLOAD_ENABLED=1\n")

    def test_invalid_delay_cancels_without_changing_existing_files(self):
        original = b"!not-a-delay\nlocal/ftp\n"
        self.autoload.write_bytes(original)
        self.assertEqual(self.run_installer().returncode, 1)
        self.assertEqual(self.autoload.read_bytes(), original)
        self.assertEqual(self.config.read_bytes(), self.original_config)
        self.assert_no_dashboard_change()

    def test_service_write_failure_never_configures_or_registers(self):
        self.service.mkdir()
        self.assertEqual(self.run_installer().returncode, 1)
        self.assertEqual(self.transfers, [])
        self.assertEqual(self.config.read_bytes(), self.original_config)
        self.assert_no_dashboard_change()

    def test_first_time_payload_manager_configuration(self):
        self.config.unlink()
        self.autoload.unlink()
        self.assertEqual(self.run_installer().returncode, 0)
        self.assertEqual(self.config.read_bytes(), b"AUTOLOAD_ENABLED=1\n")
        self.assertEqual(self.autoload.read_bytes(), str(self.service).encode() + b"\n")


class FullscreenSingleInstaller(SingleInstaller):
    """The same lifecycle/failure proofs must hold for the metadata experiment."""
    fullscreen = True


class ServiceEmbedding(unittest.TestCase):
    def test_bundle_uses_exact_elf_and_preview_identity_and_rejects_invalid_inputs(self):
        with tempfile.TemporaryDirectory(prefix="obsidian-embed-service-") as temporary:
            folder = Path(temporary)
            source = folder / "service.elf"
            source.write_bytes(SERVICE)
            assets = folder / "assets.h"
            assets.write_text('/* generated */\n#include <stddef.h>\n'
                              'static const char web_build_id[] = "' + 'a'*64 + '";\n')
            target = folder / "service.h"
            command = [sys.executable, str(ROOT / "scripts/embed_ps5_service.py"),
                       str(source), str(assets), str(target)]
            subprocess.run(command, check=True)
            header = target.read_text()
            body = header.split('{', 1)[1].split('}', 1)[0]
            self.assertEqual(bytes(int(n) for n in body.split(',') if n.strip()), SERVICE)
            self.assertIn(HEALTH.decode().strip(), header)
            source.write_bytes(b"not ELF")
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            source.write_bytes(SERVICE)
            assets.write_text('invalid assets')
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)


if __name__ == "__main__":
    unittest.main()
