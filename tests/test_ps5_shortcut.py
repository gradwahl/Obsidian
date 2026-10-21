"""Test shortcut metadata, title ownership and registration error handling."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ShortcutTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="wolodrive-shortcut-")
        self.directory = Path(self.temp.name)
        self.app_root = self.directory / "app"
        self.app_root.mkdir()
        self.system_root = self.directory / "system_app"
        self.system_root.mkdir()
        self.log = self.directory / "install.log"
        self.status = self.directory / "shortcut.status"
        (self.directory / "ps5").mkdir()
        (self.directory / "ps5/kernel.h").write_text(
            '#include <stdint.h>\n#include <sys/types.h>\n'
            'int kernel_dynlib_handle(pid_t, const char *, uint32_t *);\n'
            'intptr_t kernel_dynlib_resolve(pid_t, uint32_t, const char *);\n')
        subprocess.run([sys.executable, str(ROOT / "scripts/generate_ps5_shortcut_icon.py"),
                        str(self.directory / "icon.png"), str(self.directory / "ps5_shortcut_icon.h")], check=True)
        stub = self.directory / "stub.c"
        stub.write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
int webapp_begin_write(FILE *log) {
    fputs("MOUNT_BEGIN\n", log);
    if (getenv("WOLO_INJECT_WRITE_FAILURE")) mkdir(WOLO_SYSTEM_APP_ROOT "/WOLO00001", 0755);
    return getenv("WOLO_FAIL_MOUNT") ? -1 : 0;
}
int webapp_end_write(FILE *log) {
    fputs("MOUNT_END\n", log); return getenv("WOLO_FAIL_RESTORE") ? -1 : 0;
}
int sceAppInstUtilInitialize(void) { puts("INITIALIZE"); return 0; }
int sceAppInstUtilTerminate(void) { puts("TERMINATE"); return 0; }
int sceAppInstUtilAppInstallTitleDir(const char *title, const char *root, void *params) {
    assert(strcmp(title, "WOLO00001") == 0 && root[0] == '/' && params == NULL);
    puts("REGISTER"); return getenv("WOLO_FAIL_REGISTER") ? -5 : 0;
}
int kernel_dynlib_handle(pid_t pid, const char *name, uint32_t *handle) {
    assert(pid > 0 && strcmp(name, "libSceAppInstUtil.sprx") == 0);
    *handle = 19; return 0;
}
intptr_t kernel_dynlib_resolve(pid_t pid, uint32_t handle, const char *nid) {
    assert(pid > 0 && handle == 19 && strcmp(nid, "Wudg3Xe3heE") == 0);
    return getenv("WOLO_NO_TITLE_DIR") ? 0 : (intptr_t)sceAppInstUtilAppInstallTitleDir;
}
int sceAppInstUtilAppInstallAll(void *params) {
    assert(params == NULL); puts("REGISTRATION_SCAN");
    return getenv("WOLO_FAIL_REGISTER") ? -5 : 0;
}
''')
        self.executable = self.directory / "installer"
        self.embedded_executable = self.directory / "embedded-installer"
        (self.directory / "ps5_app_payload.h").write_text(
            'static const unsigned char app_payload_elf[] = {127,69,76,70,1,2,3};\n')
        self.webfirst_executable = self.directory / "webfirst-installer"
        self.fullscreen_executable = self.directory / "fullscreen-installer"
        self.restore_executable = self.directory / "restore-installer"
        self.single_executable = self.directory / "single-installer"
        common = ["gcc", "-D__PS5__", "-Wall", "-Wextra", "-Werror", "-I", str(self.directory),
                  f'-DWOLO_APP_ROOT="{self.app_root}"',
                  f'-DWOLO_SYSTEM_APP_ROOT="{self.system_root}"', "-DWOLO_WEBAPP_MOUNT_TEST", f'-DWOLO_INSTALL_LOG="{self.log}"',
                  f'-DSHORTCUT_STATUS="{self.status}"']
        sources = [str(ROOT / "src/web_shortcut_installer.c"), str(stub)]
        subprocess.run([*common, *sources, "-o", str(self.executable)], check=True)
        subprocess.run([*common, '-DWOLO_EMBED_APP',
                        f'-DPOLDRIVE_APP_PATH="{self.directory}/dashboard.elf"',
                        *sources, '-o', str(self.embedded_executable)], check=True)
        subprocess.run([*common, "-DWOLO_SHORTCUT_WEBFIRST", *sources,
                        "-o", str(self.webfirst_executable)], check=True)
        subprocess.run([*common, "-DWOLO_SINGLE_SHORTCUT", *sources,
                        "-o", str(self.single_executable)], check=True)
        subprocess.run([*common, "-DWOLO_SINGLE_SHORTCUT", "-DWOLO_FULLSCREEN_RESTORE", *sources,
                        "-o", str(self.restore_executable)], check=True)
        subprocess.run([*common, "-DWOLO_SINGLE_SHORTCUT", "-DWOLO_FULLSCREEN_WEBAPP", *sources,
                        "-o", str(self.fullscreen_executable)], check=True)

    def tearDown(self):
        self.temp.cleanup()

    def run_installer(self, webfirst=False, **env):
        return subprocess.run([str(self.webfirst_executable if webfirst else self.executable)],
                              capture_output=True, env={**os.environ, **env}, timeout=5)

    def test_embedded_installer_saves_runtime_and_repairs_missing_dashboard_entry(self):
        for attempt in range(2):
            result = subprocess.run([str(self.embedded_executable)], capture_output=True)
            self.assertEqual(result.returncode, 0)
            self.assertIn(b'REGISTER', result.stdout)
            self.assertEqual((self.directory/'dashboard.elf').read_bytes(), b'\x7fELF\x01\x02\x03')
            self.assertEqual(self.status.read_text(), 'INSTALLED\n')
        metadata=json.loads((self.app_root/'WOLO00001/sce_sys/param.json').read_text())
        self.assertTrue(metadata['deeplinkUri'].endswith('/dashboard.elf'))

    def test_default_dashboard_target_is_inside_payload_manager_storage(self):
        self.assertEqual(self.run_installer().returncode, 0)
        metadata=json.loads((self.app_root/'WOLO00001/sce_sys/param.json').read_text())
        target=metadata['deeplinkUri'].split('/loadpayload:', 1)[1]
        self.assertTrue(target.startswith('/data/pldmgr/payloads/'))
        self.assertEqual(Path(target).name, 'poldrive-app.elf')
        self.assertNotEqual(Path(target).name, 'poldrive.elf')

    def test_fullscreen_experiment_can_restore_working_browser_card(self):
        self.assertEqual(subprocess.run([str(self.single_executable)]).returncode, 0)
        metadata = self.app_root / "WOLO00001/sce_sys/param.json"
        original = metadata.read_bytes()
        for _ in range(2):
            result = subprocess.run([str(self.fullscreen_executable)], capture_output=True)
            self.assertEqual(result.returncode, 0)
            self.assertIn(b"REGISTER", result.stdout)
            param = json.loads(metadata.read_text())
            self.assertEqual(param["titleId"], "WOLO00001")
            self.assertEqual(param["applicationCategoryType"], 66048)
            self.assertEqual(param["downloadDataSize"], 256)
            self.assertEqual(param["webAppUri"], "http://127.0.0.1:8098/")
            self.assertNotIn("deeplinkUri", param)
            system = self.system_root / "WOLO00001"
            self.assertEqual((system / "eboot.bin").read_bytes(), b"")
            self.assertEqual((system / "sce_sys/param.json").read_bytes(), metadata.read_bytes())
        self.assertEqual(subprocess.run([str(self.restore_executable)]).returncode, 0)
        self.assertFalse((self.system_root / "WOLO00001").exists())
        self.assertEqual(metadata.read_bytes(), original)

    def test_fullscreen_refuses_unowned_system_title_before_changing_user_metadata(self):
        self.assertEqual(subprocess.run([str(self.single_executable)], capture_output=True).returncode, 0)
        metadata = self.app_root / "WOLO00001/sce_sys/param.json"
        original = metadata.read_bytes()
        system = self.system_root / "WOLO00001"
        system.mkdir()
        (system / "eboot.bin").write_bytes(b"unrelated executable")
        result = subprocess.run([str(self.fullscreen_executable)], capture_output=True)
        self.assertEqual(result.returncode, 1)
        self.assertNotIn(b"INITIALIZE", result.stdout)
        self.assertEqual(metadata.read_bytes(), original)
        self.assertEqual((system / "eboot.bin").read_bytes(), b"unrelated executable")
        self.assertNotIn("MOUNT_BEGIN", self.log.read_text())

    def test_fullscreen_write_failure_still_restores_mount(self):
        result = subprocess.run([str(self.fullscreen_executable)], capture_output=True,
                                env={**os.environ, "WOLO_INJECT_WRITE_FAILURE": "1"})
        self.assertEqual(result.returncode, 1)
        self.assertNotIn(b"INITIALIZE", result.stdout)
        self.assertIn("MOUNT_BEGIN", self.log.read_text())
        self.assertIn("MOUNT_END", self.log.read_text())
        self.assertFalse(self.status.exists())
        self.assertFalse((self.app_root / "WOLO00001").exists())

    def test_fullscreen_mount_failure_does_not_prepare_any_title_files(self):
        result = subprocess.run([str(self.fullscreen_executable)], capture_output=True,
                                env={**os.environ, "WOLO_FAIL_MOUNT": "1"})
        self.assertEqual(result.returncode, 1)
        self.assertFalse((self.system_root / "WOLO00001").exists())
        self.assertFalse((self.app_root / "WOLO00001").exists())
        self.assertFalse(self.status.exists())

    def test_fullscreen_cannot_claim_success_if_mount_restore_fails(self):
        result = subprocess.run([str(self.fullscreen_executable)], capture_output=True,
                                env={**os.environ, "WOLO_FAIL_RESTORE": "1"})
        self.assertEqual(result.returncode, 1)
        self.assertIn("MOUNT_END", self.log.read_text())
        self.assertNotIn(b"INITIALIZE", result.stdout)
        self.assertFalse(self.status.exists())

    def test_restore_preserves_unexpected_system_files(self):
        self.assertEqual(subprocess.run([str(self.fullscreen_executable)], capture_output=True).returncode, 0)
        metadata = self.app_root / "WOLO00001/sce_sys/param.json"
        original = metadata.read_bytes()
        extra = self.system_root / "WOLO00001/keep-me.txt"
        extra.write_bytes(b"preserve")
        result = subprocess.run([str(self.restore_executable)], capture_output=True)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(extra.read_bytes(), b"preserve")
        self.assertEqual(metadata.read_bytes(), original)

    def test_restore_refuses_system_symlinks_without_touching_targets(self):
        self.assertEqual(subprocess.run([str(self.fullscreen_executable)], capture_output=True).returncode, 0)
        eboot = self.system_root / "WOLO00001/eboot.bin"
        target = self.directory / "unrelated-file"
        target.write_bytes(b"do not change")
        eboot.unlink()
        eboot.symlink_to(target)
        result = subprocess.run([str(self.restore_executable)], capture_output=True)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(target.read_bytes(), b"do not change")
        self.assertTrue(eboot.is_symlink())

    def test_embedded_runtime_write_failure_does_not_register(self):
        (self.directory/'dashboard.elf.tmp').mkdir()
        result=subprocess.run([str(self.embedded_executable)], capture_output=True)
        self.assertEqual(result.returncode, 1)
        self.assertNotIn(b'INITIALIZE', result.stdout)
        self.assertFalse(self.status.exists())
        self.assertIn('Cannot save dashboard app', self.log.read_text())

    def test_embedded_registration_failure_does_not_publish_success(self):
        result=subprocess.run([str(self.embedded_executable)], capture_output=True,
                              env={**os.environ, 'WOLO_FAIL_REGISTER':'1'})
        self.assertEqual(result.returncode, 1)
        self.assertFalse(self.status.exists())
        self.assertFalse((self.app_root/'WOLO00001/sce_sys/wolodrive-shortcut.registered').exists())

    def test_install_and_owned_update(self):
        for attempt in range(2):
            result = self.run_installer()
            self.assertEqual(result.returncode, 0)
            if attempt == 0:
                self.assertIn(b"REGISTER", result.stdout)
                self.assertEqual(self.status.read_text(), "INSTALLED\n")
            else:
                self.assertNotIn(b"INITIALIZE", result.stdout)
                self.assertEqual(self.status.read_text(), "CURRENT\n")
                self.assertIn("already current", self.log.read_text())
        system = self.app_root / "WOLO00001/sce_sys"
        param = json.loads((system / "param.json").read_text())
        self.assertEqual(param["titleId"], "WOLO00001")
        self.assertEqual(param["applicationCategoryType"], 65536)
        self.assertEqual(param["localizedParameters"]["en-US"]["titleName"], "PolDrive")
        self.assertEqual(param["deeplinkUri"],
                         "http://127.0.0.1:8084/loadpayload:/data/pldmgr/payloads/poldrive/poldrive-app.elf")
        self.assertEqual((system / "icon0.png").read_bytes(), (self.directory / "icon.png").read_bytes())

    def test_repairs_previously_registered_uppercase_folder(self):
        self.assertEqual(self.run_installer().returncode, 0)
        system = self.app_root / "WOLO00001/sce_sys"
        metadata = system / "param.json"
        metadata.write_text(metadata.read_text().replace('/data/pldmgr/payloads/poldrive/poldrive-app.elf', '/data/pldmgr/payloads/Poldrive/poldrive.elf'))
        self.assertTrue((system / "wolodrive-shortcut.registered").exists())
        result = self.run_installer()
        self.assertEqual(result.returncode, 0)
        self.assertIn(b"REGISTER", result.stdout)
        self.assertIn('/data/pldmgr/payloads/poldrive/poldrive-app.elf', json.loads(metadata.read_text())["deeplinkUri"])
        self.assertIn('Media launch target: /data/pldmgr/payloads/poldrive/poldrive-app.elf', self.log.read_text())
        self.assertNotIn(b"INITIALIZE", self.run_installer().stdout)

    def test_webfirst_direct_tile_and_explicit_rollback(self):
        # Install the original tile, then migrate to web-first using only the
        # owned WOLO00001 metadata. A second run must be idempotent.
        self.assertEqual(self.run_installer().returncode, 0)
        metadata = self.app_root / "WOLO00001/sce_sys/param.json"
        self.assertIn("/loadpayload:", json.loads(metadata.read_text())["deeplinkUri"])

        upgraded = self.run_installer(webfirst=True)
        self.assertEqual(upgraded.returncode, 0)
        self.assertIn(b"REGISTER", upgraded.stdout)
        self.assertEqual(self.status.read_text(), "INSTALLED\n")
        self.assertEqual(json.loads(metadata.read_text())["deeplinkUri"],
                         "http://127.0.0.1:8097/")
        self.assertIn("Web-first Media target:", self.log.read_text())

        repeat = self.run_installer(webfirst=True)
        self.assertEqual(repeat.returncode, 0)
        self.assertNotIn(b"INITIALIZE", repeat.stdout)
        self.assertEqual(self.status.read_text(), "CURRENT\n")

        rollback = self.run_installer()
        self.assertEqual(rollback.returncode, 0)
        self.assertIn(b"REGISTER", rollback.stdout)
        self.assertIn("/loadpayload:", json.loads(metadata.read_text())["deeplinkUri"])

    def test_webfirst_does_not_claim_success_on_registration_error(self):
        first = self.run_installer()
        self.assertEqual(first.returncode, 0)
        failed = self.run_installer(webfirst=True, WOLO_FAIL_REGISTER="1")
        self.assertEqual(failed.returncode, 1)
        self.assertFalse((self.app_root / "WOLO00001/sce_sys/wolodrive-shortcut.registered").exists())
        retry = self.run_installer(webfirst=True)
        self.assertEqual(retry.returncode, 0)
        metadata = self.app_root / "WOLO00001/sce_sys/param.json"
        self.assertEqual(json.loads(metadata.read_text())["deeplinkUri"],
                         "http://127.0.0.1:8097/")

    def test_refuses_unowned_existing_title(self):
        system = self.app_root / "WOLO00001/sce_sys"
        system.mkdir(parents=True)
        original = b"existing unrelated title"
        (system / "param.json").write_bytes(original)
        result = self.run_installer()
        self.assertEqual(result.returncode, 1)
        self.assertNotIn(b"INITIALIZE", result.stdout)
        self.assertEqual((system / "param.json").read_bytes(), original)

    def test_changed_metadata_retries_registration_after_failure(self):
        self.assertEqual(self.run_installer().returncode, 0)
        system = self.app_root / "WOLO00001/sce_sys"
        (system / "param.json").write_text('{}')
        self.assertEqual(self.run_installer(WOLO_FAIL_REGISTER="1").returncode, 1)
        self.assertFalse((system / "wolodrive-shortcut.registered").exists())
        retry = self.run_installer()
        self.assertEqual(retry.returncode, 0)
        self.assertIn(b"REGISTER", retry.stdout)

    def test_registration_failure_is_not_success(self):
        result = self.run_installer(WOLO_FAIL_REGISTER="1")
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"TERMINATE", result.stdout)
        self.assertNotIn("Shortcut registered", self.log.read_text())
        self.assertFalse(self.status.exists())

    def test_missing_private_api_uses_registration_scan(self):
        result = self.run_installer(WOLO_NO_TITLE_DIR="1")
        self.assertEqual(result.returncode, 0)
        self.assertIn(b"REGISTRATION_SCAN", result.stdout)
        self.assertNotIn(b"REGISTER\n", result.stdout)
        self.assertIn(b"TERMINATE", result.stdout)
        (self.app_root / "WOLO00001/sce_sys/wolodrive-shortcut.registered").unlink()
        result = self.run_installer(WOLO_NO_TITLE_DIR="1", WOLO_FAIL_REGISTER="1")
        self.assertEqual(result.returncode, 1)
        self.assertNotIn("Shortcut registered", self.log.read_text())


if __name__ == "__main__":
    unittest.main()
